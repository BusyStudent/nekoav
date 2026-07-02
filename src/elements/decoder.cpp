#include <nekoav/elements/decoder.hpp>
#include <algorithm>
#include "ffmpeg.hpp"

#if defined(_WIN32)
    #include <d3d11.h>
#endif // _WIN32

extern "C" {
    #include <libavutil/hwcontext.h>
    #include <libavutil/pixdesc.h>
    #include <libavcodec/avcodec.h>

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

static auto priorityOf(AVHWDeviceType type) -> int {
    switch (type) {
        // Windows
        case AV_HWDEVICE_TYPE_D3D11VA:      return 10;
        case AV_HWDEVICE_TYPE_D3D12VA:      return 20;
        case AV_HWDEVICE_TYPE_DXVA2:        return 30;
        
        // Linux
        case AV_HWDEVICE_TYPE_CUDA:         return 10; 
        case AV_HWDEVICE_TYPE_VAAPI:        return 20;
        case AV_HWDEVICE_TYPE_VDPAU:        return 40; // Legacy
        
        // MacOS
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: return 10;
        
        // Other
        case AV_HWDEVICE_TYPE_QSV:          return 15; // Intel QuickSync
        case AV_HWDEVICE_TYPE_MEDIACODEC:   return 10; // Android

        // Common
        case AV_HWDEVICE_TYPE_VULKAN:       return 30; 
        
        default:                            return 100;
    }
}

// Get params from the caps
struct ParamsDeleter {
    void operator()(AVCodecParameters *params) const {
        avcodec_parameters_free(&params);
    }
};

struct Decoder::Impl {
    AVCodecContext *ctxt = nullptr;
    AVFrame        *frame = nullptr;
    AVPixelFormat   hwfmt = AV_PIX_FMT_NONE; // Use for HW decoding
    bool            flush = false; // Need for flaush ?

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
    mInput.mutableCaps().insertOrAssign(Caps::AudioPacket, {});
    mInput.mutableCaps().insertOrAssign(Caps::VideoPacket, {});
    mInput.setPushCallback<&Decoder::onPadPush>(this);
    mInput.setEventCallback<&Decoder::onPadEvent>(this);

    // Output
    mOutput.mutableCaps().insertOrAssign(Caps::AudioRaw, {});
    mOutput.mutableCaps().insertOrAssign(Caps::VideoRaw, {});
}

Decoder::~Decoder() {
    assert(!d); // Should be close onTeardown
}

auto Decoder::setPolicy(Policy policy) -> void {
    mPolicy = policy;
}

auto Decoder::onPrepare() -> IoTask<void> {
    // Init codec here (fast path)
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
        // TODO: We maybe need to flush the decoder
        co_return co_await mOutput.push(std::move(sample));
    }
    if (!d) { // The decoder is not initialize, try init now
        auto reply =  mInput.sendQuery(Query::Caps{});
        if (!reply) {
            co_return Err(Error::InvalidState);
        }
        ILIAS_CO_TRYV(co_await init(reply->toCaps().caps));
    }
    if (!sample.isPacket()) {
        co_return Err(Error::SampleTypeNotSupported);
    }

    // Begin process
    auto packet = sample.toPacket();
    auto packetSent = false;
    auto process = [&]() {
        while (true) {
            if (d->flush) [[unlikely]] {
                d->flush = false;
                avcodec_flush_buffers(d->ctxt);
            }
            // First, try to drain the frame
            int res = avcodec_receive_frame(d->ctxt, d->frame);
            if (res == AVERROR(EAGAIN) && !packetSent) { // No frame left
                res = avcodec_send_packet(d->ctxt, packet->get());
                if (res != 0) { // Error Happend
                    return res;
                }
                packetSent = true;
                continue;
            }
            return res;
        }
    };
    while (true) {
        int res = co_await ilias::blocking(process);
        if (res == AVERROR(EAGAIN)) { // Packet conssumed and all frame drained
            break;
        }
        if (res < 0) {
            NEKOAV_ERROR("[Decoder] '{}' Failed to decode {} => {}", name(), res, error::toString(res));
            co_return Err(error::fromFFmpeg(res));
        }

        // Create new frame
        if (d->ctxt->codec_type == AVMEDIA_TYPE_VIDEO) {
            ILIAS_CO_TRYV(co_await mOutput.push(
                VideoFrame {
                    av_frame_clone(d->frame),
                    packet->timeBase()
                }
            ));
        }
        else if (d->ctxt->codec_type == AVMEDIA_TYPE_AUDIO) {
            ILIAS_CO_TRYV(co_await mOutput.push(
                AudioFrame {
                    av_frame_clone(d->frame),
                    packet->timeBase()
                }
            ));
        }
        else {
            // ???
            assert(false);
        }
    }
    co_return {};
}

auto Decoder::onPadEvent(Pad &pad, const Event &event) -> IoTask<void> {
    if (event.isFlushEnd()) {
        NEKOAV_INFO("[Decoder] '{}' flush end", name());
        d->flush = true;
    }
    co_return co_await mOutput.pushEvent(std::move(event));
}

auto Decoder::init(const Caps &caps) -> IoTask<void> {
    assert(!d);
    NEKOAV_INFO("[Decoder] '{}' init with caps", name());


    // Create the decoder
    const AVCodec *codec = nullptr;
    auto inner = std::make_unique<Impl>();

    // Common part
    auto createContext = [&](AVCodecParameters *params, const Value &value) -> IoResult<void> {
        codec = avcodec_find_decoder_by_name(value[Caps::Codec].toString().c_str());
        if (!codec) {
            return Err(Error::NoCodec);
        }
        inner->ctxt = avcodec_alloc_context3(codec);
        if (!inner->ctxt) {
            return Err(Error::OutOfMemory);
        }

        // Common part of the params
        params->codec_type = codec->type;
        params->codec_id = codec->id;
        params->codec_tag = static_cast<uint32_t>(value[Caps::CodecTag].toInteger());
        return {};
    };
    auto setExtraData = [](AVCodecParameters *params, const Value &value) {
        if (auto &extra = value[Caps::CodecExtraData]; !extra.isBytes()) {
            return;
        }
        else if (auto bytes = extra.toBytes(); !bytes.empty()) {
            params->extradata = static_cast<uint8_t *>(av_mallocz(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE));
            params->extradata_size = bytes.size();
            ::memcpy(params->extradata, bytes.data(), bytes.size());
        }
    };

    // Convert the info
    std::unique_ptr<AVCodecParameters, ParamsDeleter> params { avcodec_parameters_alloc() };
    if (auto &video = caps.find(Caps::VideoPacket); video.isMap()) {
        if (auto res = createContext(params.get(), video); !res) {
            co_return Err(res.error());
        }
        // Set the extra data
        setExtraData(params.get(), video);

        // Other info
        params->width = video[Caps::Width].toInteger();
        params->height = video[Caps::Height].toInteger();
        params->bit_rate = video[Caps::Bitrate].toInteger();
        params->format = pixfmt::toFFmpeg(video[Caps::PixelFormat].toPixelFormat());
        params->color_range = color_range::toFFmpeg(video[Caps::ColorRange].toColorRange());
        params->color_primaries = color_primaries::toFFmpeg(video[Caps::ColorPrimaries].toColorPrimaries());
        params->color_trc = color_transfer::toFFmpeg(video[Caps::ColorTransfer].toColorTransfer());
        params->color_space = color_space::toFFmpeg(video[Caps::ColorSpace].toColorSpace());
    }
    else if (auto &audio = caps.find(Caps::AudioPacket); audio.isMap()) {
        if (auto res = createContext(params.get(), audio); !res) {
            co_return Err(res.error());
        }
        // Set the extra data
        setExtraData(params.get(), audio);

        params->sample_rate = audio[Caps::SampleRate].toInteger();
        params->format = sample_fmt::toFFmpeg(audio[Caps::SampleFormat].toSampleFormat());
        av_channel_layout_default(&params->ch_layout, audio[Caps::Channels].toInteger());
    }
    else { // WTF?
        co_return Err(Error::NoCodec);
    }

    // Store it
    if (auto res = avcodec_parameters_to_context(inner->ctxt, params.get()); res < 0) {
        co_return Err(error::fromFFmpeg(res));
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
    bool canHardware = inner->ctxt->codec_type == AVMEDIA_TYPE_VIDEO;
    bool useHardware = canHardware && mPolicy != Policy::SoftwareOnly;
    if (useHardware) {
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

        // Sort by priority
        std::sort(hwconfigs.begin(), hwconfigs.end(), [](const AVCodecHWConfig *a, const AVCodecHWConfig *b) {
            return priorityOf(a->device_type) < priorityOf(b->device_type);
        });
        for (auto &config : hwconfigs) {
            NEKOAV_INFO("[Decoder] '{}' Found hardware config '{}'", name(), toString(config->device_type));
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
            NEKOAV_INFO("[Decoder] Select pixelformat '{}'", av_get_pix_fmt_name(outputFormat));
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
            NEKOAV_INFO("[Decoder] '{}' Try open codec for hardware device '{}' with format '{}'", name(), toString(config->device_type), av_get_pix_fmt_name(inner->hwfmt));
            if (auto res = avcodec_open2(inner->ctxt, inner->ctxt->codec, nullptr); res < 0) {
                av_buffer_unref(&inner->ctxt->hw_device_ctx);
                inner->ctxt->hw_device_ctx = nullptr;
                inner->hwfmt = AV_PIX_FMT_NONE;
                continue;
            }
            NEKOAV_INFO("[Decoder] '{}' Open hardware codec done", name());
            return {};
        }

        // ALl hardware failed, back to software
        inner->ctxt->opaque = nullptr;
        inner->ctxt->get_format = previous;
    }
    if (mPolicy == Policy::HardwareOnly) {
        // Hardware unavailable
        return Err(Error::NoCodec);
    }
    
    // Open the codec
    if (auto res = avcodec_open2(inner->ctxt, inner->ctxt->codec, nullptr); res < 0) {
        return Err(error::fromFFmpeg(res));
    }
    NEKOAV_INFO(
        "[Decoder] '{}' Open software codec done with fmt {}", 
        name(),
        inner->ctxt->codec->type == AVMEDIA_TYPE_VIDEO ? av_get_pix_fmt_name(inner->ctxt->pix_fmt) : av_get_sample_fmt_name(inner->ctxt->sample_fmt)
    );
    return {};
}


} // namespace nekoav