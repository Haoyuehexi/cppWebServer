#include "../common/config.h"
#include <iostream>

void test_config() {
    Config out;
    ConfigLoader::load("./config.json", out);
    std::cout << "Server port: " << out.server.port << std::endl;
    std::cout << "Server host: " << out.server.host << std::endl;
    std::cout << "Server thread pool size: " << out.server.thread_pool_size
              << std::endl; // 线程池大小
    std::cout << "Server max connections: " << out.server.max_connections
              << std::endl; // 最大连接数
    std::cout << "Server timeout ms: " << out.server.timeout_ms
              << std::endl; // 超时时间
    std::cout << "Server keep alive: "
              << (out.server.keep_alive ? "true" : "false")
              << std::endl; // 是否保持连接
    std::cout << "Logging level: " << out.logging.level << std::endl;
    std::cout << "Logging file: " << out.logging.file << std::endl;
    std::cout << "Logging max file size mb: " << out.logging.max_file_size_mb
              << std::endl; // 日志文件最大大小
    std::cout << "Logging enable console: "
              << (out.logging.enable_console ? "true" : "false")
              << std::endl; // 是否输出到控制台
    std::cout << "HTTP document root: " << out.http.document_root
              << std::endl; // 静态文件根目录
    std::cout << "HTTP default page: " << out.http.default_page
              << std::endl; // 默认文件，如果访问根目录没有文件，则返回该文件
    std::cout << "HTTP max request size kb: " << out.http.max_request_size_kb
              << std::endl; // 最大请求大小，单位为 kb
    std::cout << "HTTP enable directory listing: "
              << (out.http.enable_directory_listing ? "true" : "false")
              << std::endl; // 是否允许目录列表
    std::cout << "Database enable: " << (out.database.enable ? "true" : "false")
              << std::endl;
    // 是否启用数据库
    if (out.database.enable) {
        std::cout << "Database host: " << out.database.host << std::endl;
        std::cout << "Database port: " << out.database.port << std::endl;
        std::cout << "Database username: " << out.database.username
                  << std::endl; // 数据库用户名
        std::cout << "Database password: " << out.database.password
                  << std::endl; // 数据库密码
        std::cout << "Database database: " << out.database.database
                  << std::endl; // 数据库名称
        std::cout << "Database connection pool size: "
                  << out.database.connection_pool_size
                  << std::endl; // 数据库连接池大小
                                // 连接数据库，测试连接是否成功
    }
}

int main() {
    test_config();
    return 0;
}