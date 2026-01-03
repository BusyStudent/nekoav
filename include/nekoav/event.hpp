#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/caps.hpp>
#include <variant>

namespace nekoav {

/**
 * @brief Request to seek to a specific timestamp
 * 
 */
class SeekEvent {
public:
    Timestamp timestamp;
};

class FlushBeginEvent {

};

class FlushEndEvent {
    
};

class EndOfStreamEvent {
public:
    int streamIndex = 0;
};

/**
 * @brief The upstream element has decided to send new caps
 * 
 */
class CapsEvent {
    Caps caps;
};

/**
 * @brief The Event class
 * 
 */
class Event {
public:
    using Seek        = SeekEvent;
    using FlushBegin  = FlushBeginEvent;
    using FlushEnd    = FlushEndEvent;
    using EndOfStream = EndOfStreamEvent;
    using Caps        = CapsEvent;
    using Storage     = std::variant<Seek, FlushBegin, FlushEnd, EndOfStream, Caps>;
    using Ref         = Event &;

    Event(const Event &) = default;
    Event(Event &&) = default;
    Event() = delete; // Must specify event type

    // Direct construct inner
    template <typename T> requires (std::is_constructible_v<Storage, T>)
    Event(T &&event) : mStorage(std::forward<T>(event)) {}

    // Cast
    auto isSeek() const noexcept { return std::holds_alternative<Seek>(mStorage); }
    auto isFlushBegin() const noexcept { return std::holds_alternative<FlushBegin>(mStorage); }
    auto isFlushEnd() const noexcept { return std::holds_alternative<FlushEnd>(mStorage); }
    auto isCaps () const noexcept { return std::holds_alternative<Caps>(mStorage); }

    auto toSeek() const noexcept { return std::get<Seek>(mStorage); }
    auto toFlushBegin() const noexcept { return std::get<FlushBegin>(mStorage); }
    auto toFlushEnd() const noexcept { return std::get<FlushEnd>(mStorage); }
    auto toCaps() const noexcept { return std::get<Caps>(mStorage); }

    // Visit
    template <typename Fn>
    auto visit(Fn &&fn) const -> decltype(auto) {
        return std::visit(std::forward<Fn>(fn), mStorage);
    }

    // Consumed
    auto consumed() const noexcept { return mConsumed; }
    auto setConsumed(bool consumed = true) noexcept { mConsumed = consumed; }
private:
    Storage mStorage;
    bool    mConsumed = false;
};

} // namespace nekoav