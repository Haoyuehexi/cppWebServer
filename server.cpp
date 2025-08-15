#include "server.h"
#include "common/config.h"
#include "common/log.h"
#include "common/util.h"
#include "database/db_connection_pool.h"
#include "http/http_parser.h"
#include "http/http_request.h"
#include "http/http_response.h"
#include "net/connection.h"
#include "net/event_loop.h"
#include "net/server.h"
#include "net/thread_pool.h"
#include "net/timer.h"

#include <csignal>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// 全局服务器指针，用于信号处理
static WebServer *g_server = nullptr;

static void signalHandler(int sig) {
    if (g_server) {
        LOG_INFO("Received signal " + std::to_string(sig) +
                 ", shutting down...");
        g_server->stop();
    }
}

WebServer::WebServer()
    : host("0.0.0.0"), port(8080), thread_count(4), max_connections(1000),
      timeout_ms(30000), keep_alive_enabled(true), document_root("./resources"),
      default_page("index.html"), max_request_size(1024 * 1024) // 1MB
      ,
      database_enabled(false), db_host("localhost"), db_port(3306),
      db_pool_size(10), running(false), active_connections(0),
      total_requests(0), total_responses(0) {

    initializeMimeTypes();
    g_server = this;

    // 设置默认处理器
    setDefaultHandler([this](const HttpRequest &req, HttpResponse &resp) {
        handleStaticFile(req, resp);
    });
}

WebServer::~WebServer() {
    stop();
    g_server = nullptr;
}

bool WebServer::loadConfig(const std::string &config_file) {
    Config config;
    if (!ConfigLoader::load(config_file, config)) {
        LOG_ERROR("Failed to load config file: " + config_file);
        return false;
    }

    try {
        // 服务器配置
        host = config.server.host;
        port = config.server.port;
        thread_count = config.server.thread_pool_size;
        max_connections = config.server.max_connections;
        timeout_ms = config.server.timeout_ms;
        keep_alive_enabled = config.server.keep_alive;

        // HTTP配置
        document_root = config.http.document_root;
        default_page = config.http.default_page;
        max_request_size =
            static_cast<size_t>(config.http.max_request_size_kb) * 1024;

        // 日志配置
        std::string log_level = config.logging.level;
        std::string log_file = config.logging.file;

        // 重新初始化日志系统
        Logger::init(log_file, log_level);

        // 数据库配置（如果启用）
        database_enabled = config.database.enable;
        if (database_enabled) {
            db_host = config.database.host;
            db_port = config.database.port;
            db_username = config.database.username;
            db_password = config.database.password;
            db_database = config.database.database;
            db_pool_size = config.database.connection_pool_size;

            LOG_INFO("Database enabled - Host: " + db_host + ":" +
                     std::to_string(db_port));
            LOG_INFO("Database: " + db_database +
                     ", Pool size: " + std::to_string(db_pool_size));
            LOG_INFO("Username: " + db_username +
                     " (password hidden for security)");

            // 这里可以初始化数据库连接池
            // 注意：实际的数据库连接信息不要记录到日志中（安全考虑）
        } else {
            LOG_INFO("Database disabled");
        }

        // 记录配置信息
        LOG_INFO("Configuration loaded successfully from " + config_file);
        LOG_INFO("Server: " + host + ":" + std::to_string(port));
        LOG_INFO("Thread pool size: " + std::to_string(thread_count));
        LOG_INFO("Max connections: " + std::to_string(max_connections));
        LOG_INFO("Document root: " + document_root);
        LOG_INFO("Keep-alive: " + (keep_alive_enabled
                                       ? std::string("enabled")
                                       : std::string("disabled")));
        LOG_INFO("Max request size: " +
                 std::to_string(max_request_size / 1024) + "KB");

        return true;

    } catch (const std::exception &e) {
        LOG_ERROR("Exception while loading config: " + std::string(e.what()));
        return false;
    }
}

void WebServer::addRoute(const std::string &path, RequestHandler handler) {
    route_handlers[path] = std::move(handler);
    LOG_INFO("Route added: " + path);
}

void WebServer::setDefaultHandler(RequestHandler handler) {
    default_handler = std::move(handler);
}

bool WebServer::start() {
    if (running.load()) {
        LOG_WARN("Server is already running");
        return false;
    }

    LOG_INFO("Starting WebServer...");

    // 初始化组件
    if (!initializeComponents()) {
        LOG_ERROR("Failed to initialize server components");
        return false;
    }

    // 设置信号处理
    setupSignalHandlers();

    // 启动TCP服务器
    tcp_server->start();

    running.store(true);

    LOG_INFO("WebServer started successfully on " + host + ":" +
             std::to_string(port));
    LOG_INFO("Document root: " + document_root);
    LOG_INFO("Thread pool size: " + std::to_string(thread_count));

    // 启动事件循环
    main_loop->loop();

    return true;
}

void WebServer::stop() {
    if (!running.load()) {
        return;
    }

    LOG_INFO("Stopping WebServer...");

    running.store(false);

    // 注意：你的Server类没有stop方法，所以我们主要通过停止事件循环来停止服务器
    // 如果需要的话，可以在你的Server类中添加stop方法

    // 等待现有连接处理完成
    int wait_count = 0;
    while (active_connections.load() > 0 && wait_count < 50) { // 最多等待5秒
        usleep(100000);                                        // 100ms
        wait_count++;
    }

    // 停止线程池
    if (thread_pool) {
        thread_pool->stop();
    }

    // 停止事件循环
    if (main_loop) {
        main_loop->quit();
    }

    LOG_INFO("WebServer stopped. Total requests processed: " +
             std::to_string(total_requests.load()));
}

bool WebServer::initializeComponents() {
    try {
        // 创建事件循环
        main_loop = std::make_unique<EventLoop>();

        // 创建线程池
        thread_pool = std::make_unique<ThreadPool>(thread_count);

        // 创建TCP服务器 - 适配你的构造函数参数
        tcp_server = std::make_unique<net::Server>(main_loop.get(), host,
                                                   static_cast<uint16_t>(port));

        // 设置连接回调 - 适配你的回调签名
        tcp_server->setNewConnCallback(
            [this](const std::shared_ptr<Connection> &conn) {
                onNewConnection(conn);
            });

        tcp_server->setMessageCallback(
            [this](const std::shared_ptr<Connection> &conn,
                   std::string_view msg) {
                onMessage(conn, std::string(msg));
            });

        // 初始化数据库连接池（如果启用）
        if (database_enabled) {
            try {
                db_pool = std::make_unique<DBConnectionPool>(
                    db_host, db_username, db_password, db_database, db_port,
                    db_pool_size);
                LOG_INFO("Database connection pool initialized successfully");
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to initialize database connection pool: " +
                          std::string(e.what()));
                return false;
            }
        }

        return true;

    } catch (const std::exception &e) {
        LOG_ERROR("Exception in initializeComponents: " +
                  std::string(e.what()));
        return false;
    }
}

void WebServer::initializeMimeTypes() {
    mime_types[".html"] = "text/html";
    mime_types[".htm"] = "text/html";
    mime_types[".css"] = "text/css";
    mime_types[".js"] = "application/javascript";
    mime_types[".json"] = "application/json";
    mime_types[".xml"] = "application/xml";
    mime_types[".txt"] = "text/plain";

    mime_types[".jpg"] = "image/jpeg";
    mime_types[".jpeg"] = "image/jpeg";
    mime_types[".png"] = "image/png";
    mime_types[".gif"] = "image/gif";
    mime_types[".bmp"] = "image/bmp";
    mime_types[".ico"] = "image/x-icon";
    mime_types[".svg"] = "image/svg+xml";

    mime_types[".mp4"] = "video/mp4";
    mime_types[".avi"] = "video/avi";
    mime_types[".mov"] = "video/quicktime";

    mime_types[".mp3"] = "audio/mpeg";
    mime_types[".wav"] = "audio/wav";

    mime_types[".pdf"] = "application/pdf";
    mime_types[".zip"] = "application/zip";
}

void WebServer::setupSignalHandlers() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE
}

void WebServer::onNewConnection(const std::shared_ptr<Connection> &conn) {
    active_connections.fetch_add(1);

    // 设置连接的关闭回调
    conn->setCloseCallback(
        [this](const std::shared_ptr<Connection> &c) { onConnectionClose(c); });

    // 可以获取客户端地址用于日志
    // std::string client_addr = conn->getPeerAddress(); //
    // 如果你的Connection类有这个方法
    LOG_DEBUG("New connection established, active: " +
              std::to_string(active_connections.load()));
}

void WebServer::onConnectionClose(std::shared_ptr<Connection> conn) {
    active_connections.fetch_sub(1);
    LOG_DEBUG("Connection closed, active: " +
              std::to_string(active_connections.load()));
}

void WebServer::onMessage(std::shared_ptr<Connection> conn,
                          const std::string &message) {
    // 在线程池中处理HTTP请求
    thread_pool->submit([this, conn, message]() {
        try {
            // 解析HTTP请求
            HttpParser parser;

            if (parser.parse(message.c_str(), message.length())) {
                total_requests.fetch_add(1);

                auto request =
                    std::make_shared<HttpRequest>(parser.getRequest());
                handleHttpRequest(conn, request);
            } else {
                LOG_WARN("Failed to parse HTTP request");
                handleError(conn, 400, "Bad Request");
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Exception in message handling: " +
                      std::string(e.what()));
            handleError(conn, 500, "Internal Server Error");
        }
    });
}

void WebServer::handleHttpRequest(std::shared_ptr<Connection> conn,
                                  std::shared_ptr<HttpRequest> request) {

    logRequest(*request, 200); // 先记录请求，状态码稍后更新

    // 检查请求大小
    if (request->getBody().size() > max_request_size) {
        handleError(conn, 413, "Request Entity Too Large");
        return;
    }

    // 处理请求
    processRequest(conn, request);
}

void WebServer::processRequest(std::shared_ptr<Connection> conn,
                               std::shared_ptr<HttpRequest> request) {

    HttpResponse response;
    response.setVersion(request->getVersion());

    try {
        std::string path = request->getPath();

        // 查找路由处理器
        auto it = route_handlers.find(path);
        if (it != route_handlers.end()) {
            // 使用自定义处理器
            it->second(*request, response);
        } else {
            // 使用默认处理器（通常是静态文件处理）
            default_handler(*request, response);
        }

        // 设置通用响应头
        if (keep_alive_enabled &&
            request->getHeader("Connection") == "keep-alive") {
            response.addHeader("Connection", "keep-alive");
            response.addHeader("Keep-Alive",
                               "timeout=" + std::to_string(timeout_ms / 1000));
        } else {
            response.addHeader("Connection", "close");
        }

        response.addHeader("Server", "WebServer/1.0");
        response.addHeader("Date", Util::getCurrentTime());

        sendResponse(conn, response);
        total_responses.fetch_add(1);

    } catch (const std::exception &e) {
        LOG_ERROR("Exception in processRequest: " + std::string(e.what()));
        handleError(conn, 500, "Internal Server Error");
    }
}

void WebServer::handleStaticFile(const HttpRequest &request,
                                 HttpResponse &response) {
    std::string path = request.getPath();

    // 处理根路径
    if (path == "/" || path.empty()) {
        path = "/" + default_page;
    }

    // 构建完整文件路径
    std::string full_path = document_root + path;

    // 安全检查：防止路径遍历攻击
    if (full_path.find("..") != std::string::npos) {
        response.setStatusCode(HttpResponse::StatusCode(403));
        response.setContentType("Forbidden");
        response.setBody(generateErrorPage(403, "Forbidden"));
        return;
    }

    // 检查文件是否存在
    if (!isFileExists(full_path)) {
        response.setStatusCode(HttpResponse::StatusCode(404));
        response.setContentType("Not Found");
        response.setBody(generateErrorPage(404, "File Not Found"));
        return;
    }

    // 读取文件内容
    std::string content = readFile(full_path);
    if (content.empty()) {
        response.setStatusCode(HttpResponse::StatusCode(500));
        response.setContentType("Internal Server Error");
        response.setBody(generateErrorPage(500, "Failed to read file"));
        return;
    }

    // 设置响应
    response.setStatusCode(HttpResponse::StatusCode(200));
    response.setContentType("OK");
    response.addHeader("Content-Type", getMimeType(full_path));
    response.addHeader("Content-Length", std::to_string(content.size()));
    response.setBody(content);
}

std::string WebServer::getMimeType(const std::string &filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string ext = filename.substr(pos);
    auto it = mime_types.find(ext);
    return (it != mime_types.end()) ? it->second : "application/octet-stream";
}

bool WebServer::isFileExists(const std::string &filepath) {
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0);
}

std::string WebServer::readFile(const std::string &filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

void WebServer::handleError(std::shared_ptr<Connection> conn, int status_code,
                            const std::string &message) {
    try {
        sendErrorPage(conn, status_code);
        total_responses.fetch_add(1);
    } catch (const std::exception &e) {
        LOG_ERROR("Exception in handleError: " + std::string(e.what()));
    }
}

void WebServer::sendErrorPage(std::shared_ptr<Connection> conn,
                              int status_code) {
    HttpResponse response;

    // 根据状态码选择合适的便捷方法，或者使用默认的通用错误响应
    switch (status_code) {
    case HttpResponse::BAD_REQUEST:
        response = HttpResponse::badRequest();
        break;
    case HttpResponse::UNAUTHORIZED:
        response = HttpResponse::unauthorized();
        break;
    case HttpResponse::FORBIDDEN:
        // 假设我们有一个通用的forbidden方法
        // response = HttpResponse::forbidden();
        // 如果没有，可以手动构建
        response.setStatusCode(HttpResponse::FORBIDDEN);
        response.setBody("Forbidden");
        break;
    case HttpResponse::NOT_FOUND:
        response = HttpResponse::notFound();
        break;
    case HttpResponse::INTERNAL_SERVER_ERROR:
        response = HttpResponse::internalError();
        break;
    default:
        // 对于没有便捷方法的错误码，可以通用处理
        response.setStatusCode(
            static_cast<HttpResponse::StatusCode>(status_code));
        response.setBody(response.getStatusText());
        break;
    }

    // 统一设置 Content-Type 和 Connection 头
    // 因为方便方法可能没有设置，我们在这里统一处理
    response.setContentType("text/html");
    response.addHeader("Connection", "close");

    // 重新计算并设置 Content-Length，因为 body 可能被修改了
    response.addHeader("Content-Length",
                       std::to_string(response.getBody().size()));

    sendResponse(conn, response);
}

void WebServer::sendResponse(std::shared_ptr<Connection> conn,
                             const HttpResponse &response) {
    // 将整个HttpResponse对象序列化为HTTP响应字符串
    std::string response_str = response.toString();

    // 发送响应数据
    conn->send(response_str);

    // 检查Connection头部是否设置为 "close"
    const auto &headers = response.getHeaders();
    if (headers.count("Connection") && headers.at("Connection") == "close") {
        conn->shutdown();
    }
}

std::string WebServer::generateErrorPage(int status_code,
                                         const std::string &message) {
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n";
    oss << "<html><head><title>" << status_code << " " << message
        << "</title></head>\n";
    oss << "<body><h1>" << status_code << " " << message << "</h1>\n";
    oss << "<p>WebServer/1.0</p></body></html>\n";
    return oss.str();
}

std::string WebServer::getStatusText(int status_code) {
    switch (status_code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 413:
        return "Request Entity Too Large";
    case 500:
        return "Internal Server Error";
    default:
        return "Unknown";
    }
}

void WebServer::logRequest(const HttpRequest &request, int status_code) {
    std::ostringstream oss;
    oss << request.getMethod() << " " << request.getPath() << " "
        << request.getVersion() << " - " << status_code;
    LOG_INFO(oss.str());
}