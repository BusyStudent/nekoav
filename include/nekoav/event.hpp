#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/clock.hpp>
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
public:
    Caps caps;
};

class ErrorEvent {
public:
    std::error_code error;
    std::string     message;
};

/**
 * @brief The time of the clock has been updated
 * 
 */
class ClockUpdateEvent {
public:
    Clock::Ptr clock;
};

/**
 * @brief The Event class
 * 
 */
class Event final {
public:
    using Seek         = SeekEvent;
    using FlushBegin   = FlushBeginEvent;
    using FlushEnd     = FlushEndEvent;
    using EndOfStream  = EndOfStreamEvent;
    using Caps         = CapsEvent;
    using Error        = ErrorEvent;
    using ClockUpdate  = ClockUpdateEvent;
    using Storage      = std::variant<Seek, FlushBegin, FlushEnd, EndOfStream, Caps, Error, ClockUpdate>;
    using Ref          = Event &;

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
    auto isError() const noexcept { return std::holds_alternative<Error>(mStorage); }
    auto isClockUpdate() const noexcept { return std::holds_alternative<ClockUpdate>(mStorage); }

    auto toSeek() const noexcept { return std::get<Seek>(mStorage); }
    auto toFlushBegin() const noexcept { return std::get<FlushBegin>(mStorage); }
    auto toFlushEnd() const noexcept { return std::get<FlushEnd>(mStorage); }
    auto toCaps() const noexcept { return std::get<Caps>(mStorage); }
    auto toError() const noexcept { return std::get<Error>(mStorage); }
    auto toClockUpdate() const noexcept { return std::get<ClockUpdate>(mStorage); }

    // Visit
    template <typename Fn>
    auto visit(Fn &&fn) const -> decltype(auto) {
        return std::visit(std::forward<Fn>(fn), mStorage);
    }

    auto operator =(const Event &) -> Event & = default;
    auto operator =(Event &&) -> Event & = default;
private:
    Storage mStorage;
};

} // namespace nekoav

// Formatter
template <>
struct std::formatter<nekoav::Event> {
    constexpr auto parse(auto &ctxt) {
        return ctxt.begin();
    }

    auto format(const nekoav::Event &event, auto &ctxt) const {
        const auto visitor = nekoav::Overloads {
            [&](const auto &_) { return std::format_to(ctxt.out(), "Event(Unknown)"); },
            [&](const nekoav::Event::Seek &seek) { return std::format_to(ctxt.out(), "Event(Seek({}))", seek.timestamp); },
            [&](const nekoav::Event::FlushBegin &) { return std::format_to(ctxt.out(), "Event(FlushBegin)"); },
            [&](const nekoav::Event::FlushEnd &) { return std::format_to(ctxt.out(), "Event(FlushEnd)"); },
            [&](const nekoav::Event::EndOfStream &eos) { return std::format_to(ctxt.out(), "Event(EndOfStream({}))", eos.streamIndex); },
            [&](const nekoav::Event::Caps &caps) { return std::format_to(ctxt.out(), "Event(Caps({}))", caps.caps); },
            [&](const nekoav::Event::Error &error) { return std::format_to(ctxt.out(), "Event(Error({}))", error.message); },
            [&](const nekoav::Event::ClockUpdate &clock) { return std::format_to(ctxt.out(), "Event(ClockUpdate({}))", clock.clock->time()); },
        };
        return event.visit(visitor);
    }
};