#ifndef NET_CHANNEL_H
#define NET_CHANNEL_H

#include <functional>
#include <memory>

class EventLoop;

class Channel {
  public:
    using ReadCallback = std::function<void()>;
    using WriteCallback = std::function<void()>;
    using ErrorCallback = std::function<void()>;
    using CloseCallback = std::function<void()>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // // 禁用拷贝构造和赋值
    // Channel(const Channel &) = delete;
    // Channel &operator=(const Channel &) = delete;
    Channel(Channel &&other) noexcept;
    Channel &operator=(Channel &&other) noexcept;

    // 设置回调函数
    void setReadCallback(ReadCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(WriteCallback cb) { writeCallback_ = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

    // 事件启用/禁用
    void enableReading() {
        events_ |= kReadEvent;
        update();
    }
    void disableReading() {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting() {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting() {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll() {
        events_ = kNoneEvent;
        update();
    }

    // 获取文件描述符和事件
    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // 检查事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // 处理事件
    void handleEvent();

    // 获取所属的EventLoop
    EventLoop *ownerLoop() { return loop_; }

    // 设置在epoll中的索引
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // 从EventLoop中移除
    void remove();

    // 绑定对象生命周期
    void tie(const std::shared_ptr<void> &);

  private:
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    void update();
    void handleEventWithGuard();

    EventLoop *loop_;
    const int fd_;
    int events_;
    int revents_;
    int index_;

    ReadCallback readCallback_;
    WriteCallback writeCallback_;
    ErrorCallback errorCallback_;
    CloseCallback closeCallback_;

    bool tied_;
    std::weak_ptr<void> tie_;
    bool eventHandling_;
};

#endif // NET_CHANNEL_H