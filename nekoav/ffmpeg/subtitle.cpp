#if __has_include(<ass.h>)
#define _NEKO_SOURCE
#include "../elements/subtitle.hpp"
#include "../detail/base.hpp"
#include "../factory.hpp"
#include "../format.hpp"
#include "../pad.hpp"
#include "common.hpp"
#include <ass.h>

NEKO_NS_BEGIN

namespace FFmpeg {

class SubtitleFilterImpl : public Impl<SubtitleFilter> {
public:
    SubtitleFilterImpl() {
        mSink = addInput("sink");
        mSrc = addOutput("src");

        mSink->addProperty(Properties::PixelFormatList, {PixelFormat::RGBA});
    }
    ~SubtitleFilterImpl() {

    }

    // SubtitleFilter here
    void setUrl(std::string_view url) override {
        mUrl = url;
    }
    Vec<std::string> subtitles() override {
        // if (!mFormatContext) {
        //     return Vec<std::string>();
        // }
        // For all subtitles streams
        // Vec<std::string> subtitleList;
        // for (int i = 0; i < mFormatContext->nb_streams; i++) {
        //     if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
        //         AVDictionary *metadata = mFormatContext->streams[i]->metadata;
        //         // Get the title
        //         auto title = av_dict_get(metadata, "title", nullptr, 0);
        //         if (title) {
        //             subtitleList.push_back(std::string(title->value));
        //         }
        //         else {
        //             subtitleList.push_back(std::string());
        //         }
        //     }
        // }
        return mSubtitles;
    }
    void setSubtitle(int idx) override {
        mSubtitleIndex = idx;
    }

    Error onInitialize() override {
        mAssLibrary = ass_library_init();
        if (!mAssLibrary) {
            onTeardown();
            return Error::OutOfMemory;
        }
        mAssRenderer = ass_renderer_init(mAssLibrary);
        if (!mAssRenderer) {
            onTeardown();
            return Error::OutOfMemory;
        }
        if (auto err = loadSubtitleByFFmpeg(); err != Error::Ok) {
            onTeardown();
            return err;
        }

        return Error::Ok;
    }
    Error onTeardown() override {
        for (auto track : mAssTracks) {
            ass_free_track(track);
        }
        ass_renderer_done(mAssRenderer);
        ass_library_done(mAssLibrary);
        mAssRenderer = nullptr;
        mAssLibrary = nullptr;
        mSubtitleIndex = -1;
        mAssTracks.clear();
        mSubtitles.clear();
        return Error::Ok;
    }
    Error loadSubtitleByFFmpeg() {
        AVFormatContext *formatContext = nullptr;
        int ret;
        ret = avformat_open_input(&formatContext, mUrl.c_str(), nullptr, nullptr);
        if (ret < 0) {
            avformat_close_input(&formatContext);
            return ToError(ret);
        }
        ret = avformat_find_stream_info(formatContext, nullptr);
        if (ret < 0) {
            avformat_close_input(&formatContext);
            return ToError(ret);
        }
        mSubtitleIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);

        // Auto disable all non-subtitle streams and add embedded font
        Vec<int> streamMapping; //< Maping AVStream index to mSubtitleIndexs
        streamMapping.resize(formatContext->nb_streams, -1);

        Vec<AVCodecContext*> codecContext;

        int subtitleIndex = 0;
        for (int i = 0; i < formatContext->nb_streams; i++) {
            if (formatContext->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
                formatContext->streams[i]->discard = AVDISCARD_ALL;
            }
            else {
                // IS subtitle
                auto track = ass_new_track(mAssLibrary);
                mAssTracks.push_back(track);
                streamMapping[i] = subtitleIndex;

                subtitleIndex += 1;

                // Make codec Context
                auto ctxt = OpenCodecContext4(formatContext->streams[i]->codecpar);
                codecContext.push_back(ctxt);

                // Process private parts
                if (ctxt->subtitle_header) {
                    ass_process_codec_private(track, (char*) ctxt->subtitle_header, ctxt->subtitle_header_size);
                }

                // Add name
                AVDictionary *metadata = formatContext->streams[i]->metadata;
                auto title = av_dict_get(metadata, "title", nullptr, 0);
                if (title) {
                    mSubtitles.push_back(std::string(title->value));
                }
                else {
                    mSubtitles.push_back(std::string());
                }
            }
            // Add embedded font
        }

        readAllSubtitleFrames(formatContext, streamMapping, codecContext);

        // Done
        for (auto &ctxt : codecContext) {
            avcodec_free_context(&ctxt);
        }
        avformat_close_input(&formatContext);
        return Error::Ok;
    }
    Error readAllSubtitleFrames(AVFormatContext *formatContext, const Vec<int> &streamMapping, const Vec<AVCodecContext *> &codecContexts) {
        AVPacket *packet = av_packet_alloc();
        while (av_read_frame(formatContext, packet) == 0) {
            auto stream = formatContext->streams[packet->stream_index];
            if (stream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
                continue;
            }
            // Do
            int idx = streamMapping[packet->stream_index];
            NEKO_ASSERT(idx >= 0);

            AVSubtitle subtitle;
            int got;
            avcodec_decode_subtitle2(codecContexts[idx], &subtitle, &got, packet);
            if (!got) {
                continue;
            }

            // Add subtitle
            for (int i = 0; i < subtitle.num_rects; ++i) {
                if (!subtitle.rects[i]->ass) {
                    continue;
                }
                ass_process_chunk(
                    mAssTracks[idx], 
                    subtitle.rects[i]->ass, 
                    ::strlen(subtitle.rects[i]->ass),
                    subtitle.start_display_time,
                    subtitle.start_display_time + subtitle.end_display_time
                );
            }

            avsubtitle_free(&subtitle);
            av_packet_unref(packet);
        }

        av_packet_free(&packet);
    }

    // Data Processing
    Error onSinkPush(View<Pad> pad, View<Resource> resource) override {
        auto frame = resource.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!frame->makeWritable()) {
            return Error::UnsupportedResource;
        }
        
        return Error::Ok;
    }
private:
    std::string      mUrl;
    Pad             *mSink;
    Pad             *mSrc;
    int              mSubtitleIndex = -1; //< In AVStream position

    // ASS Here
    ASS_Library     *mAssLibrary = nullptr;
    ASS_Renderer    *mAssRenderer = nullptr;
    Vec<ASS_Track*>  mAssTracks;
    Vec<std::string> mSubtitles;
};

// TODO : It stll not done
// NEKO_REGISTER_ELEMENT(SubtitleFilter, SubtitleFilterImpl);

}




NEKO_NS_END

#endif