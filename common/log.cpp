#include "log.h"
#include <chrono>
#include <iomanip>
#include <iostream>

std::ofstream Logger::logFile;
std::mutex Logger::logMutex;
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

void Logger::init(const std::string &filename, LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
    logFile.open(filename, std::ios::app);
    if (!logFile) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
    currentLevel = level;
}

void Logger::log(LogLevel level, const std::string &message) {
    if (level < currentLevel)
        return;

    std::lock_guard<std::mutex> lock(logMutex);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count() << " ["
        << levelToString(level) << "] " << message << "\n";

    std::string output = oss.str();

    if (logFile.is_open()) {
        logFile << output;
        logFile.flush();
    }
    std::cout << output;
}

void Logger::debug(const std::string &message) { log(DEBUG, message); }
void Logger::info(const std::string &message) { log(INFO, message); }
void Logger::warn(const std::string &message) { log(WARN, message); }
void Logger::error(const std::string &message) { log(ERROR, message); }

void Logger::close() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
}
