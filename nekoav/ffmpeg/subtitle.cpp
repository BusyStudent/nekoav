#if   __has_include(<libass/ass.h>)
    #define HAVE_LIBASS 
    #include <libass/ass.h>
#elif __has_include(<ass/ass.h>)
    #define HAVE_LIBASS 
    #include <ass/ass.h>
#endif

#ifdef HAVE_LIBASS
#define _NEKO_SOURCE
#include "../elements/subtitle.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "common.hpp"

NEKO_NS_BEGIN

namespace FFmpeg {

class SubtitleFilterImpl : public Template::GetImpl<SubtitleFilter> {
public:
    SubtitleFilterImpl() {
        mSink = addInput("sink");
        mSrc = addOutput("src");

        // Just forward
        mSink->setEventCallback(std::bind(&Pad::pushEvent, mSrc, std::placeholders::_1));
    }
    ~SubtitleFilterImpl() {

    }

    // SubtitleFilter here
    void setUrl(std::string_view url) override {
        mUrl = url;
    }
    Vec<std::string> subtitles() override {
        if (!mFormatContext) {
            return Vec<std::string>();
        }
        // For all subtitles streams
        Vec<std::string> subtitleList;
        for (int i = 0; i < mFormatContext->nb_streams; i++) {
            if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
                AVDictionary *metadata = mFormatContext->streams[i]->metadata;
                // Get the title
                auto title = av_dict_get(metadata, "title", nullptr, 0);
                if (title) {
                    subtitleList.push_back(std::string(title->value));
                }
                else {
                    subtitleList.push_back(std::string());
                }
            }
        }
        return subtitleList;
    }
    void setSubtitle(int idx) override {
        mSubtitleIndex = toAVStreamIndex(idx);
        NEKO_ASSERT(mSubtitleIndex >= 0 && mSubtitleIndex < mFormatContext->nb_streams);
    }

    Error onInitialize() override {
        mAssLibrary = ass_library_init();
        mFormatContext = avformat_alloc_context();
        if (!mFormatContext || !mAssLibrary) {
            avformat_close_input(&mFormatContext);
            ass_library_done(mAssLibrary);
            mAssLibrary = nullptr;
            return Error::OutOfMemory;
        }
        int ret;
        ret = avformat_open_input(&mFormatContext, mUrl.c_str(), nullptr, nullptr);
        if (ret < 0) {
            avformat_close_input(&mFormatContext);
            return ToError(ret);
        }
        ret = avformat_find_stream_info(mFormatContext, nullptr);
        if (ret < 0) {
            avformat_close_input(&mFormatContext);
            return ToError(ret);
        }
        mSubtitleIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);

        // Auto disable all non-subtitle streams and add embedded font
        for (int i = 0; i < mFormatContext->nb_streams; i++) {
            if (mFormatContext->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
                mFormatContext->streams[i]->discard = AVDISCARD_ALL;
            }
            // Add embedded font

        }

        return Error::Ok;
    }
    Error onTeardown() override {
        avformat_close_input(&mFormatContext);
        ass_library_done(mAssLibrary);
        mAssLibrary = nullptr;
        mSubtitleIndex = -1;
        return Error::Ok;
    }

    // Data Processing


    int toAVStreamIndex(int idx) {
        for (int i = 0; i < mFormatContext->nb_streams; i++) {
            if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
                if (idx == 0) {
                    return i;
                }
                idx--;
            }
        }
        return -1;
    }
private:
    AVFormatContext *mFormatContext = nullptr;
    std::string      mUrl;
    Pad             *mSink;
    Pad             *mSrc;
    int              mSubtitleIndex = -1; //< In AVStream position

    // ASS Here
    ASS_Library     *mAssLibrary = nullptr;
    ASS_Renderer    *mAssRenderer = nullptr;
};

// TODO : It stll not done
// NEKO_REGISTER_ELEMENT(SubtitleFilter, SubtitleFilterImpl);

}




NEKO_NS_END

#endif