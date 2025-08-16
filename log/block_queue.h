#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>

template <typename T> class block_queue {
  public:
    // Pushes an item to the back of the queue.
    void push(const T &item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(item);
        m_cond.notify_one(); // Wake up one waiting consumer.
    }

    // Pops an item from the front of the queue, blocking until an item is
    // available.
    bool pop(T &item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] { return !m_queue.empty(); });

        if (m_queue.empty()) {
            return false;
        }

        item = m_queue.front();
        m_queue.pop();
        return true;
    }

    // // Pops an item with a timeout.
    // bool pop(T &item, int ms_timeout) {
    //     std::unique_lock<std::mutex> lock(m_mutex);

    //     if (!m_cond.wait_for(lock, std::chrono::milliseconds(ms_timeout),
    //                          [this] { return !m_queue.empty(); })) {
    //         return false; // Timeout occurred.
    //     }

    //     if (m_queue.empty()) {
    //         return false; // Spurious wakeup, but still empty.
    //     }

    //     item = m_queue.front();
    //     m_queue.pop();
    //     return true;
    // }

    // Other methods...
    int size() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

  private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};