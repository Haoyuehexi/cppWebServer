#include <cerrno>
#include <condition_variable> // C++11
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex> // C++11
#include <netdb.h>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread> // C++11
#include <unistd.h>
#include <vector>

#define MAXLINE 8192
#define MAXBUF 8192
#define LISTENQ 1024
#define NTHREADS 4 /* Number of worker threads */

// 自定义的异常类，用于处理特定错误
class SocketError : public std::runtime_error {
  public:
    explicit SocketError(const std::string &msg) : std::runtime_error(msg) {}
};

class PosixError : public SocketError {
  public:
    explicit PosixError(const std::string &msg, int code)
        : SocketError(msg + ": " + std::string(strerror(code))) {}
};

class GaiError : public SocketError {
  public:
    explicit GaiError(const std::string &msg, int code)
        : SocketError(msg + ": " + std::string(gai_strerror(code))) {}
};

// -----------------------------------------------------------
// Socket 类：封装文件描述符和基本I/O操作
// -----------------------------------------------------------
class Socket {
  public:
    explicit Socket(int fd) : fd_(fd) {
        if (fd_ < 0) {
            throw SocketError(
                "Invalid file descriptor passed to Socket constructor");
        }
    }

    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    Socket(Socket &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

    Socket &operator=(Socket &&other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int getFd() const { return fd_; }

    std::string readLine() {
        std::string line;
        char c;
        int n;
        while ((n = read(fd_, &c, 1)) > 0) {
            line += c;
            if (c == '\n') {
                return line;
            }
        }
        if (n == 0 && line.empty()) {
            throw SocketError("Connection closed by peer.");
        }
        if (n < 0) {
            throw PosixError("Failed to read from socket", errno);
        }
        return line;
    }

    void write(const std::string &data) {
        if (::write(fd_, data.c_str(), data.size()) < 0) {
            throw PosixError("Failed to write to socket", errno);
        }
    }

  private:
    int fd_;
};

// -----------------------------------------------------------
// ServerSocket 类：封装监听套接字
// -----------------------------------------------------------
class ServerSocket {
  public:
    explicit ServerSocket(const std::string &port) {
        struct addrinfo hints, *listp, *p;
        int rc;
        int optval = 1;

        std::memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

        if ((rc = getaddrinfo(nullptr, port.c_str(), &hints, &listp)) != 0) {
            throw GaiError("getaddrinfo failed", rc);
        }

        for (p = listp; p; p = p->ai_next) {
            if ((listenfd_ = socket(p->ai_family, p->ai_socktype,
                                    p->ai_protocol)) < 0) {
                continue;
            }

            setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &optval,
                       sizeof(int));

            if (bind(listenfd_, p->ai_addr, p->ai_addrlen) == 0) {
                break;
            }
            close(listenfd_);
        }

        freeaddrinfo(listp);
        if (!p) {
            throw SocketError("No address worked for binding");
        }

        if (listen(listenfd_, LISTENQ) < 0) {
            close(listenfd_);
            throw PosixError("listen failed", errno);
        }
    }

    ~ServerSocket() {
        if (listenfd_ >= 0) {
            close(listenfd_);
        }
    }

    ServerSocket(const ServerSocket &) = delete;
    ServerSocket &operator=(const ServerSocket &) = delete;

    int getListenFd() const { return listenfd_; }

    int acceptConnection() {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int connfd = ::accept(listenfd_,
                              reinterpret_cast<struct sockaddr *>(&clientaddr),
                              &clientlen);

        if (connfd < 0) {
            throw PosixError("accept failed", errno);
        }

        char hostname[MAXLINE], port[MAXLINE];
        if (getnameinfo(reinterpret_cast<struct sockaddr *>(&clientaddr),
                        clientlen, hostname, MAXLINE, port, MAXLINE, 0) != 0) {
            std::cerr << "Warning: getnameinfo failed." << std::endl;
        } else {
            std::cout << "Accepted connection from (" << hostname << ", "
                      << port << ")" << std::endl;
        }

        return connfd;
    }

  private:
    int listenfd_;
};

// -----------------------------------------------------------
// HTTP Server 实现
// -----------------------------------------------------------
void clientError(Socket &sock, const std::string &cause,
                 const std::string &errnum, const std::string &shortmsg,
                 const std::string &longmsg) {
    std::stringstream body_stream;
    body_stream << "<html><title>Tiny Error</title><body>" << errnum << ": "
                << shortmsg << "<p>" << longmsg << ": " << cause
                << "</p></body></html>";
    std::string body = body_stream.str();

    std::stringstream response_stream;
    response_stream << "HTTP/1.0 " << errnum << " " << shortmsg << "\r\n"
                    << "Content-type: text/html\r\n"
                    << "Content-length: " << body.size() << "\r\n\r\n"
                    << body;
    sock.write(response_stream.str());
}

void readRequestHeaders(Socket &sock) {
    std::string line;
    while (true) {
        line = sock.readLine();
        std::cout << line;
        if (line == "\r\n" || line.empty()) {
            break;
        }
    }
}

bool parseUri(const std::string &uri, std::string &filename,
              std::string &cgiargs) {
    if (uri.find("cgi-bin") == std::string::npos) {
        cgiargs.clear();
        filename = "." + uri;
        if (uri.back() == '/') {
            filename += "home.html";
        }
        return true;
    } else {
        size_t query_pos = uri.find('?');
        if (query_pos != std::string::npos) {
            cgiargs = uri.substr(query_pos + 1);
            filename = "." + uri.substr(0, query_pos);
        } else {
            cgiargs.clear();
            filename = "." + uri;
        }
        return false;
    }
}

std::string get_filetype(const std::string &filename) {
    if (filename.rfind(".html") != std::string::npos) {
        return "text/html";
    } else if (filename.rfind(".gif") != std::string::npos) {
        return "image/gif";
    } else if (filename.rfind(".png") != std::string::npos) {
        return "image/png";
    } else if (filename.rfind(".jpg") != std::string::npos ||
               filename.rfind(".jpeg") != std::string::npos) {
        return "image/jpeg";
    } else {
        return "text/plain";
    }
}

void serveStatic(Socket &sock, const std::string &filename, size_t filesize) {
    int srcfd = -1;
    try {
        std::string filetype = get_filetype(filename);

        std::stringstream header_stream;
        header_stream << "HTTP/1.0 200 OK\r\n"
                      << "Server: Tiny Web Server\r\n"
                      << "Content-type: " << filetype << "\r\n"
                      << "Content-length: " << filesize << "\r\n\r\n";

        sock.write(header_stream.str());

        srcfd = open(filename.c_str(), O_RDONLY, 0);
        if (srcfd < 0) {
            throw PosixError("serveStatic open failed", errno);
        }

        std::vector<char> buf(MAXBUF);
        ssize_t n;
        while ((n = read(srcfd, buf.data(), buf.size())) > 0) {
            sock.write(std::string(buf.data(), n));
        }

        close(srcfd);
        srcfd = -1;

    } catch (const PosixError &e) {
        if (srcfd >= 0) {
            close(srcfd);
        }
        throw;
    }
}

void serveDynamic(Socket &sock, const std::string &filename,
                  const std::string &cgiargs) {
    std::stringstream header_stream;
    header_stream << "HTTP/1.0 200 OK\r\n"
                  << "Server: Tiny Web Server\r\n\r\n";

    sock.write(header_stream.str());

    pid_t pid = fork();
    if (pid < 0) {
        throw PosixError("Fork failed", errno);
    }

    if (pid == 0) { // 子进程
        setenv("QUERY_STRING", cgiargs.c_str(), 1);
        dup2(sock.getFd(), STDOUT_FILENO);

        execl(filename.c_str(), filename.c_str(), nullptr);

        _exit(1);
    } else { // 父进程
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            throw PosixError("waitpid failed", errno);
        }
    }
}

void handleHttpRequest(Socket sock) {
    try {
        std::string requestLine = sock.readLine();
        std::cout << requestLine;

        std::stringstream ss(requestLine);
        std::string method, uri, version;
        ss >> method >> uri >> version;

        if (method != "GET") {
            readRequestHeaders(sock);
            clientError(sock, method, "501", "Not Implemented",
                        "Tiny does not implement this method");
            return;
        }

        readRequestHeaders(sock);

        std::string filename, cgiargs;
        bool is_static = parseUri(uri, filename, cgiargs);

        struct stat sbuf;
        if (stat(filename.c_str(), &sbuf) < 0) {
            clientError(sock, filename, "404", "Not found",
                        "Tiny couldn't find this file");
            return;
        }

        if (is_static) {
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
                clientError(sock, filename, "403", "Forbidden",
                            "Tiny couldn't read the file");
                return;
            }
            serveStatic(sock, filename, sbuf.st_size);
        } else {
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
                clientError(sock, filename, "403", "Forbidden",
                            "Tiny couldn't run the CGI program");
                return;
            }
            serveDynamic(sock, filename, cgiargs);
        }

    } catch (const SocketError &e) {
        std::cerr << "Error handling request: " << e.what() << std::endl;
    }
}

// -----------------------------------------------------------
// 线程池和任务队列（C++11风格）
// -----------------------------------------------------------
class ThreadPool {
  public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    int connfd;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }
                        connfd = this->tasks.front();
                        this->tasks.pop();
                    }
                    Socket connSock(connfd);
                    handleHttpRequest(std::move(connSock));
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            worker.join();
        }
    }

    void enqueue(int connfd) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks.push(connfd);
        }
        condition.notify_one();
    }

  private:
    std::vector<std::thread> workers;
    std::queue<int> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// -----------------------------------------------------------
// Main 函数
// -----------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    try {
        ServerSocket server(argv[1]);
        ThreadPool pool(NTHREADS);

        while (true) {
            int connfd = server.acceptConnection();
            pool.enqueue(connfd);
        }
    } catch (const SocketError &e) {
        std::cerr << "Server fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
