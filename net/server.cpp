#include "server.h"
#include "../common/log.h"
#include "channel.h"
#include "connection.h"
#include "event_loop.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

Server::Server(EventLoop *loop, const std::string &ip, uint16_t port,
               int backlog)
    : loop_(loop), listen_fd_(-1), running_(false) {

    // 创建监听socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        LOG_ERROR("Failed to create listen socket");
        return;
    }

    // 设置地址复用
    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
        0) {
        LOG_WARN("Failed to set SO_REUSEADDR");
    }

    // 绑定地址
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid IP address: " + ip);
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    if (bind(listen_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind to " + ip + ":" + std::to_string(port));
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    // 开始监听
    if (listen(listen_fd_, backlog) < 0) {
        LOG_ERROR("Failed to listen on socket");
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    // 创建accept channel
    accept_channel_ = std::make_unique<Channel>(loop_, listen_fd_);
    accept_channel_->setReadCallback([this]() { handleAccept(); });
}

Server::~Server() { stop(); }

void Server::start() {
    if (listen_fd_ < 0) {
        LOG_ERROR("Invalid listen socket, cannot start server");
        return;
    }

    if (running_.load()) {
        LOG_WARN("Server is already running");
        return;
    }

    running_.store(true);
    accept_channel_->enableReading();
    LOG_INFO("Server started and listening");
}

void Server::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // 停止接受新连接
    if (accept_channel_) {
        accept_channel_->disableAll();
    }

    // 关闭所有现有连接
    for (auto &pair : conns_) {
        pair.second->shutdown();
    }
    conns_.clear();

    // 关闭监听socket
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }

    LOG_INFO("Server stopped");
}

void Server::handleAccept() {
    if (!running_.load()) {
        return;
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd =
        accept(listen_fd_, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        LOG_ERROR("Failed to accept connection");
        return;
    }

    // 创建Connection对象
    auto conn = std::make_shared<Connection>(loop_, client_fd);

    // 设置连接的消息回调
    conn->setMessageCallback(
        [this](const std::shared_ptr<Connection> &c, std::string_view data) {
            if (message_cb_) {
                message_cb_(c, data);
            }
        });

    // 设置连接关闭回调
    conn->setCloseCallback(
        [this](const std::shared_ptr<Connection> &c) { onConnectionClose(c); });

    // 保存连接
    conns_[client_fd] = conn;

    // 调用用户回调
    if (new_conn_cb_) {
        new_conn_cb_(conn);
    }

    LOG_DEBUG("New connection accepted, fd = " + std::to_string(client_fd));
}

void Server::onConnectionClose(const std::shared_ptr<Connection> &conn) {
    // 调用用户回调
    if (close_cb_) {
        close_cb_(conn);
    }

    // 从连接映射中移除
    removeConnection(conn);
}

void Server::removeConnection(const std::shared_ptr<Connection> &conn) {
    int fd = conn->fd(); // 假设Connection有fd方法
    auto it = conns_.find(fd);
    if (it != conns_.end()) {
        conns_.erase(it);
        LOG_DEBUG("Connection removed, fd = " + std::to_string(fd));
    }
}

} // namespace net