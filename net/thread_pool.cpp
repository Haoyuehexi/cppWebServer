#include "thread_pool.h"

ThreadPool::ThreadPool(size_t nthreads) {
    workers_.reserve(nthreads ? nthreads : 1);
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    if (running_) return;
    running_ = true;
    stopping_ = false;
    size_t n = workers_.capacity();
    for (size_t i = 0; i < n; ++i) {
        workers_.emplace_back([this]{ workerLoop(); });
    }
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
    running_ = false;
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (stopping_) return;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this]{ return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) break;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        if (task) task();
    }
}
