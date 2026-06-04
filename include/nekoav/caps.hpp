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

// For ”“sv;
using std::literals::operator ""sv;

/**
 * @brief The value for multimedia
 * 
 */
class Value {
public:
    using List    = std::vector<Value>;
    using Map     = std::map<std::string, Value, std::less<> >;
    using Bytes   = std::vector<std::byte>;
    using Storage = std::variant<
        std::monostate,
        std::string,
        int64_t,
        double,
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

    Value(const Value &) = default;
    Value(Value &&) = default;
    Value() = default;

    // Direct construct inner
    template <typename T> requires(std::is_constructible_v<Storage, T>)
    Value(T &&value) : mStorage(std::forward<T>(value)) {}

    // Checked conversions
    auto isNull() const noexcept { return std::holds_alternative<std::monostate>(mStorage); }
    auto isString() const noexcept { return std::holds_alternative<std::string>(mStorage); }
    auto isInteger() const noexcept { return std::holds_alternative<int64_t>(mStorage); }
    auto isDouble() const noexcept { return std::holds_alternative<double>(mStorage); }
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
    auto toString() const -> const std::string & { return std::get<std::string>(mStorage); }
    auto toInteger() const -> int64_t { return std::get<int64_t>(mStorage); }
    auto toDouble() const -> double { return std::get<double>(mStorage); }
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
        static constinit Value null;
        auto map = std::get_if<Map>(&mStorage);
        if (!map) { return null;}
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

/**
 * @brief The single capability
 * 
 */
class Cap {
public:
    std::string key;
    Value       value;

    auto operator <=>(const Cap &other) const noexcept = default;
};

/**
 * @brief The capabilities of a media stream, used for negotiation between elements.
 * 
 */
class Caps {
public:
    // Some builtin types
    static constexpr auto VideoPacket = "video/packet"sv;
    static constexpr auto VideoRaw = "video/raw"sv;
    static constexpr auto AudioPacket = "audio/packet"sv;
    static constexpr auto AudioRaw = "audio/raw"sv;
    static constexpr auto SubtitlePacket = "subtitle/packet";
    static constexpr auto Any = "any"sv;

    // Somple builtin name
    // Video
    static constexpr auto Width = "width"sv;
    static constexpr auto Height = "height"sv;
    static constexpr auto PixelFormat = "pixelFormat"sv;
    static constexpr auto ColorRange = "colorRange"sv;
    static constexpr auto ColorPrimaries = "colorPrimaries"sv;
    static constexpr auto ColorTransfer = "colorTransfer"sv;
    static constexpr auto ColorSpace = "colorSpace"sv;

    // Audio
    static constexpr auto SampleFormat = "sampleFormat"sv;
    static constexpr auto Channels = "channels"sv;
    static constexpr auto ChannelMask = "channelMask"sv;
    static constexpr auto SampleRate = "sampleRate"sv;
    
    // Common
    static constexpr auto Duration = "duration"sv;
    static constexpr auto TimeBase = "timeBase"sv;
    static constexpr auto Codec = "codec"sv;
    static constexpr auto CodecTag = "codecTag"sv;
    static constexpr auto CodecExtraData = "codecExtraData"sv;
    static constexpr auto Bitrate = "bitrate"sv;
    static constexpr auto FrameRate = "frameRate"sv;

    Caps(const Caps &) = default;
    Caps(Caps &&) = default;
    Caps() = default;

    auto begin() const { return mCaps.begin(); }
    auto end() const { return mCaps.end(); }
    auto clear() { mCaps.clear(); }
    auto size() const { return mCaps.size(); }

    // Insert an item to the vaps
    auto insert(std::string_view type, Value value) -> void { mCaps.emplace_back(Cap { .key = std::string {type}, .value = std::move(value) }); }

    // Check
    auto empty() const noexcept { return mCaps.empty(); }
    auto isAny() const noexcept { return mCaps.size() == 1 && mCaps.front().key == Any; }

    // Find
    auto find(std::string_view type) const -> const Value & {
        static constinit Value null;
        auto it = std::find_if(mCaps.begin(), mCaps.end(), [&](auto &v) { return v.key == type; });
        if (it == mCaps.end()) {
            return null;
        }
        auto &[_, value] = *it;
        return value;
    }

    // Erase
    auto erase(std::string_view type) -> void {
        auto it = std::find_if(mCaps.begin(), mCaps.end(), [&](auto &v) { return v.key == type; });
        if (it != mCaps.end()) {
            mCaps.erase(it);
        }
    }

    auto operator [](std::string_view type) const -> const Value & { return find(type); }

    // Compare
    auto operator <=>(const Caps &other) const noexcept = default;
    auto operator =(const Caps &other) -> Caps & = default;
    auto operator =(Caps &&other) -> Caps & = default;
private:
    std::vector<Cap> mCaps;
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
struct std::formatter<nekoav::Cap> {
    constexpr auto parse(std::format_parse_context &ctxt) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const nekoav::Cap &cap, FormatContext &ctxt) const {
        return std::format_to(ctxt.out(), "\"{}\": {}", cap.key, cap.value);
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