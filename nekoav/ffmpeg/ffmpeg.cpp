#define _NEKO_SOURCE
#include "factory_impl.hpp"
#include "ffmpeg.hpp"
#include "../media.hpp"

NEKO_NS_BEGIN

namespace {

class FFDemuxer final : public Demuxer {
public:
    FFDemuxer() {
        // Require a single thread
        setThreadPolicy(ThreadPolicy::SingleThread);
    }
    ~FFDemuxer() {
        
    }

protected:
    Error initialize() override {
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

        mVideoStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        mAudioStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        mSubtitleStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_SUBTITLE, -1, mAudioStreamIndex, nullptr, 0);
        if (mVideoStreamIndex < 0 && mAudioStreamIndex < 0 && mSubtitleStreamIndex && 0) {
            return Error::NoStream;
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
        avformat_close_input(&mFormatContext);
        return Error::Ok;
    }
    // Error pause() override {
        
    // }
    // Error resume() override {
        
    // }
    void setSource(std::string_view view) override {
        mSource = view;
    }
    int  onInterrupt() {
        dispatchTask();
        return state() == State::Stopped;
    }
    void onSeek(double second) {

    }
    void sendPacket(AVPacket *packet) {

    }
    Error toError(int ffcode) {
        char message[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ffcode, message, AV_ERROR_MAX_STRING_SIZE);
        puts(message);
        return Error::Unknown; //< TODO : Use a meaningful eror code
    }
private:
    AVFormatContext *mFormatContext;
    std::string      mSource;
    int              mVideoStreamIndex = -1;
    int              mAudioStreamIndex = -1;
    int              mSubtitleStreamIndex = -1;
};


}

NEKO_FF_REGISTER(Demuxer, FFDemuxer);
NEKO_FF_REGISTER_NAME("Demuxer", FFDemuxer);

NEKO_NS_END