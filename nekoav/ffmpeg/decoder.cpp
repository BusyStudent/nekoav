#define _NEKO_SOURCE
#include "../elements/decoder.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../pad.hpp"
#include "../log.hpp"
#include "common.hpp"
#include "ffmpeg.hpp"

#ifdef _WIN32
    #define HAVE_D3D11VA
    #include "../hwcontext/d3d11va.hpp"
    extern "C" {
        #include <libavutil/hwcontext.h>
        #include <libavutil/hwcontext_d3d11va.h>
    }
#endif

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
            return _process(v);
        });
        mSink->setEventCallback([this](View<Event> eventView) {
            if (eventView->type() == Event::FlushRequested) {
                avcodec_flush_buffers(mCtxt);
            }
            else if (eventView->type() == Event::SeekRequested) {
                mSeekTime = eventView.viewAs<SeekEvent>()->position();
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
        mSeekTime = 0.0;
        return Error::Ok;
    }
    Error _process(View<Packet> packet) {
        if (!mCtxt) {
            if (auto err = _initCodecContext(packet->stream()); err != Error::Ok) {
                return err;
            }
        }
        auto pts = packet->timestamp();
        auto drop = (pts < mSeekTime); //< Need we drop the frame ?
        int ret = avcodec_send_packet(mCtxt, packet->get());
        if (ret < 0) {
            return ToError(ret);
        }
        while (ret >= 0) {
            if (!mFrame) {
                mFrame = av_frame_alloc();
            }
            mFrame->flags = 0;
            if (drop) {
                mFrame->flags |= AV_FRAME_FLAG_DISCARD;
            }
            ret = avcodec_receive_frame(mCtxt, mFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                return ToError(ret);
            }
            if (drop) {
                // Drop 
                // NEKO_LOG("Packet({}) in pts {}, want pts {}, drop !", packet->stream()->codecpar->codec_type, pts, mSeekTime);
                return Error::Ok;
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
    Error _initCodecContext(AVStream *stream) {
        if (mHardwarePolicy != HardwarePolicy::ForceSoftware) {
            // Try hardware
            if (auto err = _initHardwareCodecContext(stream); err != Error::Ok) {
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
    Error _initHardwareCodecContext(AVStream *stream) {
        auto codecpar = stream->codecpar;
        auto codec = avcodec_find_decoder(codecpar->codec_id);
        mCtxt = avcodec_alloc_context3(codec);
        if (!mCtxt) {
            return Error::OutOfMemory;
        }
        int ret = avcodec_parameters_to_context(mCtxt, codecpar);
        if (ret < 0) {
            avcodec_free_context(&mCtxt);
            return ToError(ret); //< TODO : Add more detailed error
        }

        // Query hardware here
        Vec<const AVCodecHWConfig*> hwconfigs;
        for (int i = 0; ; i++) {
            auto conf = avcodec_get_hw_config(codec, i);
            if (!conf) {
                break;
            }
            if (!(conf->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
                continue;
            }
            NEKO_LOG("Enum HwConfig format on {}", av_pix_fmt_desc_get(conf->pix_fmt)->name);
            if (conf->pix_fmt== AV_PIX_FMT_D3D11) {
                // Prefer D3D11VA than DXVA
                hwconfigs.insert(hwconfigs.begin(), conf);
            }
            else {
                hwconfigs.emplace_back(conf);
            }
        }
        for (auto conf : hwconfigs) {
            // Got
            auto hardwareType = conf->device_type;
            mHardwareFmt = conf->pix_fmt;

            // Override HW get format
            mCtxt->opaque = this;
            mCtxt->get_format = [](struct AVCodecContext *s, const enum AVPixelFormat * fmt) {
                auto self = static_cast<FFDecoderImpl*>(s->opaque);
                auto p = fmt;
                auto outputFormat = *p;
                while (*p != AV_PIX_FMT_NONE) {
                    outputFormat = *p;
                    if (outputFormat == self->mHardwareFmt) {
                        break;
                    }
                    if (*(p + 1) == AV_PIX_FMT_NONE) {
                        // Fallback
                        break;
                    }
                    p ++;
                }
                // Init for hw frame if needed
#ifdef HAVE_D3D11VA
                if (outputFormat == AV_PIX_FMT_D3D11) {
                    self->_initHardwareFrameD3D11();
                }
#endif
                return outputFormat;
            };

#ifdef HAVE_D3D11VA
            if (hardwareType == AV_HWDEVICE_TYPE_D3D11VA) {
                auto d3d11Context = D3D11VAContext::create(this);
                if (!d3d11Context) {
                    continue;
                }
                mCtxt->hw_device_ctx = av_buffer_ref(d3d11Context->ffd3d11context());
            }
#endif
            if (!mCtxt->hw_device_ctx) {
                // Try init hw
                AVBufferRef *hardwareDeviceCtxt = nullptr;
                if (av_hwdevice_ctx_create(&hardwareDeviceCtxt, hardwareType, nullptr, nullptr, 0) < 0) {
                    // Failed
                    continue;
                }
                mCtxt->hw_device_ctx = hardwareDeviceCtxt;
            }

            // Init codec
            if (avcodec_open2(mCtxt, codec, nullptr) < 0) {
                av_buffer_unref(&mCtxt->hw_device_ctx);
                mCtxt->hw_device_ctx = nullptr;
                continue;
            }

            // Done
            NEKO_LOG("Hardware inited by {}", av_pix_fmt_desc_get(mHardwareFmt)->name);
            return Error::Ok;
        }

        // Fail
        avcodec_free_context(&mCtxt);
        return Error::Unknown;
    }
#ifdef HAVE_D3D11VA
    Error _initHardwareFrameD3D11() {
        NEKO_DEBUG("Try Init D3D11 HW Frame, add D3D11_RESOURCE_MISC_SHARED MiscFlags");

        int ret = avcodec_get_hw_frames_parameters(mCtxt, mCtxt->hw_device_ctx, AV_PIX_FMT_D3D11, &mCtxt->hw_frames_ctx);
        if (ret < 0) {
            av_buffer_unref(&mCtxt->hw_frames_ctx);
            return ToError(ret);
        }
        auto frameCtxt = reinterpret_cast<AVHWFramesContext *>(mCtxt->hw_frames_ctx->data);
        auto hwctxt = reinterpret_cast<AVD3D11VAFramesContext *>(frameCtxt->hwctx);
        // hwctxt->BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
        hwctxt->MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        ret = av_hwframe_ctx_init(mCtxt->hw_frames_ctx);
        if (ret < 0) {
            av_buffer_unref(&mCtxt->hw_frames_ctx);
            return ToError(ret);
        }
        NEKO_DEBUG("Done Init D3D11 HW Frame");
        return Error::Ok;
    }
#endif
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
    double          mSeekTime = 0.0;
};

NEKO_REGISTER_ELEMENT(Decoder, FFDecoderImpl);

}

NEKO_NS_END