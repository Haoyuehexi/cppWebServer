#include "socket.h"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

Socket::Socket(int fd) : fd_(fd) {
    // if (fd_ < 0) {
    //     throw SocketError(
    //         "Invalid file descriptor passed to Socket constructor");
    // }
    memset(&addr_, 0, sizeof(addr_));
}

Socket::~Socket() { close(); }

Socket::Socket(Socket &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Socket &Socket::operator=(Socket &&other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        addr_ = other.addr_;
        other.fd_ = -1;
    }
    return *this;
}

bool Socket::create() {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        throw PosixError("Failed to create socket", errno);
    }
    return true;
}

bool Socket::bind(int port) {
    ensureValid();
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<struct sockaddr *>(&addr_),
               sizeof(addr_)) < 0) {
        throw PosixError("Failed to bind socket", errno);
    }
    return true;
}

bool Socket::listen(int backlog) {
    ensureValid();
    if (::listen(fd_, backlog) < 0) {
        throw PosixError("Failed to listen on socket", errno);
    }
    return true;
}

int Socket::accept() {
    ensureValid();
    socklen_t len = sizeof(addr_);
    int client_fd =
        ::accept(fd_, reinterpret_cast<struct sockaddr *>(&addr_), &len);
    if (client_fd < 0) {
        throw PosixError("Failed to accept connection", errno);
    }
    return client_fd;
}

bool Socket::connect(const std::string &host, int port) {
    ensureValid();
    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        throw SocketError("Invalid IP address");
    }

    if (::connect(fd_, reinterpret_cast<sockaddr *>(&serv_addr),
                  sizeof(serv_addr)) < 0) {
        throw PosixError("Failed to connect to server", errno);
    }
    return true;
}

std::string Socket::readLine() {
    ensureValid();
    std::string line;
    char c;
    ssize_t n;

    while ((n = ::read(fd_, &c, 1)) > 0) {
        line += c;
        if (c == '\n') {
            return line;
        }
    }

    if (n == 0 && line.empty()) {
        throw SocketError("Connection closed by peer");
    }
    if (n < 0) {
        throw PosixError("Failed to read from socket", errno);
    }
    return line;
}

ssize_t Socket::read(void *buffer, size_t size) {
    ensureValid();
    ssize_t n = ::read(fd_, buffer, size);
    if (n < 0) {
        throw PosixError("Failed to read from socket", errno);
    }
    return n;
}

ssize_t Socket::write(const void *data, size_t size) {
    ensureValid();
    ssize_t n = ::write(fd_, data, size);
    if (n < 0) {
        throw PosixError("Failed to write to socket", errno);
    }
    return n;
}

void Socket::write(const std::string &data) {
    write(data.c_str(), data.size());
}

void Socket::setNonBlocking(bool non_blocking) {
    ensureValid();
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        throw PosixError("Failed to get socket flags", errno);
    }

    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(fd_, F_SETFL, flags) < 0) {
        throw PosixError("Failed to set socket non-blocking", errno);
    }
}

void Socket::setReuseAddr(bool reuse) {
    ensureValid();
    int opt = reuse ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw PosixError("Failed to set SO_REUSEADDR", errno);
    }
}

void Socket::setKeepAlive(bool keep_alive) {
    ensureValid();
    int opt = keep_alive ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        throw PosixError("Failed to set SO_KEEPALIVE", errno);
    }
}

void Socket::setTcpNoDelay(bool no_delay) {
    ensureValid();
    int opt = no_delay ? 1 : 0;
    if (setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        throw PosixError("Failed to set TCP_NODELAY", errno);
    }
}

std::string Socket::getPeerAddress() const {
    ensureValid();
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (getpeername(fd_, reinterpret_cast<sockaddr *>(&addr), &len) < 0) {
        throw PosixError("Failed to get peer address", errno);
    }

    return std::string(inet_ntoa(addr.sin_addr));
}

int Socket::getPeerPort() const {
    ensureValid();
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (getpeername(fd_, reinterpret_cast<sockaddr *>(&addr), &len) < 0) {
        throw PosixError("Failed to get peer port", errno);
    }

    return ntohs(addr.sin_port);
}

void Socket::shutdown(int how) {
    if (fd_ >= 0) {
        ::shutdown(fd_, how);
    }
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void Socket::ensureValid() const {
    if (fd_ < 0) {
        throw SocketError("Socket is not valid");
    }
}
