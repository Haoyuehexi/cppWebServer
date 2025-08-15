#include "timer.h"
#include <sys/time.h>

std::atomic<int64_t> Timer::s_numCreated_(0);

void Timer::restart(int64_t now) {
    if (repeat_) {
        expiration_ = addTime(now, interval_);
    } else {
        expiration_ = 0;
    }
}

int64_t now() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t seconds = tv.tv_sec;
    return seconds * 1000000 + tv.tv_usec;
}

int64_t addTime(int64_t timestamp, double seconds) {
    int64_t delta = static_cast<int64_t>(seconds * 1000000);
    return timestamp + delta;
}