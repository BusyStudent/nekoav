#include <nekoav/sample.hpp>
#include <stdexcept>
#include "internal.hpp"

namespace nekoav {

// Frame
Frame::Frame() {
    mRtti = SampleRtti::Frame;
}

Frame::~Frame() {
    av_frame_free(&mFrame);
}

// Setters
auto Frame::setPts(Timestamp pts) -> void {
    if (!isWritable()) {
        throw std::runtime_error("Frame is not writable");
    }
    mFrame->pts = time::toFFmpeg(pts, AVRational{mTimeBase.num, mTimeBase.den});
    mPts = pts;
}

auto Frame::setDts(Timestamp dts) -> void {
    if (!isWritable()) {
        throw std::runtime_error("Frame is not writable");
    }
    mFrame->pkt_dts = time::toFFmpeg(dts, AVRational{mTimeBase.num, mTimeBase.den});
}

// Getters
auto Frame::pixelFormat() const -> PixelFormat {
    return pixfmt::fromFFmpeg(AVPixelFormat(mFrame->format));
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
    return av_frame_is_writable(mFrame) > 0;
}

auto Frame::makeWritable() -> void {
    if (av_frame_make_writable(mFrame) < 0) {
        throw std::runtime_error("Failed to make frame writable");
    }
}


auto Frame::make(AVFrame *avframe, Rational timeBase) -> Ptr {
    assert(avframe);
    auto ptr = std::make_shared<Frame>();

    // FIll the frame part
    ptr->mFrame = avframe;
    ptr->mTimeBase = timeBase;

    // Fill the sample part
    auto ffTimeBase = AVRational {timeBase.num, timeBase.den};
    ptr->mPts = time::fromFFmpeg(avframe->pts, ffTimeBase);
    ptr->mDts = time::fromFFmpeg(avframe->pkt_dts, ffTimeBase);
    ptr->mDuration = time::fromFFmpeg(avframe->duration, ffTimeBase);
    return ptr;
}

// Packet
Packet::Packet() {
    mRtti = SampleRtti::Packet;
}

Packet::~Packet() {
    mPacket->duration = 0;
    av_packet_free(&mPacket);
}

} // namespace nekoav