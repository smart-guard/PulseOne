#ifndef PULSEONE_UTILS_THREAD_SAFE_QUEUE_H
#define PULSEONE_UTILS_THREAD_SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <vector>

namespace PulseOne {
namespace Utils {

/**
 * @brief Thread-safe queue for producer-consumer patterns.
 */
template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    
    // Push a single item
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }
        cond_.notify_one();
    }

    // Pop a single item (blocking)
    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    // Try to pop a single item (non-blocking)
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    // Pop multiple items up to max_items (blocking if empty, with optional timeout)
    std::vector<T> pop_batch(size_t max_items, uint32_t timeout_ms = 0) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout_ms > 0) {
            cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return !queue_.empty(); });
        } else {
            cond_.wait(lock, [this] { return !queue_.empty(); });
        }
        
        std::vector<T> batch;
        while (!queue_.empty() && batch.size() < max_items) {
            batch.push_back(std::move(queue_.front()));
            queue_.pop();
        }
        return batch;
    }

    // Try to pop all current items
    std::vector<T> try_pop_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> batch;
        while (!queue_.empty()) {
            batch.push_back(std::move(queue_.front()));
            queue_.pop();
        }
        return batch;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable cond_;
};

} // namespace Utils
} // namespace PulseOne

#endif // PULSEONE_UTILS_THREAD_SAFE_QUEUE_H
