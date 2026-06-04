#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/clock.hpp>
#include <nekoav/caps.hpp>
#include <variant>

namespace nekoav {

/**
 * @brief Query the duration of the stream
 * 
 */
class QueryDuration { 
public:
    // Compare
    auto operator <=>(const QueryDuration &) const noexcept = default;
};

class QueryPosition {
public:
    // Compare
    auto operator <=>(const QueryPosition &) const noexcept = default;
};

class QueryCaps {
public:
    // Compare
    auto operator <=>(const QueryCaps &) const noexcept = default;
};

class QueryClockSource {
public:
    // Compare
    auto operator <=>(const QueryClockSource &) const noexcept = default;
};

class Query final {
public:
    using Duration    = QueryDuration;
    using Position    = QueryPosition;
    using Caps        = QueryCaps;
    using ClockSource = QueryClockSource;
    using Storage     = std::variant<Duration, Position, Caps, ClockSource>;

    Query(const Query &) = default;
    Query(Query &&) = default;
    Query() = delete; // Must specify query type

    // Direct construct inner
    template <typename T> requires (std::is_constructible_v<Storage, T>)
    Query(T &&query) : mStorage(std::forward<T>(query)) {}

    // Cast
    auto isDuration() const noexcept { return std::holds_alternative<Duration>(mStorage); }
    auto isPosition() const noexcept { return std::holds_alternative<Position>(mStorage); }
    auto isCaps() const noexcept { return std::holds_alternative<Caps>(mStorage); }
    auto isClockSource() const noexcept { return std::holds_alternative<ClockSource>(mStorage); }

    // Visit
    template <typename Fn>
    auto visit(Fn &&fn) const -> decltype(auto) {
        return std::visit(std::forward<Fn>(fn), mStorage);
    }

    // Compare
    auto operator <=>(const Query &) const noexcept = default;
private:
    Storage mStorage;
};

// Reply...
class ReplyDuration {
public:
    Duration duration;

    // Compare
    auto operator <=>(const ReplyDuration &) const noexcept = default;
};

class ReplyPosition {
public:
    Timestamp position;

    // Compare
    auto operator <=>(const ReplyPosition &) const noexcept = default;
};

class ReplyCaps {
public:
    Caps caps;

    // Compare
    auto operator <=>(const ReplyCaps &) const noexcept = default;
};

class ReplyClockSource {
public:
    Clock::Ptr clock;

    // Compare
    auto operator <=>(const ReplyClockSource &) const noexcept = default;
};

/**
 * @brief The reply of the query
 * 
 */
class Reply final {
public:
    using Duration    = ReplyDuration;
    using Position    = ReplyPosition;
    using Caps        = ReplyCaps;
    using ClockSource = ReplyClockSource;
    using Storage     = std::variant<Duration, Position, Caps, ClockSource>;

    Reply(const Reply &) = default;
    Reply(Reply &&) = default;
    Reply() = delete; // Must specify reply type

    // Direct construct inner
    template <typename T> requires (std::is_constructible_v<Storage, T>)
    Reply(T &&reply) : mStorage(std::forward<T>(reply)) {}

    // Cast
    auto isDuration() const noexcept { return std::holds_alternative<Duration>(mStorage); }
    auto isPosition() const noexcept { return std::holds_alternative<Position>(mStorage); }
    auto isCaps()    const noexcept { return std::holds_alternative<Caps>(mStorage); }
    auto isClockSource() const noexcept { return std::holds_alternative<ClockSource>(mStorage); }

    auto toDuration() const noexcept { return std::get<Duration>(mStorage); }
    auto toPosition() const noexcept { return std::get<Position>(mStorage); }
    auto toCaps() const noexcept { return std::get<Caps>(mStorage); }
    auto toClockSource() const noexcept { return std::get<ClockSource>(mStorage); }

    // Visit
    template <typename Fn>
    auto visit(Fn &&fn) const -> decltype(auto) {
        return std::visit(std::forward<Fn>(fn), mStorage);
    }

    // Compare
    auto operator <=>(const Reply &) const noexcept = default;
private:
    Storage mStorage;
};

} // namespace nekoav

// Formatter
template <>
struct std::formatter<nekoav::Query> {
    constexpr auto parse(std::format_parse_context &ctxt) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const nekoav::Query &query, FormatContext &ctxt) const {
        const auto visitor = nekoav::Overloads {
            [&](const auto _) { return std::format_to(ctxt.out(), "Query(Unknown)"); },
            [&](const nekoav::QueryDuration &duration) { return std::format_to(ctxt.out(), "Query(Duration)"); },
            [&](const nekoav::QueryPosition &position) { return std::format_to(ctxt.out(), "Query(Position)"); },
            [&](const nekoav::QueryCaps &caps) { return std::format_to(ctxt.out(), "Query(Caps)"); },
            [&](const nekoav::QueryClockSource &clock) { return std::format_to(ctxt.out(), "Query(ClockSource)"); },
        };
        return query.visit(visitor);
    }
};

template <>
struct std::formatter<nekoav::Reply> {
    constexpr auto parse(std::format_parse_context &ctxt) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const nekoav::Reply &reply, FormatContext &ctxt) const {
        const auto visitor = nekoav::Overloads {
            [&](const auto _) { return std::format_to(ctxt.out(), "Reply(Unknown)"); },
            [&](const nekoav::ReplyDuration &duration) { return std::format_to(ctxt.out(), "Reply(Duration({}))", duration.duration); },
            [&](const nekoav::ReplyPosition &position) { return std::format_to(ctxt.out(), "Reply(Position({}))", position.position); },
            [&](const nekoav::ReplyCaps &caps) { return std::format_to(ctxt.out(), "Reply(Caps({}))", caps.caps); },
            [&](const nekoav::ReplyClockSource &clock) { return std::format_to(ctxt.out(), "Reply(ClockSource({}))", static_cast<const void *>(clock.clock.get())); },
        };
        return reply.visit(visitor);
    }
};