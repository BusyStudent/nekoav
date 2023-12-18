#define _NEKO_SOURCE
#include "../media/reader.hpp"
#include "../property.hpp"
#include "common.hpp"
#include <vector>
#include <map>

NEKO_NS_BEGIN

namespace FFmpeg {

NEKO_IMPL_BEGIN

class FFMediaReaderImpl final : public MediaReader {
public:
    FFMediaReaderImpl() {
        mPacket = av_packet_alloc();
    }
    ~FFMediaReaderImpl() {
        av_packet_free(&mPacket);
        av_frame_free(&mFrame);
        _close();
    }
    Error openUrl(std::string_view url, const Properties *options) override {
        _close();

        AVDictionary *dict = nullptr;
        mFormatContext = avformat_alloc_context();
        if (options) {
            for (const auto &[key, value] : *options) {
                // TODO : Convert
                if (key == Properties::HttpUserAgent) {
                    av_dict_set(&dict, "user_agent", value.toString().c_str(), 0);
                }
                else if (key == Properties::HttpReferer) {
                    av_dict_set(&dict, "referer", value.toString().c_str(), 0);
                }
            }
        }

        int ret = avformat_open_input(&mFormatContext, std::string(url).c_str(), nullptr, &dict);
        av_dict_free(&dict);
        if (ret < 0) {
            _close();
            return ToError(ret);
        }
        ret = avformat_find_stream_info(mFormatContext, nullptr);
        if (ret < 0) {
            _close();
            return ToError(ret);
        }
        for (int idx = 0; idx < mFormatContext->nb_streams; idx++) {
            auto stream = mFormatContext->streams[idx];
            StreamType type = StreamType::Unknown;
            
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                type = StreamType::Video;
            }
            else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                type = StreamType::Audio;
            }

            mStreams.push_back(
                MediaStream {
                    .index = idx,
                    .type = type,
                }
            );
        }
        return Error::Ok;
    }
    Error readFrame(Arc<MediaFrame> *frame, int *index) override {
        if (!frame) {
            return Error::InvalidArguments;
        }
        int ret = 0;
        while ((ret = av_read_frame(mFormatContext, mPacket)) >= 0) {
            auto ok = _processPacket(mPacket, frame);
            auto idx = mPacket->stream_index;
            av_packet_unref(mPacket);
            if (!ok) {
                continue;
            }
            // Ok, Frame generated
            if (index) {
                *index = idx;
            }
            break;
        }
        if (ret >= 0) {
            return Error::Ok;
        }
        if (ret == AVERROR_EOF) {
            mIsEndOfFile = true;
            return Error::EndOfFile;
        }
        return ToError(ret);
    }
    Error setPosition(double pos) override {
        if (pos < 0 || (mFormatContext->duration && pos > double(mFormatContext->duration) / AV_TIME_BASE)) {
            return Error::InvalidArguments;
        }
        if (int ret = av_seek_frame(mFormatContext, -1, pos * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD); ret < 0) {
            return ToError(pos);
        }
        // Flush all codec
        for (auto &[_, codecContext] : mCodecContexts) {
            avcodec_flush_buffers(codecContext.get());
        }
        mSeekPosition = pos;
        mIsEndOfFile = false;
        return Error::Ok;
    }
    Error selectStream(int idx, bool select) override {
        if (!select) {
            // Close
            mCodecContexts.erase(idx);
            return Error::Ok;
        }
        // Open codec
        return _initContextAt(idx);
    }
    bool isEndOfFile() const override {
        return mIsEndOfFile;
    }
    bool isStreamSelected(int idx) const override {
        return mCodecContexts.find(idx) != mCodecContexts.end();
    }
    Property query(int what) const override {
        if (!mFormatContext) {
            return Property();
        }
        switch (what) {
            case Duration: return double(mFormatContext->duration) / AV_TIME_BASE;
            case Seekable: return mFormatContext->pb->seekable == 0 ? false : true;
            default: return Property();
        }
    }
    std::span<const MediaStream> streams() const override {
        return mStreams;
    }
    void _close() {
        mStreams.clear();
        mCodecContexts.clear();
        avformat_close_input(&mFormatContext);
        mIsEndOfFile = false;
        mSeekPosition = 0.0;
    }
    Error _initContextAt(int index) {
        if (index < 0 || index >= mFormatContext->nb_streams) {
            return Error::InvalidArguments;
        }
        AVStream *stream = mFormatContext->streams[index];
        AVCodecContext *codecContext = OpenCodecContext4(stream->codecpar);
        if (!codecContext) {
            return Error::NoCodec;
        }
        mCodecContexts.insert(std::make_pair(
            index,
            Arc<AVCodecContext>(codecContext, [](AVCodecContext *&ctxt) {
                avcodec_free_context(&ctxt);
            })
        ));
        return Error::Ok;
    }
    bool _processPacket(AVPacket *pakcet, Arc<MediaFrame> *frame) {
        auto iter = mCodecContexts.find(pakcet->stream_index);
        if (iter == mCodecContexts.end()) {
            return false;
        }
        auto &[_, codecContext] = *iter;
        int ret = avcodec_send_packet(codecContext.get(), pakcet);
        if (ret < 0) {
            return false;
        }
        if (!mFrame) {
            mFrame = av_frame_alloc();
        }
        ret = avcodec_receive_frame(codecContext.get(), mFrame);
        if (ret < 0) {
            return false;
        }
        // Check pts here
        auto stream = mFormatContext->streams[pakcet->stream_index];
        double pts = 0.0;
        if (mFrame->pts != AV_NOPTS_VALUE) {
            pts = av_q2d(stream->time_base) * mFrame->pts;
        }
        if (pts < mSeekPosition) {
            // Before the seek position, drop !!!
            return false;
        }
        
        // Output
        *frame = Frame::make(mFrame, stream->time_base, stream->codecpar->codec_type);
        mFrame = nullptr;
        return true;
    }
private:
    AVPacket *mPacket = nullptr;
    AVFrame  *mFrame = nullptr;
    AVFormatContext *mFormatContext = nullptr;
    std::vector<MediaStream> mStreams;
    std::map<int, Arc<AVCodecContext> > mCodecContexts;
    bool mIsEndOfFile = false;
    double mSeekPosition = 0.0; //< Output frame will not earlier than this
};

NEKO_IMPL_END

NEKO_CONSTRUCTOR(FFMediaReaderImpl_ctr) {
    MediaReader::registers<FFMediaReaderImpl>();
}

}

NEKO_NS_END