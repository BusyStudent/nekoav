#define _NEKO_SOURCE
#include "factory_impl.hpp"
#include "ffmpeg.hpp"
#include "../log.hpp"
#include "../media.hpp"
#include "../time.hpp"
#include <queue>
#include <mutex>
#include <map>

extern "C" {
    #include <libavutil/imgutils.h>
    #include <libswscale/version.h>
}

NEKO_NS_BEGIN

namespace {

class FFFrame final : public MediaFrame {
public:
    explicit FFFrame(AVFrame* frame, AVRational t) : mFrame(frame), mTimebase(t) {

    }
    ~FFFrame() {
        std::lock_guard locker(mMutex);
        mFrame.reset();
    }

    AVFrame *get() const noexcept {
        return mFrame.get();
    }

    // Impl MediaFrame
    void lock() override {
        return mMutex.lock();        
    }
    void unlock() override {
        return mMutex.unlock();
    }
    int  format() const override {
        return mFrame->format;
    }
    double timestamp() const override {
        // Because AVFrame's time_base is currently unused, so we have to carray it by ourself
        // return mFrame->pts * av_q2d(mFrame->time_base);
        if (mFrame->pts == AV_NOPTS_VALUE) {
            return 0;
        }
        return mFrame->pts * av_q2d(mTimebase);
    }
    double duration() const override {
        // return mFrame->pkt_duration * av_q2d(mFrame->time_base);
        if (mFrame->pkt_duration == AV_NOPTS_VALUE) {
            return 0;
        }
        return mFrame->pkt_duration * av_q2d(mTimebase);
    }
    bool isKeyFrame() const override {
        return mFrame->key_frame;
    }

    int linesize(int idx) const override {
        if (idx >= 8 || idx < 0) {
            return 0;
        }
        return mFrame->linesize[idx];
    }
    void *data(int idx) const override {
        if (idx >= 8 || idx < 0) {
            return nullptr;
        }
        return mFrame->data[idx];
    }

    int query(Query q) const override {
        switch (q) {
            case Query::Channels: return mFrame->channels;
            case Query::SampleRate: return mFrame->sample_rate;
            case Query::SampleCount: return mFrame->nb_samples;
            case Query::Height: return mFrame->height;
            case Query::Width: return mFrame->width;
            default: return 0;
        }
    }
    AVRational timebase() const noexcept {
        return mTimebase;
    }

    static auto make(AVFrame *f, AVRational timebase) {
        return make_shared<FFFrame>(f, timebase);
    }
private:
    std::mutex     mMutex;
    FFPtr<AVFrame> mFrame;
    AVRational     mTimebase {0, 1};
};

template <typename T>
class FFWrap final : public Resource, public T {
public:
    using T::T;
    
    AVFormatContext *formatContext = nullptr;

    template <typename ...Args>
    static auto make(Args &&...args) {
        return make_shared<FFWrap<T> >(std::forward<Args>(args)...);
    }
};

using FFPacket = FFPtr<AVPacket>;

class FFDemuxer final : public Demuxer {
public:
    FFDemuxer() {
        // Require a single thread
        setThreadPolicy(ThreadPolicy::SingleThread);
    }
    ~FFDemuxer() {
        
    }

protected:
    Error init() override {
        mController = graph()->queryInterface<MediaController>();

        if (mSource.empty()) {
            // User didnot set args
            return Error::InvalidArguments;
        }
        mFormatContext = avformat_alloc_context();
        mFormatContext->interrupt_callback.opaque = this;
        mFormatContext->interrupt_callback.callback = [](void *opaque) {
            return static_cast<FFDemuxer*>(opaque)->onInterrupt();
        };

        // Open 
        int ret = avformat_open_input(&mFormatContext, mSource.c_str(), nullptr, nullptr);
        if (ret < 0) {
            avformat_close_input(&mFormatContext);
            return toError(ret);
        }
        ret = avformat_find_stream_info(mFormatContext, nullptr);
        if (ret < 0) {
            avformat_close_input(&mFormatContext);
            return toError(ret);
        }

        registerStreams();

        mVideoStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        mAudioStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        mSubtitleStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_SUBTITLE, -1, mAudioStreamIndex, nullptr, 0);
        if (mVideoStreamIndex < 0 && mAudioStreamIndex < 0 && mSubtitleStreamIndex && 0) {
            return Error::NoStream;
        }

        if (mLoadedCallback) {
            mLoadedCallback();
        }

        return Error::Ok;
    }
    Error run() override {
        // Mainloop here
        AVPacket *packet = av_packet_alloc();
        while (state() != State::Stopped) {
            while (state() != State::Running) {
                waitTask(); //< Wait for state changed
            }
            dispatchTask(); //< Poll from framework
            if (state() == State::Stopped) {
                return Error::Ok;
            }

            int ret = av_read_frame(mFormatContext, packet);
            if (ret < 0) {
                // Something wrong
            }
            // Send 
            sendPacket(packet);

            // Cleanup
            av_packet_unref(packet);
        }
        av_packet_free(&packet);
        return Error::Ok;
    }
    Error teardown() override {
        removeAllInputs();
        removeAllOutputs();

        avformat_close_input(&mFormatContext);
        mStreamMapping.clear();
        mController = nullptr;
        return Error::Ok;
    }
    Error pause() override {
        av_read_pause(mFormatContext);
        return Error::Ok;        
    }
    Error resume() override {
        av_read_play(mFormatContext);
        return Error::Ok;
    }
    void setSource(std::string_view view) override {
        mSource = view;
    }
    void setLoadedCallback(std::function<void()> &&callback) override {
        mLoadedCallback = std::move(callback);
    }
    int  onInterrupt() {
        // dispatchTask();
        // return state() == State::Stopped;
        return 0;
    }
    void onSeek(double second) {

    }
    void registerStreams() {
        // Add a lot of pads
        int nowVideoIndex = -1;
        int nowAudioIndex = -1;
        int nowSubtitleIndex = -1;
        for (int n = 0; n < mFormatContext->nb_streams; ++n) {
            auto stream = mFormatContext->streams[n];
            auto type = stream->codecpar->codec_type;
            if (type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_SUBTITLE) {
                continue;
            }
            char name[16] = {0};
            if (type == AVMEDIA_TYPE_VIDEO) {
                nowVideoIndex += 1;
                snprintf(name, sizeof(name), "video%d", nowVideoIndex);
            }
            else if (type == AVMEDIA_TYPE_AUDIO) {
                nowAudioIndex += 1;
                snprintf(name, sizeof(name), "audio%d", nowAudioIndex);
            }
            else if (type == AVMEDIA_TYPE_SUBTITLE) {
                nowSubtitleIndex += 1;
                snprintf(name, sizeof(name), "subtitle%d", nowSubtitleIndex);
            }
            auto pad = addOutput(name);
            auto &prop = pad->properties();

            // Internal types
            prop["ffstream"] = n;

            // Public datas
            if (type == AVMEDIA_TYPE_VIDEO) {
                prop[MediaProperty::Width] = stream->codecpar->width;
                prop[MediaProperty::Height] = stream->codecpar->height;
                prop[MediaProperty::PixelFormat] = ToPixelFormat(AVPixelFormat(stream->codecpar->format));
            }
            else if (type == AVMEDIA_TYPE_AUDIO) {
                prop[MediaProperty::SampleRate] = stream->codecpar->sample_rate;
                prop[MediaProperty::Channels] = stream->codecpar->channels;
                prop[MediaProperty::SampleFormat] = ToSampleFormat(AVSampleFormat(stream->codecpar->format));
            }

            if (stream->duration != AV_NOPTS_VALUE) {
                prop[MediaProperty::Duration] = double(stream->duration) * av_q2d(stream->time_base);
            }

            // Map stream id to pad
            mStreamMapping[n] = pad;
        }
    }
    void sendPacket(AVPacket *packet) {
        auto iter = mStreamMapping.find(packet->stream_index);
        if (iter == mStreamMapping.end()) {
            return;
        }
        auto [_, pad] = *iter;
        if (!pad->isLinked()) {
            return;
        }

        // Wrap if by resource interface
        auto resource = FFWrap<FFPacket>::make(av_packet_clone(packet));
        resource->formatContext = mFormatContext;

        pad->send(resource);
    }
    Error toError(int ffcode) {
        char message[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ffcode, message, AV_ERROR_MAX_STRING_SIZE);
        puts(message);

        switch (ffcode) {

        }
        return Error::Unknown; //< TODO : Use a meaningful eror code
    }
private:
    std::map<int, Pad*> mStreamMapping;
    std::function<void()> mLoadedCallback;
    AVFormatContext *mFormatContext = nullptr;
    MediaController *mController = nullptr;
    std::string      mSource;
    int              mVideoStreamIndex = -1;
    int              mAudioStreamIndex = -1;
    int              mSubtitleStreamIndex = -1;
};
class FFDecoder final : public Decoder {
public:
    FFDecoder() {
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
    }
    ~FFDecoder() {

    }

    Error teardown() override {
        avcodec_free_context(&mCtxt);
        av_frame_free(&mFrame);
        mHardwareFmt = AV_PIX_FMT_NONE;
        return Error::Ok;
    }
    Error init() override {
        return Error::Ok;
    }
    Error processInput(Pad &pad, ResourceView resourceView) override {
        auto res = resourceView.viewAs<FFWrap<FFPacket> >();
        if (!res) {
            return Error::UnsupportedResource;
        }
        auto fmtctxt = res->formatContext;
        auto packet = res->get();
        auto stream = fmtctxt->streams[packet->stream_index];
        if (!packet) {
            avcodec_flush_buffers(mCtxt);
            return Error::Ok;
        }
        if (!mCtxt) {
            if (auto err = initCodecContext(stream); err != Error::Ok) {
                return err;
            }
        }

        // Done, decode here
        int ret = avcodec_send_packet(mCtxt, packet);
        if (ret < 0) {
            return Error::Unknown;
        }
        while (ret >= 0) {
            if (!mFrame) {
                mFrame = av_frame_alloc();
            }
            ret = avcodec_receive_frame(mCtxt, mFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                return Error::Unknown;
            }

            auto resource = FFFrame::make(mFrame, stream->time_base);
            mFrame = nullptr; //< Move ownship
            if (mSourcePad) {
                mSourcePad->send(resource.get());
            }
        }
        return Error::Ok;
    }
    Error initCodecContext(AVStream *stream) {
        if (initHardwareCodecContext(stream) == Error::Ok) {
            return Error::Ok;
        }

        auto codecpar = stream->codecpar;
        auto codec = avcodec_find_decoder(codecpar->codec_id);
        mCtxt = avcodec_alloc_context3(codec);
        if (!mCtxt) {
            return Error::OutOfMemory;
        }
        int ret = avcodec_parameters_to_context(mCtxt, codecpar);
        if (ret < 0) {
            avcodec_free_context(&mCtxt);
            return Error::Unknown; //< TODO : Add more detailed error
        }
        ret = avcodec_open2(mCtxt, mCtxt->codec, nullptr);
        if (ret < 0) {
            avcodec_free_context(&mCtxt);
            return Error::Unknown; //< TODO : Add more detailed error
        }
        return Error::Ok;
    }
    Error initHardwareCodecContext(AVStream *stream) {
        auto codecpar = stream->codecpar;
        auto codec = avcodec_find_decoder(codecpar->codec_id);
        mCtxt = avcodec_alloc_context3(codec);
        if (!mCtxt) {
            return Error::OutOfMemory;
        }
        int ret = avcodec_parameters_to_context(mCtxt, codecpar);
        if (ret < 0) {
            avcodec_free_context(&mCtxt);
            return Error::Unknown; //< TODO : Add more detailed error
        }

        // Query hardware here
        const AVCodecHWConfig *conf = nullptr;
        for (int i = 0; ; i++) {
            conf = avcodec_get_hw_config(codec, i);
            if (!conf) {
                // Fail
                avcodec_free_context(&mCtxt);
                return Error::Unknown;
            }

            if (!(conf->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
                continue;
            }
            // Got
            auto hardwareType = conf->device_type;
            mHardwareFmt = conf->pix_fmt;

            // Override HW get format
            mCtxt->opaque = this;
            mCtxt->get_format = [](struct AVCodecContext *s, const enum AVPixelFormat * fmt) {
                auto self = static_cast<FFDecoder*>(s->opaque);
                auto p = fmt;
                while (*p != AV_PIX_FMT_NONE) {
                    if (*p == self->mHardwareFmt) {
                        return self->mHardwareFmt;
                    }
                    if (*(p + 1) == AV_PIX_FMT_NONE) {
                        // Fallback
                        return *p;
                    }
                    p ++;
                }
                ::abort();
                return AV_PIX_FMT_NONE;
            };

            
            // Try init hw
            AVBufferRef *hardwareDeviceCtxt = nullptr;
            if (av_hwdevice_ctx_create(&hardwareDeviceCtxt, hardwareType, nullptr, nullptr, 0) < 0) {
                // Failed
                continue;
            }
            // hardwareCtxt->hw_device_ctx = av_buffer_ref(hardwareDeviceCtxt);
            mCtxt->hw_device_ctx = hardwareDeviceCtxt;

            // Init codec
            if (avcodec_open2(mCtxt, codec, nullptr) < 0) {
                continue;
            }

            // Done
            NEKO_LOG("Hardware inited by {}", std::string_view(av_pix_fmt_desc_get(mHardwareFmt)->name));
            return Error::Ok;
        }

        // Fail
        avcodec_free_context(&mCtxt);
        return Error::Unknown;
    }
private:
    AVCodecContext *mCtxt = nullptr;
    Pad            *mSinkPad = nullptr;
    Pad            *mSourcePad = nullptr;
    AVFrame        *mFrame = nullptr; //< Cached frame
    AVPixelFormat   mHardwareFmt = AV_PIX_FMT_NONE;
};
class FFVideoConverter final : public VideoConverter {
public:
    FFVideoConverter() {
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
    }
    void setPixelFormat(PixelFormat format) override {
        mTargetFormat = format;
    }
    Error init() override {
        return Error::Ok;
    }
    Error teardown() override {
        sws_freeContext(mCtxt);
        av_frame_free(&mSwFrame);
        mPassthrough = false;
        mCopyback = false;
        mCtxt = nullptr;
        return Error::Ok;
    }
    Error processInput(Pad &, ResourceView resourceView) override {
        auto frame = resourceView.viewAs<FFFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mSourcePad->isLinked()) {
            return Error::NoConnection;
        }
        if (!mCtxt && !mPassthrough) {
            if (auto err = initContext(frame->get()); err != Error::Ok) {
                return err;
            }
        }
        if (mPassthrough) {
            mSourcePad->send(resourceView);
            return Error::Ok;
        }

        auto srcFrame = frame->get();
        if (mCopyback) {
            assert(mSwFrame);
            int ret = av_hwframe_transfer_data(mSwFrame, srcFrame, 0);
            if (ret < 0) {
                // Failed to transfer
                return Error::Unknown;
            }
            av_frame_copy_props(mSwFrame, srcFrame);
            srcFrame = mSwFrame;
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
        return mSourcePad->send(FFFrame::make(dstFrame, frame->timebase()).get());
    }
    Error initContext(AVFrame *f) {
        AVPixelFormat fmt = AV_PIX_FMT_RGBA;
        if (mTargetFormat == PixelFormat::None) {
            // Try get info by pad
            auto &pixfmt = mSourcePad->next()->property(MediaProperty::PixelFormatList);
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
                return Error::Unknown;
            }
            mCopyback = true;
            f = mSwFrame;
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
            return Error::Unknown;
        }
        mSwsFormat = fmt;
        return Error::Ok;
    }
private:
    SwsContext *mCtxt = nullptr;
    AVFrame    *mSwFrame = nullptr; //< Used when hardware format
    Pad        *mSinkPad = nullptr;
    Pad        *mSourcePad = nullptr;
    bool        mPassthrough = false;
    bool        mCopyback = false;
    PixelFormat mTargetFormat = PixelFormat::None; //< If not None, force
    AVPixelFormat mSwsFormat = AV_PIX_FMT_NONE; //< Current target format
};
class FFAudioConverter final : public AudioConverter {
public:
    FFAudioConverter() {
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
    }
    Error init() override {
        return Error::Ok;
    }
    Error teardown() override {
        swr_free(&mCtxt);
        mPassthrough = false;
        mSwrFormat = AV_SAMPLE_FMT_NONE;
        return Error::Ok;
    }
    Error processInput(Pad &, ResourceView resourceView) override {
        auto frame = resourceView.viewAs<FFFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mSourcePad->isLinked()) {
            return Error::NoConnection;
        }
        if (!mCtxt && !mPassthrough) {
            if (auto err = initContext(frame->get()); err != Error::Ok) {
                return err;
            }
        }
        if (mPassthrough) {
            mSourcePad->send(resourceView);
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

        return mSourcePad->send(FFFrame::make(dstFrame, frame->timebase()).get());
    }
    Error initContext(AVFrame *frame) {
        AVSampleFormat fmt;
        auto &spfmt = mSourcePad->next()->property(MediaProperty::SampleFormatList);
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
    void setSampleFormat(SampleFormat fmt) override {

    }
private:
    AVSampleFormat mSwrFormat = AV_SAMPLE_FMT_NONE;
    bool        mPassthrough = false;
    SwrContext *mCtxt = nullptr;
    Pad        *mSinkPad = nullptr;
    Pad        *mSourcePad = nullptr;
};

}

// NEKO_FF_REGISTER(Demuxer, FFDemuxer);
// NEKO_FF_REGISTER_NAME("Demuxer", FFDemuxer);

NEKO_CONSTRUCTOR(ffregister) {
    auto factory = static_cast<FFElementFactory*>(GetFFmpegFactory());
    factory->registerElement<FFDemuxer>("Demuxer");
    factory->registerElement<Demuxer, FFDemuxer>();

    factory->registerElement<FFDecoder>("Decoder");    
    factory->registerElement<Decoder, FFDecoder>();

    factory->registerElement<FFVideoConverter>("VideoConverter");
    factory->registerElement<VideoConverter, FFVideoConverter>();

    factory->registerElement<FFAudioConverter>("AudioConverter");
    factory->registerElement<AudioConverter, FFAudioConverter>();

    // Register self as a  global pub
    GetMediaFactory()->registerFactory(factory);
}

NEKO_NS_END