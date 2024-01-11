#pragma once

#include "../media/frame.hpp"
#include "../property.hpp"
#include "ffmpeg.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/frame.h>
}

NEKO_NS_BEGIN

namespace FFmpeg {

class Frame final : public MediaFrame {
public:
    explicit Frame(AVFrame* frame, AVRational t, AVMediaType type) : mFrame(frame), mTimebase(t), mType(type) {

    }
    ~Frame() {
        av_frame_free(&mFrame);
    }

    AVFrame *get() const noexcept {
        return mFrame;
    }

    // Impl MediaFrame
    int  format() const override {
        if (mType == AVMEDIA_TYPE_AUDIO) {
            return int(ToSampleFormat(AVSampleFormat(mFrame->format)));
        }
        else {
            return int(ToPixelFormat(AVPixelFormat(mFrame->format)));
        }
    }
    double timestamp() const override {
        // Because AVFrame's time_base is currently unused, so we have to carray it by ourself
        // return mFrame->pts * av_q2d(mFrame->time_base);
        if (mFrame->pts == AV_NOPTS_VALUE) {
            return 0;
        }
        return mFrame->pts * av_q2d(mTimebase);
    }
    double duration() const override {
        // Check API
#if LIBAVUTIL_VERSION_INT == AV_VERSION_INT(58, 29, 100)
        int64_t duration = mFrame->duration;
#else
        int64_t duration = mFrame->pkt_duration;
#endif
        if (duration == AV_NOPTS_VALUE) {
            return 0;
        }
        return duration * av_q2d(mTimebase);
    }
    // bool isKeyFrame() const override {
    //     return mFrame->key_frame;
    // }
    bool makeWritable() override {
        return av_frame_make_writable(mFrame) >= 0;
    }

    int linesize(int idx) const override {
        if (idx >= 8 || idx < 0) {
            return 0;
        }
        return mFrame->linesize[idx];
    }
    void *data(int idx) const override {
        if (idx >= 8 || idx < 0) {
            return nullptr;
        }
        return mFrame->data[idx];
    }

    int query(Value q) const override {
        switch (q) {
            case Value::Channels: return mFrame->channels;
            case Value::SampleRate: return mFrame->sample_rate;
            case Value::SampleCount: return mFrame->nb_samples;
            case Value::Height: return mFrame->height;
            case Value::Width: return mFrame->width;
            default: return 0;
        }
    }
    bool set(Value q, const void *) override {
        return false;
    }
    AVRational timebase() const noexcept {
        return mTimebase;
    }


    static auto make(AVFrame *f, AVRational timebase, AVMediaType type) {
        return make_shared<Frame>(f, timebase, type);
    }
private:
    // std::mutex     mMutex;
    AVFrame       *mFrame {nullptr};
    AVRational     mTimebase {0, 1};
    AVMediaType    mType;
};

class Packet final : public MediaPacket {
public:
    static auto make(AVPacket *p, AVStream *stream) {
        return make_shared<Packet>(p, stream);
    }

    Packet(AVPacket *pack, AVStream *stream) : mPacket(pack), mStream(stream) {
        mTimebase = mStream->time_base;
        mType = mStream->codecpar->codec_type;
    }
    ~Packet() {
        av_packet_free(&mPacket);
    }

    int64_t size() const override {
        return mPacket->size;
    }
    void *data() const override {
        return mPacket->data;
    }
    double duration() const override {
        if (mPacket->duration == AV_NOPTS_VALUE) {
            return 0;
        }
        return mPacket->duration * av_q2d(mStream->time_base);
    }
    double timestamp() const override {
        if (mPacket->pts == AV_NOPTS_VALUE) {
            return 0;
        }
        return mPacket->pts * av_q2d(mStream->time_base);
    }

    AVPacket *get() const noexcept {
        return mPacket;
    }
    AVStream *stream() const noexcept {
        return mStream;
    }
    AVRational timebase() const noexcept {
        return mTimebase;
    }
    AVMediaType type() const noexcept {
        return mType;
    }
private:
    AVPacket *mPacket {nullptr};
    AVStream *mStream {nullptr};
    AVRational mTimebase;
    AVMediaType mType;
};

}

NEKO_NS_END