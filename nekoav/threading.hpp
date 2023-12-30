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
    size_t waitTask(int64_t timeout = -1);
    /**
     * @brief Send a task into queue and wait for it finish
     * @note If you throw an exception at callback, The exception will be rethrow to the caller
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
     * @brief Wrap a callable and args and send it to the queue, wait for it done
     * @note If you throw an exception at callback, The exception will be rethrow to the caller
     * 
     * @tparam Callable 
     * @tparam Args 
     * @param callable 
     * @param args 
     * @return std::invoke_result_t<T, Args...> 
     */
    template <typename Callable, typename ...Args>
    auto invokeQueued(Callable &&callable, Args &&...args) -> std::invoke_result_t<Callable, Args...>;
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
     * @param milliseconds The time you want to sleep (<= 0 on no-op)
     * @return Ok by default, Interrupted on a new task that has arrived to the current thread
     */
    static Error msleep(int64_t milliseconds) noexcept;
    /**
     * @brief Sleep for microseconds but it will be interrupted if new task comming
     * 
     * @param microseconds The time you want to sleep (<= 0 on no-op)
     * @return Ok by default, Interrupted on a new task that has arrived to the current thread
     */
    static Error usleep(int64_t microseconds) noexcept;
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

#ifdef _WIN32
    uint32_t                          mThreadId = 0;
    uint32_t                          mWeakupMessage = 0;
#endif

};

// -- IMPL
template <typename Callable, typename ...Args>
auto Thread::invokeQueued(Callable &&callable, Args &&...args) -> std::invoke_result_t<Callable, Args...> {
    using RetT = std::invoke_result_t<Callable, Args...>;
    if constexpr (std::is_same_v<RetT, void>) {
        return sendTask(std::bind(std::forward<Callable>(callable), std::forward<Args>(args)...));
    }
    else {
        RetT ret {};
        sendTask([&]() { ret = std::invoke(std::forward<Callable>(callable), std::forward<Args>(args)...); });
        return ret;
    }
}

NEKO_NS_END