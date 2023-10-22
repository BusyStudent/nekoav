#pragma once

#include "../defs.hpp"
#include "../format.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libavutil/error.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}

NEKO_NS_BEGIN

template <typename T>
struct _AlwaysFalse {
    static constexpr bool value = false;
};

template <typename T>
struct FFTraitsBase {
    void doDeref(T *ctxt) {
        static_assert(_AlwaysFalse<T>::value, "FFPtr: deref not implemented for this type");
    }
    void doRef(T *ctxt) {
        static_assert(_AlwaysFalse<T>::value, "FFPtr: ref not implemented for this type");
    }
};

template <typename T>
struct FFTraits;

template <>
struct FFTraits<AVFormatContext> {
    void doDeref(AVFormatContext *&ctxt) {
        avformat_close_input(&ctxt);
    }
    void doRef(AVFormatContext *ctxt) = delete;
};

template <>
struct FFTraits<AVCodecContext> {
    void doDeref(AVCodecContext *&ctxt) {
        avcodec_free_context(&ctxt);
    }
    void doRef(AVCodecContext *ctxt) = delete;
};

template <>
struct FFTraits<AVPacket> {
    void doDeref(AVPacket *&p) {
        av_packet_free(&p);
    }
    void doRef(AVPacket *p) = delete;
    auto doClone(AVPacket *&p) -> AVPacket * {
        return av_packet_clone(p);
    }
};

template <>
struct FFTraits<AVFrame> {
    void doDeref(AVFrame *&p) {
        av_frame_free(&p);
    }
    void doRef(AVFrame *p) = delete;
    auto doClone(AVFrame *&p) -> AVFrame * {
        return av_frame_clone(p);
    }
};

template <typename T, typename Traits = FFTraits<T> >
class FFPtr : protected Traits {
public:
    explicit FFPtr(T *ptr) : mPtr(ptr) { }
    FFPtr() {

    }
    FFPtr(const FFPtr &another) : mPtr(another.mPtr) {
        Traits::doRef(mPtr);
    }
    FFPtr(FFPtr &&another) {
        mPtr = another.mPtr;
        another.mPtr = nullptr;
    }
    ~FFPtr() {
        Traits::doDeref(mPtr);
    }
    void reset(T *newPtr = nullptr) {
        Traits::doDeref(mPtr);
        mPtr = newPtr;
    }
    T *release(T *newPtr = nullptr) {
        T *ret = mPtr;
        mPtr = newPtr;
        return ret;
    }
    T *get() const {
        return mPtr;
    }
    T *operator ->() const {
        return mPtr;
    }
    T &operator *() const {
        return *mPtr;
    }
    FFPtr<T> clone() const {
        return FFPtr<T>(Traits::doClone(mPtr));
    }

    T **operator &() {
        reset();
        return &mPtr;
    }
private:
    T *mPtr { };
};

inline AVPixelFormat ToAVPixelFormat(PixelFormat fmt) {
    return AVPixelFormat(fmt);
}
inline   PixelFormat ToPixelFormat(AVPixelFormat fmt) {
    return PixelFormat(fmt);
}

inline AVSampleFormat ToAVSampleFormat(SampleFormat fmt) {
    return AVSampleFormat(fmt);
}
inline SampleFormat ToSampleFormat(AVSampleFormat fmt) {
    return SampleFormat(fmt);
}

inline bool IsHardwareAVPixelFormat(AVPixelFormat fmt) {
    const AVPixelFormat hwfmts [] = {
        AV_PIX_FMT_OPENCL,
        AV_PIX_FMT_D3D11,
        AV_PIX_FMT_DXVA2_VLD,
        AV_PIX_FMT_D3D11VA_VLD,
        AV_PIX_FMT_CUDA,
        AV_PIX_FMT_VAAPI,
        AV_PIX_FMT_VIDEOTOOLBOX,
        AV_PIX_FMT_QSV,
        AV_PIX_FMT_MMAL,
        AV_PIX_FMT_VDPAU,
    };

    for (auto v : hwfmts) {
        if (v == fmt) {
            return true;
        }
    }

    return false;
}

NEKO_NS_END