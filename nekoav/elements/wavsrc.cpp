#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../pad.hpp"
#include "wavsrc.hpp"

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

class WavSourceImpl final : public Template::GetImpl<WavSource> {
public:
    WavSourceImpl() {
        mSrc = addOutput("src");
    }
    Error onInitialize() override {
        WaveHeader header;
        WaveFmt    fmt;
        WaveData   data;

        mStream = ::fopen(mUrl.c_str(), "rb");
        if (mStream == nullptr) {
            return Error::FileNotFound;
        }
        if (::fread(&header, sizeof(header), 1, mStream) != 1) {
            goto err;
        }
        if (::memcmp(header.chunkId, "RIFF", 4) != 0 ||
            ::memcmp(header.format, "WAVE", 4) != 0) 
        {
            goto err;
        }
        if (::fread(&fmt, sizeof(fmt), 1, mStream) != 1) {
            goto err;
        }
        if (::memcmp(fmt.chunkId, "fmt ", 4) != 0) {
            goto err;
        }
        if (::fread(&data, sizeof(data), 1, mStream) != 1) {
            goto err;
        }
        if (::memcmp(data.chunkId, "data", 4) != 0) {
            goto err;
        }
        return Error::Ok;

        err :
            ::fclose(mStream);
            mStream = nullptr;
            return Error::UnsupportedFormat;
    }
    Error onTeardown() override {

    }
    void setUrl(std::string_view url) override {
        mUrl = url;
    }
private:
    Pad        *mSrc = nullptr;
    std::string mUrl;
    FILE       *mStream = nullptr;
};


NEKO_NS_END