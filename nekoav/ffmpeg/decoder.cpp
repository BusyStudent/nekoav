#define _NEKO_SOURCE
#include "../elements/decoder.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../pad.hpp"
#include "../log.hpp"
#include "common.hpp"
#include "ffmpeg.hpp"

NEKO_NS_BEGIN

namespace FFmpeg {

class FFDecoderImpl final : public Template::GetImpl<Decoder> {
public:
    FFDecoderImpl() {
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
            if (eventView->type() == Event::FlushRequested) {
                avcodec_flush_buffers(mCtxt);
            }
            // Forward to next
            return mSrc->pushEvent(eventView);
        });
    }
    ~FFDecoderImpl() {
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
        if (mHardwarePolicy != HardwarePolicy::ForceSoftware) {
            // Try hardware
            if (auto err = initHardwareCodecContext(stream); err != Error::Ok) {
                if (mHardwarePolicy == HardwarePolicy::ForceHardware) {
                    // ForceHard, failed
                    return err;
                }
            }
            else {
                return Error::Ok;
            }
        }

        NEKO_ASSERT(mCtxt == nullptr);

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
    Error initHardwareCodecContext(AVStream *stream) {
        auto codecpar = stream->codecpar;
        auto codec = avcodec_find_decoder(codecpar->codec_id);
        mCtxt = avcodec_alloc_context3(codec);
        if (!mCtxt) {
            return Error::OutOfMemory;
        }
        int ret = avcodec_parameters_to_context(mCtxt, codecpar);
        if (ret < 0) {
            avcodec_free_context(&mCtxt);
            return Error::Unknown; //< TODO : Add more detailed error
        }

        // Query hardware here
        const AVCodecHWConfig *conf = nullptr;
        for (int i = 0; ; i++) {
            conf = avcodec_get_hw_config(codec, i);
            if (!conf) {
                // Fail
                avcodec_free_context(&mCtxt);
                return Error::Unknown;
            }

            if (!(conf->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
                continue;
            }
            // Got
            auto hardwareType = conf->device_type;
            mHardwareFmt = conf->pix_fmt;

            // Override HW get format
            mCtxt->opaque = this;
            mCtxt->get_format = [](struct AVCodecContext *s, const enum AVPixelFormat * fmt) {
                auto self = static_cast<FFDecoderImpl*>(s->opaque);
                auto p = fmt;
                while (*p != AV_PIX_FMT_NONE) {
                    if (*p == self->mHardwareFmt) {
                        return self->mHardwareFmt;
                    }
                    if (*(p + 1) == AV_PIX_FMT_NONE) {
                        // Fallback
                        return *p;
                    }
                    p ++;
                }
                ::abort();
                return AV_PIX_FMT_NONE;
            };

            
            // Try init hw
            AVBufferRef *hardwareDeviceCtxt = nullptr;
            if (av_hwdevice_ctx_create(&hardwareDeviceCtxt, hardwareType, nullptr, nullptr, 0) < 0) {
                // Failed
                continue;
            }
            // hardwareCtxt->hw_device_ctx = av_buffer_ref(hardwareDeviceCtxt);
            mCtxt->hw_device_ctx = hardwareDeviceCtxt;

            // Init codec
            if (avcodec_open2(mCtxt, codec, nullptr) < 0) {
                continue;
            }

            // Done
            NEKO_LOG("Hardware inited by {}", std::string_view(av_pix_fmt_desc_get(mHardwareFmt)->name));
            return Error::Ok;
        }

        // Fail
        avcodec_free_context(&mCtxt);
        return Error::Unknown;
    }
    Error setHardwarePolicy(HardwarePolicy policy) override {
        mHardwarePolicy = policy;
        return Error::Ok;
    }
private:
    AVFrame        *mFrame = nullptr;
    AVCodecContext *mCtxt = nullptr;
    Pad            *mSink = nullptr;
    Pad            *mSrc = nullptr;
    AVPixelFormat   mHardwareFmt = AV_PIX_FMT_NONE;
    HardwarePolicy  mHardwarePolicy = HardwarePolicy::Auto;
};

NEKO_REGISTER_ELEMENT(Decoder, FFDecoderImpl);

}

NEKO_NS_END