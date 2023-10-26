#pragma once

#include "../utils.hpp"
#include "./renderer.hpp"
#include <wrl/client.h>
#include <d2d1.h>

#ifdef _MSC_VER
    #pragma commit(lib, "d2d1.lib")
#endif


NEKO_NS_BEGIN

class D2DVideoRenderer final : public VideoRenderer {
public:
    D2DVideoRenderer() {

    }
    void init() override {
        ::D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, mFactory.GetAddressOf());
    }
    void teardown() override {
        mFactory.Reset();
    }
    void setHwnd(HWND hwnd) {
        if (mInited) {
            
        }
    }
private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID2D1Factory>      mFactory;
    ComPtr<ID2D1RenderTarget> mRenderTarget;
    ComPtr<ID2D1Bitmap>       mBitmap;

    bool                      mInited = false;
};

NEKO_NS_END