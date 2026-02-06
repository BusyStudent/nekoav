#pragma once

#include <nekoav/defines.hpp>
#include <chrono>
#include <memory>

namespace nekoav {

/**
 * @brief The category of the clock (lower category has higher priority)
 * 
 */
enum class ClockCategory {
    Audio  = 0, // Audio clock, perfer to use this as the master clock
    System = 1, // System clock, used for without audio
    Video  = 2,
};

/**
 * @brief Generic clock interface
 * 
 */
class Clock {
public:
    /**
     * @brief Pointer to observe it
     * 
     */
    using Ptr = std::shared_ptr<const Clock>;

    /**
     * @brief Get the current time of the clock (the epoch is 0)
     * 
     * @return Timestamp 
     */
    virtual auto time() const -> Timestamp = 0;

    /**
     * @brief Get the clock category
     * 
     * @return ClockCategory 
     */
    virtual auto category() const -> ClockCategory = 0;
protected:
    Clock() = default;
    ~Clock() = default;
};


inline auto toString(ClockCategory cat) -> std::string_view {
    switch (cat) {
        case ClockCategory::Audio:  return "Audio";
        case ClockCategory::System: return "System";
        case ClockCategory::Video:  return "Video";
        default: return "Unknown";
    }
}

} // namespace nekoav