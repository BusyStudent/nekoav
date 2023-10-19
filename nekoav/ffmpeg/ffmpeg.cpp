#define _NEKO_SOURCE
#include "factory_impl.hpp"
#include "ffmpeg.hpp"
#include "../media.hpp"
#include "../audio/device.hpp"
#include <queue>
#include <mutex>
#include <map>

extern "C" {
    #include <libavutil/imgutils.h>
}

NEKO_NS_BEGIN

namespace {

class FFFrame final : public MediaFrame {
public:
    explicit FFFrame(AVFrame* frame) : mFrame(frame) {

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
        return mFrame->pts * av_q2d(mFrame->time_base);
    }
    double duration() const override {
        return mFrame->pkt_duration * av_q2d(mFrame->time_base);
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
            case Query::Height: return mFrame->height;
            case Query::Width: return mFrame->width;
            default: return 0;
        }
    }

    static auto make(AVFrame *f) {
        return make_shared<FFFrame>(f);
    }
private:
    std::mutex     mMutex;
    FFPtr<AVFrame> mFrame;
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
                prop["width"] = stream->codecpar->width;
                prop["height"] = stream->codecpar->height;
                prop["pixfmt"] = ToPixelFormat(AVPixelFormat(stream->codecpar->format));
            }
            else if (type == AVMEDIA_TYPE_AUDIO) {
                prop["sampleRate"] = stream->codecpar->sample_rate;
                prop["channels"] = stream->codecpar->channels;
            }

            if (stream->duration != AV_NOPTS_VALUE) {
                prop["duration"] = double(stream->duration) * av_q2d(stream->time_base);
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
class FFDecoder : public Decoder {
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
        if (!packet) {
            avcodec_flush_buffers(mCtxt);
            return Error::Ok;
        }
        if (!mCtxt) {
            auto codecpar = fmtctxt->streams[packet->stream_index]->codecpar;
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

            auto resource = FFFrame::make(mFrame);
            mFrame = nullptr; //< Move ownship
            if (mSourcePad) {
                mSourcePad->send(resource.get());
            }
        }
        return Error::Ok;
    }
private:
    AVCodecContext *mCtxt = nullptr;
    Pad            *mSinkPad = nullptr;
    Pad            *mSourcePad = nullptr;
    AVFrame        *mFrame = nullptr; //< Cached frame
};
class FFVideoConverter final : public VideoConverter {
public:
    FFVideoConverter() {
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
    }
    Error init() override {
        return Error::Ok;
    }
    Error teardown() override {
        sws_freeContext(mCtxt);
        mPassThrough = false;
        mCtxt = nullptr;
        return Error::Ok;
    }
    Error processInput(Pad &, ResourceView resourceView) {
        auto frame = resourceView.viewAs<FFFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mSourcePad->isLinked()) {
            return Error::NoConnection;
        }
        if (!mCtxt && !mPassThrough) {
            if (auto err = initContext(frame->get()); err != Error::Ok) {
                return err;
            }
        }
        if (mPassThrough) {
            mSourcePad->send(resourceView);
            return Error::Ok;
        }

        // TODO Finish it

        // Alloc frame and data
        auto dstFrame = av_frame_alloc();
        auto srcFrame = frame->get();
        auto ret = av_image_alloc(
            dstFrame->data,
            dstFrame->linesize,
            srcFrame->width,
            srcFrame->height,
            AVPixelFormat(srcFrame->format),
            0
        );
        if (ret < 0) {
            return Error::OutOfMemory;
        }
        ret = sws_scale_frame(mCtxt, dstFrame, srcFrame);
        if (ret < 0) {
            return Error::OutOfMemory;
        }
        return mSourcePad->send(FFFrame::make(dstFrame).get());
    }
    Error initContext(AVFrame *f) {
        auto &pixfmt = mSourcePad->next()->property("pixfmts");
        if (pixfmt.isNull()) {
            return Error::InvalidTopology;
        }
        AVPixelFormat fmt;
        for (const auto &v : pixfmt.toList()) {
            fmt = ToAVPixelFormat(v.toEnum<PixelFormat>());
            if (fmt == AVPixelFormat(f->format)) {
                mPassThrough = true;
                return Error::Ok;
            }
        }

        // Prepare sws conetxt
        // TODO Finish it
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
        return Error::Ok;
    }
private:
    SwsContext *mCtxt = nullptr;
    Pad        *mSinkPad = nullptr;
    Pad        *mSourcePad = nullptr;
    bool        mPassThrough = false;
};

/**
 * @brief Present for ffmpeg audio
 * 
 */
class FFAudioPresenter : public AudioPresenter, MediaClock {
public:
    FFAudioPresenter() {
        addInput("sink");
    }
    ~FFAudioPresenter() {

    }

    Error init() override {
        mDevice = CreateAudioDevice();
        // mDevice->setCallback(std::bind(&FFAudioPresenter::audioCallback, this));

        mController = graph()->queryInterface<MediaController>();
        if (mController) {
            mController->addClock(this);
        }
        return Error::Ok;
    }
    Error teardown() override {
        mDevice.reset();
        if (mController) {
            mController->removeClock(this);
        }
        mController = nullptr;
        return Error::Ok;
    }
    Error processInput(Pad &, ResourceView resourceView) override {
        auto frame = resourceView.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mOpened) {
            // Try open it
            mOpened = mDevice->open(frame->sampleFormat(), frame->sampleRate(), frame->channels());
            if (mOpened) {
                return Error::UnsupportedFormat;
            }
            // Start it
            mDevice->pause(false);
        }

        // Is Opened, write to queue
        std::lock_guard lock(mMutex);
        mFrames.push(frame->shared_from_this<MediaFrame>());
    }

    double position() const override {
        return mPosition;
    }
    Type type() const override {
        return Audio;
    }
    void audioCallback(uint8_t *buf, int len) {
        while (len > 0) {
            // No current frame
            if (!mCurrentFrame) {
                std::lock_guard lock(mMutex);
                if (mFrames.empty()) {
                    break;
                }
                mCurrentFramePosition = 0; //< Reset position
                mCurrentFrame = std::move(mFrames.front());
                mFrames.pop();

                // Update audio clock
                mPosition = mCurrentFrame->timestamp();
            }
            void *frameData = mCurrentFrame->data(0);
            int frameLen = mCurrentFrame->linesize(0);
            int bytes = std::min(len, frameLen - mCurrentFramePosition);
            memcpy(buf, (uint8_t*) frameData + mCurrentFramePosition, bytes);

            mCurrentFramePosition += bytes;
            buf += bytes;
            len -= bytes;

            // Update audio clock
#if         NEKO_CXX20
            mPosition += mCurrentFrame->duration() * bytes / frameLen;
#else
            mPosition  = mPosition + mCurrentFrame->duration() * bytes / frameLen;
#endif

            if (mCurrentFramePosition == frameLen) {
                mCurrentFrame.reset();
                mCurrentFramePosition = 0;
            }
        }
        if (len > 0) {
            ::memset(buf, 0, len);
        }
    }
private:
    MediaController *mController = nullptr;
    Atomic<double>   mPosition {0.0}; //< Current media clock position
    Box<AudioDevice> mDevice;
    bool             mPaused = false;
    bool             mOpened = false;

    // Audio Callback data
    std::mutex                   mMutex;
    std::queue<Arc<MediaFrame> > mFrames;
    Arc<MediaFrame>              mCurrentFrame;
    int                          mCurrentFramePosition = 0;
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
}

NEKO_NS_END