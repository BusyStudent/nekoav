#include <nekoav/sample.hpp>
#include <stdexcept>
#include "ffmpeg.hpp"

namespace nekoav {

// Frame
#pragma region Frame
auto Frame::free(AVFrame *frame) -> void {
    av_frame_free(&frame);
}

// Setters
auto Frame::setPts(std::optional<Timestamp> pts) -> void {
    if (pts) {
        mFrame->pts = time::toFFmpeg(*pts, mTimeBase);
    }
    else {
        mFrame->pts = AV_NOPTS_VALUE;
    }
}

auto Frame::setDts(std::optional<Timestamp> dts) -> void {
    if (dts) {
        mFrame->pkt_dts = time::toFFmpeg(*dts, mTimeBase);
    }
    else {
        mFrame->pkt_dts = AV_NOPTS_VALUE;
    }
}

auto Frame::setDuration(std::optional<Duration> duration) -> void {
    if (duration) {
        mFrame->duration = time::toFFmpeg(*duration, mTimeBase);
    }
    else {
        mFrame->duration = 0;
    }
}

// Getters
auto Frame::pts() const -> std::optional<Timestamp> {
    if (mFrame->pts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mFrame->pts, mTimeBase);
}

auto Frame::dts() const -> std::optional<Timestamp> {
    if (mFrame->pkt_dts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mFrame->pkt_dts, mTimeBase);
}

auto Frame::duration() const -> std::optional<Duration> {
    if (mFrame->duration == 0) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mFrame->duration, mTimeBase);
}

auto Frame::data(int plane) -> void * {
    assert(plane >= 0 && plane < AV_NUM_DATA_POINTERS);
    return mFrame->data[plane];
}

auto Frame::linesize(int plane) -> int {
    assert(plane >= 0 && plane < AV_NUM_DATA_POINTERS);
    return mFrame->linesize[plane];
}

// MARK: VideoFrame
auto VideoFrame::pixelFormat() const -> PixelFormat {
    return pixfmt::fromFFmpeg(AVPixelFormat(mFrame->format));
}

auto VideoFrame::height() const -> int {
    return mFrame->height;
}

auto VideoFrame::width() const -> int {
    return mFrame->width;
}

// MARK: AudioFrame
auto AudioFrame::sampleFormat() const -> SampleFormat {
    return sample_fmt::fromFFmpeg(AVSampleFormat(mFrame->format));
}

auto AudioFrame::channels() const -> int {
    return mFrame->ch_layout.nb_channels;
}

auto AudioFrame::samples() const -> int {
    return mFrame->nb_samples;
}

auto AudioFrame::sampleRate() const -> int {
    return mFrame->sample_rate;
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

auto Frame::clone(AVFrame *f) -> AVFrame * {
    assert(f);
    return av_frame_clone(f);
}

// Packet
#pragma region Packet
auto Packet::free(AVPacket *packet) -> void {
    av_packet_free(&packet);
}

// Setters
auto Packet::setPts(std::optional<Timestamp> pts) -> void {
    if (pts) {
        mPacket->pts = time::toFFmpeg(*pts, mTimeBase);
    }
    else {
        mPacket->pts = AV_NOPTS_VALUE;
    }
}

auto Packet::setDts(std::optional<Timestamp> dts) -> void {
    if (dts) {
        mPacket->dts = time::toFFmpeg(*dts, mTimeBase);
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
    return time::fromFFmpeg(mPacket->pts, mTimeBase);
}

auto Packet::dts() const -> std::optional<Timestamp> {
    if (mPacket->dts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    return time::fromFFmpeg(mPacket->dts, mTimeBase);
}

auto Packet::data() const -> std::span<std::byte> {
    return {reinterpret_cast<std::byte *>(mPacket->data), static_cast<size_t>(mPacket->size)};
}

auto Packet::isKeyFrame() const -> bool {
    return mPacket->flags & AV_PKT_FLAG_KEY;
}

// Other
auto Packet::clone() const -> Packet {
    return Packet{av_packet_clone(mPacket.get()), mTimeBase};
}

} // namespace nekoav