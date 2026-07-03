/**
 * @file message.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief The message used to communicate from pipeline to application. used by bus
 * @version 0.1
 * @date 2026-06-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <nekoav/defines.hpp>
#include <variant>
#include <chrono>

namespace nekoav {

// Forward declaration
class Clock;
class Element;

class ErrorMessage {
public:
    std::error_code error;
    std::string     message;
};

/**
 * @brief The time of the clock has been updated
 * 
 */
class ClockUpdateMessage {
public:
    std::shared_ptr<const Clock> clock;
    Timestamp time;
};

/**
 * @brief The media has been loaded
 * 
 */
class MediaLoadedMessage {
public:
    // The media info
    Timestamp startTime;
    Duration  duration;
};

/**
 * @brief The 
 * 
 */
class EndOfStreamMessage {
public:
    std::shared_ptr<Element> element; // Which element has finished?
};

/**
 * @brief The pipeline begin to seek
 * 
 */
class SeekBeginMessage {};

/**
 * @brief The pipeline finished seeking
 * 
 */
class SeekEndMessage {};

/**
 * @brief The event class
 * 
 */
class Message final {
public:
    using Error       = ErrorMessage;
    using ClockUpdate = ClockUpdateMessage;
    using MediaLoaded = MediaLoadedMessage;
    using SeekBegin   = SeekBeginMessage;
    using SeekEnd     = SeekEndMessage;
    using EndOfStream = EndOfStreamMessage;
    using Storage = std::variant<Error, ClockUpdate, MediaLoaded, SeekBegin, SeekEnd, EndOfStream>;

    Message(const Message &) = default;
    Message(Message &&) = default;
    Message() = delete; // Must specify message type

    // Direct construct inner
    template <typename T> requires (std::is_constructible_v<Storage, T>)
    Message(T &&message) : mStorage(std::forward<T>(message)), mTimestamp(std::chrono::steady_clock::now()) {}

    auto isError() const noexcept { return std::holds_alternative<Error>(mStorage); }
    auto isClockUpdate() const noexcept { return std::holds_alternative<ClockUpdate>(mStorage); }
    auto isMediaLoaded() const noexcept { return std::holds_alternative<MediaLoaded>(mStorage); }
    auto isSeekBegin() const noexcept { return std::holds_alternative<SeekBegin>(mStorage); }
    auto isSeekEnd() const noexcept { return std::holds_alternative<SeekEnd>(mStorage); }
    auto isEndOfStream() const noexcept { return std::holds_alternative<EndOfStream>(mStorage); }

    auto toError() const noexcept { return std::get<Error>(mStorage); }
    auto toClockUpdate() const noexcept { return std::get<ClockUpdate>(mStorage); }
    auto toMediaLoaded() const noexcept { return std::get<MediaLoaded>(mStorage); }
    auto toEndOfStream() const noexcept { return std::get<EndOfStream>(mStorage); }

    // Get the timestamp when the message was created
    auto timestamp() const noexcept { return mTimestamp; }

    // Visit
    template <typename Fn>
    auto visit(Fn &&fn) const -> decltype(auto) {
        return std::visit(std::forward<Fn>(fn), mStorage);
    }

    auto operator =(const Message &) -> Message & = default;
    auto operator =(Message &&) -> Message & = default;
private:
    Storage mStorage;
    std::chrono::steady_clock::time_point mTimestamp; // When the message was created
};


} // namespace nekoav