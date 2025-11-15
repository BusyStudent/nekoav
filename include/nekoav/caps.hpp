#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/format.hpp>
#include <variant>
#include <string>
#include <chrono>
#include <vector>
#include <map>

namespace nekoav {

using namespace std::literals;

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
        SampleFormat,
        Rational,
        std::chrono::nanoseconds,
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
    auto isSampleFormat() const noexcept { return std::holds_alternative<SampleFormat>(mStorage); }
    auto isRational() const noexcept { return std::holds_alternative<Rational>(mStorage); }
    auto isDuration() const noexcept { return std::holds_alternative<std::chrono::nanoseconds>(mStorage); }
    auto isBytes() const noexcept { return std::holds_alternative<Bytes>(mStorage); }
    auto isList() const noexcept { return std::holds_alternative<List>(mStorage); }
    auto isMap() const noexcept { return std::holds_alternative<Map>(mStorage); }

    // Conversions
    auto toString() const -> const std::string & { return std::get<std::string>(mStorage); }
    auto toInteger() const -> int64_t { return std::get<int64_t>(mStorage); }
    auto toDouble() const -> double { return std::get<double>(mStorage); }
    auto toPixelFormat() const -> PixelFormat { return std::get<PixelFormat>(mStorage); }
    auto toSampleFormat() const -> SampleFormat { return std::get<SampleFormat>(mStorage); }
    auto toRational() const -> Rational { return std::get<Rational>(mStorage); }
    auto toDuration() const -> std::chrono::nanoseconds { return std::get<std::chrono::nanoseconds>(mStorage); }
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
    static constexpr auto Width = "width"sv;
    static constexpr auto Height = "height"sv;
    static constexpr auto PixelFormat = "pixelFormat"sv;
    static constexpr auto SampleFormat = "sampleFormat"sv;
    static constexpr auto Channels = "channels"sv;
    static constexpr auto SampleRate = "sampleRate"sv;
    static constexpr auto Duration = "duration"sv;
    static constexpr auto Codec = "codec"sv;
    static constexpr auto CodecExtraData = "codecExtraData"sv;
    static constexpr auto Bitrate = "bitrate"sv;
    static constexpr auto FrameRate = "frameRate"sv;

    // Internal name
    static constexpr auto CodecParams = "codecParams"sv;

    Caps(const Caps &) = default;
    Caps(Caps &&) = default;
    Caps() = default;

    auto begin() const { return mValues.begin(); }
    auto end() const { return mValues.end(); }
    auto clear() { mValues.clear(); }

    // Insert an item to the vaps
    auto insert(std::string_view type, Value value) -> void { mValues.emplace_back(type, std::move(value)); }

    // Check
    auto empty() const noexcept { return mValues.empty(); }
    auto isAny() const noexcept { return mValues.size() == 1 && mValues.front().first == Any; }

    // Find
    auto find(std::string_view type) const -> const Value & {
        static constinit Value null;
        auto it = std::find_if(mValues.begin(), mValues.end(), [&](auto &v) { return v.first == type; });
        if (it == mValues.end()) {
            return null;
        }
        auto &[_, value] = *it;
        return value;
    } 

    auto operator [](std::string_view type) const -> const Value & { return find(type); }

    // Compare
    auto operator <=>(const Caps &other) const noexcept = default;
    auto operator =(const Caps &other) -> Caps & = default;
    auto operator =(Caps &&other) -> Caps & = default;

    // Construct
    static auto makeAny() -> Caps {
        auto caps = Caps {};
        caps.insert(Any, {});
        return caps;
    }
private:
    std::vector<std::pair<std::string, Value> > mValues;
};

} // namespace nekoav