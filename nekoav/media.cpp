#define _NEKO_SOURCE
#include "time.hpp"
#include "media.hpp"
#include "format.hpp"
#include "context.hpp"
#include "elements.hpp"
#include <memory_resource>

NEKO_NS_BEGIN

NEKO_IMPL_BEGIN

class MediaFrameImpl final : public MediaFrame {
public:
    MediaFrameImpl(SampleFormat format, int channels, int sampleCount) :
        mSampleFormat(format), mChannels(channels), mSampleCount(sampleCount) 
    {
        mType = Audio;
        if (IsSampleFormatPlanar(format)) {
            // Planar
            for (int n = 0; n < channels; n++) {
                mLinesize[n] = GetBytesPerSample(format) * sampleCount;
                mSize[n] = GetBytesPerSample(format) * sampleCount;
                mData[n] = mPool->allocate(mLinesize[n]);
            }
        }
        else {
            // Packed
            mLinesize[0] = GetBytesPerSample(format) * channels * sampleCount;
            mSize[0] = GetBytesPerSample(format) * channels * sampleCount;
            mData[0] = mPool->allocate(mLinesize[0]);
        }
    }
    MediaFrameImpl(PixelFormat format, int width, int height) : 
        mPixelFormat(format), mWidth(width), mHeight(height)
    {
        switch (format) {
            case PixelFormat::RGBA: {
                mData[0] = mPool->allocate(width * height * 4);
                mSize[0] = width * height * 4;
                mLinesize[0] = width * 4;
                break;
            }
            default: ::abort();
        }
        mType = Video;
    }
    ~MediaFrameImpl() {
        for (size_t n = 0; n < sizeof(mData) / sizeof(void*); n++) {
            if (!mData[n]) {
                continue;
            }
            mPool->deallocate(mData[n], mSize[n]);
        }
    }

    int  format() const override {
        if (mType == Video) {
            return int(mPixelFormat);
        }
        else {
            return int(mSampleFormat);
        }
    }
    double duration() const override {
        return mSampleCount / double(mSampleRate);
    }
    double timestamp() const override {
        return mTimestamp;
    }
    bool makeWritable() override {
        return true;
    }

    int linesize(int p) const override {
        return mLinesize[p];
    }
    void *data(int p) const override {
        return mData[p];
    }
    int query(Value v) const override {
        switch (v) {
            // Audio
            case Value::Channels: return mChannels;
            case Value::SampleRate: return mSampleRate;
            case Value::SampleCount: return mSampleCount;
            // Video
            case Value::Width: return mWidth;
            case Value::Height: return mHeight;
            default: return 0;
        }
    }
    bool set(Value v, const void *p) override {
        switch (v) {
            case Value::SampleRate: mSampleRate = *static_cast<const int *>(p); break;
            case Value::Timestamp: mTimestamp = *static_cast<const double *>(p); break;
            case Value::Duration: mDuration = *static_cast<const double *>(p); break;
            default : return false;
        }
        return true;
    }
private:
    // Header
    enum {
        Video,
        Audio
    } mType;

    // Video part
    PixelFormat mPixelFormat = PixelFormat::None;
    int mHeight = 0;
    int mWidth = 0;

    // Audio part
    SampleFormat mSampleFormat = SampleFormat::None;
    int mSampleRate = 0;
    int mChannels = 0;
    int mSampleCount = 0;

    // Common part
    void *mData[8]     {0};
    int   mSize[8]     {0};
    int   mLinesize[8] {0};

    double mTimestamp = 0.0;
    double mDuration = 0.0;

    std::pmr::memory_resource* mPool = std::pmr::get_default_resource();
};

NEKO_IMPL_END

// MediaReader
static Box<MediaReader> (*readerCreate)() = nullptr;
void MediaReader::_registers(Box<MediaReader> (*fn)()) noexcept {
    readerCreate = fn;
}
Box<MediaReader> MediaReader::create() {
    if (readerCreate) {
        return readerCreate();
    }
    return nullptr;
}

ExternalClock::ExternalClock() {

}
ExternalClock::~ExternalClock() {

}
double ExternalClock::position() const {
    if (mPaused) {
        return mCurrent;
    }

    // Calc
    return double(GetTicks() - mTicks) / 1000.0;
}
auto ExternalClock::type() const -> ClockType {
    return ClockType::External;
}
void ExternalClock::start() {
    if (!mPaused) {
        return;
    }
    mTicks = GetTicks() - mCurrent * 1000;
    mPaused = false;
}
void ExternalClock::pause() {
    if (mPaused) {
        return;
    }
    mCurrent = double(GetTicks() - mTicks) / 1000.0;
    mPaused = true;
}
void ExternalClock::setPosition(double position) {
    mTicks = GetTicks() - position * 1000;
    mCurrent = position;
}

Arc<MediaFrame> CreateAudioFrame(SampleFormat fmt, int channels, int samples) {
    return make_shared<MediaFrameImpl>(fmt, channels, samples);
}
Arc<MediaFrame> CreateVideoFrame(PixelFormat fmt, int width, int height) {
    return make_shared<MediaFrameImpl>(fmt, width, height);
}
MediaController *GetMediaController(View<Element> element) {
    if (!element || !element->context()) {
        return nullptr;
    }
    return element->context()->queryObject<MediaController>();
}


NEKO_NS_END