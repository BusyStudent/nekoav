#pragma once

#include "../media.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/frame.h>
}

NEKO_NS_BEGIN

namespace FFmpeg {

class Frame final : public MediaFrame {
public:
    explicit Frame(AVFrame* frame, AVRational t) : mFrame(frame), mTimebase(t) {

    }
    ~Frame() {
        av_frame_free(&mFrame);
    }

    AVFrame *get() const noexcept {
        return mFrame;
    }

    // Impl MediaFrame
    void lock() override {
        // return mMutex.lock();        
    }
    void unlock() override {
        // return mMutex.unlock();
    }
    int  format() const override {
        return mFrame->format;
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
        // return mFrame->pkt_duration * av_q2d(mFrame->time_base);
        if (mFrame->pkt_duration == AV_NOPTS_VALUE) {
            return 0;
        }
        return mFrame->pkt_duration * av_q2d(mTimebase);
    }
    bool isKeyFrame() const override {
        return mFrame->key_frame;
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

    int query(Query q) const override {
        switch (q) {
            case Query::Channels: return mFrame->channels;
            case Query::SampleRate: return mFrame->sample_rate;
            case Query::SampleCount: return mFrame->nb_samples;
            case Query::Height: return mFrame->height;
            case Query::Width: return mFrame->width;
            default: return 0;
        }
    }
    AVRational timebase() const noexcept {
        return mTimebase;
    }


    static auto make(AVFrame *f, AVRational timebase) {
        return make_shared<Frame>(f, timebase);
    }
private:
    // std::mutex     mMutex;
    AVFrame       *mFrame {nullptr};
    AVRational     mTimebase {0, 1};
};

class Packet final : public MediaPacket {
public:
    static auto make(AVPacket *p, AVStream *stream) {
        return make_shared<Packet>(p, stream);
    }

    Packet(AVPacket *pack, AVStream *stream) : mPacket(pack), mStream(stream) { }
    ~Packet() {
        av_packet_free(&mPacket);
    }

    void lock() override {

    }
    void unlock() override {

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
private:
    AVPacket *mPacket {nullptr};
    AVStream *mStream {nullptr};
};

}

NEKO_NS_END