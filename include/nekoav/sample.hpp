#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/format.hpp>
#include <optional>
#include <variant>
#include <cassert>
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
class NEKOAV_API Frame {
public:
    Frame(Frame &&) = default;

    // Setter
    auto setPts(std::optional<Timestamp> pts) -> void;
    auto setDts(std::optional<Timestamp> dts) -> void;

    // Getters for AVFrame fields
    auto pts() const -> std::optional<Timestamp>;
    auto dts() const -> std::optional<Timestamp>;

    auto data(int plane) -> void *;
    auto linesize(int plane) -> int;

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

    // Operators
    auto operator =(Frame &&) -> Frame & = default;
    auto operator <=>(const Frame &rhs) const noexcept = default;

    /**
     * @brief Check the frame is valid
     * 
     * @return true 
     * @return false 
     */
    explicit operator bool() const { return static_cast<bool>(mFrame); }
protected:
    /**
     * @brief Construct a new Frame object
     * 
     * @param ptr The avframe, can't be nullptr, must match the frame type
     * @param timeBase The time base of the frame
     */
    explicit Frame(AVFrame *ptr, Rational timeBase) : mFrame(ptr), mTimeBase(timeBase) { assert(ptr); }
    Frame() = default;

    // Wrapper of ffmpeg functions
    static auto free(AVFrame *ptr) -> void;
    static auto clone(AVFrame *ptr) -> AVFrame *;

    struct Deleter {
        auto operator()(AVFrame *ptr) { free(ptr); }
    };

    std::unique_ptr<AVFrame, Deleter> mFrame; // Placeholder for AVFrame*
    Rational                          mTimeBase {0, 1};
};

/**
 * @brief The audio frame
 * 
 */
class NEKOAV_API AudioFrame final : public Frame {
public:
    explicit AudioFrame(AVFrame *ptr, Rational timeBase) : Frame(ptr, timeBase) {}
    AudioFrame(AudioFrame &&) = default;
    AudioFrame() = default;

    // Audio specific
    auto sampleFormat() const -> SampleFormat;
    auto sampleRate() const -> int;
    auto channels() const -> int;
    auto samples() const -> int;

    /**
     * @brief Clone the frame, it will create a new frame and ref the data (COW)
     * 
     * @return Frame 
     */
    auto clone() const -> AudioFrame;

    // Operators
    auto operator =(AudioFrame &&) -> AudioFrame & = default;
    auto operator <=>(const AudioFrame &rhs) const noexcept = default;
};

/**
 * @brief The video frame
 * 
 */
class NEKOAV_API VideoFrame final : public Frame {
public:
    explicit VideoFrame(AVFrame *ptr, Rational timeBase) : Frame(ptr, timeBase) {}
    VideoFrame(VideoFrame &&) = default;
    VideoFrame() = default;

    // Video specific
    auto pixelFormat() const -> PixelFormat;
    auto height() const -> int;
    auto width() const -> int;

    /**
     * @brief Clone the frame, it will create a new frame and ref the data (COW)
     * 
     * @return Frame 
     */
    auto clone() const -> VideoFrame;

    // Operators
    auto operator =(VideoFrame &&) -> VideoFrame & = default;
    auto operator <=>(const VideoFrame &rhs) const noexcept = default;
};

/**
 * @brief A Packet of encoded data, usually compressed. wrapping AVPacket.
 * 
 */
class NEKOAV_API Packet final {
public:
    /**
     * @brief Construct a new Packet object, it will take the ownership of it
     * 
     * @param ptr The avpacket, can't be nullptr
     * @param timeBase The time base of the packet
     */
    explicit Packet(AVPacket *ptr, Rational timeBase) : mPacket(ptr), mTimeBase(timeBase) { assert(ptr); }
    Packet(Packet &&) = default;
    Packet() = default;

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
    using Storage = std::variant<std::monostate, VideoFrame, AudioFrame, Packet>;

    Sample(std::nullptr_t) noexcept : mStorage(std::monostate()) {}
    Sample(const Sample &) noexcept = delete;
    Sample(Sample &&) noexcept = default;
    Sample() = default;

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
    auto isAudioFrame() const -> bool { return std::holds_alternative<AudioFrame>(mStorage); }
    auto isVideoFrame() const -> bool { return std::holds_alternative<VideoFrame>(mStorage); }
    auto isPacket() const -> bool { return std::holds_alternative<Packet>(mStorage); }
    auto isNull() const -> bool { return std::holds_alternative<std::monostate>(mStorage); }
    
    auto toPacket() -> Packet *;
    auto toVideoFrame() -> VideoFrame *;
    auto toAudioFrame() -> AudioFrame *;

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
        [](const AudioFrame &frame) { return Sample{frame.clone()}; },
        [](const VideoFrame &frame) { return Sample{frame.clone()}; },
        [](const Packet &packet) { return Sample{packet.clone()}; },
    };
    return std::visit(visitor, mStorage);
}

inline auto Sample::toPacket() -> Packet * {
    return std::get_if<Packet>(&mStorage);
}

inline auto Sample::toVideoFrame() -> VideoFrame * {
    return std::get_if<VideoFrame>(&mStorage);
}

inline auto Sample::toAudioFrame() -> AudioFrame * {
    return std::get_if<AudioFrame>(&mStorage);
}

} // namespace nekoav


// Formatter
template <>
struct std::formatter<nekoav::Sample> {
    constexpr auto parse(std::format_parse_context &ctxt) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const nekoav::Sample &sample, FormatContext &ctxt) const {
        const auto zero = nekoav::Timestamp {};
        const auto visitor = nekoav::Overloads {
            [&](std::monostate) { return std::format_to(ctxt.out(), "Sample(Null)"); },
            [&](const nekoav::VideoFrame &frame) { return std::format_to(ctxt.out(), "Sample(VideoFrame(pts: {}))", frame.pts().value_or(zero)); },
            [&](const nekoav::AudioFrame &frame) { return std::format_to(ctxt.out(), "Sample(AudioFrame(pts: {}))", frame.pts().value_or(zero)); },
            [&](const nekoav::Packet &packet) { return std::format_to(ctxt.out(), "Sample(Packet(pts: {}))", packet.pts().value_or(zero)); },
        };
        return std::visit(visitor, sample.mStorage);
    }
};