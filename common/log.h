#ifndef LOG_H
#define LOG_H

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

enum LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
  private:
    static std::ofstream logFile;
    static std::mutex queueMutex;
    static std::condition_variable cv;
    static std::queue<std::string> logQueue;
    static std::atomic<bool> running;
    static std::thread workerThread;
    static LogLevel currentLevel;

    static std::string levelToString(LogLevel level);
    static LogLevel stringToLevel(const std::string &level);
    static void worker();
    static void enqueue(LogLevel level, const std::string &message);

  public:
    static void init(const std::string &filename = "server.log",
                     std::string level = "INFO");
    static void log(LogLevel level, const std::string &message);
    static void debug(const std::string &message);
    static void info(const std::string &message);
    static void warn(const std::string &message);
    static void error(const std::string &message);
    static void close();
};

// 便捷宏
#define LOG_DEBUG(msg) Logger::debug(msg)
#define LOG_INFO(msg) Logger::info(msg)
#define LOG_WARN(msg) Logger::warn(msg)
#define LOG_ERROR(msg) Logger::error(msg)

#endif
