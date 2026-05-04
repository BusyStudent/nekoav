#include <nekoav/elements/url_source.hpp>
#include <nekoav/element.hpp>
#include <nekoav/format.hpp>
#include <nekoav/sample.hpp>
#include <nekoav/caps.hpp>
#include <ilias/task.hpp>
#include <ilias/sync.hpp>
#include <unordered_map>
#include <stop_token>
#include <ranges>
#include "internal.hpp"

namespace nekoav {

struct UrlSource::Impl {
    // FFmpeg
    AVFormatContext               *ctxt = nullptr;
    std::unordered_map<int, Pad *> padsMapping; // Mapping ffmpeg stream index to pad
    std::atomic<int>               interrupted {0}; // Read for interrupted handler

    // Seem
    std::optional<Timestamp>       seekTime;

    // Worker
    ilias::WaitHandle<void>        readWorker;
    ilias::Event                   runningEvent;
    ilias::Event                   seekEvent;
};

UrlSource::UrlSource(std::string_view name) : Element(name) {}

UrlSource::~UrlSource() {
    assert(!d); // Should be cleanup on onTeardown
}

auto UrlSource::onInitialize() -> IoTask<void> {
    d = std::make_unique<Impl>();
    co_return {};
}

auto UrlSource::onTeardown() -> IoTask<void> {
    assert(d);

    // Stop the worker
    d->interrupted.store(1);
    if (d->readWorker) {
        d->readWorker.stop();
        co_await std::exchange(d->readWorker, {});
    }

    // Clear the output & notify it
    outputs().clear();

    // The avformat_close_input may blocking?
    co_await ilias::blocking([&]() {
        avformat_close_input(&d->ctxt);
    });
    d.reset();
    co_return {};
}

auto UrlSource::onPrepare() -> IoTask<void> {
    // Because the ffmpeg is blocking, run it on thread pool
    auto res = co_await ilias::blocking([&]() {
        d->ctxt = avformat_alloc_context();
        d->ctxt->interrupt_callback.opaque = this;
        d->ctxt->interrupt_callback.callback = [](void *self) -> int {
            return static_cast<UrlSource*>(self)->interruptCallback();
        };

        // Do open
        if (auto res = avformat_open_input(&d->ctxt, mUrl.c_str(), nullptr, nullptr); res != 0) {
            return res;
        }
        if (auto res = avformat_find_stream_info(d->ctxt, nullptr); res != 0) {
            return res;
        }
        return 0;
    });
    if (res != 0) {
        // Error happen
        logger::error("[UrlSource] '{}' failed to open url: {} => {}", name(), mUrl, error::toString(res));
        co_return Err(error::fromFFmpeg(res));
    }

#if !defined(NDEBUG)
    av_dump_format(d->ctxt, 0, mUrl.c_str(), 0);
#endif // NDEBUG

    // Adding pad to streams
    size_t audioIdx = 0;
    size_t videoIdx = 0;
    size_t subtitleIdx = 0;
    for (size_t idx = 0; idx < d->ctxt->nb_streams; ++idx) {
        auto stream = d->ctxt->streams[idx];
        auto type = stream->codecpar->codec_type;

        Pad *pad = nullptr;
        switch (type) {
            case AVMEDIA_TYPE_VIDEO: {
                auto extraData = reinterpret_cast<std::byte*>(stream->codecpar->extradata);
                auto info = Value::fromMap({
                    { std::string{Caps::Width},    stream->codecpar->width },
                    { std::string{Caps::Height},   stream->codecpar->height },
                    { std::string{Caps::Codec},    avcodec_get_name(stream->codecpar->codec_id) },
                    { std::string{Caps::CodecTag}, static_cast<int64_t>(stream->codecpar->codec_tag) },
                    { std::string{Caps::CodecExtraData}, std::vector<std::byte>{extraData, extraData + stream->codecpar->extradata_size} },
                    { std::string{Caps::PixelFormat}, pixfmt::fromFFmpeg(AVPixelFormat(stream->codecpar->format)) },
                    { std::string{Caps::ColorRange}, color_range::fromFFmpeg(stream->codecpar->color_range) },
                    { std::string{Caps::ColorPrimaries}, color_primaries::fromFFmpeg(stream->codecpar->color_primaries) },
                    { std::string{Caps::ColorTransfer}, color_transfer::fromFFmpeg(stream->codecpar->color_trc) },
                    { std::string{Caps::ColorSpace}, color_space::fromFFmpeg(stream->codecpar->color_space) },
                    { std::string{Caps::Bitrate}, stream->codecpar->bit_rate },
                });
                pad = &createOutputPad("video/" + std::to_string(videoIdx++));
                pad->mutableCaps().insert(Caps::VideoPacket, std::move(info));
                break;
            }
            case AVMEDIA_TYPE_AUDIO: {
                auto extraData = reinterpret_cast<std::byte*>(stream->codecpar->extradata);
                auto info = Value::fromMap({
                    { std::string{Caps::SampleRate},  stream->codecpar->sample_rate },
                    { std::string{Caps::Channels},    stream->codecpar->ch_layout.nb_channels },
                    { std::string{Caps::Codec},       avcodec_get_name(stream->codecpar->codec_id) },
                    { std::string{Caps::CodecTag}, static_cast<int64_t>(stream->codecpar->codec_tag) },
                    { std::string{Caps::CodecExtraData}, std::vector<std::byte>{extraData, extraData + stream->codecpar->extradata_size} },
                    { std::string{Caps::SampleFormat}, sample_fmt::fromFFmpeg(AVSampleFormat(stream->codecpar->format)) },
                    { std::string{Caps::Bitrate}, stream->codecpar->bit_rate },
                });
                pad = &createOutputPad("audio/" + std::to_string(audioIdx++));
                pad->mutableCaps().insert(Caps::AudioPacket, std::move(info));
                break;
            }
            case AVMEDIA_TYPE_SUBTITLE: {
                pad = &createOutputPad("subtitle/" + std::to_string(subtitleIdx++));
                break;
            }
            default: continue;
        }

        // Binding
        d->padsMapping[idx] = pad;
        pad->setEventCallback<&UrlSource::onPadEvent>(this);
        pad->setQueryCallback<&UrlSource::onPadQuery>(this);
    }

    // Notify the bus, media is ready
    if (auto bus = pipelineBus(); bus) {
        auto _ = co_await bus.send(Event::MediaLoaded {
            .startTime = time::fromFFmpeg(d->ctxt->start_time, AV_TIME_BASE_Q),
            .duration = time::fromFFmpeg(d->ctxt->duration, AV_TIME_BASE_Q),
        });
    }

    // Start the worker
    d->readWorker = ilias::spawn(readWorker());
    
    // Done
    co_return {};
}

auto UrlSource::onRun() -> IoTask<void> {
    assert(d);
    d->runningEvent.set();
    co_return {};
}

auto UrlSource::onPause() -> IoTask<void> {
    assert(d);
    d->runningEvent.clear();
    co_return {};
}

auto UrlSource::onPadEvent(Pad &pad, const Event &event) -> IoTask<void> {
    co_return {};
}

auto UrlSource::onPadQuery(Pad &pad, const Query &query) -> std::optional<Reply> {
    if (query.isDuration()) { // QueryDuration
        if (!d) {
            return std::nullopt;
        }
        return Reply::Duration { time::fromFFmpeg(d->ctxt->duration, AV_TIME_BASE_Q) };
    }
    if (query.isCaps()) { // QueryCaps
        return Reply::Caps { pad.caps() };
    }
    return std::nullopt;
}

auto UrlSource::readWorker() -> Task<void> {
    logger::debug("[UrlSource] '{}' read worker started", name());

    // Packet
    auto packet = av_packet_alloc();
    struct Guard {
        ~Guard() {
            av_packet_unref(packet);
            av_packet_free(&packet);

            logger::info("[UrlSource] '{}' read worker stopped", self->name());
        }

        AVPacket *packet;
        UrlSource *self;
    } guard { packet, this };

    while (true) {
        if (d->seekEvent.isSet() && d->seekTime) {
            d->seekEvent.clear();

            if (auto res = co_await ilias::unstoppable(doSeek()); !res) {
                logger::error("[UrlSource] '{}' seek failed: {}", name(), res.error().message());
                setErrorState(res.error());
                co_return;
            }
        }

        // Only run on the running state
        co_await d->runningEvent;
        
        // Do the job
        av_packet_unref(packet);
        int res = co_await ilias::blocking([&]() { return av_read_frame(d->ctxt, packet); });
        switch (res) {
            case 0: break; // OK

            case AVERROR_EXIT: co_return; // interruptCallback was request exit 
            case AVERROR_EOF: { // We are done, push the eof to all the pads
                for (auto &pad : outputs()) {
                    if (!pad.isLinked()) {
                        continue;
                    }
                    if (auto res = co_await pad.push(nullptr); !res) {
                        setErrorState(res.error());
                        co_return;
                    }
                }
                co_await d->seekEvent; // only wait for seek event
                continue;
            }

            // TODO: Need handle
            default: {
                logger::error("[UrlSource] '{}' read frame failed: {} => {}", name(), res, error::toString(res));
                setErrorState(error::fromFFmpeg(res));
                co_return;
            }
        }

        // Find the pad
        auto it = d->padsMapping.find(packet->stream_index);
        if (it == d->padsMapping.end()) {
            continue;
        }
        auto [_, pad] = *it;
        auto stream = d->ctxt->streams[packet->stream_index];
        auto timeBase = Rational { .num = stream->time_base.num, .den = stream->time_base.den };
        if (!pad->isLinked()) {
            continue;
        }
        
        // Alloc the packet, push to the pad
        auto pak = av_packet_alloc();
        av_packet_move_ref(pak, packet);

        auto sample = Packet::from(pak, timeBase);
        if (auto res = co_await ilias::unstoppable(pad->push(std::move(sample))); !res) {
            logger::error("[UrlSource] '{}' push {} packet failed: {}", name(), pad->name(), res.error().message());
            co_return;
        }
    }
}

auto UrlSource::sendEvent(Event event) -> IoTask<void> {
    if (event.isSeek()) {
        auto seek = event.toSeek();
        d->seekTime = seek.timestamp;
        d->seekEvent.set();
    }
    co_return {};
}

auto UrlSource::doSeek() -> IoTask<void> {
    assert(d->seekTime);
    logger::info("[UrlSource] '{}' seek to {}", name(), d->seekTime.value());

    // Seek all stream to the same time
    auto ts = d->seekTime.value();
    auto fftime = time::toFFmpeg(ts, AV_TIME_BASE_Q);
    auto res = co_await ilias::blocking([&]() {
        return av_seek_frame(d->ctxt, -1, fftime, AVSEEK_FLAG_BACKWARD);
    });
    if (res != 0) {
        co_return Err(error::fromFFmpeg(res));
    }

    // Notify all the downstream that flush the buffer
    for (auto &pad : outputs()) {
        auto _ = co_await pad.pushEvent(Event::FlushBegin {});
    }
    for (auto &pad : outputs()) {
        auto _ = co_await pad.pushEvent(Event::FlushEnd {});
    }
    logger::info("[UrlSource] '{}' seek done", name());
    co_return {};
}

auto UrlSource::interruptCallback() -> int {
    return d->interrupted.load();
}

auto UrlSource::setUrl(std::string_view url) -> void {
    mUrl = url;
}

auto UrlSource::videoOutputs() -> std::vector<Pad *> {
    auto vec = std::vector<Pad *> {};
    for (auto &pad : std::views::filter(outputs(), [](auto &pad) { return pad.name().starts_with("video/"); })) {
        vec.push_back(&pad);
    }
    return vec;
}

auto UrlSource::audioOutputs() -> std::vector<Pad *> {
    auto vec = std::vector<Pad *> {};
    for (auto &pad : std::views::filter(outputs(), [](auto &pad) { return pad.name().starts_with("audio/"); })) {
        vec.push_back(&pad);
    }
    return vec;
}

auto UrlSource::subtitleOutputs() -> std::vector<Pad *> {
    auto vec = std::vector<Pad *> {};
    for (auto &pad : std::views::filter(outputs(), [](auto &pad) { return pad.name().starts_with("subtitle/"); })) {
        vec.push_back(&pad);
    }
    return vec;
}

} // namespace nekoav