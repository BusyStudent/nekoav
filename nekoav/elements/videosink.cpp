#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../format.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../utils.hpp"
#include "../pad.hpp"
#include "../log.hpp"
#include "videosink.hpp"

#if defined(_WIN32)
    #include <wrl/client.h>
    #include <d2d1.h>
#endif

NEKO_NS_BEGIN

class VideoSinkImpl final : public Template::GetThreadImpl<VideoSink, MediaClock, MediaElement> {
public:
    VideoSinkImpl() {
        mSink = addInput("sink");
        mSink->setCallback(std::bind(&VideoSinkImpl::onSink, this, std::placeholders::_1));
        mSink->setEventCallback(std::bind(&VideoSinkImpl::onSinkEvent, this, std::placeholders::_1));
    }
    ~VideoSinkImpl() {

    }
    Error setRenderer(VideoRenderer *renderer) override {
        mRenderer = renderer;
        return Error::Ok;
    }

    // Element
    Error onInitialize() override {
        mController = GetMediaController(this);
        if (!mRenderer) {
            return Error::InvalidState;
        }
        auto prop = Property::newList();
        for (auto fmt : mRenderer->supportedFormats()) {
            prop.push_back(fmt);
        }
        mSink->addProperty(Properties::PixelFormatList, std::move(prop));
        if (context()) {
            mRenderer->setContext(context());
        }
        return Error::Ok;
    }
    Error onTeardown() override {
        mSink->properties().clear();
        mRenderer->setFrame(nullptr); //< Tell Renderer is no frame now
        if (context()) {
            mRenderer->setContext(nullptr);
        }
        mController = nullptr;
        mAfterSeek = false;
        return Error::Ok;
    }
    Error sendEvent(View<Event> event) override {
        return Error::Ok;
    }
    Error onSink(View<Resource> resource) {
        auto frame = resource.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (mAfterSeek) {
            mAfterSeek = false;
            NEKO_LOG("After seek, first frame arrived pts {}", frame->timestamp());
        }
        // Wait if too much
        std::unique_lock lock(mMutex);
        while (mFrames.size() > 4) {
            lock.unlock();
            if (Thread::msleep(10) == Error::Interrupted) {
                // Is Interrupted, we need return right now
                lock.lock();
                break;
            }
            lock.lock();
        }
        // Push one to the frame queue
        mFrames.emplace(frame->shared_from_this<MediaFrame>());
        thread()->postTask([]() {}); //< To wakeup
        return Error::Ok;
    }
    Error onSinkEvent(View<Event> event) {
        if (event->type() == Event::FlushRequested) {
            NEKO_DEBUG("Flush Queue");
            // Flush frames
            std::lock_guard lock(mMutex);
            while (!mFrames.empty()) {
                mFrames.pop();
            }
            mNumFramesDropped = 0;
            mSleepDeviation = 0;
            mCondition.notify_one();
        }
        else if (event->type() == Event::SeekRequested) {
            mAfterSeek = true;
            NEKO_DEBUG(event.viewAs<SeekEvent>()->position());
        }
        return Error::Ok;
    }
    void drawFrame(const Arc<MediaFrame> &frame) {
        if (!mController) {
            mRenderer->setFrame(frame);
            return;
        }
        // Sync here
        auto current = mController->masterClock()->position();
        auto pts = frame->timestamp();
        auto diff = current - pts;

        mPosition = pts;
        if (diff < -0.01 && diff > -10.0) {
            // Too fast
            doSleep(-diff * 1000);
        }
        else if (diff > 0.3) {
            mSleepDeviation = 0;
            mNumFramesDropped += 1;
            if (mNumFramesDropped > 10) {
                // NEKO_BREAKPOINT();
                NEKO_DEBUG(mNumFramesDropped.load());
            }
            // Too slow, drop
            NEKO_DEBUG("Too slow, drop");
            NEKO_DEBUG(diff);
            return;
        }
        mRenderer->setFrame(frame);
    }
    void doSleep(int64_t ms) {
        ms -= mSleepDeviation; //< Apply prev deviation
        
        auto now = GetTicks();
        
        std::unique_lock lock(mCondMutex);
        mCondition.wait_for(lock, std::chrono::milliseconds(ms));

        auto diff = GetTicks() - now - ms;
        mSleepDeviation = diff;
    }
    Error onLoop() override {
        while (!stopRequested()) {
            thread()->waitTask();
            while (state() == State::Running) {
                thread()->waitTask(10);
                std::unique_lock lock(mMutex);
                while (!mFrames.empty() && state() == State::Running) {
                    auto frame = std::move(mFrames.front());
                    mFrames.pop();
                    lock.unlock();

                    // Do drawing
                    drawFrame(frame);
                    thread()->dispatchTask();

                    lock.lock();
                }
            }
        }
        return Error::Ok;
    }
    // MediaClock
    double position() const override {
        return mPosition;
    }
    ClockType type() const override {
        return ClockType::Video;
    }

    // MediaElement
    bool isEndOfFile() const override {
        std::lock_guard lock(mMutex);
        return mFrames.empty();
    }
    MediaClock *clock() const override {
        return const_cast<VideoSinkImpl*>(this);
    }
private:
    // MediaController
    MediaController *mController = nullptr;

    // Vec<PixelFormat> mFormats;
    VideoRenderer   *mRenderer = nullptr;
    Pad             *mSink = nullptr;
    bool             mAfterSeek = false;

    std::condition_variable mCondition;
    std::mutex              mCondMutex;

    mutable std::mutex           mMutex;
    std::queue<Arc<MediaFrame> > mFrames;
    
    Atomic<size_t> mNumFramesDropped {0};
    Atomic<double> mPosition {0.0}; //< Current time
    Atomic<int64_t> mSleepDeviation {0};
};
NEKO_REGISTER_ELEMENT(VideoSink, VideoSinkImpl);

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;
static std::once_flag d2dMessageWindow;

class D2DRendererImpl final : public D2DRenderer {
public:
    constexpr static UINT WM_D2D_PAINT = WM_USER;
    constexpr static UINT WM_D2D_UPDATE = WM_USER + 1;

    D2DRendererImpl(HWND hwnd) : mHwnd(hwnd) {
        mDll = ::LoadLibraryA("d2d1.dll");
        auto proc = reinterpret_cast<HRESULT (__stdcall *)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS *, void **)>(
            ::GetProcAddress(mDll, "D2D1CreateFactory")
        );
        std::call_once(d2dMessageWindow, []() {
            WNDCLASSEXW wx {};
            wx.cbSize = sizeof(wx);
            wx.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                D2DRendererImpl *renderer = reinterpret_cast<D2DRendererImpl*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (renderer) {
                    return renderer->_wndProc(hwnd, msg, wParam, lParam);
                }
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);
            };
            wx.hInstance = ::GetModuleHandleW(nullptr);
            wx.lpszClassName = L"NekoD2DMessageWindow";
            ::RegisterClassExW(&wx);
        });
        if (proc) {
            proc(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), nullptr, reinterpret_cast<void**>(mFactory.GetAddressOf()));
        }
        _initMessageWindow();
    }
    ~D2DRendererImpl() {
        if (mMessageHwnd) {
            ::DestroyWindow(mMessageHwnd);
        }
        mFactory.Reset();
        ::FreeLibrary(mDll);
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
        ::PostMessageW(mMessageHwnd, WM_D2D_UPDATE, 0, 0);
        return Error::Ok;
    }
    Error setContext(Context *) override {
        return Error::Ok;
    }

    Error moveToThread(Thread *thread) override {
        if (thread) {
            thread->invokeQueued(&D2DRendererImpl::_initMessageWindow, this);
        }
        else {
            _initMessageWindow();
        }
        return Error::Ok;
    }
    Error setAspectMode(AspectMode mode) override {
        mMode = mode;
        return paint();
    }
    Error paint() override {
        ::SendMessageW(mMessageHwnd, WM_D2D_PAINT, 0, 0);
        return Error::Ok;
    }

    void _doUpdate() {
        auto frame = mFrame.load();
        if (!frame) {
            mBitmap.Reset();
            _doPaint();
            return;
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

    LRESULT _wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
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
    void _initMessageWindow() {
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
            mThreadId = std::this_thread::get_id();

            _createD2D();
        }
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
    std::thread::id               mThreadId;
    Atomic<Arc<MediaFrame> >      mFrame; //< Current frame
    AspectMode                    mMode = Auto;

    HMODULE                       mDll = nullptr;
    ComPtr<ID2D1Factory>          mFactory;
    ComPtr<ID2D1HwndRenderTarget> mRenderTarget;
    ComPtr<ID2D1Bitmap>           mBitmap;
    HWND                          mHwnd = nullptr;
    HWND                          mMessageHwnd = nullptr;
};

Box<D2DRenderer> CreateD2DRenderer(void *hwnd) {
    return std::make_unique<D2DRendererImpl>(static_cast<HWND>(hwnd));
}


#endif

NEKO_NS_END