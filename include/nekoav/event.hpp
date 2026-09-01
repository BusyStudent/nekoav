/**
 * @file event.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief The control event used to send between elements
 * @version 0.1
 * @date 2026-06-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/clock.hpp>
#include <nekoav/caps.hpp>
#include <variant>

namespace nekoav {

/**
 * @brief The flags of the events
 * 
 */
enum class EventFlags : uint8_t {
    None       = 0,
    Upstream   = 1 << 0,
    Downstream = 1 << 1,
    Serialized = 1 << 2, // The event is serialized with sample (in-bound)
    Sticky     = 1 << 3, // The event is sticky
};

// Operator for flags
constexpr auto operator|(EventFlags lhs, EventFlags rhs) -> EventFlags {
    return static_cast<EventFlags>(std::to_underlying(lhs) | std::to_underlying(rhs) );
}

constexpr auto hasFlag(EventFlags value, EventFlags flag) -> bool {
    return (std::to_underlying(value) & std::to_underlying(flag)) != 0;
}

/**
 * @brief Request to seek to a specific timestamp
 * 
 */
class SeekEvent {
public:
    static constexpr auto flags() { return EventFlags::None; }

    Timestamp timestamp;
};

class FlushBeginEvent {
public:
    static constexpr auto flags() { return EventFlags::Downstream; }
};

class FlushEndEvent {
public:
    static constexpr auto flags() { return EventFlags::Downstream | EventFlags::Serialized; }
};

/**
 * @brief The stream has been end
 * 
 */
class EosEvent {
public:
    static constexpr auto flags() { return EventFlags::Downstream | EventFlags::Serialized | EventFlags::Sticky;  }
};

/**
 * @brief The upstream element has decided to send new caps
 * 
 */
class CapsEvent {
public:
    static constexpr auto flags() { return EventFlags::Downstream | EventFlags::Serialized | EventFlags::Sticky; }

    Caps caps;
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
    using Eos          = EosEvent;
    using Caps         = CapsEvent;
    using Storage      = std::variant<Seek, FlushBegin, FlushEnd, Eos, Caps>;
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
    auto isEos() const noexcept { return std::holds_alternative<Eos>(mStorage); }

    auto toSeek() const noexcept { return std::get<Seek>(mStorage); }
    auto toFlushBegin() const noexcept { return std::get<FlushBegin>(mStorage); }
    auto toFlushEnd() const noexcept { return std::get<FlushEnd>(mStorage); }
    auto toCaps() const noexcept { return std::get<Caps>(mStorage); }

    // Flags
    auto flags() const noexcept -> EventFlags {
        auto fn = [](const auto &t) {
            return t.flags();
        };
        return std::visit(fn, mStorage);
    }
    auto isSerialzed() const noexcept { return hasFlag(flags(), EventFlags::Serialized); }
    auto isSticky() const noexcept { return hasFlag(flags(), EventFlags::Sticky); }

    // Index (used internally)
    auto index() const noexcept -> size_t {
        return mStorage.index();
    }

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
    constexpr auto parse(std::format_parse_context &ctxt) -> decltype(ctxt.begin()) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const nekoav::Event &event, FormatContext &ctxt) const {
        const auto visitor = nekoav::Overloads {
            [&](const auto &_) { return std::format_to(ctxt.out(), "Event(Unknown)"); },
            [&](const nekoav::Event::Seek &seek) { return std::format_to(ctxt.out(), "Event(Seek({}))", seek.timestamp); },
            [&](const nekoav::Event::FlushBegin &) { return std::format_to(ctxt.out(), "Event(FlushBegin)"); },
            [&](const nekoav::Event::FlushEnd &) { return std::format_to(ctxt.out(), "Event(FlushEnd)"); },
            [&](const nekoav::Event::Eos &eos) { return std::format_to(ctxt.out(), "Event(Eos)"); },
            [&](const nekoav::Event::Caps &caps) { return std::format_to(ctxt.out(), "Event(Caps({}))", caps.caps); },
            // [&](const nekoav::Event::Error &error) { return std::format_to(ctxt.out(), "Event(Error({}))", error.message); },
            // [&](const nekoav::Event::ClockUpdate &clock) { return std::format_to(ctxt.out(), "Event(ClockUpdate({}: {}))", clock.clock->category(), clock.time); },
            // [&](const nekoav::Event::MediaLoaded &loaded) { return std::format_to(ctxt.out(), "Event(MediaLoaded({}))", loaded.duration); },
        };
        return event.visit(visitor);
    }
};