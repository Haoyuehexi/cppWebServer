#ifndef NET_SERVER_H
#define NET_SERVER_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <atomic>

class EventLoop;
class Channel;
class Connection;

namespace net {

class Server {
public:
    using NewConnCallback = std::function<void(const std::shared_ptr<Connection>&)>;
    using MessageCallback = std::function<void(const std::shared_ptr<Connection>&, std::string_view)>;
    using CloseCallback = std::function<void(const std::shared_ptr<Connection>&)>; // 新增
    
    Server(EventLoop* loop, const std::string& ip, uint16_t port, int backlog = 128);
    ~Server();
    
    void start(); // 绑定监听并开始接受
    void stop();  // 新增：停止服务器
    bool isRunning() const { return running_.load(); } // 新增：检查运行状态
    
    // 业务回调
    void setMessageCallback(MessageCallback cb) { message_cb_ = std::move(cb); }
    void setNewConnCallback(NewConnCallback cb) { new_conn_cb_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { close_cb_ = std::move(cb); } // 新增
    
    // 统计信息
    size_t getConnectionCount() const { return conns_.size(); } // 新增
    
private:
    void handleAccept();  // 有新连接
    void removeConnection(const std::shared_ptr<Connection>& conn);
    void onConnectionClose(const std::shared_ptr<Connection>& conn); // 新增：内部连接关闭处理
    
private:
    EventLoop* loop_;
    int listen_fd_;
    std::unique_ptr<Channel> accept_channel_;
    std::unordered_map<int, std::shared_ptr<Connection>> conns_;
    std::atomic<bool> running_; // 新增：运行状态
    
    MessageCallback message_cb_;
    NewConnCallback new_conn_cb_;
    CloseCallback close_cb_; // 新增
};

} // namespace net

#endif
