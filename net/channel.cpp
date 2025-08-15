#include "channel.h"
#include "event_loop.h"
#include <cassert>
#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false),
      eventHandling_(false) {}

Channel::~Channel() {
    assert(!eventHandling_);
    if (loop_->isInLoopThread()) {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::update() { loop_->updateChannel(this); }

void Channel::handleEvent() {
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard();
        }
    } else {
        handleEventWithGuard();
    }
}

void Channel::handleEventWithGuard() {
    eventHandling_ = true;

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_)
            closeCallback_();
    }

    if (revents_ & EPOLLERR) {
        if (errorCallback_)
            errorCallback_();
    }

    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_)
            readCallback_();
    }

    if (revents_ & EPOLLOUT) {
        if (writeCallback_)
            writeCallback_();
    }

    eventHandling_ = false;
}

void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

void Channel::remove() {
    assert(isNoneEvent());
    loop_->removeChannel(this);
}