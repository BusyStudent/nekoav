#pragma once

#pragma once

#include "defs.hpp"

#if NEKO_CXX20
    #include <barrier>
    #include <latch>
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
using Latch = std::latch;
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
private:
    mutable std::condition_variable mCondition;
    mutable std::mutex mMutex;
    Atomic<ptrdiff_t> mCount;
};

#endif

NEKO_NS_END