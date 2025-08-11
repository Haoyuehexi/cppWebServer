#ifndef SOCKET_H
#define SOCKET_H

#include "../common/log.h"
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

// 自定义异常类
class SocketError : public std::runtime_error {
  public:
    explicit SocketError(const std::string &msg) : std::runtime_error(msg) {
        LOG_ERROR(std::string("SocketError: ") + msg);
    }
};

class PosixError : public std::runtime_error {
  public:
    PosixError(const std::string &msg, int error_code)
        : std::runtime_error(msg + ": " + std::to_string(error_code)) {
        LOG_ERROR("PosixError: " + msg + ": " + std::to_string(error_code));
    }
};

class Socket {
  public:
    explicit Socket(int fd);
    ~Socket();

    // 禁止拷贝，允许移动
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&other) noexcept;
    Socket &operator=(Socket &&other) noexcept;

    // 基础接口
    int getFd() const { return fd_; }
    bool isValid() const { return fd_ >= 0; }
    bool create();
    bool bind(int port);
    bool listen(int backlog = 1024);
    int accept();
    bool connect(const std::string &host, int port);

    // 读写接口
    std::string readLine();
    ssize_t read(void *buffer, size_t size);
    ssize_t write(const void *data, size_t size);
    void write(const std::string &data);

    // 网络配置
    void setNonBlocking(bool non_blocking = true);
    void setReuseAddr(bool reuse = true);
    void setKeepAlive(bool keep_alive = true);
    void setTcpNoDelay(bool no_delay = true);

    // 连接管理
    void shutdown(int how = SHUT_RDWR);
    void close();

    // 地址信息
    std::string getPeerAddress() const;
    int getPeerPort() const;

  private:
    int fd_;
    struct sockaddr_in addr_;

    void ensureValid() const;
};

#endif