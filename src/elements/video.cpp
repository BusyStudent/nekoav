#include <nekoav/elements/video.hpp>
#include <ranges>
#include "ffmpeg.hpp"

#if defined(_WIN32)
    #include <d3d11.h>
    #include <d2d1.h>
#endif // _WIN32

extern "C" {
    #include <libavutil/frame.h>
    #include <libavutil/pixdesc.h>
    #include <libswscale/swscale.h>
}

namespace nekoav {

namespace {

auto getCopybackFormats(AVFrame *frame) -> std::vector<AVPixelFormat> {
    std::vector<AVPixelFormat> vec {};
    AVPixelFormat *tmp = nullptr;
    if (av_hwframe_transfer_get_formats(frame->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &tmp, 0) >= 0) {
        size_t n = 0;
        for (n = 0; tmp[n] != AV_PIX_FMT_NONE; ++n) {} // Get the number of formats
        vec.assign(tmp, tmp + n);
        av_freep(&tmp);
    }
    return vec;
}

} // namespace

// MARK: NullVideoRenderer
auto NullVideoRenderer::init() -> IoTask<void> {
    co_return {};
}

auto NullVideoRenderer::render(VideoFrame frame) -> IoTask<void> {
    NEKOAV_INFO("[NullVideoRenderer] Render frame: {}", frame.pts().value_or(Timestamp {}));
    co_return {};
}

auto NullVideoRenderer::shutdown() -> IoTask<void> {
    co_return {};
}

auto NullVideoRenderer::pixelFormats() const -> std::vector<PixelFormat> {
    return std::vector {
        PixelFormat::RGBA,

        // All YUV familes
        PixelFormat::YUV410P,
        PixelFormat::YUV411P,
        PixelFormat::YUV420P,
        PixelFormat::YUV422P,
        PixelFormat::YUV440P,
        PixelFormat::YUV444P,

        // All YUV packed
        PixelFormat::NV12,
        PixelFormat::NV21,
        PixelFormat::NV16,
        PixelFormat::NV24,
        PixelFormat::YUYV422,
        PixelFormat::UYVY422,

        // All hardware
        PixelFormat::DXVA2_VLD,
        PixelFormat::D3D11,
        PixelFormat::D3D12,
        PixelFormat::VAAPI,
        PixelFormat::VDPAU,
        PixelFormat::CUDA,
        PixelFormat::QSV,
        PixelFormat::OPENCL,
    };
}

// MARK: VideoSink
struct VideoSink::Impl {
    Timestamp renderDuration {};
};

VideoSink::VideoSink(std::string_view name) : Sink(name), mInput(createInputPad("in")) {
    mInput.setPushCallback<&VideoSink::onPadPush>(this);
    mInput.setQueryCallback<&VideoSink::onPadQuery>(this);
    mInput.mutableCaps().insertOrAssign(Caps::VideoRaw, Value::Map {});
}

VideoSink::~VideoSink() {
    
}

auto VideoSink::setRenderer(VideoRenderer::Ptr renderer) -> void {
    mRenderer.swap(renderer);
}

auto VideoSink::onStop() -> IoTask<void> {
    if (auto res = co_await mRenderer->shutdown(); !res) {
        NEKOAV_WARN("Failed to shutdown the renderer: {}", res.error().message());
    }
    // Set to empty caps
    mInput.mutableCaps().insertOrAssign(Caps::VideoRaw, Value::Map {});
    d.reset();
    co_return {};
}

auto VideoSink::onPrepare() -> IoTask<void> {
    if (!mRenderer) {
        // Try to find the renderer from the context
        if (auto ctxt = context(); ctxt) {
            mRenderer = ctxt->find<VideoRenderer>();
        }
        if (!mRenderer) {
            NEKOAV_WARN("[VideoSink] '{}', no renderer found, using NullVideoRenderer as fallback", name());
            mRenderer = std::make_unique<NullVideoRenderer>();
        }
    }
    if (auto res = co_await mRenderer->init(); !res) {
        co_return Err(res.error());
    }

    // Build the caps
    auto values = Value::Map {};
    auto list = Value::List {};
    for (auto &fmt : mRenderer->pixelFormats()) {
        list.emplace_back(fmt);
    }
    values.emplace(Caps::PixelFormat, std::move(list));

    // Set it to the input pad
    mInput.mutableCaps().insertOrAssign(Caps::VideoRaw, std::move(values));
    d = std::make_unique<Impl>();
    co_return {};
}

auto VideoSink::onPadPush(Pad &, Sample sample) -> IoTask<void> {
    using namespace std::literals;

    if (!sample) { // EOF
        postMessage(Message::EndOfStream {
            .element = shared_from_this(),
        });
        NEKOAV_INFO("[VideoSink] '{}', End of stream", name());
        co_return {};
    }
    if (!sample.isVideoFrame()) {
        co_return Err(Error::SampleTypeNotSupported);
    }
    auto frame = sample.toVideoFrame();
    auto pts = frame->pts().value_or(Timestamp {});
    auto time = clock()->time(); // Get the master clock time

    // Sync here
    if (pts > time) {
        auto waitTime = pts - time;
        if (waitTime > 1ms && waitTime < 500ms) {
            NEKOAV_DEBUG("[VideoSink] Waiting for {}", std::chrono::duration_cast<std::chrono::milliseconds>(waitTime));
            co_await ilias::sleep(waitTime);
        }
        if (waitTime > 500ms) { // What, we are too fast?
            NEKOAV_WARN("Frame is too far ahead: {}ns", waitTime.count());
            co_return {};
        }
    }
    else {
        auto lateTime = time - pts;
        if (lateTime >= 100ms) { // Drop
            NEKOAV_WARN("Dropping late frame: {}ns", lateTime.count());
            co_return {};
        }
    }

    co_return co_await mRenderer->render(std::move(*frame));
}

auto VideoSink::onPadQuery(Pad &pad, Query query) -> std::optional<Reply> {
    if (query.isCaps()) {
        return Reply::Caps { .caps = pad.caps() };
    }
    return std::nullopt;
}

// MARK: VideoConverter
struct VideoConverter::Impl {
    // Common part
    AVPixelFormat dstFormat = AV_PIX_FMT_NONE;

    // Field for swsScale
    SwsContext   *swsCtxt = nullptr;
    AVFrame      *swsFrame = nullptr;
    bool          passThrough = false; // If true, don't convert the frame
    bool          swsScale = false; // If true, use sws_scale to convert the frame

    // Field for GPU
    bool          copyback = false; // If true, copy the gpu frame back to cpu
    AVFrame      *backFrame = nullptr; // The frame that will be copied back to cpu
    AVPixelFormat backFormat = AV_PIX_FMT_NONE; // The format of the frame that will be copied back to cpu

    Impl() {
        swsFrame = av_frame_alloc();
        backFrame = av_frame_alloc();
    }

    ~Impl() {
        sws_freeContext(swsCtxt);
        av_frame_free(&swsFrame);
        av_frame_free(&backFrame);
    }
};

VideoConverter::VideoConverter(std::string_view name) : 
    Transform(name), 
    mInput(createInputPad("in")), 
    mOutput(createOutputPad("out"))
{
    mInput.setPushCallback<&VideoConverter::onPush>(this);
}

VideoConverter::~VideoConverter() {
    assert(!d);
}

auto VideoConverter::onStop() -> IoTask<void> {
    d.reset();
    co_return {};
}

auto VideoConverter::onPush(Pad &, Sample sample) -> IoTask<void> {
    if (!sample) { // Forward EOF
        co_return co_await mOutput.push(std::move(sample));
    }
    if (!sample.isVideoFrame()) {
        co_return Err(Error::SampleTypeNotSupported);
    }
    auto frame = sample.toVideoFrame();
    if (!d) { // Lazy init
        if (auto res = init(frame); !res) {
            co_return Err(res.error());
        }
    }
    if (d->passThrough) {
        co_return co_await mOutput.push(std::move(sample));
    }

    // Do conversion
    AVFrame *dstFrame = nullptr;
    auto res = co_await ilias::blocking([&]() {
        auto inFrame = frame->get();
        if (d->copyback) {
            d->backFrame->format = d->backFormat;
            if (auto res = av_hwframe_transfer_data(d->backFrame, inFrame, 0); res < 0) {
                return res;
            }
            inFrame = d->backFrame;
            dstFrame = d->backFrame;
        }
        if (d->swsScale) {
            if (auto res = sws_scale_frame(d->swsCtxt, d->swsFrame, inFrame); res < 0) {
                return res;
            }
            dstFrame = d->swsFrame;
        }
        return 0;
    });
    if (res < 0) {
        co_return Err(error::fromFFmpeg(res));
    }

    // Move it
    auto output = av_frame_alloc();
    av_frame_move_ref(output, dstFrame);
    av_frame_copy_props(output, frame->get());

    // Send it
    co_return co_await mOutput.push(VideoFrame {output, frame->timeBase()});
}

auto VideoConverter::init(VideoFrame *frame) -> IoResult<void> {
    if (d) {
        return {};
    }
    // Get the downstream format
    auto reply = mOutput.sendQuery(Query::Caps {});
    if (!reply) {
        return Err(Error::InvalidTopology);
    }
    auto [caps] = reply->toCaps();
    auto video = caps.find(Caps::VideoRaw);
    if (!video.isMap()) { // Not found
        NEKOAV_WARN("[VideoConverter] '{}' No downstream video caps found", name());
        return Err(Error::InvalidTopology);
    }

    // Begin readthe properties
    // Get the width and height (if unspecified, use the frame's width, useful when the downstream only need convert the format)
    auto width = video[Caps::Width].visit(Overloads {
        [&](Value::Integer w) { return int(w); },   // If width is specified, use it
        [&](auto &) { return frame->width(); }      // any?
    });
    
    auto height = video[Caps::Height].visit(Overloads {
        [&](Value::Integer h) { return int(h); }, // If height is specified, use it
        [&](auto &) { return frame->height(); }   // any?
    });

    auto fmts = video[Caps::PixelFormat].visit(Overloads {
        [&](const Value::List &list) {
            std::vector<AVPixelFormat> vec;
            for (auto &item : list) {
                vec.emplace_back(pixfmt::toFFmpeg(item.toPixelFormat()));
            }
            return vec;
        },
        [&](PixelFormat fmt) {
            std::vector<AVPixelFormat> v = {pixfmt::toFFmpeg(fmt)};
            return v;
        },
        [&](auto &) { // Other, empty...
            std::vector<AVPixelFormat> v;
            return v;
        },
    });

    // Check....
    auto ffmt = pixfmt::toFFmpeg(frame->pixelFormat());
    if (width == frame->width() && height == frame->height()) {
        if (fmts.empty() || std::ranges::find(fmts, ffmt) != fmts.end()) { // Not Require fmt or Format matches
            NEKOAV_INFO("[VideoConverter] Passthrough enabled");
            d = std::make_unique<Impl>();
            d->passThrough = true;
            return {};
        }
    }
    
    // Need to convert
    auto inFrame = frame->get();
    auto dstFormat = fmts.empty() ? ffmt : fmts.front();
    auto impl = std::make_unique<Impl>();
    impl->copyback = av_pix_fmt_desc_get(ffmt)->flags & AV_PIX_FMT_FLAG_HWACCEL;

    // Need copyback
    if (impl->copyback) {
        auto copybackFormats = getCopybackFormats(inFrame);
        if (copybackFormats.empty()) {
            NEKOAV_WARN("[VideoConverter] '{}' No copyback formats found", name());
            return Err(Error::External);
        }
        // Find the best format
        auto it = std::ranges::find(copybackFormats, dstFormat);
        auto fmt = it == copybackFormats.end() ? copybackFormats.front() : *it;
        impl->backFrame->format = fmt;
        impl->backFormat = fmt;

        // Try coypback to get the width & height
        if (auto res = av_hwframe_transfer_data(impl->backFrame, inFrame, 0); res < 0) {
            return Err(error::fromFFmpeg(res));
        }
        NEKOAV_INFO("[VideoConverter] '{}' Copyback to {}x{} format {}", name(), impl->backFrame->width, impl->backFrame->height, av_get_pix_fmt_name(fmt));
        inFrame = impl->backFrame;
    }

    // Need sws_scale
    if (inFrame->width != width || inFrame->height != height || inFrame->format != dstFormat) {
        impl->swsScale = true;
        impl->swsCtxt = sws_getContext(
            inFrame->width, inFrame->height, static_cast<AVPixelFormat>(inFrame->format),
            width, height, dstFormat,
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );
        if (!impl->swsCtxt) { // ?
            return Err(Error::OutOfMemory);
        }
        NEKOAV_INFO("[VideoConverter] '{}' Sws scale to {}x{} format {}", name(), width, height, av_get_pix_fmt_name(dstFormat));
    }

    impl->dstFormat = dstFormat;
    d.swap(impl);
    return {};
}


} // namespace nekoav
