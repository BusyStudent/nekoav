#pragma once

#pragma once

#include "defs.hpp"

#if NEKO_CXX20
    #include <semaphore>
    #include <barrier>
    #include <latch>
#elif defined(_WIN32)
    extern "C" {
        int __stdcall WaitOnAddress(
            volatile void* Address,
            void *CompareAddress,
            size_t AddressSize,
            unsigned long dwMilliseconds
        );
        void __stdcall WakeByAddressAll(
            void *Address
        );
    }

    #ifdef _MSC_VER
        #pragma comment(lib, "synchronization.lib")
    #endif
#else
    #include <condition_variable>
    #include <mutex>
#endif

#include <functional>
#include <thread>

NEKO_NS_BEGIN

struct _NoCompeleteFunction {
    void operator()() noexcept {}
};

#if NEKO_CXX20

template <typename T = _NoCompeleteFunction>
using Barrier = std::barrier<T>;
class Latch final : public std::latch {
public:
    using std::latch::latch;
};

#elif defined(_WIN32)

class Latch final {
public:
    explicit Latch(ptrdiff_t count) noexcept : mCount(count) {
    
    }
    Latch(const Latch&) = delete;
    ~Latch() = default;

    void arrive_and_wait(ptrdiff_t count = 1) noexcept {
        count_down(count);
        wait();
    }
    void count_down(ptrdiff_t count = 1) noexcept {
        mCount -= count;
        ::WakeByAddressAll((void*) &mCount);
    }
    void wait() const noexcept {
        while (mCount > 0) {
            ptrdiff_t v = mCount;
            ::WaitOnAddress(&mCount, &v, sizeof(mCount), -1);
        }
    }
private:
    mutable volatile ptrdiff_t mCount;
};

#else

class Latch final {
public:
    explicit Latch(ptrdiff_t count) noexcept : mCount(count) {
        assert(count >= 0);
    }
    Latch(const Latch&) = delete;
    ~Latch() = default;

    void arrive_and_wait(ptrdiff_t count = 1) noexcept {
        auto current = mCount.fetch_sub(count) - count;
        if (current == 0) {
            mCondition.notify_all();
        }
        else {
            std::unique_lock lock(mMutex);
            mCondition.wait(lock);
        }
    }
    void count_down(ptrdiff_t count = 1) noexcept {
        auto current = mCount.fetch_sub(count) - count;
        if (current == 0) {
            mCondition.notify_all();
        }
    }
    void wait() const noexcept {
        while (mCount.load() != 0) {
            std::unique_lock lock(mMutex);
            mCondition.wait(lock);
        }
    }
    bool try_wait() noexcept {
        return mCount.load() == 0;
    }
    bool tryWait() noexcept {
        return try_wait();
    }
private:
    mutable std::condition_variable mCondition;
    mutable std::mutex mMutex;
    Atomic<ptrdiff_t> mCount;
};

#endif

NEKO_NS_END