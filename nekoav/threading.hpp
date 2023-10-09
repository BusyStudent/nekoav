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

    void setName(const char *name);
    /**
     * @brief Poll task from the queue and execute it
     * 
     */
    void dispatchTask();
    /**
     * @brief Wait task from the queue and execute it
     * 
     * @param timeoutMS 
     */
    void waitTask(const int *timeoutMS = 0);
    /**
     * @brief Send a task into queue and wait for it finish
     * 
     * @param func the callable function
     */
    void sendTask(std::function<void()> &&func);
    /**
     * @brief Send a task into queue and return
     * @details It will block the thread
     * 
     * @param func the callable function
     */
    void postTask(std::function<void()> &&func);
    /**
     * @brief Check the thread is idle, waiting for task
     * 
     * @return true 
     * @return false 
     */
    bool idle() const noexcept {
        return mIdle;
    }

    /**
     * @brief Current thread object
     * 
     * @return Thread* (nullptr on no-thread worker or in main thread)
     */
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