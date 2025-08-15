#ifndef DB_CONNECTION_POOL_H
#define DB_CONNECTION_POOL_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <mysql/mysql.h>
#include <queue>
#include <string>

class DBConnection {
  private:
    MYSQL *conn;

  public:
    DBConnection();
    ~DBConnection();

    bool connect(const std::string &host, const std::string &user,
                 const std::string &password, const std::string &database,
                 int port = 3306);
    void disconnect();

    MYSQL *getConnection() const { return conn; }
    bool isConnected() const;

    // 用户相关操作
    bool registerUser(const std::string &username, const std::string &password);
    bool loginUser(const std::string &username, const std::string &password);
    int getUserId(const std::string &username);

    // 文件相关操作
    bool insertFile(const std::string &filename, const std::string &filepath,
                    int user_id, bool is_public);
    bool deleteFile(int file_id, int user_id);
    std::vector<std::tuple<int, std::string, std::string, bool>>
    getPublicFiles();
    std::vector<std::tuple<int, std::string, std::string, bool>>
    getUserFiles(int user_id);
    bool isFileAccessible(int file_id, int user_id);
    std::string getFilePath(int file_id);
};

class DBConnectionPool {
  private:
    std::queue<std::shared_ptr<DBConnection>> pool;
    std::mutex pool_mutex;
    std::condition_variable condition;

    std::string host;
    std::string user;
    std::string password;
    std::string database;
    int port;
    int max_connections;
    int current_connections;

  public:
    DBConnectionPool(const std::string &host, const std::string &user,
                     const std::string &password, const std::string &database,
                     int port = 3306, int max_conn = 10);
    ~DBConnectionPool();

    bool initialize();
    std::shared_ptr<DBConnection> getConnection();
    void releaseConnection(std::shared_ptr<DBConnection> conn);
    void close();
};

#endif