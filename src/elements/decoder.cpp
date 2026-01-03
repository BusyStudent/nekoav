#include <nekoav/elements/decoder.hpp>
#include "internal.hpp"

extern "C" {
    #include <libavutil/hwcontext.h>
    #include <libavutil/pixdesc.h>

#if defined(_WIN32)
    #include <libavutil/hwcontext_d3d11va.h>
#endif // _WIN32
}

namespace nekoav {

static auto toString(AVHWDeviceType type) -> const char * {
    switch (type) {
        case AV_HWDEVICE_TYPE_NONE: return "none";
        case AV_HWDEVICE_TYPE_VDPAU: return "vdpau";
        case AV_HWDEVICE_TYPE_CUDA: return "cuda";
        case AV_HWDEVICE_TYPE_VAAPI: return "vaapi";
        case AV_HWDEVICE_TYPE_DXVA2: return "dxva2";
        case AV_HWDEVICE_TYPE_QSV: return "qsv";
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: return "videotoolbox";
        case AV_HWDEVICE_TYPE_D3D11VA: return "d3d11va";
        case AV_HWDEVICE_TYPE_DRM: return "drm";
        case AV_HWDEVICE_TYPE_OPENCL: return "opencl";
        case AV_HWDEVICE_TYPE_MEDIACODEC: return "mediacodec";
        case AV_HWDEVICE_TYPE_VULKAN: return "vulkan";
        case AV_HWDEVICE_TYPE_D3D12VA: return "d3d12va";
        default: return "unknown";
    }
}

struct Decoder::Impl {
    AVCodecContext *ctxt = nullptr;
    AVFrame        *frame = nullptr;
    AVPixelFormat   hwfmt = AV_PIX_FMT_NONE; // Use for HW decoding

    Impl() {
        frame = av_frame_alloc();
    }
    ~Impl() {
        avcodec_free_context(&ctxt);
        av_frame_free(&frame);
    }
};

Decoder::Decoder(std::string_view name) : 
    Element(name), 
    mInput(createInputPad("in")), 
    mOutput(createOutputPad("out")) 
{
    // Input accept both audio and video
    mInput.mutableCaps().insert(Caps::AudioPacket, {});
    mInput.mutableCaps().insert(Caps::VideoPacket, {});
    mInput.setPushCallback<&Decoder::onPadPush>(this);

    // Output
    mOutput.mutableCaps().insert(Caps::AudioRaw, {});
    mOutput.mutableCaps().insert(Caps::VideoRaw, {});
}

Decoder::~Decoder() {
    assert(!d); // Should be close onTeardown
}

auto Decoder::onPrepare() -> IoTask<void> {
    // Init codec here
    auto reply = mInput.sendQuery(Query::Caps{});
    if (reply) {
        co_return co_await init(reply->toCaps().caps);   
    }
    co_return {};
}

auto Decoder::onTeardown() -> IoTask<void> {
    d.reset();
    co_return {};
}

auto Decoder::onPadPush(Pad &pad, Sample sample) -> IoTask<void> {
    if (!sample) { // Forward EOS
        co_return co_await mOutput.push(std::move(sample));
    }
    if (!d) { // The decoder is not initialize, try init now
        auto reply =  mInput.sendQuery(Query::Caps{});
        if (!reply) {
            co_return Err(Error::InvalidState);
        }
        if (auto res = co_await init(reply->toCaps().caps); !res) {
            co_return Err(res.error());
        }
    }
    if (!sample.isPacket()) {
        co_return Err(Error::UnsupportedSampleType);
    }
    // Begin process
    auto packet = sample.toPacket();
    auto res = co_await ilias::blocking([&]() {
        int res = avcodec_send_packet(d->ctxt, packet->get());
        if (res != 0) {
            return res;
        }
        return avcodec_receive_frame(d->ctxt, d->frame);
    });

    // Need more data
    if (res == AVERROR(EAGAIN)) {
        co_return {};
    }
    else if (res < 0) {
        logger::error("[Decoder] Failed to decode {} => {}", res, error::toString(res));
        co_return Err(error::fromFFmpeg(res));
    }

    // Create new frame
    auto frame = Frame::from(av_frame_clone(d->frame), packet->timeBase());
    co_return co_await mOutput.push(std::move(frame));
}

auto Decoder::init(const Caps &caps) -> IoTask<void> {
    assert(!d);
    logger::info("[Decoder] init with caps");


    // Create the decoder
    const AVCodec *codec = nullptr;
    auto inner = std::make_unique<Impl>();

    // Common part
    auto createContext = [&](const Value &value) -> IoResult<void>{
        codec = avcodec_find_decoder_by_name(value[Caps::Codec].toString().c_str());
        if (!codec) {
            return Err(Error::NoCodec);
        }
        inner->ctxt = avcodec_alloc_context3(codec);
        if (!inner->ctxt) {
            return Err(Error::OutOfMemory);
        }
        return {};
    };
    auto setExtraData = [&](const Value &value) {
        if (auto &extra = value[Caps::CodecExtraData]; !extra.isBytes()) {
            return;
        }
        else if (auto bytes = extra.toBytes(); !bytes.empty()) {
            inner->ctxt->extradata = static_cast<uint8_t *>(av_mallocz(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE));
            inner->ctxt->extradata_size = bytes.size();
            ::memcpy(inner->ctxt->extradata, bytes.data(), bytes.size());
        }
    };

    // Convert the info
    if (auto &video = caps.find(Caps::VideoPacket); video.isMap()) {
        if (auto res = createContext(video); !res) {
            co_return Err(res.error());
        }
        // Set the extra data
        setExtraData(video);

        // Other info
        inner->ctxt->width = video[Caps::Width].toInteger();
        inner->ctxt->height = video[Caps::Height].toInteger();
        inner->ctxt->bit_rate = video[Caps::Bitrate].toInteger();
        inner->ctxt->pix_fmt = pixfmt::toFFmpeg(video[Caps::PixelFormat].toPixelFormat());
    }
    else if (auto &audio = caps.find(Caps::AudioPacket); audio.isMap()) {
        if (auto res = createContext(video); !res) {
            co_return Err(res.error());
        }
        // Set the extra data
        setExtraData(video);

        // TODO:
        inner->ctxt->sample_rate = audio[Caps::SampleRate].toInteger();
        inner->ctxt->sample_fmt = sample_fmt::toFFmpeg(audio[Caps::SampleFormat].toSampleFormat());
        inner->ctxt->ch_layout.nb_channels = audio[Caps::Channels].toInteger();
    }
    else { // WTF?
        co_return Err(Error::NoCodec);
    }

    // Open the codec, init hardware context may block
    if (auto res = co_await ilias::blocking([&]() { return open(inner.get()); }); !res) {
        co_return Err(res.error());
    }
    d.swap(inner);
    co_return {};
}

auto Decoder::open(Impl *inner) -> IoResult<void> {
    // Check if we can use hardware
    if (inner->ctxt->codec_type == AVMEDIA_TYPE_VIDEO) {
        std::vector<const AVCodecHWConfig *> hwconfigs {};
        for (int i = 0; ; i++) {
            auto conf = avcodec_get_hw_config(inner->ctxt->codec, i);
            if (!conf) {
                break;
            }
            if (!(conf->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
                continue;
            }
            hwconfigs.emplace_back(conf);
        }

        // Try to open it one by one
        auto previous = inner->ctxt->get_format;

        inner->ctxt->opaque = inner;
        inner->ctxt->get_format = [](struct AVCodecContext *s, const enum AVPixelFormat * fmt) {
            auto self = static_cast<Impl*>(s->opaque);
            auto p = fmt;
            auto outputFormat = *p;
            while (*p != AV_PIX_FMT_NONE) {
                outputFormat = *p;
                if (outputFormat == self->hwfmt) {
                    break;
                }
                if (*(p + 1) == AV_PIX_FMT_NONE) {
                    // Fallback
                    break;
                }
                p++;
            }
            logger::info("[Decoder] Select pixelformat '{}'", av_get_pix_fmt_name(outputFormat));
            return outputFormat;
        };

        for (auto &config : hwconfigs) {
            if (!inner->ctxt->hw_device_ctx) {
                // Try init hw
                AVBufferRef *hardwareDeviceCtxt = nullptr;
                if (av_hwdevice_ctx_create(&hardwareDeviceCtxt, config->device_type, nullptr, nullptr, 0) < 0) {
                    // Failed
                    continue;
                }
                inner->ctxt->hw_device_ctx = hardwareDeviceCtxt;
            }

            // Try init codec
            inner->hwfmt = config->pix_fmt;
            if (avcodec_open2(inner->ctxt, inner->ctxt->codec, nullptr) < 0) {
                av_buffer_unref(&inner->ctxt->hw_device_ctx);
                inner->ctxt->hw_device_ctx = nullptr;
                inner->hwfmt = AV_PIX_FMT_NONE;
                continue;
            }
            logger::info("[Decoder] Open codec for hardware device '{}' with format '{}'", toString(config->device_type), av_get_pix_fmt_name(inner->hwfmt));
            return {};
        }

        // ALl hardware failed, back to software
        inner->ctxt->opaque = nullptr;
        inner->ctxt->get_format = previous;
    }
    
    // Open the codec
    if (auto res = avcodec_open2(inner->ctxt, inner->ctxt->codec, nullptr); res < 0) {
        return Err(error::fromFFmpeg(res));
    }
    return {};
}


} // namespace nekoav