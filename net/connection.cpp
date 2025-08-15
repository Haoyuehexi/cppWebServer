#include "connection.h"
#include "event_loop.h"
#include "channel.h"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

Connection::Connection(EventLoop* loop, int fd)
: loop_(loop), fd_(fd) {}

Connection::~Connection() {}

void Connection::connectEstablished() {
    assert(state_ == State::kConnecting);
    channel_ = std::make_unique<Channel>(loop_, fd_);
    channel_->setReadCallback([self = shared_from_this()] { self->handleRead(); });
    channel_->setWriteCallback([self = shared_from_this()] { self->handleWrite(); });
    channel_->setCloseCallback([self = shared_from_this()] { self->handleClose(); });
    channel_->setErrorCallback([self = shared_from_this()] { self->handleError(); });

    state_ = State::kConnected;
    channel_->enableReading();   // 关注可读
}

void Connection::connectDestroyed() {
    if (state_ == State::kConnected) {
        state_ = State::kDisconnected;
        channel_->disableAll();
    }
    // 让 EventLoop 移除 Channel（取决于你的 EventLoop/Channel 实现）
    channel_.reset();
    ::close(fd_);
}

void Connection::send(const void* data, size_t len) {
    if (state_ != State::kConnected) return;
    if (loop_) {
        // 如果你的 EventLoop 有 runInLoop/queueInLoop，请改成它们
        loop_->runInLoop([self = shared_from_this(), data_str = std::string((const char*)data, len)]() {
            self->sendInLoop(data_str.data(), data_str.size());
        });
    }
}

void Connection::sendInLoop(const void* data, size_t len) {
    if (state_ != State::kConnected) return;

    ssize_t nwrote = 0;
    bool writing = channel_->isWriting(); // 取决于你的 Channel 是否有此查询接口，没有可以用内部状态代替
    if (!writing && outbuf_.empty()) {
        nwrote = ::send(fd_, data, len, 0);
        if (nwrote >= 0) {
            if ((size_t)nwrote == len) {
                if (write_complete_cb_) write_complete_cb_(shared_from_this());
                return;
            }
        } else {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                // 出错：交给 error 处理，但仍把没写完的缓冲起来，等待 handleWrite
            }
            nwrote = 0;
        }
    }
    // 还有未写完的部分缓存下来
    outbuf_.append((const char*)data + nwrote, len - nwrote);
    // 关注可写
    channel_->enableWriting();
}

void Connection::shutdown() {
    if (state_ == State::kConnected) {
        state_ = State::kDisconnecting;
        loop_->runInLoop([self = shared_from_this()] { self->shutdownInLoop(); });
    }
}

void Connection::shutdownInLoop() {
    if (!channel_->isWriting()) {
        ::shutdown(fd_, SHUT_WR);
    }
    // 若仍在写，就等 handleWrite 清空后再 shutdown
}

void Connection::forceClose() {
    loop_->runInLoop([self = shared_from_this()] { self->forceCloseInLoop(); });
}

void Connection::forceCloseInLoop() {
    if (state_ == State::kDisconnected) return;
    handleClose();
}

void Connection::handleRead() {
    char buf[8192];
    for (;;) {
        ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
        if (n > 0) {
            inbuf_.append(buf, n);
        } else if (n == 0) {
            // 对端关闭
            handleClose();
            return;
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break; // 没有更多数据
            } else if (errno == EINTR) {
                continue;
            } else {
                handleError();
                return;
            }
        }
    }
    if (message_cb_ && !inbuf_.empty()) {
        // 直接把当前缓冲全部上交给业务层（简单模型）
        std::string data;
        data.swap(inbuf_);
        message_cb_(shared_from_this(), std::string_view(data));
    }
}

void Connection::handleWrite() {
    if (!channel_->isWriting()) return;
    ssize_t n = ::send(fd_, outbuf_.data(), outbuf_.size(), 0);
    if (n > 0) {
        outbuf_.erase(0, n);
        if (outbuf_.empty()) {
            channel_->disableWriting();
            if (write_complete_cb_) write_complete_cb_(shared_from_this());
            if (state_ == State::kDisconnecting) {
                ::shutdown(fd_, SHUT_WR);
            }
        }
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return;
        } else if (errno == EINTR) {
            return;
        } else {
            handleError();
        }
    }
}

void Connection::handleClose() {
    state_ = State::kDisconnected;
    channel_->disableAll();
    if (close_cb_) close_cb_(shared_from_this());
}

void Connection::handleError() {
    // 记录 errno or 日志（你已有 log 系统）
    handleClose();
}
