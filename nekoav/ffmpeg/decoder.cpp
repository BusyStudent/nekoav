#define _NEKO_SOURCE
#include "../elements/decoder.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../pad.hpp"
#include "common.hpp"
#include "ffmpeg.hpp"

NEKO_NS_BEGIN

namespace FFmpeg {

class FFDecoder final : public Template::GetImpl<Decoder> {
public:
    FFDecoder() {
        mSink = addInput("sink");
        mSrc = addOutput("src");

        mSink->setCallback([this](View<Resource> resourceView) {
            auto v = resourceView.viewAs<Packet>();
            if (!v) {
                return Error::UnsupportedResource;
            }
            return process(v);
        });
        mSink->setEventCallback([this](View<Event> eventView) {
            if (eventView->type() ==Event::FlushRequested) {
                avcodec_flush_buffers(mCtxt);
            }
            // Forward to next
            return mSrc->pushEvent(eventView);
        });
    }
    ~FFDecoder() {
        avcodec_free_context(&mCtxt);
        av_frame_free(&mFrame);
    }
    Error onTeardown() override {
        avcodec_free_context(&mCtxt);
        return Error::Ok;
    }
    Error process(View<Packet> packet) {
        if (!mCtxt) {
            if (auto err = initCodecContext(packet->stream()); err != Error::Ok) {
                return err;
            }
        }

        int ret = avcodec_send_packet(mCtxt, packet->get());
        if (ret < 0) {
            return ToError(ret);
        }
        while (ret >= 0) {
            if (!mFrame) {
                mFrame = av_frame_alloc();
            }
            ret = avcodec_receive_frame(mCtxt, mFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                return ToError(ret);
            }

            // Push data if
            if (mSrc->isLinked()) {
                auto resource = Frame::make(mFrame, packet->timebase(), packet->type());
                mFrame = nullptr; //< Move ownship
                mSrc->push(resource);
            }
            else {
                av_frame_unref(mFrame);
            }
        }
        return Error::Ok;
    }
    Error initCodecContext(AVStream *stream) {
        auto codec = avcodec_find_decoder(stream->codecpar->codec_id);
        mCtxt = avcodec_alloc_context3(codec);
        if (!mCtxt) {
            return Error::OutOfMemory;
        }
        int ret = avcodec_parameters_to_context(mCtxt, stream->codecpar);
        if (ret < 0) {
            avcodec_free_context(&mCtxt);
            return ToError(ret);
        }
        ret = avcodec_open2(mCtxt, mCtxt->codec, nullptr);
        if (ret < 0) {
            avcodec_free_context(&mCtxt);
            return ToError(ret);
        }
        return Error::Ok;
    }
private:
    AVFrame        *mFrame = nullptr;
    AVCodecContext *mCtxt = nullptr;
    Pad            *mSink = nullptr;
    Pad            *mSrc = nullptr;
};

NEKO_REGISTER_ELEMENT(Decoder, FFDecoder);

}

NEKO_NS_END