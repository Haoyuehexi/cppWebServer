#ifndef NET_EVENT_LOOP_H
#define NET_EVENT_LOOP_H

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct epoll_event;
class Channel;
class TimerQueue;

class EventLoop {
  public:
    using Functor = std::function<void()>;
    using ChannelList = std::vector<Channel *>;

    EventLoop();
    ~EventLoop();

    // 禁用拷贝构造和赋值
    EventLoop(const EventLoop &) = delete;
    EventLoop &operator=(const EventLoop &) = delete;

    // 事件循环
    void loop();
    void quit();

    // Channel 管理
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 定时器相关
    void runAt(const std::function<void()> &cb, int64_t when);
    void runAfter(const std::function<void()> &cb, double delay);
    void runEvery(const std::function<void()> &cb, double interval);

    // 在I/O线程中执行
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    // 唤醒事件循环
    void wakeup();

    // 线程安全检查
    void assertInLoopThread();
    bool isInLoopThread() const {
        return threadId_ == std::this_thread::get_id();
    }

    static EventLoop *getEventLoopOfCurrentThread();

  private:
    void handleRead(); // 处理wakeup
    void doPendingFunctors();

    // Epoll相关方法
    int poll(int timeoutMs, ChannelList *activeChannels);
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    void updateChannelInEpoll(Channel *channel);
    void removeChannelFromEpoll(Channel *channel);
    void epollUpdate(int operation, Channel *channel);

    static const int kInitEventListSize = 16;
    static const int kNew = -1;
    static const int kAdded = 1;
    static const int kDeleted = 2;

    bool looping_;
    std::atomic<bool> quit_;
    bool eventHandling_;
    bool callingPendingFunctors_;
    const std::thread::id threadId_;

    // Epoll相关成员
    int epollfd_;
    std::vector<struct epoll_event> events_;
    std::map<int, Channel *> channels_;

    std::unique_ptr<TimerQueue> timerQueue_;
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;
    ChannelList activeChannels_;

    std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;
};

#endif // NET_EVENT_LOOP_H