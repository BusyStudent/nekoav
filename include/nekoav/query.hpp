#pragma once

#include <nekoav/defines.hpp>
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

class QueryCaps {
public:
    // Compare
    auto operator <=>(const QueryCaps &) const noexcept = default;
};

class Query {
public:
    using Duration = QueryDuration;
    using Caps     = QueryCaps;
    using Storage  = std::variant<Duration, Caps>;

    Query(const Query &) = default;
    Query(Query &&) = default;
    Query() = delete; // Must specify query type

    // Direct construct inner
    template <typename T> requires (std::is_constructible_v<Storage, T>)
    Query(T &&query) : mStorage(std::forward<T>(query)) {}

    // Cast
    auto isDuration() const noexcept { return std::holds_alternative<Duration>(mStorage); }
    auto isCaps() const noexcept { return std::holds_alternative<Caps>(mStorage); }

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

class ReplyCaps {
public:
    Caps caps;

    // Compare
    auto operator <=>(const ReplyCaps &) const noexcept = default;
};

/**
 * @brief This element can't give the reply, the framework should send the query to the downstream / upstream
 * 
 */
class ReplyUnavailable {
public:
    // Compare
    auto operator <=>(const ReplyUnavailable &) const noexcept = default;
};

/**
 * @brief The reply of the query
 * 
 */
class Reply {
public:
    using Duration = ReplyDuration;
    using Unavailable = ReplyUnavailable;
    using Caps    = ReplyCaps;
    using Storage = std::variant<Duration, Unavailable, Caps>;

    Reply(const Reply &) = default;
    Reply(Reply &&) = default;
    Reply() = delete; // Must specify reply type

    // Direct construct inner
    template <typename T> requires (std::is_constructible_v<Storage, T>)
    Reply(T &&reply) : mStorage(std::forward<T>(reply)) {}

    // Cast
    auto isDuration() const noexcept { return std::holds_alternative<Duration>(mStorage); }
    auto isUnavailable() const noexcept { return std::holds_alternative<Unavailable>(mStorage); }

    auto toDuration() const noexcept { return std::get<Duration>(mStorage); }
    auto toUnavailable() const noexcept { return std::get<Unavailable>(mStorage); }

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