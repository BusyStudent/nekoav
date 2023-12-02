#define _NEKO_SOURCE
#include "../elements/audiocvt.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../pad.hpp"
#include "common.hpp"
#include "ffmpeg.hpp"

NEKO_NS_BEGIN

namespace FFmpeg {

class FFAudioConverterImpl final : public Template::GetImpl<AudioConverter> {
public:
    FFAudioConverterImpl() {
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
        mSinkPad->setCallback(std::bind(&FFAudioConverterImpl::processInput, this, std::placeholders::_1));
        mSinkPad->setEventCallback(std::bind(&Pad::pushEvent, mSourcePad, std::placeholders::_1));
    }
    Error onInitialize() override {
        return Error::Ok;
    }
    Error onTeardown() override {
        swr_free(&mCtxt);
        mPassthrough = false;
        mSwrFormat = AV_SAMPLE_FMT_NONE;
        return Error::Ok;
    }
    Error processInput(ResourceView resourceView) {
        auto frame = resourceView.viewAs<Frame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mSourcePad->isLinked()) {
            return Error::NoLink;
        }
        if (!mCtxt && !mPassthrough) {
            if (auto err = initContext(frame->get()); err != Error::Ok) {
                return err;
            }
        }
        if (mPassthrough) {
            mSourcePad->push(resourceView);
            return Error::Ok;
        }

        // Alloc frame and data
        auto dstFrame = av_frame_alloc();
        dstFrame->format = mSwrFormat;
        dstFrame->channel_layout = frame->get()->channel_layout;
        dstFrame->sample_rate = frame->get()->sample_rate;

        auto ret = swr_convert_frame(mCtxt, dstFrame, frame->get());
        if (ret < 0) {
            av_frame_free(&dstFrame);
            return Error::UnsupportedFormat;
        }

        // Copy metadata
        av_frame_copy_props(dstFrame, frame->get());

        return mSourcePad->push(Frame::make(dstFrame, frame->timebase(), AVMEDIA_TYPE_AUDIO).get());
    }
    Error initContext(AVFrame *frame) {
        AVSampleFormat fmt;
        auto &spfmt = mSourcePad->next()->property(Properties::SampleFormatList);
        if (spfmt.isNull()) {
            mPassthrough = true;
            return Error::Ok;
        }
        for (const auto &v : spfmt.toList()) {
            fmt = ToAVSampleFormat(v.toEnum<SampleFormat>());
            if (fmt == AVSampleFormat(frame->format)) {
                mPassthrough = true;
                return Error::Ok;
            }
        }

        // Make a convert context
        mCtxt = swr_alloc_set_opts(
            nullptr,
            frame->channel_layout,
            fmt,
            frame->sample_rate,
            frame->channel_layout,
            AVSampleFormat(frame->format),
            frame->sample_rate,
            0,
            nullptr
        );

        if (!mCtxt) {
            return Error::OutOfMemory;
        }
        if (swr_init(mCtxt) < 0) {
            return Error::Unknown;
        }
        mSwrFormat = fmt;
        return Error::Ok;
    }
    // void setSampleFormat(SampleFormat fmt) override {

    // }
private:
    AVSampleFormat mSwrFormat = AV_SAMPLE_FMT_NONE;
    bool        mPassthrough = false;
    SwrContext *mCtxt = nullptr;
    Pad        *mSinkPad = nullptr;
    Pad        *mSourcePad = nullptr;

};

NEKO_REGISTER_ELEMENT(AudioConverter, FFAudioConverterImpl);

}

NEKO_NS_END