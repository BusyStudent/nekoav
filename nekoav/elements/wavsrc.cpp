#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../format.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../libc.hpp"
#include "../pad.hpp"
#include "../log.hpp"
#include "wavsrc.hpp"
#include <cstring>

NEKO_NS_BEGIN

struct WaveHeader {
    uint8_t chunkId[4];
    int32_t chunkSize;
    uint8_t format[4];
};
struct WaveFmt {
    uint8_t chunkId[4];
    int32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    int32_t sampleRate;
    int32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
struct WaveData {
    uint8_t chunkId[4];
    int32_t chunkSize;
};

class WavSourceImpl final : public Template::GetThreadImpl<WavSource> {
public:
    WavSourceImpl() {
        mSrc = addOutput("src");
    }
    Error onInitialize() override {
        mFileMapping = libc::mmap(mUrl.c_str());
        if (mFileMapping.empty()) {
            return Error::FileNotFound;
        }
        if (mFileMapping.size() < sizeof(mHeader) + sizeof(mFmt) + sizeof(mData)) {
            goto err;
        }
        ::memcpy(&mHeader, mFileMapping.data(), sizeof(mHeader));
        ::memcpy(&mFmt, mFileMapping.data() + sizeof(mHeader), sizeof(mFmt));
        ::memcpy(&mData, mFileMapping.data() + sizeof(mHeader) + sizeof(mFmt), sizeof(mData));

        if (::memcmp(mHeader.chunkId, "RIFF", 4) != 0 ||
            ::memcmp(mHeader.format, "WAVE", 4) != 0) 
        {
            goto err;
        }
        if (::memcmp(mFmt.chunkId, "fmt ", 4) != 0) {
            goto err;
        }
        if (::memcmp(mData.chunkId, "data", 4) != 0) {
            goto err;
        }

        switch (mFmt.audioFormat) {
            case 1: NEKO_DEBUG("PCM"); break;
            case 6: NEKO_DEBUG("ADPCM"); break;
            case 7: NEKO_DEBUG("IEEE Float"); break;
            case 8: NEKO_DEBUG("ALaw"); break;
            case 9: NEKO_DEBUG("MuLaw"); break;
            default: NEKO_DEBUG("Unknown"); break;
        }

        if (setupProperties() != Error::Ok) {
            goto err;
        }
        mPcm = wavPcmData();
        if (mPcm.size_bytes() != mData.chunkSize) {
            NEKO_DEBUG("Maybe bad wave file");
        }
        return Error::Ok;

        err :
            libc::munmap(mFileMapping);
            mFileMapping = {};
        return Error::UnsupportedFormat;
    }
    Error onTeardown() override {
        mSrc->properties().clear();
        libc::munmap(mFileMapping);
        mFileMapping = {};
        mPcm = {};
        return Error::Ok;
    }
    Error onLoop() override {
        while (!stopRequested()) {
            thread()->waitTask();
            while (state() == State::Running) {
                thread()->dispatchTask();
                writePcm();
            }
        }
        return Error::Ok;
    }
    void setUrl(std::string_view url) override {
        mUrl = url;
    }
    Error setupProperties() {
        switch (mFmt.bitsPerSample) {
            case 8: mSampelFormat = SampleFormat::U8; break;
            case 16: mSampelFormat = SampleFormat::S16; break;
            case 24: mSampelFormat = SampleFormat::S32; break;
            case 32: mSampelFormat = SampleFormat::S32; break;
            default: NEKO_DEBUG("Unknown sample format"); return Error::UnsupportedFormat;
        }
        mSrc->addProperty(Properties::SampleRate, mFmt.sampleRate);
        mSrc->addProperty(Properties::Channels, mFmt.numChannels);
        mSrc->addProperty(Properties::SampleFormat, mSampelFormat);

        NEKO_DEBUG(numFrames());
        return Error::Ok;
    }
    void writePcm() {
        // Once write by blockAlign
        auto buffer = CreateAudioFrame(mSampelFormat, mFmt.numChannels, mFmt.blockAlign);
        buffer->setSampleRate(mFmt.sampleRate);
    }
    std::span<uint8_t> wavPcmData() {
        auto offset = sizeof(mHeader) + sizeof(mFmt) + sizeof(mData);
        return mFileMapping.subspan(offset, mData.chunkSize);
    }
    /**
     * @brief Get number of samples
     * 
     * @return size_t 
     */
    size_t numFrames() const {
        return mData.chunkSize / mFmt.blockAlign;
    }
    size_t bytesPerSample() const {
        return mFmt.bitsPerSample / 8;
    }
private:
    Pad        *mSrc = nullptr;
    std::string mUrl;
    std::span<uint8_t> mFileMapping;
    std::span<uint8_t> mPcm;
    SampleFormat       mSampelFormat;

    size_t     mPosition = 0; //< Current position

    WaveHeader mHeader;
    WaveFmt    mFmt;
    WaveData   mData;
};

NEKO_REGISTER_ELEMENT(WavSource, WavSourceImpl);

NEKO_NS_END