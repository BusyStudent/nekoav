#include <nekoav/sample.hpp>
#include <stdexcept>
#include "internal.hpp"

namespace nekoav {

// Frame
#pragma region Frame
auto Frame::free(AVFrame *frame) -> void {
    av_frame_free(&frame);
}

// Setters
auto Frame::setPts(std::optional<Timestamp> pts) -> void {
    if (pts) {
        mFrame->pts = time::toFFmpeg(*pts, AVRational{mTimeBase.num, mTimeBase.den});
    }
    else {
        mFrame->pts = AV_NOPTS_VALUE;
    }
}

auto Frame::setDts(std::optional<Timestamp> dts) -> void {
    if (dts) {
        mFrame->pkt_dts = time::toFFmpeg(*dts, AVRational{mTimeBase.num, mTimeBase.den});
    }
    else {
        mFrame->pkt_dts = AV_NOPTS_VALUE;
    }
}

// Getters
auto Frame::pts() const -> std::optional<Timestamp> {
    if (mFrame->pts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mFrame->pts, AVRational {.num = mTimeBase.num, .den = mTimeBase.den});
}

auto Frame::dts() const -> std::optional<Timestamp> {
    if (mFrame->pkt_dts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mFrame->pkt_dts, AVRational {.num = mTimeBase.num, .den = mTimeBase.den});
}

auto Frame::pixelFormat() const -> PixelFormat {
    return pixfmt::fromFFmpeg(AVPixelFormat(mFrame->format));
}

auto Frame::sampleFormat() const -> SampleFormat {
    return sample_fmt::fromFFmpeg(AVSampleFormat(mFrame->format));
}

auto Frame::height() const -> int {
    return mFrame->height;
}

auto Frame::width() const -> int {
    return mFrame->width;
}

auto Frame::data(int plane) -> void * {
    assert(plane >= 0 && plane < AV_NUM_DATA_POINTERS);
    return mFrame->data[plane];
}

auto Frame::linesize(int plane) -> int {
    assert(plane >= 0 && plane < AV_NUM_DATA_POINTERS);
    return mFrame->linesize[plane];
}

// Other
auto Frame::isWritable() const -> bool {
    return av_frame_is_writable(mFrame.get()) > 0;
}

auto Frame::makeWritable() -> IoResult<void> {
    if (int err = av_frame_make_writable(mFrame.get()); err < 0) {
        return Err(error::fromFFmpeg(err));
    }
    return {};
}

auto Frame::clone() const -> Frame {
    return from(av_frame_clone(mFrame.get()), mTimeBase);
}

auto Frame::from(AVFrame *avframe, Rational timeBase) -> Frame {
    assert(avframe);
    Frame frame;
    frame.mFrame.reset(avframe);
    frame.mTimeBase = timeBase;
    return frame;
}

// Packet
#pragma region Packet
auto Packet::free(AVPacket *packet) -> void {
    av_packet_free(&packet);
}

// Setters
auto Packet::setPts(std::optional<Timestamp> pts) -> void {
    if (pts) {
        mPacket->pts = time::toFFmpeg(*pts, AVRational{mTimeBase.num, mTimeBase.den});
    }
    else {
        mPacket->pts = AV_NOPTS_VALUE;
    }
}

auto Packet::setDts(std::optional<Timestamp> dts) -> void {
    if (dts) {
        mPacket->dts = time::toFFmpeg(*dts, AVRational{mTimeBase.num, mTimeBase.den});
    }
    else {
        mPacket->dts = AV_NOPTS_VALUE;
    }
}

// Getters
auto Packet::pts() const -> std::optional<Timestamp> {
    if (mPacket->pts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mPacket->pts, AVRational {.num = mTimeBase.num, .den = mTimeBase.den});
}

auto Packet::dts() const -> std::optional<Timestamp> {
    if (mPacket->dts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mPacket->dts, AVRational {.num = mTimeBase.num, .den = mTimeBase.den});
}

auto Packet::data() const -> std::span<std::byte> {
    return {reinterpret_cast<std::byte *>(mPacket->data), mPacket->size};
}

auto Packet::isKeyFrame() const -> bool {
    return mPacket->flags & AV_PKT_FLAG_KEY;
}

// Other
auto Packet::clone() const -> Packet {
    return from(av_packet_clone(mPacket.get()), mTimeBase);
}

auto Packet::from(AVPacket *avpacket, Rational timeBase) -> Packet {
    assert(avpacket);
    Packet packet;
    packet.mPacket.reset(avpacket);
    packet.mTimeBase = timeBase;
    return packet;
}

} // namespace nekoav