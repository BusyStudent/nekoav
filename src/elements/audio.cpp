#include <nekoav/elements/audio.hpp>
#include <ilias/sync/mpsc.hpp>
#include <queue>
#include <mutex>
#include "internal.hpp"

// Import miniaudio
#if !defined(NDEBUG)
    #define MA_DEBUG_OUTPUT
#endif

#define  MA_IMPLEMENTATION
#define  MA_USE_STDINT
#define  MA_NO_DECODING
#define  MA_NO_ENCODING
#define  MA_NO_GENERATION
#define  MA_NO_ENGINE
#define  MA_NO_NODE_GRAPH
#define  MA_API static
#include <miniaudio.h>

#undef min
#undef max

namespace nekoav {

struct AudioSink::Impl {
    ma_context        context {};
    ma_device         device {};

    bool              contextInited = false;
    bool              deviceInited  = false;

    // Send the Frame to the device callback
    ilias::mpsc::Sender<Frame>   frameSender {};

    // Access by the Callback, multithreaded
    ilias::mpsc::Receiver<Frame> frameReceiver {};
    std::optional<Frame>         currentFrame {};
    size_t                       currentFrameOffset = 0;
    std::atomic<Timestamp>       currentPts {};

    Impl() {
        auto [sender, receiver] = ilias::mpsc::channel<Frame>(20); // Cache 20 frames ?, I think it's enough
        frameSender = std::move(sender);
        frameReceiver = std::move(receiver);
    }

    ~Impl() {
        if (deviceInited) {
            ma_device_uninit(&device);
        }
        if (contextInited) {
            ma_context_uninit(&context);            
        }
    }

    auto audioCallback(ma_device *device, std::byte *output, const std::byte *input, ma_uint32 frameCount) -> void;
};

AudioSink::AudioSink(std::string_view name) : Element(name), mInput(createInputPad("in")) {
    mInput.setPushCallback<&AudioSink::onPush>(this);
}

AudioSink::~AudioSink() {
    assert(!d);
}

auto AudioSink::onPrepare() -> IoTask<void> {
    auto impl = std::make_unique<Impl>();
    if (auto res = ma_context_init(nullptr, 0, nullptr, &impl->context); res != MA_SUCCESS) {
        co_return Err(Error::External);
    }
    impl->contextInited = true;

    // Try to enumerate devices and get the formats
    ma_device_info *info = nullptr;
    ma_uint32       count = 0;
    if (auto res = ma_context_get_devices(&impl->context, &info, &count, nullptr, nullptr); res != MA_SUCCESS) {
        co_return Err(Error::External);
    }

    // Log the devices
    // TODO: Use the native format to neg
    // ma_device_info *selected = nullptr;
    // for (ma_uint32 i = 0; i < count; ++i) {
    //     auto &device = info[i];
    //     if (device.isDefault) {
    //         selected = &device;
    //     }
    // }

    d.swap(impl);
    co_return {};
}

auto AudioSink::onStop() -> IoTask<void> {
    d.reset();
    co_return {};
}

auto AudioSink::onPause() -> IoTask<void> {
    if (d->deviceInited) {
        ma_device_stop(&d->device);
    }
    co_return {};
}

auto AudioSink::onRun() -> IoTask<void> {
    if (d->deviceInited) {
        ma_device_start(&d->device);
    }
    co_return {};
}

auto AudioSink::onPush(Pad &pad, Sample sample) -> IoTask<void> {
    if (!sample) { // EOF
        co_return {};
    }
    if (!sample.isFrame()) {
        co_return Err(Error::UnsupportedSampleType);
    }
    auto frame = sample.toFrame();

    // Lazy init the device
    if (!d->deviceInited) {
        if (auto res = initDevice(frame); !res) {
            co_return res;
        }
        auto _ = ma_device_start(&d->device);
    }

    // Ok, push the frame to the queue
    // assert(ma_device_is_started(&d->device));
    auto _ = co_await d->frameSender.send(std::move(*frame));
    co_return {};
}

auto AudioSink::initDevice(Frame *frame) -> IoResult<void> {
    auto config = ma_device_config_init(ma_device_type_playback);
    config.pUserData = d.get();
    config.sampleRate = frame->sampleRate();
    config.playback.channels = frame->channels();
    switch (frame->sampleFormat()) {
        case SampleFormat::U8: case SampleFormat::U8P: config.playback.format = ma_format_u8; break;
        case SampleFormat::S16: case SampleFormat::S16P: config.playback.format = ma_format_s16; break;
        case SampleFormat::S32: case SampleFormat::S32P: config.playback.format = ma_format_s32; break;
        case SampleFormat::FLT: case SampleFormat::FLTP: config.playback.format = ma_format_f32; break;
        default: {
            logger::error("[AudioSink] '{}' Unsupported sample format: {}", name(), toString(frame->sampleFormat()));
            return Err(Error::UnsupportedAudioFormat);
        }
    }
    config.dataCallback = [](ma_device *device, void *output, const void *input, ma_uint32 frameCount) {
        auto impl = static_cast<Impl*>(device->pUserData);
        return impl->audioCallback(device, static_cast<std::byte *>(output), static_cast<const std::byte *>(input), frameCount);
    };
    if (auto res = ma_device_init(&d->context, &config, &d->device); res != MA_SUCCESS) {
        return Err(Error::External);
    }
    d->deviceInited = true;
    return {};
}

auto AudioSink::Impl::audioCallback(ma_device *device, std::byte *output, const std::byte *input, ma_uint32 frameCount) -> void {
    while (frameCount > 0) {
        if (!currentFrame) {
            if (auto frame = frameReceiver.tryRecv(); frame) {
                currentFrameOffset = 0;
                currentFrame = std::move(*frame);
                currentPts = currentFrame->pts().value_or({});
                // logger::info("[AudioSink] Got a new frame with {} samples, pts {}", currentFrame->samples(), currentPts.load());
            }
            else { // No more frames
                logger::info("[AudioSink] No more frames, fill with silence");
                break;
            }
        }

        // Begin fill it
        auto format = currentFrame->sampleFormat();
        auto perSample = bytesPerSample(format);
        auto channels = currentFrame->channels();
        auto samples = currentFrame->samples();

        auto numToCopy = std::min(samples - currentFrameOffset, size_t {frameCount});
        auto bytesToCopy = numToCopy * perSample * channels;
        if (isPlanarFormat(format)) { // Current is planar
            for (size_t i = 0; i < numToCopy; ++i) {
                for (size_t j = 0; j < channels; ++j) {
                    auto dst = output + (i * channels + j) * perSample;
                    auto src = static_cast<const std::byte *>(currentFrame->data(j)) + (currentFrameOffset + i) * perSample;
                    switch (format) {
                        case SampleFormat::U8P: *reinterpret_cast<uint8_t *>(dst) = *reinterpret_cast<const uint8_t *>(src); break;
                        case SampleFormat::S16P: *reinterpret_cast<int16_t *>(dst) = *reinterpret_cast<const int16_t *>(src); break;
                        case SampleFormat::S32P: *reinterpret_cast<int32_t *>(dst) = *reinterpret_cast<const int32_t *>(src); break;
                        case SampleFormat::FLTP: *reinterpret_cast<float *>(dst) = *reinterpret_cast<const float *>(src); break;
                        default: assert(false); // should not happen
                    }
                }
            }
        }
        else { // packed
            auto src = static_cast<const std::byte *>(currentFrame->data(0)) + currentFrameOffset * perSample * channels;
            ::memcpy(output, src, bytesToCopy);
        }

        output += bytesToCopy;
        frameCount -= numToCopy;
        currentFrameOffset += numToCopy;
        if (currentFrameOffset == samples) {
            currentFrame.reset();
        }
    }
    
    if (frameCount > 0) {
        ::memset(output, 0, ma_get_bytes_per_frame(device->playback.format, device->playback.channels) * frameCount);
    }
}


} // namespace nekoav