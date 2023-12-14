#pragma once
#include <version>

#ifdef __cpp_lib_latch
    #include <latch>
#else
    #include <condition_variable>
    #include <atomic>
    #include <mutex>
#endif

#ifdef __cpp_lib_source_location
    #include <source_location>
#endif

namespace std {
inline namespace _neko_fallback {

#ifndef __cpp_lib_latch
class latch final {
public:
    explicit latch(ptrdiff_t count) noexcept : mCount(count) {

    }
    latch(const latch&) = delete;
    ~latch() = default;

    void arrive_and_wait(ptrdiff_t count = 1) noexcept {
        std::unique_lock lock(mMutex);
        auto current = mCount.fetch_sub(count) - count;
        if (current == 0) {
            mCondition.notify_all();
        }
        else {
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
        std::unique_lock lock(mMutex);
        while (mCount.load() != 0) {
            mCondition.wait(lock);
        }
    }
    bool try_wait() noexcept {
        return mCount.load() == 0;
    }
private:
    mutable condition_variable mCondition;
    mutable mutex mMutex;
    atomic<ptrdiff_t> mCount;
};
#endif

#ifndef __cpp_lib_source_location
struct source_location {
    static consteval source_location current() noexcept {
        return source_location();
    }

    constexpr source_location() noexcept = default;

    constexpr uint_least32_t line() const noexcept {
        return 0;
    }
    constexpr uint_least32_t column() const noexcept {
        return 0;
    }
    constexpr const char* file_name() const noexcept {
        return "";
    }
    constexpr const char* function_name() const noexcept {
        return "";
    }
};
#endif

}
}