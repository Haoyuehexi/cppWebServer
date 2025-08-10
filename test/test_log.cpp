#include "../common/log.h"
#include <thread>

int main() {
    Logger::init("./logs/test.log", DEBUG);

    std::thread t1([] {
        for (int i = 0; i < 10; i++) {
            LOG_INFO("Thread 1 message " + std::to_string(i));
        }
    });

    std::thread t2([] {
        for (int i = 0; i < 10; i++) {
            LOG_WARN("Thread 2 warning " + std::to_string(i));
        }
    });

    t1.join();
    t2.join();

    Logger::close();
    return 0;
}
