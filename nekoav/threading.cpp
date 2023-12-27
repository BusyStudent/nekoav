#define _NEKO_SOURCE
#include "threading.hpp"
#include "utils.hpp"
#include "error.hpp"
#include "log.hpp"
#include <mutex>

#ifdef _WIN32
    #include <Windows.h>
    #define NEKO_WIN_DISPATCHER
#endif

#ifdef _MSC_VER
    #pragma comment(lib, "user32.lib")
#endif


NEKO_NS_BEGIN

static thread_local Thread *_currentThread = nullptr;

Thread::Thread() {
    std::latch latch {1};
    mThread = std::thread(&Thread::_run, this, &latch);
    latch.wait();
}
Thread::~Thread() {
    postTask([this]() {
        mRunning = false;
    });
    mThread.join();
}
void Thread::_run(void *latch) {
    // Initalize Common parts
    NEKO_SetThreadName("NekoWorkThread");
    _currentThread = this;

#ifdef NEKO_WIN_DISPATCHER
    // Initialize Message Queue
    ::IsGUIThread(TRUE);
    mThreadId = ::GetCurrentThreadId();
    mWeakupMessage = ::RegisterWindowMessageW(L"NekoThreadWakeUp");
#endif

    // Initialize Ok
    static_cast<std::latch *>(latch)->count_down();

    while (true) {
        mIdle = false;
        dispatchTask();
        mIdle = true;
        if (!mRunning) {
            break;
        }

        std::unique_lock lock(mMutex);
#ifdef  NEKO_WIN_DISPATCHER
        if (!mQueue.empty()) {
            continue;
        }
        lock.unlock();
        ::WaitMessage();
#else
        mCondition.wait(lock, [this]() { return !mQueue.empty(); });
#endif
    }
}
void Thread::setName(std::string_view name) {
    if (Thread::currentThread() != this) {
        return sendTask(std::bind(&Thread::setName, this, name));
    }
    if (name.empty()) {
        name = "NekoWorkThread";
    }
    mName = name;
    NEKO_SetThreadName(mName);
}
void Thread::setPriority(ThreadPriority p) {
    if (Thread::currentThread() != this) {
        return sendTask(std::bind(&Thread::setPriority, this, p));
    }

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
    ::SetThreadPriority(::GetCurrentThread(), priority);
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
    ::pthread_setschedparam(::pthread_self(), SCHED_OTHER, &param);
#endif

}
void Thread::postTask(std::function<void()> &&fn) {
    std::lock_guard lock(mMutex);
    mQueue.emplace(std::move(fn));
    mCondition.notify_one();

#ifdef NEKO_WIN_DISPATCHER
    BOOL ret = ::PostThreadMessageW(mThreadId, mWeakupMessage, 0, 0);
    NEKO_ASSERT(ret);
#endif
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
    _dispatchWin32();

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
size_t Thread::waitTask(int64_t timeoutMS) {
    _dispatchWin32();

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
Error Thread::usleep(int64_t us) noexcept {
    if (us <= 0) {
        return Error::Ok;
    }
    auto current = Thread::currentThread();
    if (!current) {
        std::this_thread::sleep_for(std::chrono::microseconds(us));
        return Error::Ok;
    }
    std::unique_lock lock(current->mMutex);
    size_t numofTasks = current->mQueue.size();
    current->mCondition.wait_for(lock, std::chrono::microseconds(us));
    if (current->mQueue.size() == numofTasks) {
        return Error::Ok;
    }
    NEKO_LOG("sleep({}us) Interrupted at thread {}", us, current->name());
    // New Task imcoming
    return Error::Interrupted;
}
Error Thread::msleep(int64_t ms) noexcept {
    return Thread::usleep(ms * 1000);
}
#ifdef NEKO_WIN_DISPATCHER
void Thread::_dispatchWin32() {
    ::MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == mWeakupMessage) {
            continue;
        }
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}
#else
inline void Thread::_dispatchWin32() {}
#endif

NEKO_NS_END