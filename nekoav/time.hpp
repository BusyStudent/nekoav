#pragma once

#include "defs.hpp"
#include <functional>
#include <cstdio>

#ifndef NDEBUG
    #define NEKO_TRACE_TIME(where) int64_t where; if (NEKO_NAMESPACE::TimeTracer t(where); true)
#else
    #define NEKO_TRACE_TIME(where) int64_t where = 0; if (true)
#endif

NEKO_NS_BEGIN

/**
 * @brief Get the Ticks, in milliseconds
 * 
 * @return int64_t 
 */
extern int64_t NEKO_API GetTicks() noexcept;
/**
 * @brief Sleep 
 * 
 * @param ms The duration to sleep in ms
 * @return int64_t The offset of target duration
 */
extern int64_t NEKO_API SleepFor(int64_t ms) noexcept;

/**
 * @brief Get the Time Cost For This expression
 * 
 * @tparam Callable 
 * @tparam Args 
 * @param cb 
 * @param args 
 * @return int64_t Time Cost
 */
template <typename Callable, typename ...Args>
inline int64_t GetTimeCostFor(Callable &&callable, Args &&...args) noexcept(std::is_nothrow_invocable_v<Callable, Args...>) {
    int64_t start = GetTicks();
    std::invoke(std::forward<Callable>(callable), std::forward<Args>(args)...);
    return GetTicks() - start;
}

class TimeTracer {
public:
    TimeTracer(int64_t &output) : mOutput(output), mStart(GetTicks()) {

    }
    ~TimeTracer() {
        mOutput = GetTicks() - mStart;
    }
private:
    int64_t             &mOutput;
    int64_t              mStart = 0;
};

NEKO_NS_END