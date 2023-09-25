#define _NEKO_SOURCE
#include "threading.hpp"
#include "utils.hpp"
#include "latch.hpp"
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
    NEKO_SetThreadName("NekoWorkThread");
    _currentThread = this;
    while (true) {
        mIdle = false;
        dispatchTask();
        mIdle = true;
        if (!mRunning) {
            break;
        }
        std::unique_lock lock(mConditionMutex);
        mCondition.wait(lock);
    }
}
void Thread::postTask(std::function<void()> &&fn) {
    std::lock_guard lock(mQueueMutex);
    mQueue.emplace(std::move(fn));
    mCondition.notify_one();
}
void Thread::sendTask(std::function<void()> &&fn) {
    Latch latch {1};
    postTask([&]() {
        fn();
        latch.count_down();
    });
    latch.wait();
}
void Thread::dispatchTask() {
    std::unique_lock lock(mQueueMutex);
    while (!mQueue.empty()) {
        auto fn = std::move(mQueue.front());
        mQueue.pop();
        lock.unlock();

        // Call task
        fn();

        lock.lock();
    }
}

NEKO_NS_END