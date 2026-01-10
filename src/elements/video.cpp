#include <nekoav/elements/video.hpp>
#include "internal.hpp"
#include <ranges>

extern "C" {
    #include <libavutil/frame.h>
    #include <libavutil/pixdesc.h>
    #include <libswscale/swscale.h>
}

namespace nekoav {

namespace {

static auto getCopybackFormats(AVFrame *frame) -> std::vector<AVPixelFormat> {
    std::vector<AVPixelFormat> vec {};
    AVPixelFormat *tmp = nullptr;
    if (av_hwframe_transfer_get_formats(frame->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &tmp, 0) >= 0) {
        size_t n = 0;
        for (n = 0; tmp[n] != AV_PIX_FMT_NONE; ++n) {
            
        }
        vec.assign(tmp, tmp + n);
        av_freep(&tmp);
    }
    return vec;
}

} // namespace

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
    Element(name), 
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
    if (!sample.isFrame()) {
        co_return Err(Error::UnsupportedSampleType);
    }
    auto frame = sample.toFrame();
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
    co_return co_await mOutput.push(Frame::from(output, frame->timeBase()));
}

auto VideoConverter::init(Frame *frame) -> IoResult<void> {
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
        logger::warn("[VideoConverter] '{}' No downstream video caps found", name());
        return Err(Error::InvalidTopology);
    }

    // Begin readthe properties
    auto width = std::optional<int> {};
    auto height = std::optional<int> {};
    auto fmts = std::vector<AVPixelFormat> {};

    // Get the width and height (if unspecified, use the frame's width, useful when the downstream only need convert the format)
    if (auto w = video[Caps::Width]; w.isInteger()) {
        width = w.toInteger();
    }
    else {
        width = frame->width();
    }
    if (auto h = video[Caps::Height]; h.isInteger()) {
        height = h.toInteger();
    }
    else {
        height = frame->height();
    }
    if (auto f = video[Caps::PixelFormat]; f.isList()) {
        for (auto &item : f.toList()) {
            auto fmt = item.toPixelFormat();
            fmts.emplace_back(pixfmt::toFFmpeg(fmt));
        }
    }
    else if (f.isPixelFormat()) {
        fmts.push_back(pixfmt::toFFmpeg(f.toPixelFormat()));
    }

    // Check....
    auto ffmt = pixfmt::toFFmpeg(frame->pixelFormat());
    if (width == frame->width() && height == frame->height()) {
        if (fmts.empty() || std::ranges::find(fmts, ffmt) != fmts.end()) { // Not Require fmt or Format matches
            logger::info("[VideoConverter] Passthrough enabled");
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
            logger::warn("[VideoConverter] '{}' No copyback formats found", name());
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
        logger::info("[VideoConverter] '{}' Copyback to {}x{} format {}", name(), impl->backFrame->width, impl->backFrame->height, av_get_pix_fmt_name(fmt));
        inFrame = impl->backFrame;
    }

    // Need sws_scale
    if (inFrame->width != width || inFrame->height != height || inFrame->format != dstFormat) {
        impl->swsScale = true;
        impl->swsCtxt = sws_getContext(
            inFrame->width, inFrame->height, static_cast<AVPixelFormat>(inFrame->format),
            width.value(), height.value(), dstFormat,
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );
        if (!impl->swsCtxt) { // ?
            return Err(Error::OutOfMemory);
        }
        logger::info("[VideoConverter] '{}' Sws scale to {}x{} format {}", name(), width.value(), height.value(), av_get_pix_fmt_name(dstFormat));
    }

    impl->dstFormat = dstFormat;
    d.swap(impl);
    return {};
}


} // namespace nekoav
