#ifndef NET_TIMER_H
#define NET_TIMER_H

#include <atomic>
#include <functional>
#include <memory>

class Timer {
  public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, int64_t when, double interval)
        : callback_(std::move(cb)), expiration_(when), interval_(interval),
          repeat_(interval > 0.0), sequence_(s_numCreated_.fetch_add(1)) {}

    void run() const { callback_(); }

    int64_t expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    void restart(int64_t now);

    static int64_t numCreated() { return s_numCreated_.load(); }

  private:
    const TimerCallback callback_;
    int64_t expiration_;
    const double interval_;
    const bool repeat_;
    const int64_t sequence_;

    static std::atomic<int64_t> s_numCreated_;
};

// 时间相关工具函数
int64_t now();
int64_t addTime(int64_t timestamp, double seconds);

#endif // NET_TIMER_H