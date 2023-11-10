#define _NEKO_SOURCE
#include "../audiodev.hpp"
#include <SDL2/SDL_audio.h>

NEKO_NS_BEGIN

namespace {

class SDLAudioDevice final : public AudioDevice {
public:
    SDLAudioDevice() {

    }
    ~SDLAudioDevice() {
        close();
    }
    bool close() override {
        if (mDeviceId == 0) {
            return false;
        }
        SDL_CloseAudioDevice(mDeviceId);
        mDeviceId = 0;
        return true;
    }
    bool open(SampleFormat fmt, int sampleRate, int channels) override {
        mSpec.freq = sampleRate;
        mSpec.channels = channels;
        mSpec.callback = [](void *self, Uint8 *stream, int len) {
            static_cast<SDLAudioDevice*>(self)->callback(stream, len);
        };
        mSpec.userdata = this;
        mSpec.silence = 0;

        switch (fmt) {
            case SampleFormat::U8: mSpec.format = AUDIO_U8; break;
            case SampleFormat::S16: mSpec.format = AUDIO_S16; break;
            case SampleFormat::S32: mSpec.format = AUDIO_S32; break;
            case SampleFormat::FLT: mSpec.format = AUDIO_F32; break;
            default: return false;
        }

        mDeviceId = SDL_OpenAudioDevice(nullptr, 0, &mSpec, nullptr, 0);
        return mDeviceId != 0;
    }
    bool pause(bool v) override {
        if (mDeviceId == 0) {
            return false;
        }
        SDL_PauseAudioDevice(mDeviceId, v ? 1 : 0);
        return true;
    }
    bool isPaused() const override {
        return mDeviceId == 0 || SDL_GetAudioDeviceStatus(mDeviceId) == SDL_AUDIO_PAUSED;
    }
    void callback(Uint8 *stream, int len) {
        auto volume = mVolume.load();
        if (!mCallback) {
            ::memset(stream, 0, len);
            return;
        }
        // Mix if not has volume
        if (volume == 1.0f) {
            mCallback(stream, len);
            return;
        }
        if (!mMixBuffer) {
            mMixBuffer = SDL_malloc(len);
        }

        // Call user to mMixBuffer
        mCallback(mMixBuffer, len);
        ::memset(stream, 0, len);
        SDL_MixAudioFormat(
            stream, static_cast<Uint8*>(mMixBuffer), mSpec.format, len, volume * 128.0f
        );
    }
    void setCallback(std::function<void(void *buffer, int bufferLen)> &&callback) {
        mCallback = std::move(callback);
    }
private:
    std::function<void(void *buffer, int bufferLen)> mCallback;
    SDL_AudioDeviceID                                mDeviceId {0};
    SDL_AudioSpec                                    mSpec { };
    Atomic<float>                                    mVolume {1.0f};
    void                                            *mMixBuffer {nullptr};
};

}

NEKO_NS_END