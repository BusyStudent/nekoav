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
 * @brief The element has received EOS
 * 
 */
class EosMessage {
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
    using MediaLoaded = MediaLoadedMessage;
    using SeekBegin   = SeekBeginMessage;
    using SeekEnd     = SeekEndMessage;
    using Eos         = EosMessage;
    using Storage = std::variant<Error, MediaLoaded, SeekBegin, SeekEnd, Eos>;

    Message(const Message &) = default;
    Message(Message &&) = default;
    Message() = delete; // Must specify message type

    // Direct construct inner
    template <typename T> requires (std::is_constructible_v<Storage, T>)
    Message(T &&message) : mStorage(std::forward<T>(message)), mTimestamp(std::chrono::steady_clock::now()) {}

    auto isError() const noexcept { return std::holds_alternative<Error>(mStorage); }
    auto isMediaLoaded() const noexcept { return std::holds_alternative<MediaLoaded>(mStorage); }
    auto isSeekBegin() const noexcept { return std::holds_alternative<SeekBegin>(mStorage); }
    auto isSeekEnd() const noexcept { return std::holds_alternative<SeekEnd>(mStorage); }
    auto isEos() const noexcept { return std::holds_alternative<Eos>(mStorage); }

    auto toError() const noexcept { return std::get<Error>(mStorage); }
    auto toMediaLoaded() const noexcept { return std::get<MediaLoaded>(mStorage); }
    auto toEos() const noexcept { return std::get<Eos>(mStorage); }

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

// template <>
// struct std::formatter<nekoav::Message> {
//     constexpr auto parse(std::format_parse_context &ctxt) -> decltype(ctxt.begin()) {
//         return ctxt.begin();
//     }

//     auto format(const nekoav::Message &msg, std::format_context &ctxt) -> decltype(ctxt.out()) {
//         return msg.visit([&ctxt](const auto &arg) {
//             return std::format_to(ctxt.out(), "{}", arg);
//         });
//     }
// };