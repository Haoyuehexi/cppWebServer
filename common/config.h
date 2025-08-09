#pragma once
#include <string>

struct ServerConfig {
    int port;
    std::string host;
    int thread_pool_size;
    int max_connections;
    int timeout_ms;
    bool keep_alive;
};

struct LoggingConfig {
    std::string level;
    std::string file;
    int max_file_size_mb;
    bool enable_console;
};

struct HttpConfig {
    std::string document_root;
    std::string default_page;
    int max_request_size_kb;
    bool enable_directory_listing;
};

struct DatabaseConfig {
    bool enable;
    std::string host;
    int port;
    std::string username;
    std::string password;
    std::string database;
    int connection_pool_size;
};

struct Config {
    ServerConfig server;
    LoggingConfig logging;
    HttpConfig http;
    DatabaseConfig database;
};

class ConfigLoader {
  public:
    static bool load(const std::string &path, Config &out);
};
