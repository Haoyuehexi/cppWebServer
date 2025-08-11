#include "../net/socket.h"
#include <iostream>
#include <thread>
#include <chrono>

// 简单回声服务器
void run_server() {
    try {
        Socket server(-1);
        server.create();
        server.setReuseAddr();
        server.bind(8080);
        server.listen();

        std::cout << "[Server] Listening on port 8080..." << std::endl;
        int client_fd = server.accept();
        Socket client(client_fd);

        std::cout << "[Server] Client connected: "
                  << client.getPeerAddress() << ":"
                  << client.getPeerPort() << std::endl;

        while (true) {
            char buf[1024] = {0};
            ssize_t n = client.read(buf, sizeof(buf) - 1);
            if (n <= 0) break;
            std::cout << "[Server] Received: " << buf << std::endl;
            client.write(buf, n); // 回显
        }

    } catch (const std::exception &e) {
        std::cerr << "[Server Error] " << e.what() << std::endl;
    }
}

// 简单客户端
void run_client() {
    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 等服务器起来
        Socket sock(-1);
        sock.create();
        sock.connect("127.0.0.1", 8080);

        std::cout << "[Client] Connected to server." << std::endl;
        std::string msg = "Hello from client!\n";
        sock.write(msg);

        char buf[1024] = {0};
        ssize_t n = sock.read(buf, sizeof(buf) - 1);
        if (n > 0) {
            std::cout << "[Client] Received: " << buf << std::endl;
        }

    } catch (const std::exception &e) {
        std::cerr << "[Client Error] " << e.what() << std::endl;
    }
}

int main() {
    std::thread server_thread(run_server);
    std::thread client_thread(run_client);

    server_thread.join();
    client_thread.join();
    return 0;
}
