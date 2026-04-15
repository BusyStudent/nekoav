#pragma once

// Async runtime
#include <ilias/io/error.hpp>
#include <ilias/result.hpp>
#include <ilias/task.hpp>

// Memory management
#include <memory>

// Formatting
#include <format>
#include <print>

// Time
#include <chrono>

// Assertion
#include <cassert>

#if defined(_MSC_VER)
    #define NEKOAV_EXPORT __declspec(dllexport)
    #define NEKOAV_IMPORT __declspec(dllimport)
#else
    #define NEKOAV_EXPORT __attribute__((visibility("default")))
    #define NEKOAV_IMPORT __attribute__((visibility("default")))
#endif // _MSC_VER

#if defined(_NEKOAV_SOURCE)
    #define NEKOAV_API NEKOAV_EXPORT
#else
    #define NEKOAV_API NEKOAV_IMPORT
#endif // _NEKOAV_SOURCE

#define NEKOAV_FORMATTER_4(type)                                          \
    template <>                                                           \
    struct std::formatter<type> {                                         \
        constexpr auto parse(auto &ctxt) { return ctxt.begin(); }         \
                                                                          \
        auto format(const type &t, auto &ctxt) const {                    \
            return std::format_to(ctxt.out(), "{}", nekoav::toString(t)); \
        }                                                                 \
    };

#define KEKOAV_THROW(exp) throw exp

namespace nekoav {

// Re-import ilias types
using ilias::IoResult;
using ilias::IoTask;
using ilias::Task;
using ilias::Err;

// Time
using Timestamp = std::chrono::nanoseconds;
using Duration = std::chrono::nanoseconds;

// Rational, taken from ffmpeg
struct Rational {
    int num; //< Numerator
    int den; //< Denominator

    constexpr auto operator <=>(const Rational &other) const noexcept { return num * other.den <=> den * other.num; }
    constexpr auto operator ==(const Rational &other) const noexcept -> bool = default;

    constexpr static auto nano() -> Rational { return {1, 1000000000}; }
    constexpr static auto micro() -> Rational { return {1, 1000000}; }
    constexpr static auto milli() -> Rational { return {1, 1000}; }
    constexpr static auto second() -> Rational { return {1, 1}; }
    constexpr static auto null() -> Rational { return {0, 1}; }
};

// Forward declarations
class Element;
class Value;
class Event;
class Query;
class Reply;
class Caps;
class Pad;

// Utils
template <typename ...Ts>
struct Overloads : Ts... { using Ts::operator()...; };

template <typename T>
inline constexpr auto toUnderlying(T value) noexcept{ return static_cast<std::underlying_type_t<T> >(value); }

} // namespace nekoav

// Formatter
template <>
struct std::formatter<nekoav::Rational> {
    constexpr auto parse(auto &ctxt) { return ctxt.begin(); }

    auto format(nekoav::Rational r, auto &ctxt) const {
        return std::format_to(ctxt.out(), "{} / {}", r.num, r.den);
    }
};