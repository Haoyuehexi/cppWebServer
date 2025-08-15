#include "timer_queue.h"
#include "channel.h"
#include "event_loop.h"
#include "timer.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/timerfd.h>
#include <unistd.h>

int createTimerfd() {
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        std::cerr << "Failed in timerfd_create" << std::endl;
    }
    return timerfd;
}

struct timespec howMuchTimeFromNow(int64_t when) {
    int64_t microseconds = when - now();
    if (microseconds < 100) {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / 1000000);
    ts.tv_nsec = static_cast<long>((microseconds % 1000000) * 1000);
    return ts;
}

void readTimerfd(int timerfd, int64_t now) {
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    if (n != sizeof howmany) {
        std::cerr << "TimerQueue::handleRead() reads " << n
                  << " bytes instead of 8" << std::endl;
    }
}

void resetTimerfd(int timerfd, int64_t expiration) {
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, 0, sizeof newValue);
    memset(&oldValue, 0, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret) {
        std::cerr << "timerfd_settime()" << std::endl;
    }
}

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop), timerfd_(createTimerfd()),
      timerfdChannel_(std::make_unique<Channel>(loop, timerfd_)), timers_(),
      callingExpiredTimers_(false) {
    timerfdChannel_->setReadCallback([this]() { handleRead(); });
    timerfdChannel_->enableReading();
}

TimerQueue::~TimerQueue() {
    timerfdChannel_->disableAll();
    timerfdChannel_->remove();
    ::close(timerfd_);
    for (const Entry &timer : timers_) {
        delete timer.second;
    }
}

void TimerQueue::addTimer(const std::function<void()> &cb, int64_t when,
                          double interval) {
    Timer *timer = new Timer(cb, when, interval);
    loop_->runInLoop([this, timer]() { addTimerInLoop(timer); });
}

void TimerQueue::addTimerInLoop(Timer *timer) {
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);

    if (earliestChanged) {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::handleRead() {
    loop_->assertInLoopThread();
    int64_t now_time = now();
    readTimerfd(timerfd_, now_time);

    std::vector<Entry> expired = getExpired(now_time);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    for (const Entry &it : expired) {
        it.second->run();
    }
    callingExpiredTimers_ = false;

    reset(expired, now_time);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(int64_t now_time) {
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;
    Entry sentry(now_time + 1, reinterpret_cast<Timer *>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now_time < end->first);
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);

    for (const Entry &it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
        (void)n;
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

void TimerQueue::reset(const std::vector<Entry> &expired, int64_t now_time) {
    int64_t nextExpire;

    for (const Entry &it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        if (it.second->repeat() &&
            cancelingTimers_.find(timer) == cancelingTimers_.end()) {
            it.second->restart(now_time);
            insert(it.second);
        } else {
            delete it.second;
        }
    }

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire > 0) {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer *timer) {
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;
    int64_t when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first) {
        earliestChanged = true;
    }
    {
        std::pair<TimerList::iterator, bool> result =
            timers_.insert(Entry(when, timer));
        assert(result.second);
        (void)result;
    }
    {
        std::pair<ActiveTimerSet::iterator, bool> result =
            activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}