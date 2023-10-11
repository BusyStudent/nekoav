#define _NEKO_SOURCE
#include "../format.hpp"
#include "../latch.hpp"
#include "../log.hpp"
#include "device.hpp"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wrl/client.h>
#include <thread>

#if defined(_MSC_VER)
    #pragma comment(lib, "ole32.lib")
#endif

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
        // Prepare flags
        mRunning = true;
        mFail = false;

        Latch latch {1};
        mThread = std::thread(&WASAudioDevice::threadMain, this, fmt, sampleRate, channels, std::ref(latch));
        latch.wait();

        return mFail;
    }
    bool close() override {
        if (mThread.joinable()) {
            mThread.join();
        }
    }
    bool pause(bool v) override {

    }
    bool isPaused() const override {

    }
    void setCallback(std::function<void(void *buffer, int bufferLen)> &&cb) override {
        mCallback = std::move(cb);
    }
    void threadMain(SampleFormat fmt, int sampleRate, int channels, Latch &latch) {
        mFail = !init();

        // Check format code here
        WAVEFORMATEX wfx;
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = channels;
        wfx.nSamplesPerSec = sampleRate;
        wfx.wBitsPerSample = GetBytesPerSample(fmt) * 8;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        if (mAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &wfx, nullptr) != S_OK) {
            // Unsupported
            mFail = true;
        }
        mAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, &wfx, nullptr);

        latch.count_down();
        if (mFail) {
            teardown();
            return;
        }

        mainloop();
        teardown();
    }
    bool init() {
        auto hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) {
            NEKO_LOG("CoInitializeEx failed");
            return false;
        }
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice>           device;
        hr = ::CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_SERVER, IID_PPV_ARGS(&enumerator));
        if (FAILED(hr)) {
            return false;
        }
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr)) {
            return false;
        }

        // Get Audio Client
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(mAudioClient.GetAddressOf()));
        return SUCCEEDED(hr);
    }
    void teardown() {
        mAudioClient.Reset();
        ::CoUninitialize();
    }
    void mainloop() {

    }
private:
    ComPtr<IAudioClient>                             mAudioClient;
    std::function<void(void *buffer, int bufferLen)> mCallback;
    std::thread                                      mThread;
    Atomic<bool>                                     mRunning {false};
    Atomic<bool>                                     mFail    {false}; //< Backend has an error
};

NEKO_NS_END