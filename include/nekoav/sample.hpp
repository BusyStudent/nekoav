#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/format.hpp>
#include <nekoav/caps.hpp>
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

using NanoSeconds = std::chrono::nanoseconds;
using Timestamp = NanoSeconds;
using Duration = NanoSeconds;

// Forward declare
class Sample;
class Frame;
class Packet;

/**
 * @brief The bultin rtti for sample, make cast faster on frequency use type
 * 
 */
enum class SampleRtti : uint8_t {
    Base = 0, // The Base class
    Frame = 1,
    Packet = 2,
};

/**
 * @brief The data passed between elements.
 * 
 */
class NEKOAV_API Sample : public std::enable_shared_from_this<Sample> {
public:
    using Ptr = std::shared_ptr<Sample>;

    Sample() = default;
    Sample(const Sample &) = delete;
    virtual ~Sample() = default;

    // Get the presentation timestamp
    auto pts() const -> Timestamp { return mPts; }

    // Get the decoding timestamp
    auto dts() const -> Timestamp { return mDts; }

    // Set the Sample is discontinuity
    auto setDiscontinuity(bool discontinuity) -> void { mDiscontinuity = discontinuity; }
    auto isDiscontinuity() const -> bool { return mDiscontinuity; }

    // Cast
    auto isFrame() const -> bool { return mRtti == SampleRtti::Frame; }
    auto isPacket() const -> bool { return mRtti == SampleRtti::Packet; }
    auto toFrame() -> Frame *;
    auto toPacket() -> Packet *;
private:
    Timestamp  mPts = {};
    Timestamp  mDts = {};
    Duration   mDuration = {};
    SampleRtti mRtti = {};
    bool       mDiscontinuity = false; // The Element should flush the codec context before use this sample
friend class Frame; // For impl the cast
friend class Packet;
};

/**
 * @brief A Frame of raw data, usually uncompressed. wrapping AVFrame.
 * 
 */
class NEKOAV_API Frame : public Sample {
public:
    using Ptr = std::shared_ptr<Frame>;

    Frame();
    ~Frame();

    // Sette, only can use when writable
    auto setPts(Timestamp pts) -> void;
    auto setDts(Timestamp dts) -> void;

    // Getters for AVFrame fields
    auto data(int plane) -> void *;
    auto linesize(int plane) -> int;

    auto pixelFormat() const -> PixelFormat;
    auto height() const -> int;
    auto width() const -> int;
    
    /**
     * @brief Make the frame writable, doing COW
     */
    auto makeWritable() -> void;

    /**
     * @brief CHeck the frame is writeable, can use the Setter
     * 
     * @return true 
     * @return false 
     */
    auto isWritable() const -> bool;

    /**
     * @brief Create an frame from an exisited avfeame, it will take the ownship of it
     * 
     * @param avframe The avframe, can't be nullptr
     * @return Ptr 
     */
    static auto make(AVFrame *avframe, Rational timeBase) -> Ptr;
private:
    AVFrame *mFrame = nullptr; // Placeholder for AVFrame*
    Rational mTimeBase = {0, 1};
};

/**
 * @brief A Packet of encoded data, usually compressed. wrapping AVPacket.
 * 
 */
class NEKOAV_API Packet : public Sample {
public:
    using Ptr = std::shared_ptr<Packet>;

    Packet();
    ~Packet();

    auto data() -> std::span<std::byte>;

    /**
     * @brief Create an Packet from an exisited avpacket, it will take the ownship of it
     * 
     * @param avpacket The avpacket, can't be nullptr
     * @return Ptr 
     */
    static auto make(AVPacket *avpacket, Rational timeBase) -> Ptr;
private:
    AVPacket *mPacket = nullptr; // Placeholder for AVPacket*
    Rational mTimeBase = {0, 1};
};

// Impl
inline auto Sample::toFrame() -> Frame * {
    return isFrame() ? static_cast<Frame *>(this) : nullptr;
}

inline auto Sample::toPacket() -> Packet * {
    return isPacket() ? static_cast<Packet *>(this) : nullptr;
}

} // namespace nekoav