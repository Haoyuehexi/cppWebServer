#include "../common/util.h"
#include <iostream>

void test_util() {
    std::cout << "Testing Util class..." << std::endl;

    // 测试时间函数
    std::cout << "Current time: " << Util::getCurrentTime() << std::endl;

    // 测试字符串分割
    auto parts = Util::split("GET /index.html HTTP/1.1", ' ');
    for (const auto &part : parts) {
        std::cout << "Part: [" << part << "]" << std::endl;
    }

    auto lines = Util::trim("  \n  hello world  \n  ");
    std::cout << lines << std::endl;

    std::string path = "./resources/html/index.html";
    std::cout << "Path: " << path << std::endl;
    std::cout << (Util::fileExists(path) ? "File exists" : "File not exists")
              << std::endl;
}

int main() {
    test_util();
    return 0;
}