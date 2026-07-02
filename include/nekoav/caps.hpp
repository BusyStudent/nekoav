/**
 * @file caps.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief The properties system for multimedia
 * @version 0.1
 * @date 2026-06-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/format.hpp>
#include <variant>
#include <string>
#include <chrono>
#include <vector>
#include <ranges>
#include <map>

namespace nekoav {

/**
 * @brief The value for multimedia
 * 
 */
class Value {
public:
    using Null    = std::monostate;
    using String  = std::string;
    using Integer = int64_t;
    using Double  = double;
    using List    = std::vector<Value>;
    using Map     = std::map<std::string, Value, std::less<> >;
    using Bytes   = std::vector<std::byte>;
    using Storage = std::variant<
        Null,
        String,
        Integer,
        Double,
        PixelFormat,
        ColorRange,
        ColorPrimaries,
        ColorTransfer,
        ColorSpace,
        SampleFormat,
        Rational,
        Duration,
        Bytes,
        List,
        Map
    >;

    // Empty tags
    NEKOAV_API
    static const Value null;

    Value(const Value &) = default;
    Value(Value &&) = default;
    Value() = default;

    // Direct construct inner
    template <typename T> requires(std::is_constructible_v<Storage, T>)
    Value(T &&value) : mStorage(std::forward<T>(value)) {}

    // Checked conversions
    auto isNull() const noexcept { return std::holds_alternative<Null>(mStorage); }
    auto isString() const noexcept { return std::holds_alternative<String>(mStorage); }
    auto isInteger() const noexcept { return std::holds_alternative<Integer>(mStorage); }
    auto isDouble() const noexcept { return std::holds_alternative<Double>(mStorage); }
    auto isPixelFormat() const noexcept { return std::holds_alternative<PixelFormat>(mStorage); }
    auto isColorRange() const noexcept { return std::holds_alternative<ColorRange>(mStorage); }
    auto isColorPrimaries() const noexcept { return std::holds_alternative<ColorPrimaries>(mStorage); }
    auto isColorTransfer() const noexcept { return std::holds_alternative<ColorTransfer>(mStorage); }
    auto isColorSpace() const noexcept { return std::holds_alternative<ColorSpace>(mStorage); }
    auto isSampleFormat() const noexcept { return std::holds_alternative<SampleFormat>(mStorage); }
    auto isRational() const noexcept { return std::holds_alternative<Rational>(mStorage); }
    auto isDuration() const noexcept { return std::holds_alternative<Duration>(mStorage); }
    auto isBytes() const noexcept { return std::holds_alternative<Bytes>(mStorage); }
    auto isList() const noexcept { return std::holds_alternative<List>(mStorage); }
    auto isMap() const noexcept { return std::holds_alternative<Map>(mStorage); }

    // Conversions
    auto toString() const -> const std::string & { return std::get<String>(mStorage); }
    auto toInteger() const -> int64_t { return std::get<Integer>(mStorage); }
    auto toDouble() const -> double { return std::get<Double>(mStorage); }
    auto toPixelFormat() const -> PixelFormat { return std::get<PixelFormat>(mStorage); }
    auto toColorRange() const -> ColorRange { return std::get<ColorRange>(mStorage); }
    auto toColorPrimaries() const -> ColorPrimaries { return std::get<ColorPrimaries>(mStorage); }
    auto toColorTransfer() const -> ColorTransfer { return std::get<ColorTransfer>(mStorage); }
    auto toColorSpace() const -> ColorSpace { return std::get<ColorSpace>(mStorage); }
    auto toSampleFormat() const -> SampleFormat { return std::get<SampleFormat>(mStorage); }
    auto toRational() const -> Rational { return std::get<Rational>(mStorage); }
    auto toDuration() const -> Duration { return std::get<Duration>(mStorage); }
    auto toBytes() const -> const Bytes & { return std::get<Bytes>(mStorage); }
    auto toList() const -> const List & { return std::get<List>(mStorage); }
    auto toMap() const -> const Map & { return std::get<Map>(mStorage); }

    // Visit
    template <typename Fn>
    auto visit(Fn &&fn) const -> decltype(auto) {
        return std::visit(std::forward<Fn>(fn), mStorage);
    }

    // Compare
    auto operator <=>(const Value &other) const noexcept = default;
    auto operator =(const Value &other) -> Value & = default;
    auto operator =(Value &&other) -> Value & = default;

    // Access
    template <char = 0>
    auto operator [](std::string_view name) const -> const Value & {
        auto map = std::get_if<Map>(&mStorage);
        if (!map) return null;
        if (auto it = map->find(name); it != map->end()) {
            auto &[_, value] = *it;
            return value;
        }
        return null;
    }

    // Constructhelper
    static auto fromString(std::string_view s) -> Value { return Value(std::string(s)); }
    static auto fromInteger(int64_t i) -> Value { return Value(i); }
    static auto fromDouble(double d) -> Value { return Value(d); }
    static auto fromList(List list) -> Value { return Value(std::move(list)); }
    static auto fromMap(Map m) -> Value { return Value(std::move(m)); }
private:
    Storage mStorage;
};

// Caps: [
//  { Caps::Any, Null },
//  { Caps::VideoPacket, Maps {} },
//  { Caps::VideoRaw, Maps {} },
//  { Caps::AudioPacket, Maps {} },
//  { Caps::AudioRaw, Maps {} },
//  { Caps::SubtitlePacket, Maps {} },
// ]
//
/**
 * @brief The capabilities of a media stream, used for negotiation between elements.
 * 
 */
class Caps {
public:
    // Some builtin types
    static constexpr std::string_view VideoPacket = "video/packet";
    static constexpr std::string_view VideoRaw = "video/raw";
    static constexpr std::string_view AudioPacket = "audio/packet";
    static constexpr std::string_view AudioRaw = "audio/raw";
    static constexpr std::string_view SubtitlePacket = "subtitle/packet";
    static constexpr std::string_view Any = "any";

    // Somple builtin name
    // Video
    static constexpr std::string_view Width = "width";
    static constexpr std::string_view Height = "height";
    static constexpr std::string_view PixelFormat = "pixelFormat";
    static constexpr std::string_view ColorRange = "colorRange";
    static constexpr std::string_view ColorPrimaries = "colorPrimaries";
    static constexpr std::string_view ColorTransfer = "colorTransfer";
    static constexpr std::string_view ColorSpace = "colorSpace";

    // Audio
    static constexpr std::string_view SampleFormat = "sampleFormat";
    static constexpr std::string_view Channels = "channels";
    static constexpr std::string_view ChannelMask = "channelMask";
    static constexpr std::string_view SampleRate = "sampleRate";
    
    // Common
    static constexpr std::string_view Duration = "duration";
    static constexpr std::string_view TimeBase = "timeBase";
    static constexpr std::string_view Codec = "codec";
    static constexpr std::string_view CodecTag = "codecTag";
    static constexpr std::string_view CodecExtraData = "codecExtraData";
    static constexpr std::string_view Bitrate = "bitrate";
    static constexpr std::string_view FrameRate = "frameRate";

    Caps(const Caps &) = default;
    Caps(Caps &&) = default;
    Caps() = default;

    auto begin() const { return mCaps.begin(); }
    auto end() const { return mCaps.end(); }
    auto clear() -> void { mCaps.clear(); }
    auto size() const -> size_t { return mCaps.size(); }

    /**
     * @brief Insert an item into the Caps, it will overwrite the old item if it exists
     * 
     * @param type The type of the item
     * @param value The value of the item
     */
    auto insertOrAssign(std::string_view type, Value value) -> void {
        mCaps.insert_or_assign(std::string{type}, std::move(value));
    }

    /**
     * @brief Find the item in the Caps
     * 
     * @param type 
     * @return const Value &, If not found, return a null value
     */
    auto find(std::string_view type) const -> const Value & {
        auto it = mCaps.find(type);
        if (it == mCaps.end()) return Value::null;
        return it->second;
    }

    /**
     * @brief Check the Caps is empty ?
     * 
     * @return true 
     * @return false 
     */
    auto empty() const noexcept -> bool { return mCaps.empty(); }

    /**
     * @brief Check if the Caps contains the type
     * 
     * @param type 
     * @return true 
     * @return false 
     */
    auto contains(std::string_view type) const noexcept -> bool { return mCaps.contains(type); }

    /**
     * @brief Check if the Caps contains any type
     * 
     * @return true 
     * @return false 
     */
    auto containsAny() const noexcept -> bool { return mCaps.contains(Caps::Any); }

    // Operators
    auto operator <=>(const Caps &other) const noexcept = default;
    auto operator =(const Caps &other) -> Caps & = default;
    auto operator =(Caps &&other) -> Caps & = default;
private:
    std::map<std::string, Value, std::less<> > mCaps;
};

} // namespace nekoav

// MARK: Formatter
template <>
struct std::formatter<nekoav::Value> {
    constexpr auto parse(std::format_parse_context &ctxt){
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const nekoav::Value &value, FormatContext &ctxt) const {
        const auto visitor = nekoav::Overloads {
            [&](std::monostate) {
                return std::format_to(ctxt.out(), "null");
            },
            [&](const nekoav::Value::List &l) {
                auto out = std::format_to(ctxt.out(), "[");
                bool first = true;
                for (const auto& item : l) {
                    if (!first) out = std::format_to(out, ", ");
                    out = std::format_to(out, "{}", item);
                    first = false;
                }
                return std::format_to(out, "]");
            },
            [&](const nekoav::Value::Map &m) {
                auto out = std::format_to(ctxt.out(), "{{");
                bool first = true;
                for (const auto& [key, item] : m) {
                    if (!first) out = std::format_to(out, ", ");
                    out = std::format_to(out, "\"{}\": {}", key, item);
                    first = false;
                }
                return std::format_to(out, "}}"); 
            },
            [&](const nekoav::Value::Bytes &b) {
                return std::format_to(ctxt.out(), "bytes[{}]", b.size());
            },
            [&](const auto &other) {
                return std::format_to(ctxt.out(), "{}", other);
            }
        };
        return value.visit(visitor);
    }
};

template <>
struct std::formatter<nekoav::Caps> {
    constexpr auto parse(std::format_parse_context &ctxt) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const nekoav::Caps &caps, FormatContext &ctxt) const {
        return std::format_to(ctxt.out(), "{}", std::ranges::subrange(caps.begin(), caps.end()));
    }
};