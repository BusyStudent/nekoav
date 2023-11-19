#define _NEKO_SOURCE
#include "threading.hpp"
#include "utils.hpp"
#include "error.hpp"
#include "log.hpp"
#include <mutex>

NEKO_NS_BEGIN

static thread_local Thread *_currentThread = nullptr;

Thread::Thread() : mThread(&Thread::run, this) {

}
Thread::~Thread() {
    postTask([this]() {
        mRunning = false;
    });
    mThread.join();
}
void Thread::run() {
    setName("NekoWorkThread");
    _currentThread = this;
    while (true) {
        mIdle = false;
        dispatchTask();
        mIdle = true;
        if (!mRunning) {
            break;
        }
        std::unique_lock lock(mMutex);
        mCondition.wait(lock);
    }
}
void Thread::setName(std::string_view name) {
    if (name.empty()) {
        name = "NekoWorkThread";
    }
    auto handle = mThread.native_handle();

#if !defined(NEKO_MINGW)
    _Neko_SetThreadName(handle, name);
#endif
    mName = name;
}
void Thread::setPriority(ThreadPriority p) {
    auto handle = mThread.native_handle();

#if defined(_WIN32) && !defined(NEKO_MINGW)
    int priority = THREAD_PRIORITY_NORMAL;
    switch (p) {
        case ThreadPriority::Lowest: priority = THREAD_PRIORITY_LOWEST; break;
        case ThreadPriority::Low: priority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case ThreadPriority::Normal: priority = THREAD_PRIORITY_NORMAL; break;
        case ThreadPriority::High: priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        case ThreadPriority::Highest: priority = THREAD_PRIORITY_HIGHEST; break;
        case ThreadPriority::RealTime: priority = THREAD_PRIORITY_TIME_CRITICAL; break;
    }
    ::SetThreadPriority(handle, priority);
#elif __has_include(<sched.h>)
    int priority = 0;
    switch (p) {
        case ThreadPriority::Lowest: priority = -20; break;
        case ThreadPriority::Low: priority = -10; break;
        case ThreadPriority::Normal: priority = 0; break;
        case ThreadPriority::High: priority = 10; break;
        case ThreadPriority::Highest: priority = 20; break;
        case ThreadPriority::RealTime: priority = 1; break;
    }
    ::sched_param param;
    param.sched_priority = priority;
    ::pthread_setschedparam(handle, SCHED_OTHER, &param);
#endif

}
void Thread::postTask(std::function<void()> &&fn) {
    std::lock_guard lock(mMutex);
    mQueue.emplace(std::move(fn));
    mCondition.notify_one();
}
void Thread::sendTask(std::function<void()> &&fn) {
    std::latch latch {1};
    postTask([&]() {
        fn();
        latch.count_down();
    });
    latch.wait();
}
size_t Thread::dispatchTask() {
    size_t n = 0;
    std::unique_lock lock(mMutex);
    while (!mQueue.empty()) {
        auto fn = std::move(mQueue.front());
        mQueue.pop();
        lock.unlock();

        // Call task
        n += 1;
        fn();

        lock.lock();
    }
    return n;
}
size_t Thread::waitTask(int timeoutMS) {
    size_t n = 0;
    std::unique_lock lock(mMutex);
    while (mQueue.empty()) {
        if (timeoutMS != -1) {
            if (mCondition.wait_for(lock, std::chrono::milliseconds(timeoutMS)) == std::cv_status::timeout) {
                return n;
            }
        }
        else {
            mCondition.wait(lock);
        }
    }

    // Do dispatch
    while (!mQueue.empty()) {
        auto fn = std::move(mQueue.front());
        mQueue.pop();
        lock.unlock();

        // Call task
        n += 1;
        fn();

        lock.lock();
    }
    return n;
}
std::string_view Thread::name() const noexcept {
    return mName;
}

Thread *Thread::currentThread() noexcept {
    return _currentThread;
}
Error Thread::msleep(int ms) noexcept {
    if (ms <= 0) {
        return Error::Ok;
    }
    auto current = Thread::currentThread();
    if (!current) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Error::Ok;
    }
    std::unique_lock lock(current->mMutex);
    size_t numofTasks = current->mQueue.size();
    current->mCondition.wait_for(lock, std::chrono::milliseconds(ms));
    if (current->mQueue.size() == numofTasks) {
        return Error::Ok;
    }
    NEKO_LOG("Interrupted at {}", current->name());
    // New Task imcoming
    return Error::Interrupted;
}

NEKO_NS_END