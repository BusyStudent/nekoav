#pragma once

#include "defs.hpp"

#include <condition_variable>
#include <functional>
#include <thread>
#include <string>
#include <queue>
#include <mutex>

// #ifdef _WIN32
//     #define NEKO_WIN_DISPATCHER
// #endif

NEKO_NS_BEGIN

/**
 * @brief ThreadPriority
 * 
 */
enum class ThreadPriority {
    Lowest,
    Low,
    Normal,
    High,
    Highest,
    RealTime
};

/**
 * @brief Thread with callback queue
 * 
 */
class NEKO_API Thread {
public:
    Thread();
    Thread(const Thread &) = delete;
    ~Thread();

    template <typename Callable, typename ...Args>
    Thread(Callable &&callable, Args &&...args) : Thread() {
        postTask(std::bind(callable, std::forward<Args>(args)...));
    }

    /**
     * @brief Set the Thread Name
     * 
     * @param name 
     */
    void setName(std::string_view name);
    /**
     * @brief Set the Priority of the thread
     * 
     * @param priority 
     */
    void setPriority(ThreadPriority priority);
    /**
     * @brief Poll task from the queue and execute it
     * 
     * @return Num of task processed
     * 
     */
    size_t dispatchTask();
    /**
     * @brief Wait task from the queue and execute it
     * 
     * @param timeout timeout in millseconds
     * @return Num of task processed
     * 
     */
    size_t waitTask(int timeout = -1);
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
     * @brief Get the name of this thread
     * 
     * @return std::string_view 
     */
    std::string_view name() const noexcept;
    
    void *operator new(size_t size) {
        return libc::malloc(size);
    }
    void operator delete(void *ptr) {
        return libc::free(ptr);
    }

    /**
     * @brief Current thread object
     * 
     * @return Thread* (nullptr on no-thread worker or in main thread)
     */
    static Thread *currentThread() noexcept;
    /**
     * @brief Sleep for millseconds but it will be interrupted if new task comming
     * 
     * @param milliseconds 
     * @return Ok by default, Interrupted on a new task that has arrived to the current thread
     */
    static Error msleep(int milliseconds) noexcept;
private:
    void _run(void *latch);
    void _dispatchWin32();

    Atomic<bool> mIdle {true};
    Atomic<bool> mRunning {true};

    std::queue<std::function<void()>> mQueue;
    std::mutex                        mMutex;
    std::string                       mName {"NekoWorkThread"};
    std::condition_variable           mCondition;
    std::thread                       mThread;

#ifdef NEKO_WIN_DISPATCHER
    uint32_t                          mThreadId = 0;
    uint32_t                          mWeakupMessage = 0;
#endif

};

NEKO_NS_END