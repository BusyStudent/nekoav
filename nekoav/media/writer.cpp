#define _NEKO_SOURCE
#include "../error.hpp"
#include "writer.hpp"

#ifdef _WIN32
    #include <wincodec.h>
    #include <wrl/client.h>
    #pragma comment(lib, "ole32.lib")
#endif

NEKO_NS_BEGIN

#ifdef _WIN32
class VideoFrameSource final : public IWICBitmapSource {
public:
    ULONG   AddRef() override { return 1; }
    ULONG   Release() override { return 1; }
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == __uuidof(IWICBitmapSource)) {
            *ppvObject = static_cast<IWICBitmapSource*>(this);
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    // Begin IWICBitmapSource
    HRESULT GetSize(UINT *w, UINT *h) override {
        assert(w && h);
        *w = mFrame->width();
        *h = mFrame->height();
        return S_OK;
    }
    HRESULT GetPixelFormat(GUID *fmt) override {
        assert(fmt);
        *fmt = GUID_WICPixelFormat32bppRGBA;
        return S_OK;
    }
    HRESULT GetResolution(double *x, double *y) override {
        assert(x && y);
        *x = *y = 1.0;
        return S_OK;
    }
    HRESULT CopyPalette(IWICPalette *) override {
        return E_NOTIMPL;
    }
    HRESULT CopyPixels(const WICRect *rect, UINT stride, UINT size, BYTE *ut) override {
        int x, y, w, h;
        if (rect) {
            x = rect->X;
            y = rect->Y;
            w = rect->Width;
            h = rect->Height;
        }
        else {
            x = 0;
            y = 0;
            w = mFrame->width();
            h = mFrame->height();
        }

        // Begin copy
        auto src = static_cast<const BYTE *>(mFrame->data(0));
        int width = mFrame->width();
        auto bytes_per = mFrame->linesize(0) / width;

        for (int i = 0; i < h; ++i) {
            auto dst = ut + i * stride;
            auto src_ = src + (y + i) * width * bytes_per + x * bytes_per;
            ::memcpy(dst, src_, w * bytes_per);
        }
        return S_OK;
    }

    VideoFrameSource(MediaFrame *f) : mFrame(f) { }
private:
    MediaFrame *mFrame;
};
#endif

Error WriteVideoFrameToFile(View<MediaFrame> frame, const char *path) {
#ifdef _WIN32
    using Microsoft::WRL::ComPtr;

    // Convert to WCHAR
    int len = ::MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    std::wstring wpath;
    wpath.resize(len);
    len = ::MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], len);

    // Make factory
    // Init Com

    ComPtr<IWICImagingFactory> factory;
    auto hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_SERVER, IID_PPV_ARGS(&factory));
    }

    if (!factory) {
        return Error::NoImpl;
    }
    if (frame->pixelFormat() != PixelFormat::RGBA) {
        return Error::UnsupportedPixelFormat;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) {
        return Error::NoImpl;
    }

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        return Error::NoImpl;
    }

    hr = stream->InitializeFromFilename(wpath.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        return Error::NoImpl;
    }

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);

    ComPtr<IWICBitmapFrameEncode> frameEncode;
    hr = encoder->CreateNewFrame(&frameEncode, nullptr);
    if (FAILED(hr)) {
        return Error::NoImpl;
    }

    hr = frameEncode->Initialize(nullptr);
    hr = frameEncode->SetSize(frame->width(), frame->height());

    WICPixelFormatGUID pf = GUID_WICPixelFormat32bppRGBA;
    hr = frameEncode->SetPixelFormat(&pf);
    if (FAILED(hr)) {
        return Error::NoImpl;
    }
    if (pf != GUID_WICPixelFormat32bppRGBA) {
        ComPtr<IWICFormatConverter> cvt;
        VideoFrameSource source(frame.get());
        hr = factory->CreateFormatConverter(&cvt);
        if (FAILED(hr)) {
            return Error::NoImpl;
        }
        hr = cvt->Initialize(
            &source, 
            GUID_WICPixelFormat32bppRGBA, 
            WICBitmapDitherTypeNone, 
            nullptr, 
            0.f, 
            WICBitmapPaletteTypeCustom
        );
        if (FAILED(hr)) {
            return Error::NoImpl;
        }
        hr = frameEncode->WriteSource(cvt.Get(), nullptr);
    }
    else {
        hr = frameEncode->WritePixels(
            frame->height(), 
            frame->linesize(0), 
            frame->linesize(0) * frame->height(), 
            (BYTE*) frame->data(0)
        );
    }
    hr = frameEncode->Commit();
    hr = encoder->Commit();
    return Error::Ok;
#else
    return Error::NoImpl;
#endif
}

NEKO_NS_END