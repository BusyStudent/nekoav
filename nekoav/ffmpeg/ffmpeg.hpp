#pragma once

#include "../defs.hpp"
#include "../format.hpp"
#include "../error.hpp"
#include "../property.hpp"
#include "../media/io.hpp"
#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/error.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}

NEKO_NS_BEGIN

namespace FFmpeg {

inline AVPixelFormat ToAVPixelFormat(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::None:    return AV_PIX_FMT_NONE;
        case PixelFormat::YUV420P: return AV_PIX_FMT_YUV420P;
        case PixelFormat::YUV422P: return AV_PIX_FMT_YUV422P;
        case PixelFormat::YUV444P: return AV_PIX_FMT_YUV444P;
        case PixelFormat::YUV410P: return AV_PIX_FMT_YUV410P;
        case PixelFormat::YUV411P: return AV_PIX_FMT_YUV411P;
        case PixelFormat::YUVJ420P: return AV_PIX_FMT_YUVJ420P;
        case PixelFormat::YUVJ422P: return AV_PIX_FMT_YUVJ422P;
        case PixelFormat::YUVJ444P: return AV_PIX_FMT_YUVJ444P;
        case PixelFormat::UYVY422: return AV_PIX_FMT_UYVY422;
        case PixelFormat::UYYVYY411: return AV_PIX_FMT_UYYVYY411;
        case PixelFormat::BGR8: return AV_PIX_FMT_BGR8;
        case PixelFormat::BGR4: return AV_PIX_FMT_BGR4;
        case PixelFormat::BGR4_BYTE: return AV_PIX_FMT_BGR4_BYTE;
        case PixelFormat::RGB8: return AV_PIX_FMT_RGB8;
        case PixelFormat::RGB4: return AV_PIX_FMT_RGB4;
        case PixelFormat::NV12: return AV_PIX_FMT_NV12;
        case PixelFormat::NV21: return AV_PIX_FMT_NV21;
        
        case PixelFormat::RGBA: return AV_PIX_FMT_RGBA;
        case PixelFormat::BGRA: return AV_PIX_FMT_BGRA;
        case PixelFormat::ARGB: return AV_PIX_FMT_ARGB;

        // Hardware
        case PixelFormat::DXVA2: return AV_PIX_FMT_DXVA2_VLD;
        case PixelFormat::D3D11: return AV_PIX_FMT_D3D11;

        default: return AV_PIX_FMT_NONE;
    }
}
inline   PixelFormat ToPixelFormat(AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_NONE: return PixelFormat::None;
        case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
        case AV_PIX_FMT_YUV422P: return PixelFormat::YUV422P;
        case AV_PIX_FMT_YUV444P: return PixelFormat::YUV444P;
        case AV_PIX_FMT_YUV410P: return PixelFormat::YUV410P;
        case AV_PIX_FMT_YUV411P: return PixelFormat::YUV411P;
        case AV_PIX_FMT_YUVJ420P: return PixelFormat::YUVJ420P;
        case AV_PIX_FMT_YUVJ422P: return PixelFormat::YUVJ422P;
        case AV_PIX_FMT_YUVJ444P: return PixelFormat::YUVJ444P;
        case AV_PIX_FMT_UYVY422: return PixelFormat::UYVY422;
        case AV_PIX_FMT_UYYVYY411: return PixelFormat::UYYVYY411;
        case AV_PIX_FMT_BGR8: return PixelFormat::BGR8;
        case AV_PIX_FMT_BGR4: return PixelFormat::BGR4;
        case AV_PIX_FMT_BGR4_BYTE: return PixelFormat::BGR4_BYTE;
        case AV_PIX_FMT_RGB8: return PixelFormat::RGB8;
        case AV_PIX_FMT_RGB4: return PixelFormat::RGB4;
        case AV_PIX_FMT_NV12: return PixelFormat::NV12;
        case AV_PIX_FMT_NV21: return PixelFormat::NV21;
        
        case AV_PIX_FMT_RGBA: return PixelFormat::RGBA;
        case AV_PIX_FMT_BGRA: return PixelFormat::BGRA;
        case AV_PIX_FMT_ARGB: return PixelFormat::ARGB;

        // Hardware
        case AV_PIX_FMT_DXVA2_VLD: return PixelFormat::DXVA2;
        case AV_PIX_FMT_D3D11: return PixelFormat::D3D11;
        default: return PixelFormat::None;
    }
}

inline AVSampleFormat ToAVSampleFormat(SampleFormat fmt) noexcept {
    switch (fmt) {
        case SampleFormat::None: return AV_SAMPLE_FMT_NONE;
        case SampleFormat::U8: return AV_SAMPLE_FMT_U8;
        case SampleFormat::S16: return AV_SAMPLE_FMT_S16;
        case SampleFormat::S32: return AV_SAMPLE_FMT_S32;
        case SampleFormat::FLT: return AV_SAMPLE_FMT_FLT;
        case SampleFormat::DBL: return AV_SAMPLE_FMT_DBL;

        case SampleFormat::U8P: return AV_SAMPLE_FMT_U8P;
        case SampleFormat::S16P: return AV_SAMPLE_FMT_S16P;
        case SampleFormat::S32P: return AV_SAMPLE_FMT_S32P;
        case SampleFormat::FLTP: return AV_SAMPLE_FMT_FLTP;
        case SampleFormat::DBLP: return AV_SAMPLE_FMT_DBLP;

        default: return AV_SAMPLE_FMT_NONE;
    }
}
inline SampleFormat ToSampleFormat(AVSampleFormat fmt) noexcept {
    switch (fmt) {
        case AV_SAMPLE_FMT_NONE: return SampleFormat::None;
        case AV_SAMPLE_FMT_U8: return SampleFormat::U8;
        case AV_SAMPLE_FMT_S16: return SampleFormat::S16;
        case AV_SAMPLE_FMT_S32: return SampleFormat::S32;
        case AV_SAMPLE_FMT_FLT: return SampleFormat::FLT;
        case AV_SAMPLE_FMT_DBL: return SampleFormat::DBL;

        case AV_SAMPLE_FMT_U8P: return SampleFormat::U8P;
        case AV_SAMPLE_FMT_S16P: return SampleFormat::S16P;
        case AV_SAMPLE_FMT_S32P: return SampleFormat::S32P;
        case AV_SAMPLE_FMT_FLTP: return SampleFormat::FLTP;
        case AV_SAMPLE_FMT_DBLP: return SampleFormat::DBLP;

        default: return SampleFormat::None;
    }
}

inline bool IsHardwareAVPixelFormat(AVPixelFormat fmt) noexcept {
    // const AVPixelFormat hwfmts [] = {
    //     AV_PIX_FMT_OPENCL,
    //     AV_PIX_FMT_D3D11,
    //     AV_PIX_FMT_DXVA2_VLD,
    //     AV_PIX_FMT_D3D11VA_VLD,
    //     AV_PIX_FMT_CUDA,
    //     AV_PIX_FMT_VAAPI,
    //     AV_PIX_FMT_VIDEOTOOLBOX,
    //     AV_PIX_FMT_QSV,
    //     AV_PIX_FMT_MMAL,
    //     AV_PIX_FMT_VDPAU,
    // };

    // for (auto v : hwfmts) {
    //     if (v == fmt) {
    //         return true;
    //     }
    // }

    // return false;
    return av_pix_fmt_desc_get(fmt)->flags & AV_PIX_FMT_FLAG_HWACCEL;
}

inline std::string FormatErrorCode(int code) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(code, buf, AV_ERROR_MAX_STRING_SIZE);
    return buf;
}
inline Error ToError(int code) noexcept {
    switch (code) {
        case AVERROR_INVALIDDATA: return Error::UnsupportedMediaFormat;
        case AVERROR(ENOMEM): return Error::OutOfMemory;
        case AVERROR(EAGAIN): return Error::TemporarilyUnavailable;
        case AVERROR(EINVAL): return Error::InvalidArguments;
        case AVERROR_EXIT   : return Error::Interrupted;
        case AVERROR_EOF    : return Error::EndOfFile;
        default             : return Error::Unknown;
    }
}

inline AVCodecContext *OpenCodecContext4(AVCodecParameters *codecpar) {
    auto codec = avcodec_find_decoder(codecpar->codec_id);
    auto ctxt = avcodec_alloc_context3(codec);
    if (!ctxt) {
        return nullptr;
    }
    int ret = avcodec_parameters_to_context(ctxt, codecpar);
    if (ret < 0) {
        avcodec_free_context(&ctxt);
        return nullptr;
    }
    ret = avcodec_open2(ctxt, ctxt->codec, nullptr);
    if (ret < 0) {
        avcodec_free_context(&ctxt);
        return nullptr;
    }
    return ctxt;
}
/**
 * @brief Convert Open
 * 
 * @param options 
 * @return AVDictionary* 
 */
inline AVDictionary *ParseOpenOptions(const Properties *options) {
    if (!options) {
        return nullptr;
    }
    AVDictionary *dict = nullptr;
    for (const auto &[key, value] : *options) {
        // TODO : Convert
        if (key == Properties::HttpUserAgent) {
            av_dict_set(&dict, "user_agent", value.toString().c_str(), 0);
        }
        else if (key == Properties::HttpReferer) {
            av_dict_set(&dict, "referer", value.toString().c_str(), 0);
        }
    }
    return dict;
}

class AVDictIterater {
public:
    AVDictIterater(AVDictionary *dict) : mDict(dict) {
        if (dict) {
            ++(*this);
        }
    }
    void operator ++() {
        mCur = av_dict_get(mDict, "", mCur, AV_DICT_IGNORE_SUFFIX);
    }
    bool operator ==(const AVDictIterater &other) const noexcept {
        return mCur == other.mCur;
    }
    bool operator !=(const AVDictIterater &other) const noexcept {
        return mCur != other.mCur;
    }
    AVDictionaryEntry &operator *() const noexcept {
        return *mCur;
    }
    AVDictionaryEntry *operator ->() const noexcept {
        return mCur;
    }
private:
    AVDictionary      *mDict = nullptr;
    AVDictionaryEntry *mCur = nullptr;
};
class AVDictIteration {
public:
    auto begin() const noexcept {
        return AVDictIterater(mDict);
    }
    auto end() const noexcept {
        return AVDictIterater(nullptr);
    }
    AVDictionary *mDict;
};

inline auto IterDict(AVDictionary *dict) {
    return AVDictIteration(dict);
}
/**
 * @brief Wrap a IOStream to AVIOContext
 * 
 * @param bufferSize 
 * @param io 
 * @return AVIOContext* 
 */
inline AVIOContext *WrapIOStream(int64_t bufferSize, IOStream *io) {
    return avio_alloc_context(
        (uint8_t*) av_malloc(bufferSize),
        bufferSize,
        io->isWritable(),
        io,
        io->isReadable() ?
        [](void *self, uint8_t *buf, int bufSize) -> int {
            return static_cast<IOStream *>(self)->read(buf, bufSize);
        } : nullptr,
        io->isWritable() ?
        [](void *self, uint8_t *buf, int bufSize) -> int {
            return static_cast<IOStream *>(self)->write(buf, bufSize);
        } : nullptr,
        io->isSeekable() ?
        [](void *self, int64_t offset, int whence) -> int64_t {
            return static_cast<IOStream *>(self)->seek(offset, whence);
        } : nullptr
    );
}
}

NEKO_NS_END