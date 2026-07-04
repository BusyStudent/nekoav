#include <nekoav/elements/audio.hpp>
#include <nekoav/clock.hpp>
#include <ilias/sync/mpsc.hpp>
#include <queue>
#include <mutex>
#include "ffmpeg.hpp"

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

namespace {

class AudioContextImpl final : public AudioContext {
public:
    AudioContextImpl() {
        ma_result result = ma_context_init(NULL, 0, NULL, &context);
        if (result != MA_SUCCESS) {
            NEKOAV_THROW(std::runtime_error{"Failed to initialize miniaudio context"});
        }
    }
    ~AudioContextImpl() {
        ma_context_uninit(&context);
    }

    // Method
    auto backend() const -> std::string_view override {
        return "miniaudio";
    }

    // Utils
    auto get() -> ma_context * {
        return &context;
    }

    ma_context context {};
};

} // namespace

auto AudioContext::make() -> std::shared_ptr<AudioContext> {
    return std::make_shared<AudioContextImpl>();
}

// MARK: AudioSink
class AudioSink::Impl final : public Clock { // Implementation the ClockSource
public:
    // Self
    AudioSink  *self = nullptr;

    // Audio Context and Device
    std::shared_ptr<AudioContextImpl> context;
    std::optional<ma_device>          device;

    // Access by the Callback, multithreaded
    struct {
        // Send the Frame to the device callback
        ilias::mpsc::Sender<AudioFrame>   frameSender {};
        ilias::mpsc::Receiver<AudioFrame> frameReceiver {};
        std::optional<AudioFrame>         currentFrame {};
        size_t                            currentFrameOffset = 0;
        std::atomic<int64_t>              currentPts {}; // int64_t on Timestamp unit
        std::atomic<bool>                 endOfStream {false};
        Timestamp                         currentPtsInternal; // The pts of the current frame
    } callback;

    Impl() {
        auto [sender, receiver] = ilias::mpsc::channel<AudioFrame>(20); // Cache 20 frames ?, I think it's enough
        callback.frameSender = std::move(sender);
        callback.frameReceiver = std::move(receiver);
    }

    ~Impl() {
        if (device) {
            ma_device_uninit(&*device);
        }
    }

    // Clock Interface
    auto time() const -> Timestamp override {
        return Timestamp { callback.currentPts.load() };
    }

    auto category() const -> ClockCategory override {
        return ClockCategory::Audio;
    }

    // Other...
    auto audioCallback(ma_device *device, std::byte *output, const std::byte *input, ma_uint32 frameCount) -> void;
    auto audioUpdateClock() -> void;
    auto audioNotifyEOS() -> void;
};

AudioSink::AudioSink(std::string_view name) : Sink(name), mInput(createInputPad("in")) {
    mInput.setPushCallback<&AudioSink::onPush>(this);
    mInput.setEventCallback<&AudioSink::onEvent>(this);
}

AudioSink::~AudioSink() {
    assert(!d);
}

auto AudioSink::sendQuery(Query query) -> std::optional<Reply> {
    if (!query.isClockSource() || !d) {
        return std::nullopt;
    }
    return Reply::ClockSource {
        .clock = Clock::Ptr { shared_from_this(), d.get() }
    };
}

auto AudioSink::onPrepare() -> IoTask<void> {
    auto impl = std::make_unique<Impl>();
    impl->self = this;

    // Query the global audio context
    if (auto ctxt = context(); ctxt) {
        auto found = ctxt->find<AudioContext>();
        impl->context = std::static_pointer_cast<AudioContextImpl>(found);
    }
    if (!impl->context) { // We can't get valid one, create a new one
        impl->context = std::make_shared<AudioContextImpl>();
    }

    // Try to enumerate devices and get the formats
    ma_device_info *info = nullptr;
    ma_uint32       count = 0;
    if (auto res = ma_context_get_devices(impl->context->get(), &info, &count, nullptr, nullptr); res != MA_SUCCESS) {
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
    // NOTE: Pipeline will discard the clock when stopping
    // SO, It is safe
    d.reset();
    co_return {};
}

auto AudioSink::onPause() -> IoTask<void> {
    if (d->device) {
        ma_device_stop(&*d->device);
    }
    co_return {};
}

auto AudioSink::onRun() -> IoTask<void> {
    if (d->device) {
        ma_device_start(&*d->device);
    }
    co_return {};
}

auto AudioSink::onPush(Pad &pad, Sample sample) -> IoTask<void> {
    if (!sample) { // EOF
        d->callback.endOfStream = true; // Let the audioCallback generate the eos message to it
        co_return {};
    }
    if (!sample.isAudioFrame()) {
        co_return Err(Error::SampleTypeNotSupported);
    }
    auto frame = sample.toAudioFrame();

    // Lazy init the device
    if (!d->device) {
        if (auto res = initDevice(frame); !res) {
            co_return res;
        }
        auto _ = ma_device_start(&*d->device);
    }

    // Ok, push the frame to the queue
    // assert(ma_device_is_started(&d->device));
    auto _ = co_await d->callback.frameSender.send(std::move(*frame));
    co_return {};
}

auto AudioSink::onEvent(Pad &pad, Event event) -> IoTask<void> {
    if (!event.isFlushBegin()) {
        co_return {};
    }
    if (!d->device) { // Did we mutex with d->device?
        co_return {};
    }
    // Do flush
    // Pause the device
    bool started = ma_device_is_started(&*d->device);
    if (started) {
        ma_device_stop(&*d->device);
    }

    // Drain the queue
    while (d->callback.frameReceiver.tryRecv()) {}

    // Restore if needed
    if (started) {
        ma_device_start(&*d->device);
    }
    co_return {};
}

auto AudioSink::initDevice(AudioFrame *frame) -> IoResult<void> {
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
            NEKOAV_ERROR("[AudioSink] '{}' Unsupported sample format: {}", name(), toString(frame->sampleFormat()));
            return Err(Error::AudioFormatNotSupported);
        }
    }
    config.dataCallback = [](ma_device *device, void *output, const void *input, ma_uint32 frameCount) {
        auto impl = static_cast<Impl*>(device->pUserData);
        return impl->audioCallback(device, static_cast<std::byte *>(output), static_cast<const std::byte *>(input), frameCount);
    };
    d->device.emplace();
    if (auto res = ma_device_init(d->context->get(), &config, &*d->device); res != MA_SUCCESS) {
        d->device.reset(); // Clear it
        return Err(Error::External);
    }
    return {};
}

auto AudioSink::Impl::audioCallback(ma_device *device, std::byte *output, const std::byte *input, ma_uint32 frameCount) -> void {
    auto &state = this->callback;
    while (frameCount > 0) {
        if (!state.currentFrame) {
            if (auto frame = state.frameReceiver.tryRecv(); frame) {
                state.currentFrameOffset = 0;
                state.currentFrame = std::move(*frame);
                state.currentPtsInternal = state.currentFrame->pts().value_or(Timestamp {});
                state.currentPts = state.currentPtsInternal.count();
                // NEKOAV_INFO("[AudioSink] Got a new frame with {} samples, pts {}", currentFrame->samples(), currentPts.load());
                audioUpdateClock();
            }
            else if (auto eos = state.endOfStream.exchange(false); eos) {
                audioNotifyEOS();
                break;
            }
            else { // No more frames
                // NEKOAV_INFO("[AudioSink] No more frames, fill with silence");
                break;
            }
        }

        // Begin fill it
        auto format = state.currentFrame->sampleFormat();
        auto perSample = bytesPerSample(format);
        auto sampleRate = state.currentFrame->sampleRate();
        auto channels = state.currentFrame->channels();
        auto samples = state.currentFrame->samples();

        auto numToCopy = std::min(samples - state.currentFrameOffset, size_t {frameCount});
        auto bytesToCopy = numToCopy * perSample * channels;
        if (isPlanarFormat(format)) { // Current is planar, convert it to packed
            for (size_t i = 0; i < numToCopy; ++i) {
                for (size_t j = 0; j < channels; ++j) {
                    auto dst = output + (i * channels + j) * perSample;
                    auto src = static_cast<const std::byte *>(state.currentFrame->data(j)) + (state.currentFrameOffset + i) * perSample;
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
            auto src = static_cast<const std::byte *>(state.currentFrame->data(0)) + state.currentFrameOffset * perSample * channels;
            ::memcpy(output, src, bytesToCopy);
        }

        // Calc the time offset, advance the currentPts (timestamp is in nanoseconds)
        int64_t pts = state.currentPts.load();
        int64_t offsetTime = numToCopy * 1000'000'000 / sampleRate;
        int64_t newPts = pts + offsetTime;
        if (newPts > pts) {
            state.currentPts.store(newPts);
        }

        // Update
        output += bytesToCopy;
        frameCount -= numToCopy;
        state.currentFrameOffset += numToCopy;
        if (state.currentFrameOffset == samples) {
            state.currentFrame.reset();
        }
    }
    
    if (frameCount > 0) {
        ::memset(output, 0, ma_get_bytes_per_frame(device->playback.format, device->playback.channels) * frameCount);
    }
}

auto AudioSink::Impl::audioUpdateClock() -> void {
    auto &bus = self->pipelineBus();
    if (!bus) {
        return;
    }
    auto res = bus.trySend(Message::ClockUpdate {
        .clock = Clock::Ptr { self->shared_from_this(), this },
        .time = Timestamp { callback.currentPts.load() },
    });
    if (!res) {
        NEKOAV_ERROR("[AudioSink] Failed to send clock update event");
    }
}

auto AudioSink::Impl::audioNotifyEOS() -> void {
    NEKOAV_INFO("[AudioSink] End of stream");
    auto &bus = self->pipelineBus();
    if (!bus) {
        return;
    }
    auto res = bus.trySend(Message::EndOfStream {
        .element = self->shared_from_this(),
    });
    if (!res) {
        NEKOAV_ERROR("[AudioSink] Failed to send end of stream event");
    }
}


} // namespace nekoav