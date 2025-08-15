#include "event_loop.h"
#include "channel.h"
#include "timer.h"
#include "timer_queue.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

__thread EventLoop *t_loopInThisThread = nullptr;

const int EventLoop::kInitEventListSize;
const int EventLoop::kNew;
const int EventLoop::kAdded;
const int EventLoop::kDeleted;

static int createEventfd() {
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        std::cerr << "Failed in eventfd" << std::endl;
        abort();
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false), quit_(false), eventHandling_(false),
      callingPendingFunctors_(false), threadId_(std::this_thread::get_id()),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize),
      timerQueue_(std::make_unique<TimerQueue>(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_)) {
    if (t_loopInThisThread) {
        std::cerr << "Another EventLoop exists in this thread" << std::endl;
        abort();
    } else {
        t_loopInThisThread = this;
    }

    if (epollfd_ < 0) {
        std::cerr << "Failed to create epoll" << std::endl;
        abort();
    }

    wakeupChannel_->setReadCallback([this]() { handleRead(); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    ::close(epollfd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();

        eventHandling_ = true;
        for (Channel *channel : activeChannels_) {
            channel->handleEvent();
        }
        eventHandling_ = false;

        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

int EventLoop::poll(int timeoutMs, ChannelList *activeChannels) {
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                                 static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;
    if (numEvents > 0) {
        fillActiveChannels(numEvents, activeChannels);
        if (static_cast<size_t>(numEvents) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        // timeout
    } else {
        if (savedErrno != EINTR) {
            errno = savedErrno;
            std::cerr << "EventLoop::poll() error" << std::endl;
        }
    }
    return numEvents;
}

void EventLoop::fillActiveChannels(int numEvents,
                                   ChannelList *activeChannels) const {
    assert(static_cast<size_t>(numEvents) <= events_.size());
    for (int i = 0; i < numEvents; ++i) {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        int fd = channel->fd();
        auto it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void EventLoop::updateChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    updateChannelInEpoll(channel);
}

void EventLoop::updateChannelInEpoll(Channel *channel) {
    const int index = channel->index();
    int fd = channel->fd();

    if (index == kNew || index == kDeleted) {
        // a new one, add with EPOLL_CTL_ADD
        if (index == kNew) {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        } else { // index == kDeleted
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }

        channel->set_index(kAdded);
        epollUpdate(EPOLL_CTL_ADD, channel);
    } else {
        // update existing one with EPOLL_CTL_MOD/DEL
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);
        if (channel->isNoneEvent()) {
            epollUpdate(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            epollUpdate(EPOLL_CTL_MOD, channel);
        }
    }
}

void EventLoop::removeChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if (eventHandling_) {
        assert(std::find(activeChannels_.begin(), activeChannels_.end(),
                         channel) == activeChannels_.end());
    }
    removeChannelFromEpoll(channel);
}

void EventLoop::removeChannelFromEpoll(Channel *channel) {
    int fd = channel->fd();
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());
    int index = channel->index();
    assert(index == kAdded || index == kDeleted);
    size_t n = channels_.erase(fd);
    assert(n == 1);

    if (index == kAdded) {
        epollUpdate(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

bool EventLoop::hasChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

void EventLoop::epollUpdate(int operation, Channel *channel) {
    struct epoll_event event;
    memset(&event, 0, sizeof event);
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            std::cerr << "epoll_ctl del error for fd " << fd << std::endl;
        } else {
            std::cerr << "epoll_ctl error for fd " << fd << std::endl;
            abort();
        }
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        std::cerr << "EventLoop::wakeup() writes " << n << " bytes instead of 8"
                  << std::endl;
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        std::cerr << "EventLoop::handleRead() reads " << n
                  << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::assertInLoopThread() {
    if (!isInLoopThread()) {
        std::cerr << "EventLoop was created in different thread" << std::endl;
        abort();
    }
}

EventLoop *EventLoop::getEventLoopOfCurrentThread() {
    return t_loopInThisThread;
}

void EventLoop::runAt(const std::function<void()> &cb, int64_t when) {
    timerQueue_->addTimer(cb, when, 0.0);
}

void EventLoop::runAfter(const std::function<void()> &cb, double delay) {
    int64_t when = addTime(now(), delay);
    runAt(cb, when);
}

void EventLoop::runEvery(const std::function<void()> &cb, double interval) {
    int64_t when = addTime(now(), interval);
    timerQueue_->addTimer(cb, when, interval);
}