#include "util.h"
#include <ctime>
#include <fstream>
#include <sstream>

std::vector<std::string> Util::split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string Util::trim(const std::string &str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string Util::getCurrentTime() {
    time_t now = time(0);
    char *dt = ctime(&now);
    std::string timeStr(dt);
    return timeStr.substr(0, timeStr.length() - 1); // 移除换行符
}

bool Util::fileExists(const std::string &path) {
    std::ifstream file(path);
    return file.good();
}