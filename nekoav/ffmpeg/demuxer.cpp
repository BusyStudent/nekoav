#define _NEKO_SOURCE
#include "../elements/demuxer.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../pad.hpp"
#include "../log.hpp"
#include "common.hpp"
#include "ffmpeg.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavformat/avio.h>
}

NEKO_NS_BEGIN

namespace FFmpeg {

class FFDemuxer final : public Template::GetThreadImpl<Demuxer, MediaElement> {
public:
    FFDemuxer() {
        setFlags(ElementFlags::DynamicOutput);
        
        mPacket = av_packet_alloc();
    }
    ~FFDemuxer() {
        av_packet_free(&mPacket);
    }
    void setUrl(std::string_view source) override {
        mSource = source;
    }
    void setOptions(const Properties *options) override {
        if (options) {
            mOptions = *options;
        }
        else {
            mOptions.clear();
        }
    }
    double duration() const override {
        if (!mFormatContext) {
            return 0.0;
        }
        if (mFormatContext->duration == AV_NOPTS_VALUE) {
            return 0.0;
        }
        return double(mFormatContext->duration) / AV_TIME_BASE;
    }
    bool isSeekable() const override {
        if (!mFormatContext) {
            return false;
        }
        if (!mFormatContext->pb) {
            return false;
        }
        return mFormatContext->pb->seekable != 0;
    }
    Properties metadata() const override {
        if (!mFormatContext) {
            return Properties();
        }
        Properties metadata;
        for (auto [key, value] : IterDict(mFormatContext->metadata)) {
            metadata[key] = value;
        }
        return metadata;
    }
    Error onInitialize() override {
        mFormatContext = avformat_alloc_context();
        mFormatContext->interrupt_callback.opaque = this;
        mFormatContext->interrupt_callback.callback = [](void *self) {
            return static_cast<FFDemuxer*>(self)->_interruptHandler();
        };
        if (mIOStream) {
            mFormatContext->pb = WrapIOStream(1024 * 32, mIOStream);
        }

        AVDictionary *dict = ParseOpenOptions(&mOptions);
        int ret = avformat_open_input(&mFormatContext, mSource.c_str(), nullptr, &dict);
        av_dict_free(&dict);
        if (ret < 0) {
            avformat_close_input(&mFormatContext);
            NEKO_DEBUG(FormatErrorCode(ret));
            return ToError(ret);
        }
        ret = avformat_find_stream_info(mFormatContext, nullptr);
        if (ret < 0) {
            avformat_close_input(&mFormatContext);
            NEKO_DEBUG(FormatErrorCode(ret));
            return ToError(ret);
        }

#ifndef NDEBUG
        av_dump_format(mFormatContext, 0, mSource.c_str(), 0);
#endif

        _registerStreams();
        
        return Error::Ok;
    }
    Error onTeardown() override {
        if (mIOStream && mFormatContext) {
            avio_context_free(&mFormatContext->pb);
        }
        avformat_close_input(&mFormatContext);
        mStreamMapping.clear();
        mEof = false;
        return Error::Ok;
    }

    Error onLoop() override {
        Error err = Error::Ok;
        while (!stopRequested()) {
            thread()->waitTask();
            while (state() == State::Running && (!mEof || mSeekRequested)) {
                // When Running and (not Eof or Seek)
                thread()->dispatchTask();
                
                if (mSeekRequested) {
                    err = _doSeek(mSeekPosition);
                    mSeekRequested = false;
                }
                else {
                    err = _readFrame();
                } 
                if (err != Error::Ok) {
                    return err;
                }
            }
        }
        return Error::Ok;
    }
    Error onEvent(View<Event> event) override {
        if (event->type() == Event::SeekRequested) {
            // Require a seek
            mSeekPosition = event.viewAs<SeekEvent>()->position();
            mSeekRequested = true;
            mEof = false;
        }
        return Error::Ok;
    }
    Error _readFrame() {
        int ret = av_read_frame(mFormatContext, mPacket);
        if (ret < 0) {
            NEKO_DEBUG(FormatErrorCode(ret));
            if (ret == AVERROR_EOF) {
                // Read at end of file
                mEof = true;
                return Error::Ok;
            }
            return ToError(ret);
        }
        // Send
        auto iter = mStreamMapping.find(mPacket->stream_index);
        if (iter != mStreamMapping.end()) {
            auto [_, pad] = *iter;
            if (pad->isLinked()) {
                auto packet = Packet::make(
                    av_packet_clone(mPacket),
                    mFormatContext->streams[mPacket->stream_index]
                );
                pad->push(packet);
            }
        }
        av_packet_unref(mPacket);   
        return Error::Ok;
    }
    void _registerStreams() {
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
                prop[Properties::Width] = stream->codecpar->width;
                prop[Properties::Height] = stream->codecpar->height;
                prop[Properties::PixelFormat] = ToPixelFormat(AVPixelFormat(stream->codecpar->format));
            }
            else if (type == AVMEDIA_TYPE_AUDIO) {
                prop[Properties::SampleRate] = stream->codecpar->sample_rate;
                prop[Properties::Channels] = stream->codecpar->channels;
                prop[Properties::SampleFormat] = ToSampleFormat(AVSampleFormat(stream->codecpar->format));
            }

            if (stream->duration != AV_NOPTS_VALUE) {
                prop[Properties::Duration] = double(stream->duration) * av_q2d(stream->time_base);
            }
            if (stream->metadata) {
                auto md = Property::newMap();
                for (auto [key, value] : IterDict(stream->metadata)) {
                    md[key] = value;
                }
                prop[Properties::Metadata] = std::move(md);
            }

            // Map stream id to pad
            mStreamMapping.insert(std::make_pair(n, pad));
        }
    }
    bool isEndOfFile() const override {
        return mEof;
    }
    MediaClock *clock() const override {
        return nullptr;
    }
    Error _doSeek(double time) {
        // Send pad with event
        for (auto out : outputs()) {
            out->pushEvent(Event::make(Event::FlushRequested, this)); //< Push a flush
            out->pushEvent(Event::make(Event::PlaybackPause, this)); //< Pause Playback
        }

        int64_t seekTime = time* AV_TIME_BASE;
        int ret = av_seek_frame(
            mFormatContext,
            -1,
            seekTime,
            AVSEEK_FLAG_BACKWARD
        );
        if (ret < 0) {
            NEKO_DEBUG(FormatErrorCode(ret));
            return ToError(ret);
        }

        // Send pad with event
        for (auto out : outputs()) {
            out->pushEvent(SeekEvent::make(time)); //< Push a seek
            out->pushEvent(Event::make(Event::PlaybackResume, this)); //< Resume Playback
        }

        mEof = false;
        return Error::Ok;
    }
    int _interruptHandler() {
        // NEKO_ASSERT(mInInterruptHandler);
        return stopRequested();
    }
    void _clearIOStream() {
        if (mOwnIOStream) {
            mIOStream = nullptr;
        }
        mOwnIOStream = false;
        mIOStream = nullptr;
    }
private:
    AVFormatContext *mFormatContext = nullptr;
    AVPacket        *mPacket = nullptr;
    std::string      mSource;
    std::map<int, Pad*> mStreamMapping; //< Mapping from FFmpeg stream index to Pad pointer
    Atomic<bool>        mEof = false;
    // bool                mInInterruptHandler = false;
    bool                mSeekRequested = false;
    double              mSeekPosition = -1.0;
    Properties          mOptions; //< Options for Open
    IOStream           *mIOStream = nullptr;
    bool                mOwnIOStream = false;
};

NEKO_REGISTER_ELEMENT(Demuxer, FFDemuxer);

}

NEKO_NS_END