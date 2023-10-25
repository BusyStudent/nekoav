#pragma once

#include "defs.hpp"
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <queue>

NEKO_NS_BEGIN

/**
 * @brief A MT-Safe Queue template
 * 
 * @tparam T 
 */
template <typename T>
class BlockingQueue {
public:
    BlockingQueue() = default;
    BlockingQueue(const BlockingQueue &) = delete;
    ~BlockingQueue() = default;

    void push(const T &value) {
        std::lock_guard locker(mMutex);
        mQueue.push(value);
        mCondition.notify_one();
    }
    void push(T &&value) {
        std::lock_guard locker(mMutex);
        mQueue.push(std::move(value));
        mCondition.notify_one();
    }

    template <typename ...Args>
    void emplace(Args &&...args) {
        std::lock_guard locker(mMutex);
        mQueue.emplace(std::forward<Args>(args)...);
        mCondition.notify_one();
    }

    bool empty() const {
        std::lock_guard locker(mMutex);
        return mQueue.empty();
    }
    size_t size() const {
        std::lock_guard locker(mMutex);
        return mQueue.size();
    }
    T      pop() {
        std::unique_lock locker(mMutex);
        while (mQueue.empty()) {
            mCondition.wait(locker);
        }
        auto value = std::move(mQueue.front());
        mQueue.pop();
        return value;
    }

    bool wait(T *value, int milliseconds = -1) {
        std::unique_lock locker(mMutex);
        while (mQueue.empty()) {
            if (milliseconds == -1) {
                mCondition.wait(locker);
            }
            else {
                mCondition.wait_for(locker, std::chrono::milliseconds(milliseconds));
            }
        }
        *value = std::move(mQueue.front());
        mQueue.pop();
        return true;
    }
    bool tryWait(T *value) {
        if (empty()) {
            return false;
        }
        *value = pop();
        return true;
    }
    void clear() {
        std::lock_guard locker(mMutex);
        while (!mQueue.empty()) {
            mQueue.pop();
        }
    }
private:
    std::queue<T>           mQueue;
    std::condition_variable mCondition;
    mutable std::mutex      mMutex;
};

NEKO_NS_END