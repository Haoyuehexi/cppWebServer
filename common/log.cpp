#include "log.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

std::ofstream Logger::logFile;
std::mutex Logger::queueMutex;
std::condition_variable Logger::cv;
std::queue<std::string> Logger::logQueue;
std::atomic<bool> Logger::running{false};
std::thread Logger::workerThread;
LogLevel Logger::currentLevel = INFO;

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
    case DEBUG:
        return "DEBUG";
    case INFO:
        return "INFO";
    case WARN:
        return "WARN";
    case ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

void Logger::worker() {
    while (running || !logQueue.empty()) {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [] { return !running || !logQueue.empty(); });

        while (!logQueue.empty()) {
            auto entry = logQueue.front();
            logQueue.pop();
            lock.unlock();

            if (logFile.is_open()) {
                logFile << entry;
            }
            std::cout << entry;

            lock.lock();
        }
        if (logFile.is_open()) {
            logFile.flush();
        }
    }
}

void Logger::init(const std::string &filename, LogLevel level) {
    if (running)
        return; // 防止重复初始化

    logFile.open(filename, std::ios::app);
    if (!logFile) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
    currentLevel = level;
    running = true;
    workerThread = std::thread(worker);
}

void Logger::enqueue(LogLevel level, const std::string &message) {
    if (level < currentLevel)
        return;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count() << " ["
        << levelToString(level) << "] " << message << "\n";

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        logQueue.push(oss.str());
    }
    cv.notify_one();
}

void Logger::log(LogLevel level, const std::string &message) {
    enqueue(level, message);
}

void Logger::debug(const std::string &message) { enqueue(DEBUG, message); }
void Logger::info(const std::string &message) { enqueue(INFO, message); }
void Logger::warn(const std::string &message) { enqueue(WARN, message); }
void Logger::error(const std::string &message) { enqueue(ERROR, message); }

void Logger::close() {
    running = false;
    cv.notify_all();
    if (workerThread.joinable()) {
        workerThread.join();
    }
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
}
