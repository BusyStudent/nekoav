#define _NEKO_SOURCE
#define  MA_IMPLEMENTATION
#define  MA_USE_STDINT
#define  MA_NO_DECODING
#define  MA_NO_ENCODING
#define  MA_NO_GENERATION
#define  MA_API static

#if defined(NEKOAV_DEBUG)
    #define  MA_DEBUG_OUTPUT
#endif

#if defined(__WIN32)
    #define  MA_NO_NEON
#endif
#include <miniaudio.h>
#include "../audiodev.hpp"

NEKO_NS_BEGIN

class MiniAudioDevice final : public AudioDevice {
public:
    MiniAudioDevice() {

    }
    ~MiniAudioDevice() {
        close();
    }
    bool open(SampleFormat format, int rate, int channels) {
        close();

        ma_format fmt;
        switch (format) {
            case SampleFormat::U8: fmt = ma_format_u8; break;
            case SampleFormat::S16: fmt = ma_format_s16; break;
            case SampleFormat::S32: fmt = ma_format_s32; break;
            case SampleFormat::FLT: fmt = ma_format_f32; break;
            default: return false;
        }

        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = fmt;
        config.playback.channels = channels;
        config.sampleRate = rate;
        config.pUserData = this;
        config.dataCallback = [](ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
            MiniAudioDevice* self = static_cast<MiniAudioDevice*>(device->pUserData);
            self->invoke(output, ma_get_bytes_per_frame(device->playback.format, device->playback.channels) * frameCount);
        };

        ma_result result = ma_device_init(NULL, &config, &mDevice);
        if (result != MA_SUCCESS) {
            return false;
        }
        mInited = true;
        return true;
    }
    bool close() override {
        if (!mInited) {
            return false;
        }
        ma_device_uninit(&mDevice);
        mInited = false;
        return true;
    }
    bool pause(bool v) override {
        if (!mInited) {
            return false;
        }
        if (v) {
            return ma_device_stop(&mDevice) == MA_SUCCESS;
        }
        else {
            return ma_device_start(&mDevice) == MA_SUCCESS;
        }
    }
    bool isPaused() const override {
        if (!mInited) {
            return false;
        }
        return ma_device_get_state(&mDevice) == ma_device_state_stopped;
    }
    void setCallback(std::function<void(void *buffer, int bufferLen)> &&callback) override {
        mCallback = std::move(callback);
    }
    void invoke(void *output, int len) {
        if (!mCallback) {
            ::memset(output, 0, len);
            return;
        }
        mCallback(output, len);
    }
private:
    std::function<void(void *buffer, int bufferLen)> mCallback;
    ma_device mDevice { };
    bool      mInited = false;
};

Box<AudioDevice> CreateAudioDevice() {
    return std::make_unique<MiniAudioDevice>();
}

NEKO_NS_END