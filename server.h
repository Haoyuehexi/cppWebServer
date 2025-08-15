#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <atomic>

// 前向声明
class EventLoop;
class Socket;
class Connection;
class ThreadPool;
class Timer;
class HttpRequest;
class HttpResponse;
class DBConnectionPool;

namespace net {
    class Server;
}

class WebServer {
public:
    using RequestHandler = std::function<void(const HttpRequest&, HttpResponse&)>;
    
private:
    // 核心组件
    std::unique_ptr<EventLoop> main_loop;
    std::unique_ptr<net::Server> tcp_server;
    std::unique_ptr<ThreadPool> thread_pool;
    std::unique_ptr<DBConnectionPool> db_pool;
    
    // 服务器配置
    std::string host;
    int port;
    int thread_count;
    int max_connections;
    int timeout_ms;
    bool keep_alive_enabled;
    
    // HTTP配置
    std::string document_root;
    std::string default_page;
    size_t max_request_size;
    
    // 数据库相关
    bool database_enabled;
    std::string db_host;
    int db_port;
    std::string db_username;
    std::string db_password;  
    std::string db_database;
    int db_pool_size;
    
    // 路由和处理器
    std::unordered_map<std::string, RequestHandler> route_handlers;
    RequestHandler default_handler;
    
    // MIME类型映射
    std::unordered_map<std::string, std::string> mime_types;
    
    // 服务器状态
    std::atomic<bool> running;
    std::atomic<size_t> active_connections;
    
    // 统计信息
    std::atomic<size_t> total_requests;
    std::atomic<size_t> total_responses;
    
public:
    WebServer();
    ~WebServer();
    
    // 配置方法
    bool loadConfig(const std::string& config_file);
    void setHost(const std::string& host) { this->host = host; }
    void setPort(int port) { this->port = port; }
    void setDocumentRoot(const std::string& root) { this->document_root = root; }
    void setThreadCount(int count) { this->thread_count = count; }
    
    // 路由注册
    void addRoute(const std::string& path, RequestHandler handler);
    void setDefaultHandler(RequestHandler handler);
    
    // 服务器控制
    bool start();
    void stop();
    bool isRunning() const { return running.load(); }

    void handleStaticFile(const HttpRequest& request, HttpResponse& response);
    
    // 统计信息
    size_t getActiveConnections() const { return active_connections.load(); }
    size_t getTotalRequests() const { return total_requests.load(); }
    size_t getTotalResponses() const { return total_responses.load(); }
    
private:
    // 初始化方法
    bool initializeComponents();
    void initializeMimeTypes();
    void setupSignalHandlers();
    
    // 连接处理
    void onNewConnection(const std::shared_ptr<Connection>& conn);
    void onConnectionClose(std::shared_ptr<Connection> conn);
    void onMessage(std::shared_ptr<Connection> conn, const std::string& message);
    
    // HTTP请求处理
    void handleHttpRequest(std::shared_ptr<Connection> conn, 
                          std::shared_ptr<HttpRequest> request);
    void processRequest(std::shared_ptr<Connection> conn,
                       std::shared_ptr<HttpRequest> request);
    
    // 文件处理
    std::string getMimeType(const std::string& filename);
    bool isFileExists(const std::string& filepath);
    std::string readFile(const std::string& filepath);
    
    // 错误处理
    void handleError(std::shared_ptr<Connection> conn, int status_code, 
                    const std::string& message = "");
    void sendErrorPage(std::shared_ptr<Connection> conn, int status_code);
    
    // 响应发送
    void sendResponse(std::shared_ptr<Connection> conn, 
                     const HttpResponse& response);
    
    // 工具方法
    std::string generateErrorPage(int status_code, const std::string& message);
    std::string getStatusText(int status_code);
    void logRequest(const HttpRequest& request, int status_code);
};

#endif // SERVER_H