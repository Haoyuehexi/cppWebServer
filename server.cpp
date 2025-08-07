#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXLINE 8192
#define MAXBUF 8192
#define LISTENQ 1024

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

    // 利用RAII，在对象销毁时自动关闭套接字
    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    // 禁用拷贝构造和赋值，以确保唯一所有权
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    // 移动构造函数和移动赋值操作符
    Socket(Socket &&other) noexcept : fd_(other.fd_) {
        other.fd_ = -1; // 转移所有权
    }

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

    // 简化的读取一行的方法，这里没有实现内部缓冲，实际中应该实现
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

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

        if ((rc = getaddrinfo(NULL, port.c_str(), &hints, &listp)) != 0) {
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

    // 接受新连接，返回一个Socket对象
    Socket accept() {
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

        return Socket(connfd);
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
    std::string body = "<html><title>Tiny Error</title><body>";
    body += errnum + ": " + shortmsg;
    body += "<p>" + longmsg + ": " + cause + "</p></body></html>";

    std::string response = "HTTP/1.0 " + errnum + " " + shortmsg + "\r\n";
    response += "Content-type: text/html\r\n";
    response += "Content-length: " + std::to_string(body.size()) + "\r\n\r\n";
    response += body;
    sock.write(response);
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

bool parseUri(const std::string& uri, std::string& filename, std::string& cgiargs) {
    if (uri.find("cgi-bin") == std::string::npos) {
        // 静态内容
        cgiargs.clear();
        filename = "." + uri;
        if (uri.back() == '/') {
            filename += "home.html";
        }
        return true;
    } else {
        // 动态内容
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

std::string get_filetype(const std::string& filename) {
    if (filename.find(".html") != std::string::npos) {
        return "text/html";
    } else if (filename.find(".gif") != std::string::npos) {
        return "image/gif";
    } else if (filename.find(".png") != std::string::npos) {
        return "image/png";
    } else if (filename.find(".jpg") != std::string::npos || filename.find(".jpeg") != std::string::npos) {
        return "image/jpeg";
    } else {
        return "text/plain";
    }
}

void serveStatic(Socket& sock, const std::string& filename, size_t filesize) {
    int srcfd = -1;
    try {
        // 假设 get_filetype 已经用 C++ 重写
        std::string filetype = get_filetype(filename);

        // 使用 stringstream 构建 HTTP 响应头
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

        char buf[MAXBUF];
        ssize_t n;
        while ((n = read(srcfd, buf, MAXBUF)) > 0) {
            sock.write(std::string(buf, n));
        }

        close(srcfd);
        srcfd = -1; // 标记已关闭

    } catch (const PosixError& e) {
        if (srcfd >= 0) {
            close(srcfd);
        }
        throw; // 重新抛出异常
    }
}

void serveDynamic(Socket& sock, const std::string& filename, const std::string& cgiargs) {
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
        
        // 使用execlp可以在PATH中查找可执行文件
        execl(filename.c_str(), filename.c_str(), nullptr);
        
        // 如果execl失败，_exit是子进程退出最安全的方式
        _exit(1); 
    } else { // 父进程
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            throw PosixError("waitpid failed", errno);
        }
    }
}

// -----------------------------------------------------------
// 主处理函数
// -----------------------------------------------------------
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
        // 注意：这里无法再向客户端发送错误，因为连接可能已断开
    }
}

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

        while (true) {
            Socket connSock = server.accept();
            // 在实际项目中，这里会启动一个新线程或协程来处理请求
            handleHttpRequest(std::move(connSock));
        }
    } catch (const SocketError &e) {
        std::cerr << "Server fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}