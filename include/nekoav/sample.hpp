#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/format.hpp>
#include <optional>
#include <variant>
#include <chrono>
#include <memory>
#include <span>

// Forward declare FFmpeg structs
extern "C" {
    struct AVFrame;
    struct AVPacket;
}

namespace nekoav {

/**
 * @brief A Frame of raw data, usually uncompressed. wrapping AVFrame.
 * 
 */
class NEKOAV_API Frame final {
public:
    Frame() = default;
    Frame(Frame &&) = default;
    ~Frame() = default;

    // Setter
    auto setPts(std::optional<Timestamp> pts) -> void;
    auto setDts(std::optional<Timestamp> dts) -> void;

    // Getters for AVFrame fields
    auto pts() const -> std::optional<Timestamp>;
    auto dts() const -> std::optional<Timestamp>;

    auto data(int plane) -> void *;
    auto linesize(int plane) -> int;

    // Video specific
    auto pixelFormat() const -> PixelFormat;
    auto height() const -> int;
    auto width() const -> int;

    // Audio specific
    auto sampleFormat() const -> SampleFormat;
    auto sampleRate() const -> int;
    auto channels() const -> int;
    auto samples() const -> int;

    // Get the presentation timeBase
    auto timeBase() const -> Rational { return mTimeBase; }

    // Get the AVFrame
    auto get() const -> AVFrame * { return mFrame.get(); }

    /**
     * @brief Make the frame data writable, doing COW
     * 
     * @return IoResult<void> Err if failed
     */
    auto makeWritable() -> IoResult<void>;

    /**
     * @brief Check the frame data is writeable, can use the Setter
     * 
     * @return true 
     * @return false 
     */
    auto isWritable() const -> bool;

    /**
     * @brief Clone the frame, it will create a new frame and ref the data (COW)
     * 
     * @return Frame 
     */
    auto clone() const -> Frame;

    // Operators
    auto operator =(Frame &&) -> Frame & = default;
    auto operator <=>(const Frame &rhs) const noexcept = default;

    /**
     * @brief Create an frame from an exisited avfeame, it will take the ownship of it
     * 
     * @param avframe The avframe, can't be nullptr
     * @param timeBase The time base of the frame
     * @return Ptr 
     */
    static auto from(AVFrame *avframe, Rational timeBase) -> Frame;
private:
    static auto free(AVFrame *ptr) -> void;

    struct Deleter {
        auto operator()(AVFrame *ptr) { free(ptr); }
    };

    std::unique_ptr<AVFrame, Deleter> mFrame; // Placeholder for AVFrame*
    Rational                          mTimeBase = {0, 1};
};

// class AudioFrame : Frame;
// class VideoFrame : Frame;

/**
 * @brief A Packet of encoded data, usually compressed. wrapping AVPacket.
 * 
 */
class NEKOAV_API Packet final {
public:
    Packet() = default;
    Packet(Packet &&) = default;
    ~Packet() = default;

    // Setter
    auto setPts(std::optional<Timestamp> pts) -> void;
    auto setDts(std::optional<Timestamp> dts) -> void;

    // Getters for AVFrame fields
    auto pts() const -> std::optional<Timestamp>;
    auto dts() const -> std::optional<Timestamp>;
    auto data() const -> std::span<std::byte>;
    auto isKeyFrame() const -> bool;

    // Get the presentation timeBase
    auto timeBase() const -> Rational { return mTimeBase; }

    // Get the AVPacket
    auto get() const -> AVPacket * { return mPacket.get(); }

    /**
     * @brief Clone the packet, it will create a new packet and ref the data (COW)
     * 
     * @return Packet 
     */
    auto clone() const -> Packet;

    // Operators
    auto operator =(Packet &&) -> Packet & = default;
    auto operator <=>(const Packet &rhs) const noexcept = default;

    /**
     * @brief Create an Packet from an exisited avpacket, it will take the ownership of it
     * 
     * @param avpacket The avpacket, can't be nullptr
     * @param timeBase The time base of the packet
     * @return Ptr 
     */
    static auto from(AVPacket *avpacket, Rational timeBase) -> Packet;
private:
    static auto free(AVPacket *ptr) -> void;

    struct Deleter {
        auto operator()(AVPacket *ptr) { free(ptr); }
    };

    std::unique_ptr<AVPacket, Deleter> mPacket; // Placeholder for AVPacket*
    Rational                           mTimeBase = {0, 1};
};

/**
 * @brief The data passed between elements.
 * 
 */
class NEKOAV_API Sample final {
public:
    using Storage = std::variant<std::monostate, Frame, Packet>;

    Sample() = default;
    Sample(std::nullptr_t) noexcept : mStorage(std::monostate()) {}
    Sample(const Sample &) noexcept = delete;
    Sample(Sample &&) noexcept = default;
    ~Sample() = default;

    // Construct inner
    template <typename T> requires(std::is_constructible_v<Storage, T>)
    Sample(T &&t) noexcept : mStorage(std::forward<T>(t)) {}

    // Get the presentation timestamp (based on timeBase)
    auto pts() const -> std::optional<Timestamp>;

    // Get the decoding timestamp (based on timeBase)
    auto dts() const -> std::optional<Timestamp>;

    // Set the presentation timestamp (based on timeBase)
    auto setPts(std::optional<Timestamp> pts) -> void;

    // Set the decoding timestamp (based on timeBase)
    auto setDts(std::optional<Timestamp> dts) -> void;

    // Clone
    auto clone() const -> Sample;

    // Cast
    auto isFrame() const -> bool { return std::holds_alternative<Frame>(mStorage); }
    auto isPacket() const -> bool { return std::holds_alternative<Packet>(mStorage); }
    auto isNull() const -> bool { return std::holds_alternative<std::monostate>(mStorage); }
    
    auto toFrame() -> Frame *;
    auto toPacket() -> Packet *;

    // Visit
    template <typename Fn>
    auto visit(Fn &&fn) const -> decltype(auto) {
        return std::visit(std::forward<Fn>(fn), mStorage);
    }

    // Operators
    auto operator =(Sample &&) -> Sample & = default;
    auto operator <=>(const Sample &rhs) const noexcept = default;

    explicit operator bool() const noexcept { return !isNull(); }
private:
    Storage mStorage;
};

// Impl
inline auto Sample::pts() const -> std::optional<Timestamp> {
    const auto visitor = Overloads {
        [](std::monostate) { return std::optional<Timestamp>{}; },
        [](const Frame &frame) { return frame.pts(); },
        [](const Packet &packet) { return packet.pts(); },
    };
    return std::visit(visitor, mStorage);
}

inline auto Sample::dts() const -> std::optional<Timestamp> {
    const auto visitor = Overloads {
        [](std::monostate) { return std::optional<Timestamp>{}; },
        [](const Frame &frame) { return frame.dts(); },
        [](const Packet &packet) { return packet.dts(); },
    };
    return std::visit(visitor, mStorage);
}

inline auto Sample::setPts(std::optional<Timestamp> pts) -> void {
    const auto visitor = Overloads {
        [](std::monostate) {},
        [&](Frame &frame) { frame.setPts(pts); },
        [&](Packet &packet) { packet.setPts(pts); },
    };
    std::visit(visitor, mStorage);
}

inline auto Sample::setDts(std::optional<Timestamp> dts) -> void {
    const auto visitor = Overloads {
        [](std::monostate) {},
        [&](Frame &frame) { frame.setDts(dts); },
        [&](Packet &packet) { packet.setDts(dts); },
    };
    std::visit(visitor, mStorage);
}

inline auto Sample::clone() const -> Sample {
    const auto visitor = Overloads {
        [](std::monostate) { return Sample{}; },
        [](const Frame &frame) { return Sample(frame.clone()); },
        [](const Packet &packet) { return Sample(packet.clone()); },
    };
    return std::visit(visitor, mStorage);
}

inline auto Sample::toFrame() -> Frame * {
    return std::get_if<Frame>(&mStorage);
}

inline auto Sample::toPacket() -> Packet * {
    return std::get_if<Packet>(&mStorage);
}

} // namespace nekoav


// Formatter
template <>
struct std::formatter<nekoav::Sample> {
    constexpr auto parse(auto &ctxt) {
        return ctxt.begin();
    }

    auto format(const nekoav::Sample &sample, auto &ctxt) const {
        const auto zero = nekoav::Timestamp {};
        const auto visitor = nekoav::Overloads {
            [&](std::monostate) { return std::format_to(ctxt.out(), "Sample(Null)"); },
            [&](const nekoav::Frame &frame) { return std::format_to(ctxt.out(), "Sample(Frame(pts: {}))", frame.pts().value_or(zero)); },
            [&](const nekoav::Packet &packet) { return std::format_to(ctxt.out(), "Sample(Packet(pts: {}))", packet.pts().value_or(zero)); },
        };
        return std::visit(visitor, sample.mStorage);
    }
};