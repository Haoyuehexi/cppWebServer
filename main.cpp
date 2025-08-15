#include "common/log.h"
#include "common/util.h"
#include "http/http_response.h"
#include "server.h"
#include <csignal>
#include <iostream>
#include <sys/stat.h>

WebServer *g_main_server = nullptr;

static void signalHandler(int sig) {
    if (g_main_server) {
        std::cout << "\nReceived signal " << sig << ", shutting down..."
                  << std::endl;
        g_main_server->stop();
    }
}

void setupDirectories() {
    struct stat st = {0};
    if (stat("logs", &st) == -1)
        mkdir("logs", 0755);
    if (stat("resources", &st) == -1)
        mkdir("resources", 0755);
    if (stat("resources/html", &st) == -1)
        mkdir("resources/html", 0755);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    setupDirectories();

    try {
        WebServer server;
        g_main_server = &server;

        std::string config_file = (argc > 1) ? argv[1] : "config.json";
        if (!server.loadConfig(config_file)) {
            std::cerr << "❌ Failed to load config file\n";
            return 1;
        }

        // 关键：设置静态文件目录和默认页面
        // server.setDocumentRoot("./resources/html");
        // server.setDefaultHandler(
        //     [&server](const HttpRequest& req, HttpResponse& resp) {
        //         // 默认处理静态文件
        //         server.handleStaticFile(req, resp);
        //     }
        // );

        server.addRoute("/", [](const HttpRequest &req, HttpResponse &resp) {
            resp.setStatusCode(HttpResponse::OK);
            resp.addHeader("Content-Type", "text/plain");
            resp.setBody("Hello from WebServer!\n");
        });

        std::cout << "🚀 Starting WebServer on configured host/port...\n";
        if (!server.start()) {
            std::cerr << "❌ Failed to start server\n";
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "❌ Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
