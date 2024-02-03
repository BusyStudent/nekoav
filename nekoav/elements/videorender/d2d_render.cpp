#define _NEKO_SOURCE
#include "../../hwcontext/d3d11va_utils.hpp" //< For Frame Converter
#include "../../media/frame.hpp"
#include "../../threading.hpp"
#include "../../utils.hpp"
#include "../videosink.hpp"
#include <VersionHelpers.h>
#include <wrl/client.h>
#include <d2d1_1.h>
#include <d2d1.h>
#include <d3d11.h>
#include <mutex>

NEKO_NS_BEGIN

#if defined(_WIN32)

namespace {

using Microsoft::WRL::ComPtr;
static std::once_flag d2dMessageWindow;

struct D3D11 {
    neko_library_path("d3d11.dll");
    neko_import(D3D11CreateDeviceAndSwapChain);
};
struct D2D {
    HRESULT (__stdcall *D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS *, void **);
    HMODULE _dll = nullptr;

    D2D() {
        _dll = ::LoadLibraryA("d2d1.dll");
        D2D1CreateFactory = (decltype(D2D1CreateFactory)) ::GetProcAddress(_dll, "D2D1CreateFactory");
    }
    ~D2D() {
        ::FreeLibrary(_dll);
    }
};

//< Helper for dispatch message
class MessageWindow {
public:
    constexpr static UINT WM_D2D_INIT  = WM_USER + 1;
    constexpr static UINT WM_D2D_PAINT = WM_USER + 2;
    constexpr static UINT WM_D2D_UPDATE = WM_USER + 3;

    MessageWindow() {
        std::call_once(d2dMessageWindow, []() {
            WNDCLASSEXW wx {};
            wx.cbSize = sizeof(wx);
            wx.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                MessageWindow *self = reinterpret_cast<MessageWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (self) {
                    return self->wndProc(hwnd, msg, wParam, lParam);
                }
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);
            };
            wx.hInstance = ::GetModuleHandleW(nullptr);
            wx.lpszClassName = L"NekoD2DMessageWindow";
            ::RegisterClassExW(&wx);
        });
    }
    ~MessageWindow() {
        NEKO_ASSERT(!mMessageHwnd && "MessageWindow is not destructed properly.");
    }

    void initMessageWindow() {
        if (mMessageHwnd) {
            ::DestroyWindow(mMessageHwnd);
        }
        mMessageHwnd = ::CreateWindowExW(
            0,
            L"NekoD2DMessageWindow",
            nullptr,
            0,
            10,
            10,
            10,
            10,
            HWND_MESSAGE,
            nullptr,
            ::GetModuleHandleW(nullptr),
            nullptr
        );
        if (mMessageHwnd) {
            ::SetWindowLongPtrW(mMessageHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            ::SendMessageW(mMessageHwnd, WM_D2D_INIT, 0, 0);
        }
    }
    void destroy() {
        if (mMessageHwnd) {
            ::DestroyWindow(mMessageHwnd);
            mMessageHwnd = nullptr;
        }
    }
    LRESULT sendMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        return ::SendMessageW(mMessageHwnd, msg, wParam, lParam);
    }
    BOOL postMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        return ::PostMessageW(mMessageHwnd, msg, wParam, lParam);
    }

    virtual LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
private:
    HWND mMessageHwnd = nullptr;
};

//< For Win7 D2D1.0
class D2D1RendererImpl final : public D2DRenderer, public MessageWindow {
public:
    D2D1RendererImpl(HWND hwnd) : mHwnd(hwnd) {
        ::D2D1_FACTORY_OPTIONS options {
#if !defined(NDEBUG) && defined(_MSC_VER)
            .debugLevel = D2D1_DEBUG_LEVEL_INFORMATION
#endif
        };
        if (mD2D1.D2D1CreateFactory) {
            mD2D1.D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), &options, reinterpret_cast<void**>(mFactory.GetAddressOf()));
        }
        initMessageWindow();
    }
    ~D2D1RendererImpl() {
        destroy();
    }
    PixelFormatList supportedFormats() override {
        static PixelFormat fmts [] { PixelFormat::RGBA, PixelFormat::BGRA};
        return fmts;
    }
    Error setFrame(View<MediaFrame> frame) override {
        if (frame) {
            mFrame = frame->shared_from_this<MediaFrame>();
        }
        else {
            mFrame.store(nullptr);
        }
        postMessage(WM_D2D_UPDATE, 0, 0);
        return Error::Ok;
    }
    Error setContext(Context *) override {
        return Error::Ok;
    }

    Error moveToThread(Thread *thread) override {
        if (thread) {
            thread->invokeQueued(&MessageWindow::initMessageWindow, this);
        }
        else {
            initMessageWindow();
        }
        return Error::Ok;
    }
    Error setAspectMode(AspectMode mode) override {
        mMode = mode;
        return paint();
    }
    Error paint() override {
        sendMessage(WM_D2D_PAINT, 0, 0);
        return Error::Ok;
    }

    void _doUpdate() {
        auto frame = mFrame.load();
        if (!frame) {
            mBitmap.Reset();
            _doPaint();
            return;
        }
        if (mBitmap) {
            auto [fw, fh] = frame->size();
            auto [bw, bh] = mBitmap->GetPixelSize();
            if (fw != bw ||  fh != bh) {
                mBitmap.Reset();
            }
        }
        if (!mBitmap) {
            auto hr = mRenderTarget->CreateBitmap(
                D2D1::SizeU(frame->width(), frame->height()),
                D2D1::BitmapProperties(D2D1::PixelFormat(_translateFormat(frame->pixelFormat()), D2D1_ALPHA_MODE_PREMULTIPLIED)),
                mBitmap.GetAddressOf()
            );
            if (FAILED(hr)) {
                return;
            }
        }
        mBitmap->CopyFromMemory(nullptr, frame->data(0), frame->linesize(0));
        _doPaint();
    }
    void _doPaint() {
        RECT rect;
        ::GetClientRect(mHwnd, &rect);
        auto size = D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);
        if (size != mRenderTarget->GetPixelSize()) {
            mRenderTarget->Resize(size);
        }

        mRenderTarget->BeginDraw();
        mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        if (mBitmap) {
            mRenderTarget->DrawBitmap(mBitmap.Get(), _dstRectangle());
        }
        if (mRenderTarget->EndDraw() == D2DERR_RECREATE_TARGET) {
            _createD2D();
        }
    }
    void _createD2D() {
        mBitmap.Reset();
        mRenderTarget.Reset();

        auto hr = mFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT, 
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            ),
            D2D1::HwndRenderTargetProperties(mHwnd),
            &mRenderTarget
        );
    }

    LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override {
        switch (msg) {
            case WM_D2D_INIT: {
                _createD2D();
            }
            case WM_D2D_UPDATE: {
                _doUpdate();
                return 0;
            }
            case WM_D2D_PAINT: {
                _doPaint();
                return 0;
            }
            case WM_DESTROY: {
                // Release D2D
                mBitmap.Reset();
                mRenderTarget.Reset();
                break;
            }
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    D2D1_RECT_F _dstRectangle() const noexcept {
        auto [winWidth, winHeight] = mRenderTarget->GetSize();
        if (mMode == IgnoreAspect) {
            return D2D1::RectF(0, 0, winWidth, winHeight);
        }
        // Keep aspect
        auto [texWidth, texHeight] = mBitmap->GetSize();

        float x, y, w, h;

        if (texWidth * winHeight > texHeight * winWidth) {
            w = winWidth;
            h = texHeight * winWidth / texWidth;
        }
        else {
            w = texWidth * winHeight / texHeight;
            h = winHeight;
        }
        x = (winWidth - w) / 2;
        y = (winHeight - h) / 2;

        return D2D1::RectF(x, y, x + w, y + h);
    }
    DXGI_FORMAT _translateFormat(PixelFormat fmt) const {
        switch (fmt) {
            case PixelFormat::BGRA: return DXGI_FORMAT_B8G8R8A8_UNORM;
            case PixelFormat::RGBA: return DXGI_FORMAT_R8G8B8A8_UNORM;
            default: ::abort();
        }
    }
private:
    Atomic<Arc<MediaFrame> >      mFrame; //< Current frame
    AspectMode                    mMode = Auto;

    D2D                           mD2D1;
    ComPtr<ID2D1Factory>          mFactory;
    ComPtr<ID2D1HwndRenderTarget> mRenderTarget;
    ComPtr<ID2D1Bitmap>           mBitmap;
    HWND                          mHwnd = nullptr;
};

// For Windows8 D3D11.1 D2D1.1
class D2D1ExRenderImpl final : public D2DRenderer, public MessageWindow {
public:
    D2D1ExRenderImpl(HWND hwnd) : mHwnd(hwnd) {
        ::D2D1_FACTORY_OPTIONS options {
#if !defined(NDEBUG) && defined(_MSC_VER)
            .debugLevel = D2D1_DEBUG_LEVEL_INFORMATION
#endif
        };
        auto hr = mD2D1.D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), &options, reinterpret_cast<void**>(mFactory.GetAddressOf()));
        if (FAILED(hr)) {
            return;
        }
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
        RECT rect;
        ::GetClientRect(hwnd, &rect);

#if !defined(NDEBUG) && defined(_MSC_VER)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        DXGI_SWAP_CHAIN_DESC swapChainDesc {};
        ::memset(&swapChainDesc, 0, sizeof(swapChainDesc));
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.OutputWindow = hwnd;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.BufferDesc.Width = rect.right - rect.left;
        swapChainDesc.BufferDesc.Height = rect.bottom - rect.top;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;

        for (auto type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
            hr = mD3D11.D3D11CreateDeviceAndSwapChain(
                nullptr,
                type,
                nullptr,
                flags,
                nullptr,
                0,
                D3D11_SDK_VERSION,
                &swapChainDesc,
                &mSwapChain,
                &mDevice,
                nullptr,
                &mContext
            );
            if (SUCCEEDED(hr)) {
                break;
            }
        }
        if (FAILED(hr)) {
            return;
        }

        // Create D2D1
        ComPtr<IDXGIDevice> dxDevice;
        ComPtr<ID2D1Device> d2dDevice;
        hr = mDevice.As(&dxDevice);
        hr = mFactory->CreateDevice(dxDevice.Get(), &d2dDevice);
        if (FAILED(hr)) {
            return;
        }
        hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &mD2DContext);
        if (FAILED(hr)) {
            return;
        }
        _resizeBuffer();

        // Init Message Loop
        initMessageWindow();
    }
    ~D2D1ExRenderImpl() {
        destroy();
    }
    void _resizeBuffer() {
        HRESULT hr = 0;
        mD2DContext->SetTarget(nullptr);
        hr = mSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) {
            return;
        }

        // Get 
        ComPtr<ID3D11Texture2D> backBuffer;
        hr = mSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr)) {
            return;
        }
        ComPtr<IDXGISurface> dxSurface;
        if (FAILED(backBuffer.As(&dxSurface))) {
            return;
        }

        // Create Image for this texture
        ComPtr<ID2D1Bitmap1> bitmap;
        hr = mD2DContext->CreateBitmapFromDxgiSurface(
            dxSurface.Get(),
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            ),
            &bitmap
        );
        if (FAILED(hr)) {
            return;
        }
        mD2DContext->SetTarget(bitmap.Get());
    }
    PixelFormatList supportedFormats() override {
        static PixelFormat fmts [] { PixelFormat::RGBA, PixelFormat::D3D11};
        return fmts;
    }
    Error setFrame(View<MediaFrame> frame) override {
        if (frame) {
            mFrame = frame->shared_from_this<MediaFrame>();
        }
        else {
            mFrame.store(nullptr);
        }
        postMessage(WM_D2D_UPDATE, 0, 0);
        return Error::Ok;
    }
    Error setContext(Context *) override {
        return Error::Ok;
    }

    Error moveToThread(Thread *thread) override {
        if (thread) {
            thread->invokeQueued(&MessageWindow::initMessageWindow, this);
        }
        else {
            initMessageWindow();
        }
        return Error::Ok;
    }
    Error setAspectMode(AspectMode mode) override {
        mMode = mode;
        return paint();
    }
    Error paint() override {
        sendMessage(WM_D2D_PAINT, 0, 0);
        return Error::Ok;
    }

    void _doUpdate() {
        auto frame = mFrame.load();
        if (!frame) {
            mBitmap.Reset();
            _doPaint();
            return;
        }
        if (mBitmap) {
            auto [fw, fh] = frame->size();
            auto [bw, bh] = mBitmap->GetPixelSize();
            if (fw != bw ||  fh != bh) {
                mBitmap.Reset();
                mDstTexture.Reset();
            }
        }
        if (frame->pixelFormat() == PixelFormat::D3D11) {
            _doUpdateD3D11(frame);
            return;
        }
        if (!mBitmap) {
            auto hr = mD2DContext->CreateBitmap(
                D2D1::SizeU(frame->width(), frame->height()),
                D2D1::BitmapProperties(D2D1::PixelFormat(_translateFormat(frame->pixelFormat()), D2D1_ALPHA_MODE_PREMULTIPLIED)),
                reinterpret_cast<ID2D1Bitmap **>(mBitmap.ReleaseAndGetAddressOf())
            );
            if (FAILED(hr)) {
                return;
            }
        }
        mBitmap->CopyFromMemory(nullptr, frame->data(0), frame->linesize(0));
        _doPaint();
    }
    void _doUpdateD3D11(const Arc<MediaFrame> &frame) {
        auto texture = frame->data<ID3D11Texture2D*>(0);
        auto textureIdx = frame->data<intptr_t>(1);

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        // Check share device
        if (texture != mSrcTexture.Get()) {
            mSrcHandle = nullptr;
            mSharedTexture.Reset();
            mSrcTexture.Reset();
            mSrcDevice.Reset();
            mConverter.reset();

            mSrcTexture = texture;
            mSrcTexture->GetDevice(&mSrcDevice);

            // Must be shared
            NEKO_ASSERT(desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED);
            ComPtr<IDXGIResource> resource;
            mSrcTexture.As(&resource);
            resource->GetSharedHandle(&mSrcHandle);

            // Open share resource
            mDevice->OpenSharedResource(mSrcHandle, IID_PPV_ARGS(&mSharedTexture));

            // Make a new converter
            mConverter = D3D11VAConverter::create(mDevice.Get());
            mConverter->setVideoUsage(D3D11_VIDEO_USAGE_PLAYBACK_NORMAL);
            mConverter->initialize(desc.Width, desc.Height, desc.Format, desc.Width, desc.Height, DXGI_FORMAT_B8G8R8A8_UNORM);
        }
        if (!mBitmap) {
            // Create DstTexture
            D3D11_TEXTURE2D_DESC dstTextureDesc {
                .Width          = desc.Width,
                .Height         = desc.Height,
                .MipLevels      = 1,
                .ArraySize      = 1,
                .Format         = DXGI_FORMAT_B8G8R8A8_UNORM,
                .SampleDesc     = { 1, 0 },
                .Usage          = D3D11_USAGE_DEFAULT,
                .BindFlags      = D3D11_BIND_SHADER_RESOURCE,
            };
            mDevice->CreateTexture2D(&dstTextureDesc, nullptr, mDstTexture.ReleaseAndGetAddressOf());

            // Create to image
            ComPtr<IDXGISurface> surface;
            mDstTexture.As(&surface);

            mD2DContext->CreateBitmapFromDxgiSurface(
                surface.Get(),
                D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
                mBitmap.ReleaseAndGetAddressOf()
            );
        }

        // Do convert
        mConverter->convert(mDstTexture.Get(), 0, mSharedTexture.Get(), textureIdx);
        _doPaint();
    }
    void _doPaint() {
        RECT rect;
        ::GetClientRect(mHwnd, &rect);
        auto size = D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);
        if (size != mD2DContext->GetPixelSize()) {
            _resizeBuffer();
        }

        mD2DContext->BeginDraw();
        mD2DContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        if (mBitmap) {
            mD2DContext->DrawBitmap(mBitmap.Get(), _dstRectangle());
        }
        if (mD2DContext->EndDraw() == D2DERR_RECREATE_TARGET) {
            // ???
        }
        mSwapChain->Present(0, 0);
    }

    LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override {
        switch (msg) {
            case WM_D2D_UPDATE: {
                _doUpdate();
                return 0;
            }
            case WM_D2D_PAINT: {
                _doPaint();
                return 0;
            }
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    D2D1_RECT_F _dstRectangle() const noexcept {
        auto [winWidth, winHeight] = mD2DContext->GetSize();
        if (mMode == IgnoreAspect) {
            return D2D1::RectF(0, 0, winWidth, winHeight);
        }
        // Keep aspect
        auto [texWidth, texHeight] = mBitmap->GetSize();

        float x, y, w, h;

        if (texWidth * winHeight > texHeight * winWidth) {
            w = winWidth;
            h = texHeight * winWidth / texWidth;
        }
        else {
            w = texWidth * winHeight / texHeight;
            h = winHeight;
        }
        x = (winWidth - w) / 2;
        y = (winHeight - h) / 2;

        return D2D1::RectF(x, y, x + w, y + h);
    }
    DXGI_FORMAT _translateFormat(PixelFormat fmt) const {
        switch (fmt) {
            case PixelFormat::BGRA: return DXGI_FORMAT_B8G8R8A8_UNORM;
            case PixelFormat::RGBA: return DXGI_FORMAT_R8G8B8A8_UNORM;
            default: ::abort();
        }
    }
private:
    Atomic<Arc<MediaFrame> >      mFrame; //< Current frame
    AspectMode                    mMode = Auto;

    D3D11                         mD3D11;
    D2D                           mD2D1;
    ComPtr<ID3D11Device>          mDevice;
    ComPtr<ID3D11DeviceContext>   mContext;
    ComPtr<IDXGISwapChain>        mSwapChain;

    ComPtr<ID2D1Factory1>         mFactory;
    ComPtr<ID2D1DeviceContext>    mD2DContext;
    ComPtr<ID2D1Bitmap1>          mBitmap;

    HWND                          mHwnd = nullptr;

    // Convert Area
    Box<D3D11VAConverter>         mConverter;
    ComPtr<ID3D11Texture2D>       mSharedTexture; //< Shared Texture from another device
    ComPtr<ID3D11Texture2D>       mDstTexture; //< Texture used to draw on screen

    ComPtr<ID3D11Device>          mSrcDevice;
    ComPtr<ID3D11Texture2D>       mSrcTexture;
    HANDLE                        mSrcHandle = nullptr;
};

}

Box<D2DRenderer> CreateD2DRenderer(void *hwnd) {
    if (::IsWindows8OrGreater()) {
        // D2D1.1
        return std::make_unique<D2D1ExRenderImpl>(static_cast<HWND>(hwnd));
    }
    return std::make_unique<D2D1RendererImpl>(static_cast<HWND>(hwnd));
}


#endif


NEKO_NS_END