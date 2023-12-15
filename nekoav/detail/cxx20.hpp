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

    #define NEKO_SOURCE_LOCATION() \
        std::source_location::current()
#else
    #define NEKO_SOURCE_LOCATION() \
        std::source_location::current(__FILE__, __LINE__, __FUNCTION__)
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
    inline static source_location current(const char *file_name = "", const int line = 0, const char *function_name = "") noexcept {
        return source_location(file_name, line, function_name);
    }

    inline uint_least32_t line() const noexcept {
        return mLine;
    }
    inline uint_least32_t column() const noexcept {
        return 0;
    }
    inline const char* file_name() const noexcept {
        return mFileName;
    }
    inline const char* function_name() const noexcept {
        return mFunctionName;
    }

    const char * mFileName;
    int mLine = 0;
    const char * mFunctionName;
};
#endif
}
}