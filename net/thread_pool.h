#pragma once
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t nthreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void start();
    void stop();       // 优雅停止：处理完队列再退出
    void submit(std::function<void()> task);

private:
    void workerLoop();

private:
    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::atomic<bool> running_{false};
    bool stopping_{false};
};
