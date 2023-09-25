#pragma once

#include "defs.hpp"

#include <condition_variable>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <list>

NEKO_NS_BEGIN

/**
 * @brief Thread with callback queue
 * 
 */
class NEKO_API Thread {
public:
    Thread();
    Thread(const Thread &) = delete;
    ~Thread();

    void dispatchTask();
    void sendTask(std::function<void()> &&func);
    void postTask(std::function<void()> &&func);
    bool idle() const noexcept {
        return mIdle;
    }

    static Thread *currentThread();
private:
    void run();

    Atomic<bool> mIdle {true};
    Atomic<bool> mRunning {true};

    std::queue<std::function<void()>> mQueue;
    std::condition_variable           mCondition;
    std::mutex                        mConditionMutex;
    std::mutex                        mQueueMutex;
    std::thread                       mThread;
};

NEKO_NS_END