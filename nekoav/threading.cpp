#define _NEKO_SOURCE
#include "threading.hpp"
#include "utils.hpp"
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
void Thread::setName(const char *name) {
    if (name == nullptr) {
        name = "NekoWorkThread";
    }
    auto handle = mThread.native_handle();

#if !defined(NEKO_MINGW)
    _Neko_SetThreadName(handle, name);
#endif

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
#endif

}
void Thread::postTask(std::function<void()> &&fn) {
    std::lock_guard lock(mMutex);
    mQueue.emplace(std::move(fn));
    mCondition.notify_all();
}
void Thread::sendTask(std::function<void()> &&fn) {
    std::latch latch {1};
    postTask([&]() {
        fn();
        latch.count_down();
    });
    latch.wait();
}
void Thread::dispatchTask() {
    std::unique_lock lock(mMutex);
    while (!mQueue.empty()) {
        auto fn = std::move(mQueue.front());
        mQueue.pop();
        lock.unlock();

        // Call task
        fn();

        lock.lock();
    }
}
void Thread::waitTask(int timeoutMS) {
    std::unique_lock lock(mMutex);
    while (mQueue.empty()) {
        if (timeoutMS != -1) {
            if (mCondition.wait_for(lock, std::chrono::milliseconds(timeoutMS)) == std::cv_status::timeout) {
                return;
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
        fn();

        lock.lock();
    }
}
Thread *Thread::currentThread() {
    return _currentThread;
}

NEKO_NS_END