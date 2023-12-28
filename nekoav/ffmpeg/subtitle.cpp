#define _NEKO_SOURCE
#include "../elements/subtitle.hpp"
#include "../detail/base.hpp"
#include "../factory.hpp"
#include "../format.hpp"
#include "../pad.hpp"
#include "common.hpp"
#include <functional>
#include <algorithm>
#include <array>

extern "C" {
    #include <libswscale/swscale.h>

#if __has_include(<ass.h>)
    #define HAVE_ASS
    #include <ass.h>
#endif
}

NEKO_NS_BEGIN

namespace FFmpeg {

// From ffmpeg
static constexpr std::array<std::string_view, 10> FontMimeTypes {
    "font/ttf",
    "font/otf",
    "font/sfnt",
    "font/woff",
    "font/woff2",
    "application/font-sfnt",
    "application/font-woff",
    "application/x-truetype-font",
    "application/vnd.ms-opentype",
    "application/x-font-ttf"
};

// FIXME: ASS subtitle still has blend bug and can not get font problem
// FIXME: add plain text subtitle

// Subtitle Process function
class SubtitleStream {
public:
    ~SubtitleStream() {
        for (auto &data : mAVSubtitles) {
            avsubtitle_free(&data);
        }

#ifdef HAVE_ASS
        ass_free_track(mAssTrack);
        ass_renderer_done(mAssRenderer);
#endif
        sws_freeContext(mSubConvertContext);
    }

    Vec<AVSubtitle> mAVSubtitles; //< All no-ass subtitles data (bitmap / text)
    SwsContext   *mSubConvertContext = nullptr; //< SwsContext used to convert bitmap subtitle;

#ifdef HAVE_ASS
    ASS_Track    *mAssTrack = nullptr;
    ASS_Renderer *mAssRenderer = nullptr;
#endif

    enum {
        None,
        Bitmap,
        Text,
        Ass,
    } mType = None;
};

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
        return mSubtitles;
    }
    void setSubtitle(int idx) override {
        mSubtitleIndex = idx;
    }

    Error onInitialize() override {
#ifdef HAVE_ASS
        mAssLibrary = ass_library_init();
        if (!mAssLibrary) {
            onTeardown();
            return Error::OutOfMemory;
        }
        ass_set_extract_fonts(mAssLibrary, 1);
        mAssRenderer = ass_renderer_init(mAssLibrary);
        if (!mAssRenderer) {
            onTeardown();
            return Error::OutOfMemory;
        }
#endif
        if (auto err = _loadSubtitleByFFmpeg(); err != Error::Ok) {
            onTeardown();
            return err;
        }

        return Error::Ok;
    }
    Error onTeardown() override {
        for (auto ptr : mSubtitleStreams) {
            delete ptr;
        }
        mSubtitleStreams.clear();
        mCurrentAVSubtitle = nullptr;
        mSubtitleIndex = -1;
        mSubtitles.clear();
        mSubConverted = false;
        std::free(mRGBABuffer);

#ifdef HAVE_ASS
        ass_renderer_done(mAssRenderer);
        ass_library_done(mAssLibrary);
        mAssLibrary = nullptr;
        mAssRenderer = nullptr;
        mAssConfigured = false;
#endif
        return Error::Ok;
    }
    Error _loadSubtitleByFFmpeg() {
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
            auto stream = formatContext->streams[i];
            if (stream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
                stream->discard = AVDISCARD_ALL;
            }
            else {
                // IS subtitle
                streamMapping[i] = subtitleIndex;
                subtitleIndex += 1;

                // Make codec Context
                auto ctxt = OpenCodecContext4(stream->codecpar);
                codecContext.push_back(ctxt);

                // Add it to subtitle streams
                auto subtitleStream = new SubtitleStream;
                mSubtitleStreams.push_back(subtitleStream);

#ifdef HAVE_ASS
                // Process private parts
                if (ctxt->subtitle_header) {
                    if (!subtitleStream->mAssTrack) {
                        subtitleStream->mAssTrack = ass_new_track(mAssLibrary);
                    }
                    ass_process_codec_private(subtitleStream->mAssTrack, (char*) ctxt->subtitle_header, ctxt->subtitle_header_size);
                }
#endif

                // Add name
                AVDictionary *metadata = stream->metadata;
                auto title = av_dict_get(metadata, "title", nullptr, 0);
                if (title) {
                    mSubtitles.push_back(std::string(title->value));
                }
                else {
                    mSubtitles.push_back(std::string());
                }
            }
#ifdef HAVE_ASS
            // Add embedded font
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT) {
                AVDictionary *metadata = stream->metadata;
                auto minetype = av_dict_get(metadata, "mimetype", nullptr, 0);
                auto filename = av_dict_get(metadata, "filename", nullptr, 0);
                if (!minetype || !filename) {
                    continue;
                }
                std::string_view m(minetype->value);
                bool got = false;
                for (const auto type : FontMimeTypes) {
                    if (type == m) {
                        got = true;
                        break;
                    }
                }
                if (!got) {
                    continue;
                }
                ass_add_font(
                    mAssLibrary, 
                    filename->value,
                    (const char*) stream->codecpar->extradata, 
                    stream->codecpar->extradata_size
                );
            }
#endif
        }

#ifdef HAVE_ASS
        // Set Font to renderer
        ass_set_fonts(mAssRenderer, nullptr, nullptr, 0, nullptr, 0);
#endif

        // Mapping stream index in our subtitle index space
        mSubtitleIndex = streamMapping[mSubtitleIndex];

        _readAllSubtitleFrames(formatContext, streamMapping, codecContext);

        // Done
        for (auto &ctxt : codecContext) {
            avcodec_free_context(&ctxt);
        }
        avformat_close_input(&formatContext);
        return Error::Ok;
    }
    Error _readAllSubtitleFrames(AVFormatContext *formatContext, const Vec<int> &streamMapping, const Vec<AVCodecContext *> &codecContexts) {
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
            
            // Set subtitle pts in ms;
            int64_t pts = packet->pts * av_q2d(stream->time_base) * 1000;
            int64_t duration = packet->duration * av_q2d(stream->time_base) * 1000;
            subtitle.pts = pts;

            // Add subtitle
            if (subtitle.num_rects > 0) {
                switch (subtitle.rects[0]->type) {
                    case SUBTITLE_BITMAP: _addBitmapSubtitle(subtitle, *mSubtitleStreams[idx]); break;
                    case SUBTITLE_TEXT: _addTextSubtitle(subtitle, *mSubtitleStreams[idx]); break;
                    case SUBTITLE_ASS: //< The ass decoder works not fine in my computer, it alwasy return 
                                       //< 0 end_display_time, so we set it in ourself
                        _addAssSubtitle(subtitle, *mSubtitleStreams[idx], duration); break;
                    default: avsubtitle_free(&subtitle); break;
                }
            }

            av_packet_unref(packet);
        }

        av_packet_free(&packet);
        return Error::Ok;
    }

    // Data Processing
    Error onSinkPush(View<Pad> pad, View<Resource> resource) override {
        auto frame = resource.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        Arc<MediaFrame> out;
        auto stream = mSubtitleStreams[mSubtitleIndex];
        // Get stream
        switch (stream->mType) {
            // Forward
            case SubtitleStream::Bitmap: out = _processBitmapSubtitle(frame, *stream); break;
#ifdef HAVE_ASS
            case SubtitleStream::Ass: out = _processAssSubtitle(frame, *stream); break;
#endif
            default: pushTo(mSrc, resource);

        }

        return pushTo(mSrc, out);
    }

    void _resizeRGBABuffer(int w, int h) {
        if (w * h * 4 > mRGBABufferSize) {
            mRGBABufferSize = w * h * 4;
            mRGBABuffer = (uint32_t*) std::realloc(mRGBABuffer, mRGBABufferSize);
        }
    }

    void _addTextSubtitle(AVSubtitle &subtitle, SubtitleStream &stream) {
        stream.mType = SubtitleStream::Text;
        stream.mAVSubtitles.emplace_back(subtitle);
    }
    void _addBitmapSubtitle(AVSubtitle &subtitle, SubtitleStream &stream) {
        stream.mType = SubtitleStream::Bitmap;
        stream.mAVSubtitles.emplace_back(subtitle);
    }
    void _addAssSubtitle(AVSubtitle &subtitle, SubtitleStream &stream, int64_t pktDuration) {
#ifdef HAVE_ASS
        stream.mType = SubtitleStream::Ass;
        for (int i = 0; i < subtitle.num_rects; ++i) {
            if (!subtitle.rects[i]->ass) {
                continue;
            }
            if (!stream.mAssTrack) {
                stream.mAssTrack = ass_new_track(mAssLibrary);
            }
            int duration = subtitle.end_display_time - subtitle.start_display_time;
            if (duration == 0) {
                // Emm, decoder return 0 duration, use the packet duration
                duration = pktDuration;
            }
            ass_process_chunk(
                stream.mAssTrack, 
                subtitle.rects[i]->ass, 
                ::strlen(subtitle.rects[i]->ass),
                subtitle.pts + subtitle.start_display_time,
                duration
            );
        }
#endif
        avsubtitle_free(&subtitle);
    }

    Arc<MediaFrame> _processBitmapSubtitle(View<MediaFrame> input, SubtitleStream &stream) {
        // Search target pts
        int64_t pts = input->timestamp() * 1000;
        if (mCurrentAVSubtitle) {
            // Check we in previous subtitle
            auto duration = mCurrentAVSubtitle->end_display_time - mCurrentAVSubtitle->start_display_time;
            if (pts < mCurrentAVSubtitle->pts || pts > mCurrentAVSubtitle->pts + duration) {
                mCurrentAVSubtitle = nullptr;
                mSubConverted = false;
            }
        }

        if (!mCurrentAVSubtitle) {
            for (const auto &cur : stream.mAVSubtitles) {
                auto duration = cur.end_display_time - cur.start_display_time;
                if (pts >= cur.pts && cur.pts + duration >= pts) {
                    // got
                    mCurrentAVSubtitle = &cur;
                    break;
                }
                if (cur.pts + duration > pts) {
                    break;
                }
            }
        }
        if (!mCurrentAVSubtitle) {
            // No Subtitle in this range, break
            return input->shared_from_this<MediaFrame>();
        }

        auto sub = mCurrentAVSubtitle;

        // Here use ffplay subtitle code
        int x = sub->rects[0]->x;
        int y = sub->rects[0]->y;
        int w = sub->rects[0]->w;
        int h = sub->rects[0]->h;

        if (!mSubConverted) {
            for (int i = 0; i < sub->num_rects; i++) {
                AVSubtitleRect *rect = sub->rects[i];

                _resizeRGBABuffer(rect->w, rect->h); 

                x = rect->x;
                y = rect->y;
                w = rect->w;
                h = rect->h;
                
                // int x = std::clamp(rect->x, 0, input->width());
                // int y = std::clamp(rect->y, 0, input->height());
                // int w = std::clamp(rect->w, 0, input->width() - x);
                // int h = std::clamp(rect->h, 0, input->height() - y);

                stream.mSubConvertContext = sws_getCachedContext(
                    stream.mSubConvertContext,
                    rect->w,
                    rect->h,
                    AV_PIX_FMT_PAL8,
                    rect->w,
                    rect->h,
                    AV_PIX_FMT_RGBA,
                    0,
                    nullptr,
                    nullptr,
                    nullptr
                );
                if (!stream.mSubConvertContext) {
                    // Fail
                    return input->shared_from_this<MediaFrame>();
                }
                int dstLinesize[4] {rect->w * 4, 0};
                uint8_t *dstData[4] {
                    (uint8_t*) mRGBABuffer,
                    nullptr
                };
                int n = sws_scale(
                    stream.mSubConvertContext,
                    rect->data,
                    rect->linesize,
                    0,
                    rect->h,
                    dstData,
                    dstLinesize
                );
                if (n < 0) {
                    // Error
                    ::abort();
                }
            }
            mSubConverted = true;
        }
        // Blend with current image
        uint32_t *buffer = (uint32_t*) input->data(0);
        // Let it writeable
        input->makeWritable();
        int imageWidth = input->width();

        for (int i = 0; i < w; i++) {
            for (int k = 0; k < h; k++) {
                uint32_t &dstPixel = buffer[(y + k) * imageWidth  + (x + i)];
                uint32_t srcPixel = mRGBABuffer[k * w + i];

                if (srcPixel != 0) {
                    dstPixel = srcPixel;
                }
            }
        }

        return input->shared_from_this<MediaFrame>();
    }
#ifdef HAVE_ASS
    Arc<MediaFrame> _processAssSubtitle(View<MediaFrame> input, SubtitleStream &stream) {
        int isChange = 0;
        if (!mAssConfigured) {
            mAssConfigured = true;
            ass_set_frame_size(mAssRenderer, input->width(), input->height());
            ass_set_storage_size(mAssRenderer, input->width(), input->height());
        }
        ASS_Image *image = ass_render_frame(mAssRenderer, stream.mAssTrack, input->timestamp() * 1000, &isChange);
        if (isChange) {
            ::fprintf(stderr, "[ass] ass_render_frame Change in %lf s\n", input->timestamp());
        }
        if (!image || image->w == 0 || image->h == 0) {
            return input->shared_from_this<MediaFrame>();
        }
        _blendAssImage(input, image);
        return input->shared_from_this<MediaFrame>();
    }

    void _blendAssImage(View<MediaFrame> input, ASS_Image *assImage) {
        input->makeWritable();

        // Blend texture
        uint32_t *dstPixels = (uint32_t *) input->data(0);
        int width = input->width();
        int height = input->height();
        for (auto image = assImage; image; image = image->next) {
            for (int x = 0; x < image->w; x++) {
                for (int y = 0; y < image->h; y++) {
                    uint8_t alpha = image->bitmap[y * image->stride + x];

                    if (alpha == 0) {
                        continue;
                    }

                    uint32_t srcPixel = image->color;
                    uint32_t &dstPixel = dstPixels[(image->dst_y + y) * width + (image->dst_x + x)];

                    uint8_t r = _r(srcPixel);
                    uint8_t g = _g(srcPixel);
                    uint8_t b = _b(srcPixel);
                    uint8_t opicity = 255 - _a(srcPixel);
                    if (image == assImage) {
                        // Just over
                        dstPixel = _rgba(r, g, b, _a(dstPixel));
                    }
                    else {
                        // Blend alpha to srcPixel
                        int32_t k = alpha * opicity / 255;
                        r = (r * k + (255 - k) * _r(dstPixel)) / 255;
                        g = (g * k + (255 - k) * _g(dstPixel)) / 255;
                        b = (b * k + (255 - k) * _b(dstPixel)) / 255;

                        dstPixel = _rgba(r, g, b, _a(dstPixel));
                    }
                }
            }
        }
    }
    static uint8_t _r(uint32_t pixel) noexcept {
        return (pixel >> 24) & 0xFF;
    }
    static uint8_t _g(uint32_t pixel) noexcept {
        return (pixel >> 16) & 0xFF;
    }
    static uint8_t _b(uint32_t pixel) noexcept {
        return (pixel >> 8) & 0xFF;
    }
    static uint8_t _a(uint32_t pixel) noexcept { 
        return pixel & 0xFF; 
    }
    static uint32_t _rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
        return (r << 24) | (g << 16) | (b << 8) | a;
    }
#endif
private:
    std::string      mUrl;
    Pad             *mSink;
    Pad             *mSrc;
    int              mSubtitleIndex = -1; //< In Subtitle array position

#ifdef HAVE_ASS
    // ASS Here
    ASS_Library     *mAssLibrary = nullptr;
    ASS_Renderer    *mAssRenderer = nullptr;
    bool             mAssConfigured = false; //< Does ass renderer configured
#endif

    // Bitmap / Text Here
    const AVSubtitle *mCurrentAVSubtitle = nullptr;
    bool              mSubConverted = false;

    // Common Here
    uint32_t        *mRGBABuffer = nullptr;
    size_t           mRGBABufferSize = 0;

    Vec<std::string> mSubtitles; 
    Vec<SubtitleStream*> mSubtitleStreams; //< Streams of subtitle

};

// TODO : It stll not done
NEKO_REGISTER_ELEMENT(SubtitleFilter, SubtitleFilterImpl);

}




NEKO_NS_END
