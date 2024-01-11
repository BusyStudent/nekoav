#define _NEKO_SOURCE
#include "../elements/subtitle.hpp"
#include "../detail/pixutils.hpp"
#include "../detail/base.hpp"
#include "../factory.hpp"
#include "../format.hpp"
#include "../libc.hpp"
#include "../log.hpp"
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

#if __has_include(<ass/ass.h>)
    #define HAVE_ASS
    #include <ass/ass.h>
#endif

#if __has_include(<libass/ass.h>)
    #define HAVE_ASS
    #include <libass/ass.h>
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
        std::lock_guard locker(mMutex);
        mUrl = url;
    }
    void setFont(std::string_view font) override {
        std::lock_guard locker(mMutex);
        mFont = font;

#ifdef HAVE_ASS
        if (mAssRenderer) {
            _configureAssRenderer();
        }
#endif
    }
    void setFamily(std::string_view family) override {
        std::lock_guard locker(mMutex);
        mFamily = family;
#ifdef HAVE_ASS
        if (mAssRenderer) {
            _configureAssRenderer();
        }
#endif
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
        _configureAssLibrary();
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
#if LIBASS_VERSION >= 0x01600010
                        ass_track_set_feature(subtitleStream->mAssTrack, ASS_FEATURE_WRAP_UNICODE, 1);
#endif
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
        {
            std::lock_guard locker(mMutex);
            _configureAssRenderer();
        }
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
        auto idx = mSubtitleIndex.load();
        if (idx < 0 || idx >= mSubtitleStreams.size()) {
            return pushTo(mSrc, resource);
        }
        auto stream = mSubtitleStreams[idx];
        // Get stream
        switch (stream->mType) {
            // Forward
            case SubtitleStream::Bitmap: _processBitmapSubtitle(frame, *stream); break;
#ifdef HAVE_ASS
            case SubtitleStream::Ass: _processAssSubtitle(frame, *stream); break;
#endif
            default: break;

        }

        return pushTo(mSrc, resource);
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
#if LIBASS_VERSION >= 0x01600010
                ass_track_set_feature(stream.mAssTrack, ASS_FEATURE_WRAP_UNICODE, 1);
#endif
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

    void _processBitmapSubtitle(View<MediaFrame> input, SubtitleStream &stream) {
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
            return;
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
                    return;
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
    }
#ifdef HAVE_ASS
    void _processAssSubtitle(View<MediaFrame> input, SubtitleStream &stream) {
        int isChange = 0;
        std::unique_lock lock(mMutex);
        if (!mAssConfigured) {
            mAssConfigured = true;
            ass_set_frame_size(mAssRenderer, input->width(), input->height());
            ass_set_storage_size(mAssRenderer, input->width(), input->height());
        }
        ASS_Image *image = ass_render_frame(mAssRenderer, stream.mAssTrack, input->timestamp() * 1000, &isChange);
        lock.unlock();
        if (isChange) {
            NEKO_LOG("[ass] ass_render_frame Change in {} s\n", input->timestamp());
        }
        if (!image || image->w == 0 || image->h == 0) {
            return;
        }
        if (input->makeWritable()) {
            for (auto cur = image; cur != nullptr; cur = cur->next) {
                _blendSingleImage(input, cur);
            }
        }
    }
    void _blendSingleImage(View<MediaFrame> input, const ASS_Image *image) {
        uint8_t *dst = reinterpret_cast<uint8_t*>(input->data(0));
        const int pitch = input->linesize(0);
        const int width = input->width();
        const int height = input->height();

        // Read src Pixels RGBA
        // Code from MPV ass_mp.c
        const uint32_t r = (image->color >> 24) & 0xff;
        const uint32_t g = (image->color >> 16) & 0xff;
        const uint32_t b = (image->color >>  8) & 0xff;
        const uint32_t a = 0xff - (image->color & 0xff);
        for (int x = 0; x < image->w; x++) {
            for (int y = 0; y < image->h; y++) {
                const uint32_t v = image->bitmap[y * image->stride + x];
                const int rr = (r * a * v);
                const int gg = (g * a * v);
                const int bb = (b * a * v);
                const int aa =      a * v;

                uint32_t dstr = dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 0]; //< R
                uint32_t dstg = dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 1]; //< G
                uint32_t dstb = dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 2]; //< B
                uint32_t dsta = dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 3]; //< A

                dstr = (rr       + dstr * (255 * 255 - aa)) / (255 * 255);
                dstg = (gg       + dstg * (255 * 255 - aa)) / (255 * 255);
                dstb = (bb       + dstb * (255 * 255 - aa)) / (255 * 255);
                dsta = (aa * 255 + dsta * (255 * 255 - aa)) / (255 * 255);

                dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 0] = dstr;
                dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 1] = dstg;
                dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 2] = dstb;
                dst[(image->dst_y + y) * pitch + (image->dst_x + x) * 4 + 3] = dsta;
            }
        }
    }
    void _assLog(int level, const char *fmt, va_list va) {
        if (level >= 6) {
            return;
        }
        NEKO_LOG("ASS[{}]: {}", level, libc::vasprintf(fmt, va));
    }
    void _configureAssLibrary() {
        ass_set_extract_fonts(mAssLibrary, 1);
        ass_set_message_cb(mAssLibrary, [](int level, const char *fmt, va_list varg, void *self) {
            static_cast<SubtitleFilterImpl*>(self)->_assLog(level, fmt, varg);
        }, this);

        // Check provders
        ASS_DefaultFontProvider *providers = nullptr;
        size_t numOfProviders = 0;
        ass_get_available_font_providers(mAssLibrary, &providers, &numOfProviders);
        for (size_t n = 0; n < numOfProviders; n++) {
            NEKO_DEBUG(providers[n]);
        }
        if (numOfProviders == 2) {
            // No font providers
            NEKO_DEBUG("No OS Font providers");
        }
        ::free(providers);
    }
    void _configureAssRenderer() {
        // Set Font to renderer
        ass_set_fonts(
            mAssRenderer, 
            mFont.empty() ? nullptr : mFont.c_str(), 
            mFamily.empty() ? nullptr : mFamily.c_str(), 
            ASS_FONTPROVIDER_AUTODETECT, 
            nullptr, 
            0
        );
    }
#endif
private:
    std::string      mUrl;
    std::string      mFont;
    std::string      mFamily;
    Pad             *mSink = nullptr;
    Pad             *mSrc = nullptr;
    Atomic<int>      mSubtitleIndex {-1}; //< In Subtitle array position

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

    // Subtitle Data here
    Vec<std::string> mSubtitles; 
    Vec<SubtitleStream*> mSubtitleStreams; //< Streams of subtitle

    // Mutex here, protect some shared vae like ASS_Renderer *
    std::mutex       mMutex;
};

NEKO_REGISTER_ELEMENT(SubtitleFilter, SubtitleFilterImpl);

}




NEKO_NS_END
