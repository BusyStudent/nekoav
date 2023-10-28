#pragma once

#include "./renderer.hpp"
#include "../error.hpp"
#include "../media.hpp"
#include "../format.hpp"
#include <wrl/client.h>
#include <d2d1.h>

#ifdef _MSC_VER
    #pragma comment(lib, "d2d1.lib")
#endif


NEKO_NS_BEGIN

class D2DVideoRenderer final : public VideoRenderer {
public:
    D2DVideoRenderer() {

    }
    Error init() override {
        if (!mHwnd) {
            return Error::InvalidArguments;
        }
        RECT rect;
        ::GetClientRect(mHwnd, &rect);

        auto hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, mFactory.GetAddressOf());
        hr = mFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(mHwnd, D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top)),
            mRenderTarget.GetAddressOf()
        );

        if (FAILED(hr)) {
            return Error::Internal;
        }
        return Error::Ok;
    }
    Error teardown() override {
        mBitmap.Reset();
        mRenderTarget.Reset();
        mFactory.Reset();
        return Error::Ok;
    }
    Error sendFrame(View<MediaFrame> frame) override {
        if (!mRenderTarget) {
            return Error::InvalidState;
        }
        if (frame.empty()) {
            mBitmap.Reset();
            paint();
            return Error::Ok;
        }
        int width = frame->width();
        int height = frame->height();
        if (mBitmap) {
            auto [w, h] = mBitmap->GetPixelSize();
            if (width != w || height != h) {
                mBitmap.Reset();
            }
        }
        if (!mBitmap) {
            // Create a new one
            D2D1_PIXEL_FORMAT pixelFormat;

            switch (frame->pixelFormat()) {
                case PixelFormat::RGBA: {
                    // RGBA32
                    pixelFormat = D2D1::PixelFormat(
                        DXGI_FORMAT_R8G8B8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED
                    );
                    break;
                }
                default : return Error::UnsupportedFormat;
            }

            auto hr = mRenderTarget->CreateBitmap(
                D2D1::SizeU(width, height),
                D2D1::BitmapProperties(
                    pixelFormat
                ),
                mBitmap.GetAddressOf()
            );
            if (FAILED(hr)) {
                return Error::OutOfMemory;
            }
        }
        auto hr = mBitmap->CopyFromMemory(
            nullptr,
            frame->data(0),
            frame->linesize(0)
        );
        paint();
        return Error::Ok;

    }
    std::vector<PixelFormat> supportedFormats() const override {
        return {
            PixelFormat::RGBA
        };
    }
    void setHwnd(HWND hwnd) {
        mHwnd = hwnd;
    }
    void paint() {
        if (mResized) {
            mResized = false;

            RECT rect;
            ::GetClientRect(mHwnd, &rect);
            mRenderTarget->Resize(D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top));
        }

        mRenderTarget->BeginDraw();
        mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        if (mBitmap) {
            mRenderTarget->DrawBitmap(mBitmap.Get());
        }
        HRESULT hr = mRenderTarget->EndDraw();

        if (hr == D2DERR_RECREATE_TARGET) {
            teardown();
            init();
        }
    }
    void notifyResize() {
        mResized = true;
    }
    bool isInited() const noexcept {
        return mRenderTarget;
    }
private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID2D1Factory>          mFactory;
    ComPtr<ID2D1HwndRenderTarget> mRenderTarget;
    ComPtr<ID2D1Bitmap>           mBitmap;
    Atomic<bool>                  mResized {false};
    HWND                          mHwnd = nullptr;
};

NEKO_NS_END