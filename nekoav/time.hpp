#pragma once

#include "defs.hpp"

#define NEKO_TRACE_TIME_COST(tracer) NEKO_NAMESPACE::TimeTracer tracer = [&]()

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
extern int64_t NEKO_API Sleep(int64_t ms) noexcept;

template <typename T>
class TimeTracer {
public:
    TimeTracer(T &&cb) {
        auto cur = GetTicks();
        cb();
        mDuration = GetTicks() - cur;
    }

    auto duration() const noexcept {
        return mDuration;
    }
private:
    int64_t mDuration = 0;
};

NEKO_NS_END