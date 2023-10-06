#define _NEKO_SOURCE
#include "../format.hpp"
#include "../log.hpp"
#include "device.hpp"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wrl/client.h>

NEKO_NS_BEGIN

using Microsoft::WRL::ComPtr;

class WASAudioDevice final : public AudioDevice {
public:
    WASAudioDevice() {

    }
    ~WASAudioDevice() {
        close();
    }
    bool open(SampleFormat fmt, int sampleRate, int channels) override {
        mThread = (HANDLE) ::_beginthread([](void *self) { static_cast<WASAudioDevice*>(self)->threadMain(); }, 0, this);
        if (mThread == INVALID_HANDLE_VALUE) {
            return false;
        }
        return true;
    }
    bool close() override {

    }
    bool pause(bool v) override {

    }
    bool isPaused() const override {

    }
    void setCallback(std::function<void(void *buffer, int bufferLen)> &&cb) override {
        mCallback = std::move(cb);
    }
    void threadMain() {
        auto hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) {
            NEKO_LOG("CoInitializeEx failed");
            NEKO_DEBUG(hr);
        }
        ComPtr<IMMDeviceEnumerator> enumerator;
        hr = ::CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_SERVER, IID_PPV_ARGS(&enumerator));
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mIMMDevice);

        // Get Audio Client
        hr = mIMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(mAudioClient.GetAddressOf()));
    }
private:
    std::function<void(void *buffer, int bufferLen)> mCallback;
    ComPtr<IMMDevice>                                mIMMDevice;
    ComPtr<IAudioClient>                             mAudioClient;
    HANDLE                                           mThread = 0;
};

NEKO_NS_END