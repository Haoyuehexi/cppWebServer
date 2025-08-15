#ifndef NET_TIMER_QUEUE_H
#define NET_TIMER_QUEUE_H

#include <functional>
#include <memory>
#include <set>
#include <vector>

class EventLoop;
class Timer;
class Channel;

class TimerQueue {
  public:
    explicit TimerQueue(EventLoop *loop);
    ~TimerQueue();

    // 禁用拷贝构造和赋值
    TimerQueue(const TimerQueue &) = delete;
    TimerQueue &operator=(const TimerQueue &) = delete;

    // 添加定时器，线程安全
    void addTimer(const std::function<void()> &cb, int64_t when,
                  double interval);

  private:
    using Entry = std::pair<int64_t, Timer *>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer *, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer *timer);
    void handleRead();
    std::vector<Entry> getExpired(int64_t now);
    void reset(const std::vector<Entry> &expired, int64_t now);

    bool insert(Timer *timer);

    EventLoop *loop_;
    const int timerfd_;
    std::unique_ptr<Channel> timerfdChannel_;
    TimerList timers_;
    ActiveTimerSet activeTimers_;
    bool callingExpiredTimers_;
    ActiveTimerSet cancelingTimers_;
};

#endif // NET_TIMER_QUEUE_H