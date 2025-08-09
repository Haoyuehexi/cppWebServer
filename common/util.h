#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

class Util {
  public:
    // 字符串分割
    static std::vector<std::string> split(const std::string &str,
                                          char delimiter);

    // 去除字符串首尾空白
    static std::string trim(const std::string &str);

    // 获取当前时间戳字符串
    static std::string getCurrentTime();

    // 文件是否存在
    static bool fileExists(const std::string &path);
};

#endif