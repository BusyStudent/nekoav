#define _NEKO_SOURCE
#include "time.hpp"
#include "media.hpp"
#include "format.hpp"
#include "context.hpp"
#include "elements.hpp"

NEKO_NS_BEGIN

class AudioFrameImpl final : public AudioFrame {
public:
    AudioFrameImpl(SampleFormat format, int channels, int sampleCount) :
        mFormat(format), mChannels(channels), mSampleCount(sampleCount) 
    {
        mDataSize = GetBytesPerSample(format) * channels * sampleCount;
        mData = libc::malloc(mDataSize);
    }
    ~AudioFrameImpl() {
        libc::free(mData);
    }

    void lock() override { }
    void unlock() override { }
    int  format() const override {
        return int(mFormat);
    }
    double duration() const override {
        return mSampleCount / double(mSampleRate);
    }
    double timestamp() const override {
        return mTimestamp;
    }
    // bool isKeyFrame() const override {
        // return true;
    // }

    int linesize(int p) const override {
        if (p == 0) {
            return mDataSize;
        }
        return 0;
    }
    void *data(int p) const override {
        if (p == 0) {
            return mData;
        }
        return nullptr;
    }
    int query(Query q) const override {
        switch (q) {
            case Query::Channels: return mChannels;
            case Query::SampleRate: return mSampleRate;
            case Query::SampleCount: return mSampleCount;
            default: return 0;
        }
    }
    void setSampleRate(int sampleRate) override {
        mSampleRate = sampleRate;
    }
    void setTimestamp(double t) override {
        mTimestamp = t;
    }

private:
    SampleFormat mFormat;
    int mSampleRate = 0;
    int mChannels = 0;
    int mSampleCount = 0;
    int mDataSize = 0; //
    void *mData = nullptr;
    double mTimestamp = 0;
};

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
auto ExternalClock::type() const -> Type {
    return External;
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

Arc<AudioFrame> CreateAudioFrame(SampleFormat fmt, int channels, int samples) {
    return make_shared<AudioFrameImpl>(fmt, channels, samples);
}
MediaController *GetMediaController(View<Element> element) {
    if (!element || !element->context()) {
        return nullptr;
    }
    return element->context()->queryObject<MediaController>();
}

NEKO_NS_END