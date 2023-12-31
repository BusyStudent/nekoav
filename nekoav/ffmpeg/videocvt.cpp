#define _NEKO_SOURCE
#include "../elements/videocvt.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../log.hpp"
#include "../pad.hpp"
#include "common.hpp"
#include "ffmpeg.hpp"


NEKO_NS_BEGIN

namespace FFmpeg {

class FFVideoConverterImpl final : public Template::GetImpl<VideoConverter> {
public:
    FFVideoConverterImpl() {
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
        mSinkPad->setCallback(std::bind(&FFVideoConverterImpl::_processInput, this, std::placeholders::_1));
        mSinkPad->setEventCallback(std::bind(&Pad::pushEvent, mSourcePad, std::placeholders::_1));
    }
    // void setPixelFormat(PixelFormat format) override {
    //     mTargetFormat = format;
    // }
    Error onInitialize() override {
        return Error::Ok;
    }
    Error onTeardown() override {
        sws_freeContext(mCtxt);
        av_frame_free(&mSwFrame);
        mCopybackOnly = false;
        mPassthrough = false;
        mCopyback = false;
        mCtxt = nullptr;
        return Error::Ok;
    }
    Error _processInput(ResourceView resourceView) {
        auto frame = resourceView.viewAs<Frame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mSourcePad->isLinked()) {
            return Error::NoLink;
        }
        if (!mCtxt && !mPassthrough && !mCopybackOnly) {
            if (auto err = _initContext(frame->get()); err != Error::Ok) {
                return err;
            }
        }
        if (mPassthrough) {
            mSourcePad->push(resourceView);
            return Error::Ok;
        }

        auto srcFrame = frame->get();
        if (mCopyback) {
            NEKO_ASSERT(mSwFrame);
            int ret = av_hwframe_transfer_data(mSwFrame, srcFrame, 0);
            if (ret < 0) {
                // Failed to transfer
                return ToError(ret);
            }
            av_frame_copy_props(mSwFrame, srcFrame);
            srcFrame = mSwFrame;
        }
        if (mCopybackOnly) {
            // Do not need sws_scale
            auto dstFrame = av_frame_alloc();
            av_frame_move_ref(dstFrame, srcFrame);
            return mSourcePad->push(Frame::make(dstFrame, frame->timebase(), AVMEDIA_TYPE_VIDEO).get());
        }

        // TODO Finish it

        // Alloc frame
        auto dstFrame = av_frame_alloc();
        auto ret = 0;

        // New version of api
#if     LIBSWSCALE_VERSION_INT > AV_VERSION_INT(6, 1, 100)
        ret = sws_scale_frame(mCtxt, dstFrame, srcFrame);        
#else
        // Use old one, alloc data
        auto n = av_image_get_buffer_size(mSwsFormat, srcFrame->width, srcFrame->height, 32);
        auto buffer = av_buffer_alloc(n);
        ret = av_image_fill_arrays(
            dstFrame->data,
            dstFrame->linesize,
            buffer->data,
            mSwsFormat,
            srcFrame->width,
            srcFrame->height,
            32
        );
        if (ret < 0) {
            av_buffer_unref(&buffer);
            return Error::OutOfMemory;
        }
        dstFrame->buf[0] = buffer;
        ret = sws_scale(
            mCtxt,
            srcFrame->data,
            srcFrame->linesize,
            0,
            srcFrame->height,
            dstFrame->data,
            dstFrame->linesize
        );
        dstFrame->width = srcFrame->width;
        dstFrame->height = srcFrame->height;
        dstFrame->format = mSwsFormat;
#endif
        if (ret < 0) {
            av_frame_free(&dstFrame);
            return Error::OutOfMemory;
        }
        av_frame_copy_props(dstFrame, srcFrame);
        return mSourcePad->push(Frame::make(dstFrame, frame->timebase(), AVMEDIA_TYPE_VIDEO).get());
    }
    Error _initContext(AVFrame *f) {
        AVPixelFormat fmt = AV_PIX_FMT_RGBA;
        if (mTargetFormat == PixelFormat::None) {
            // Try get info by pad
            auto &pixfmt = mSourcePad->next()->property(Properties::PixelFormatList);
            if (pixfmt.isNull()) {
                // Unsupport, Just Passthrough
                // return Error::InvalidTopology;
                mPassthrough = true;
                return Error::Ok;
            }
            for (const auto &v : pixfmt.toList()) {
                fmt = ToAVPixelFormat(v.toEnum<PixelFormat>());
                if (fmt == AVPixelFormat(f->format)) {
                    mPassthrough = true;
                    return Error::Ok;
                }
            }
        }
        else {
            // Already has target format
            fmt = ToAVPixelFormat(mTargetFormat);
        }

        // Check need copyback
        if (IsHardwareAVPixelFormat(AVPixelFormat(f->format))) {
            mSwFrame = av_frame_alloc();
            int ret = av_hwframe_transfer_data(mSwFrame, f, 0);
            if (ret < 0) {
                return ToError(ret);
            }
            mCopyback = true;
            f = mSwFrame;

            // Check copyback pixel format supported
            auto &pixfmt = mSourcePad->next()->property(Properties::PixelFormatList);
            for (const auto &v : pixfmt.toList()) {
                fmt = ToAVPixelFormat(v.toEnum<PixelFormat>());
                if (fmt == AVPixelFormat(f->format)) {
                    mCopybackOnly = true;
                    return Error::Ok;
                }
            }
        }

        // Prepare sws conetxt
        mCtxt = sws_getContext(
            f->width,
            f->height,
            AVPixelFormat(f->format),
            f->width,
            f->height,
            fmt,
            SWS_BICUBIC,
            nullptr,
            nullptr,
            nullptr
        );
        if (!mCtxt) {
            return Error::UnsupportedPixelFormat;
        }
        mSwsFormat = fmt;
        NEKO_LOG("Init VideoConvertContext from {} to {}", 
            av_pix_fmt_desc_get(AVPixelFormat(f->format))->name,
            av_pix_fmt_desc_get(AVPixelFormat(fmt))->name
        );
        return Error::Ok;
    }
private:
    SwsContext *mCtxt = nullptr;
    AVFrame    *mSwFrame = nullptr; //< Used when hardware format
    Pad        *mSinkPad = nullptr;
    Pad        *mSourcePad = nullptr;
    bool        mPassthrough = false; //< Directly passthrough
    bool        mCopyback = false; //< Need copy back
    bool        mCopybackOnly = false; //< Do not need sws_scale
    PixelFormat mTargetFormat = PixelFormat::None; //< If not None, force
    AVPixelFormat mSwsFormat = AV_PIX_FMT_NONE; //< Current target format
};

NEKO_REGISTER_ELEMENT(VideoConverter, FFVideoConverterImpl);

}

NEKO_NS_END