#include "db_connection_pool.h"
#include <cstring>
#include "../common/log.h"
#include <crypt.h>

DBConnection::DBConnection() : conn(nullptr) {}

DBConnection::~DBConnection() {
    disconnect();
}

bool DBConnection::connect(const std::string& host, const std::string& user,
                          const std::string& password, const std::string& database, int port) {
    conn = mysql_init(nullptr);
    if (!conn) {
        Logger::error("MySQL init failed");
        return false;
    }
    
    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, 0)) {
        Logger::error("MySQL connect failed: " + std::string(mysql_error(conn)));
        mysql_close(conn);
        conn = nullptr;
        return false;
    }
    
    // 设置字符集
    mysql_set_character_set(conn, "utf8");
    
    return true;
}

void DBConnection::disconnect() {
    if (conn) {
        mysql_close(conn);
        conn = nullptr;
    }
}

bool DBConnection::isConnected() const {
    return conn != nullptr && mysql_ping(conn) == 0;
}

bool DBConnection::registerUser(const std::string& username, const std::string& password) {
    if (!conn) return false;
    
    // 检查用户是否已存在
    std::string check_query = "SELECT id FROM users WHERE username = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return false;
    
    if (mysql_stmt_prepare(stmt, check_query.c_str(), check_query.length())) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username.c_str();
    bind[0].buffer_length = username.length();
    
    mysql_stmt_bind_param(stmt, bind);
    
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_RES* result = mysql_stmt_result_metadata(stmt);
    if (result) {
        mysql_free_result(result);
        if (mysql_stmt_fetch(stmt) == 0) {
            mysql_stmt_close(stmt);
            return false; // 用户已存在
        }
    }
    mysql_stmt_close(stmt);
    
    // 插入新用户
    std::string insert_query = "INSERT INTO users (username, password) VALUES (?, ?)";
    stmt = mysql_stmt_init(conn);
    if (!stmt) return false;
    
    if (mysql_stmt_prepare(stmt, insert_query.c_str(), insert_query.length())) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 简单的密码加密（实际项目中应使用更安全的方式）
    std::string encrypted_password = password; // 这里应该进行加密
    
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username.c_str();
    bind[0].buffer_length = username.length();
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)encrypted_password.c_str();
    bind[1].buffer_length = encrypted_password.length();
    
    mysql_stmt_bind_param(stmt, bind);
    
    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    
    return success;
}

bool DBConnection::loginUser(const std::string& username, const std::string& password) {
    if (!conn) return false;
    
    std::string query = "SELECT id FROM users WHERE username = ? AND password = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return false;
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username.c_str();
    bind[0].buffer_length = username.length();
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)password.c_str();
    bind[1].buffer_length = password.length();
    
    mysql_stmt_bind_param(stmt, bind);
    
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    bool success = mysql_stmt_fetch(stmt) == 0;
    mysql_stmt_close(stmt);
    
    return success;
}

int DBConnection::getUserId(const std::string& username) {
    if (!conn) return -1;
    
    std::string query = "SELECT id FROM users WHERE username = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return -1;
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return -1;
    }
    
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username.c_str();
    bind[0].buffer_length = username.length();
    
    mysql_stmt_bind_param(stmt, bind);
    
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return -1;
    }
    
    int user_id = -1;
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &user_id;
    
    mysql_stmt_bind_result(stmt, result_bind);
    
    if (mysql_stmt_fetch(stmt) == 0) {
        mysql_stmt_close(stmt);
        return user_id;
    }
    
    mysql_stmt_close(stmt);
    return -1;
}

bool DBConnection::insertFile(const std::string& filename, const std::string& filepath,
                             int user_id, bool is_public) {
    if (!conn) return false;
    
    std::string query = "INSERT INTO files (filename, filepath, user_id, is_public, upload_time) VALUES (?, ?, ?, ?, NOW())";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return false;
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)filename.c_str();
    bind[0].buffer_length = filename.length();
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)filepath.c_str();
    bind[1].buffer_length = filepath.length();
    
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &user_id;
    
    bool public_flag = is_public ? 1 : 0;
    bind[3].buffer_type = MYSQL_TYPE_TINY;
    bind[3].buffer = &public_flag;
    
    mysql_stmt_bind_param(stmt, bind);
    
    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    
    return success;
}

bool DBConnection::deleteFile(int file_id, int user_id) {
    if (!conn) return false;
    
    std::string query = "DELETE FROM files WHERE id = ? AND user_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return false;
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &file_id;
    
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &user_id;
    
    mysql_stmt_bind_param(stmt, bind);
    
    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    
    return success;
}

std::vector<std::tuple<int, std::string, std::string, bool>> DBConnection::getPublicFiles() {
    std::vector<std::tuple<int, std::string, std::string, bool>> files;
    if (!conn) return files;
    
    std::string query = "SELECT id, filename, filepath, is_public FROM files WHERE is_public = 1";
    
    if (mysql_query(conn, query.c_str())) {
        return files;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) return files;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        int id = std::atoi(row[0]);
        std::string filename = row[1] ? row[1] : "";
        std::string filepath = row[2] ? row[2] : "";
        bool is_public = row[3] && std::atoi(row[3]) == 1;
        
        files.emplace_back(id, filename, filepath, is_public);
    }
    
    mysql_free_result(result);
    return files;
}

std::vector<std::tuple<int, std::string, std::string, bool>> DBConnection::getUserFiles(int user_id) {
    std::vector<std::tuple<int, std::string, std::string, bool>> files;
    if (!conn) return files;
    
    std::string query = "SELECT id, filename, filepath, is_public FROM files WHERE user_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return files;
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return files;
    }
    
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &user_id;
    
    mysql_stmt_bind_param(stmt, bind);
    
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return files;
    }
    
    MYSQL_RES* result = mysql_stmt_result_metadata(stmt);
    if (!result) {
        mysql_stmt_close(stmt);
        return files;
    }
    
    int id;
    char filename[256], filepath[512];
    bool is_public;
    unsigned long filename_length, filepath_length;
    
    MYSQL_BIND result_bind[4];
    memset(result_bind, 0, sizeof(result_bind));
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;
    
    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = filename;
    result_bind[1].buffer_length = sizeof(filename);
    result_bind[1].length = &filename_length;
    
    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = filepath;
    result_bind[2].buffer_length = sizeof(filepath);
    result_bind[2].length = &filepath_length;
    
    result_bind[3].buffer_type = MYSQL_TYPE_TINY;
    result_bind[3].buffer = &is_public;
    
    mysql_stmt_bind_result(stmt, result_bind);
    
    while (mysql_stmt_fetch(stmt) == 0) {
        files.emplace_back(id, std::string(filename, filename_length), 
                          std::string(filepath, filepath_length), is_public == 1);
    }
    
    mysql_free_result(result);
    mysql_stmt_close(stmt);
    
    return files;
}

bool DBConnection::isFileAccessible(int file_id, int user_id) {
    if (!conn) return false;
    
    std::string query = "SELECT user_id, is_public FROM files WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return false;
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &file_id;
    
    mysql_stmt_bind_param(stmt, bind);
    
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return false;
    }
    
    int file_user_id;
    bool is_public;
    
    MYSQL_BIND result_bind[2];
    memset(result_bind, 0, sizeof(result_bind));
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &file_user_id;
    
    result_bind[1].buffer_type = MYSQL_TYPE_TINY;
    result_bind[1].buffer = &is_public;
    
    mysql_stmt_bind_result(stmt, result_bind);
    
    bool accessible = false;
    if (mysql_stmt_fetch(stmt) == 0) {
        accessible = (is_public == 1) || (file_user_id == user_id);
    }
    
    mysql_stmt_close(stmt);
    return accessible;
}

std::string DBConnection::getFilePath(int file_id) {
    if (!conn) return "";
    
    std::string query = "SELECT filepath FROM files WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return "";
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return "";
    }
    
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &file_id;
    
    mysql_stmt_bind_param(stmt, bind);
    
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return "";
    }
    
    char filepath[512];
    unsigned long filepath_length;
    
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));
    
    result_bind[0].buffer_type = MYSQL_TYPE_STRING;
    result_bind[0].buffer = filepath;
    result_bind[0].buffer_length = sizeof(filepath);
    result_bind[0].length = &filepath_length;
    
    mysql_stmt_bind_result(stmt, result_bind);
    
    std::string result = "";
    if (mysql_stmt_fetch(stmt) == 0) {
        result = std::string(filepath, filepath_length);
    }
    
    mysql_stmt_close(stmt);
    return result;
}

// DBConnectionPool implementation
DBConnectionPool::DBConnectionPool(const std::string& host, const std::string& user,
                                  const std::string& password, const std::string& database,
                                  int port, int max_conn)
    : host(host), user(user), password(password), database(database),
      port(port), max_connections(max_conn), current_connections(0) {}

DBConnectionPool::~DBConnectionPool() {
    close();
}

bool DBConnectionPool::initialize() {
    for (int i = 0; i < max_connections; ++i) {
        auto conn = std::make_shared<DBConnection>();
        if (conn->connect(host, user, password, database, port)) {
            pool.push(conn);
            current_connections++;
        } else {
            Logger::error("Failed to create database connection");
            return false;
        }
    }
    return true;
}

std::shared_ptr<DBConnection> DBConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(pool_mutex);
    
    while (pool.empty()) {
        condition.wait(lock);
    }
    
    auto conn = pool.front();
    pool.pop();
    
    // 检查连接是否有效
    if (!conn->isConnected()) {
        conn.reset(new DBConnection());
        if (!conn->connect(host, user, password, database, port)) {
            Logger::error("Failed to reconnect to database");
            return nullptr;
        }
    }
    
    return conn;
}

void DBConnectionPool::releaseConnection(std::shared_ptr<DBConnection> conn) {
    std::unique_lock<std::mutex> lock(pool_mutex);
    pool.push(conn);
    condition.notify_one();
}

void DBConnectionPool::close() {
    std::unique_lock<std::mutex> lock(pool_mutex);
    while (!pool.empty()) {
        pool.pop();
    }
    current_connections = 0;
}