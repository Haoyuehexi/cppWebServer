#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>

class EventLoop;
class Channel;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using MessageCallback = std::function<void(const Ptr&, std::string_view)>;
    using CloseCallback   = std::function<void(const Ptr&)>;
    using WriteCompleteCallback = std::function<void(const Ptr&)>;

    enum class State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    Connection(EventLoop* loop, int fd);
    ~Connection();

    // 生命周期
    void connectEstablished();   // 放到 loop 里调用：完成 Channel 绑定并关注可读
    void connectDestroyed();     // 放到 loop 里调用：移除 Channel

    // API
    void send(const void* data, size_t len);
    void send(const std::string& s) { send(s.data(), s.size()); }
    void shutdown();             // 优雅关闭写端
    void forceClose();           // 立即关闭

    // 回调
    void setMessageCallback(MessageCallback cb) { message_cb_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb)     { close_cb_   = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { write_complete_cb_ = std::move(cb); }

    // 查询
    int   fd()      const { return fd_; }
    State state()   const { return state_; }
    EventLoop* loop() const { return loop_; }

private:
    // 仅 loop 线程调用
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();

private:
    EventLoop* loop_;
    int fd_;
    std::unique_ptr<Channel> channel_;
    State state_ { State::kConnecting };

    std::string inbuf_;      // 读缓冲
    std::string outbuf_;     // 写缓冲

    MessageCallback message_cb_;
    CloseCallback   close_cb_;
    WriteCompleteCallback write_complete_cb_;
};
