#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../format.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../utils.hpp"
#include "../pad.hpp"
#include "../log.hpp"
#include "videosink.hpp"

// #ifdef _WIN32
#if 0
    #include <wrl/client.h>
    #include <d2d1_1.h>
    #include <d2d1.h>
#endif

NEKO_NS_BEGIN

// #ifdef _WIN32

#if 0
using Microsoft::WRL::ComPtr;

class TestVideoSinkImpl final : public Template::GetThreadImpl<TestVideoSink, MediaClock> {
public:
    typedef HRESULT (WINAPI *CreateFn)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS *, void **);

    TestVideoSinkImpl() {
        mSink = addInput("sink");
        mSink->setCallback(std::bind(&TestVideoSinkImpl::updateBitmap, this, std::placeholders::_1));
        mSink->addProperty(Properties::PixelFormatList, {
            PixelFormat::RGBA
        });
    }
    ~TestVideoSinkImpl() {

    }
    Error onInitialize() override {
        mController = GetMediaController(this);
        if (mController) {
            mController->addClock(this);
        }

        mDll = ::LoadLibraryA("d2d1.dll");
        if (mDll == nullptr) {
            onTeardown();
            return Error::Internal;
        }
        auto create = (CreateFn) ::GetProcAddress(mDll, "D2D1CreateFactory");
        if (create == nullptr) {
            onTeardown();
            return Error::Internal;
        }
        auto hr = create(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, (void**)mFactory.GetAddressOf());
        if (FAILED(hr)) {
            onTeardown();
            return Error::Internal;
        }

        // Make window here
        WNDCLASSEXW wx = {};
        wx.cbSize = sizeof(WNDCLASSEXW);
        wx.style = CS_HREDRAW | CS_VREDRAW;
        wx.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto v = ::GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (v == 0) {
                return ::DefWindowProc(hwnd, msg, wp, lp);
            }
            return ((TestVideoSinkImpl*)v)->onMessage(hwnd, msg, wp, lp);
        };
        wx.hInstance = ::GetModuleHandleW(nullptr);
        wx.lpszClassName = L"NekoTestVideoWindow";
        if (!::RegisterClassExW(&wx)) {
            onTeardown();
            return Error::Internal;
        }
        mHwnd = ::CreateWindowExW(
            0,
            wx.lpszClassName,
            nullptr,
            WS_OVERLAPPEDWINDOW,
            0, 0,
            100, 100,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );
        if (mHwnd == nullptr) {
            onTeardown();
            return Error::Internal;
        }
        hr = mFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(mHwnd),
            mRenderTarget.ReleaseAndGetAddressOf()
        );
        if (FAILED(hr)) {
            onTeardown();
            return Error::Internal;
        }
        ::SetWindowLongPtrW(mHwnd, GWLP_USERDATA, (LONG_PTR)this);

        if (!mTitle.empty()) {
            ::SetWindowTextW(mHwnd, _Neko_Utf8ToUtf16(mTitle).c_str());
        }
        return Error::Ok;
    }
    Error onTeardown() {
        mBitmap.Reset();
        mRenderTarget.Reset();
        mFactory.Reset();

        if (mController) {
            mController->removeClock(this);
        }

        ::DestroyWindow(mHwnd);
        ::FreeLibrary(mDll);
        mDll = nullptr;
        mHwnd = nullptr;
        mPosition = 0.0;
        mController = nullptr;
        return Error::Ok;
    }
    Error updateBitmap(View<Resource> resourceView) {
        auto frame = resourceView.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }

        // Check clock if
        if (mController) {
            auto current = mController->masterClock()->position();
            auto pts = frame->timestamp();
            auto diff = current - pts;

            mPosition = pts;
            if (diff < -0.01) {
                // Too fast
                SleepFor((int64_t(-diff * 1000)));
            }
            else if (diff > 0.3) {
                // Too slow, drop
                NEKO_DEBUG("Too slow, drop");
                NEKO_DEBUG(diff);
                return Error::Ok;
            }
            // else {
            //     // Normal, sleep by duration
            //     auto delay = std::min(diff, frame->duration());
            //     SleepFor(int64_t(delay * 1000));
            // }
        }

        mFrame = frame->shared_from_this<MediaFrame>();
        thread()->postTask([weakFrame = mFrame.load()->weak_from_this(), this]() {
            auto f = weakFrame.lock();
            if (!f) {
                return;
            }
            auto frame = f->shared_from_this<MediaFrame>();
            int width = frame->width();
            int height = frame->height();

            if (mBitmap) {
                auto [texWidth, texHeight] = mBitmap->GetPixelSize();
                if (texWidth != width || texHeight != height) {
                    mBitmap.Reset();
                }
            }
            if (!mBitmap) {
                auto hr = mRenderTarget->CreateBitmap(
                    D2D1::SizeU(width, height),
                    D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
                    mBitmap.GetAddressOf()
                );
                if (FAILED(hr)) {
                    return;
                }
            }
            mBitmap->CopyFromMemory(nullptr, frame->data(0), frame->linesize(0));
            ::InvalidateRect(mHwnd, nullptr, FALSE);

            if (!::IsWindowVisible(mHwnd)) {
                ::SetWindowPos(mHwnd, nullptr, 0, 0, width, height, 0);
                ::ShowWindow(mHwnd, SW_SHOW);
            }
        });
        return Error::Ok;
    }
    void  onPaint() {
        mRenderTarget->BeginDraw();
        mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        if (mBitmap) {
            // Draw Bitmap
            auto [texWidth, texHeight] = mBitmap->GetSize();
            auto [winWidth, winHeight] = mRenderTarget->GetSize();

            float x, y, w, h;

            if(texWidth * winHeight > texHeight * winWidth){
                w = winWidth;
                h = texHeight * winWidth / texWidth;
            }
            else{
                w = texWidth * winHeight / texHeight;
                h = winHeight;
            }
            x = (winWidth - w) / 2;
            y = (winHeight - h) / 2;

            auto dstRect = D2D1::RectF(x, y, x + w, y + h);

            mRenderTarget->DrawBitmap(mBitmap.Get(), dstRect);
        }

        mRenderTarget->EndDraw();
    }
    // Error onLoop() override {
    //     while (!stopRequested()) {
    //         thread()->waitTask(10);

    //         MSG msg;
    //         while (::PeekMessageW(&msg, mHwnd, 0, 0, PM_REMOVE)) {
    //             ::TranslateMessage(&msg);
    //             ::DispatchMessageW(&msg);
    //         }
    //     }
    //     return Error::Ok;
    // }
    LRESULT onMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        // thread()->dispatchTask();
        switch (msg) {
            case WM_CLOSE: {
                NEKO_DEBUG("WM_CLOSE");
                if (mClosedCb) {
                    mClosedCb();
                }
                break;
            }
            case WM_PAINT: {
                onPaint();
                break;
            }
            case WM_SIZE: {
                RECT rect;
                ::GetClientRect(hwnd, &rect);
                mRenderTarget->Resize(D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top));
                break;
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    Error setTitle(std::string_view title) override {
        mTitle = title;
        if (state() != State::Null) {
            // ALready inited
            thread()->sendTask([this]() {
                ::SetWindowTextW(mHwnd, _Neko_Utf8ToUtf16(mTitle).c_str());
            });
        }
        return Error::Ok;
    }
    void setClosedCallback(std::function<void()> &&cb) {
        mClosedCb = std::move(cb);
    }

    // MediaClock
    double position() const override {
        return mPosition;
    }
    Type type() const override {
        return Video;
    }
private:
    ComPtr<ID2D1Factory>          mFactory;
    ComPtr<ID2D1HwndRenderTarget> mRenderTarget;
    ComPtr<ID2D1Bitmap>           mBitmap;

    std::string                   mTitle;
    std::function<void()>         mClosedCb;
    Atomic<Arc<MediaFrame> >      mFrame; //< Current frame

    Atomic<double>                mPosition {0.0};
    MediaController              *mController = nullptr;

    HMODULE mDll = nullptr;
    HWND   mHwnd = nullptr;
    Pad   *mSink = nullptr;
};

NEKO_REGISTER_ELEMENT(TestVideoSink, TestVideoSinkImpl);
#endif

class VideoSinkImpl final : public Template::GetThreadImpl<VideoSink, MediaClock> {
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
        if (mController) {
            mController->addClock(this);
        }
        if (!mRenderer) {
            return Error::InvalidState;
        }
        mSink->addProperty(Properties::PixelFormatList, {
            PixelFormat::RGBA
        });
        return Error::Ok;
    }
    Error onTeardown() override {
        if (mController) {
            mController->removeClock(this);
        }

        mSink->properties().clear();
        mRenderer->setFrame(nullptr); //< Tell Renderer is no frame now
        mController = nullptr;
        return Error::Ok;
    }
    Error sendEvent(View<Event> event) override {
        return Error::Ok;
    }
    Error onSink(View<Resource> resource) {
        auto frame = resource.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedFormat;
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
        // if (!mCancelToken) {
        //     mCancelToken = make_shared<bool>(false);
        // }
        mFrames.emplace(frame->shared_from_this<MediaFrame>());
        // thread()->postTask(std::bind(&VideoSinkImpl::onFrameReady, this, mCancelToken, frame->shared_from_this<MediaFrame>()));
        return Error::Ok;
    }
    Error onSinkEvent(View<Event> event) {
        if (event->type() == Event::FlushRequested) {

        }
        else if (event->type() == Event::SeekRequested) {

        }
        else {
            return Error::Ok;
        }

        mNumFramesDropped = 0;
        // Flush frames
        std::lock_guard lock(mMutex);
        while (!mFrames.empty()) {
            mFrames.pop();
        }
        return Error::Ok;
    }
    void drawFrame(const Arc<MediaFrame> &frame) {
        // mFrameSize -= 1;
        // if (*cancelToken) {
        //     NEKO_DEBUG("Canceled Frame");
        //     return;
        // }
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
            std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(-diff * 1000)));
        }
        else if (diff > 0.3) {
            mNumFramesDropped += 1;
            if (mNumFramesDropped > 10) {
                NEKO_BREAKPOINT();
            }
            // Too slow, drop
            NEKO_DEBUG("Too slow, drop");
            NEKO_DEBUG(diff);
            return;
        }
        mRenderer->setFrame(frame);
    }
    Error onLoop() override {
        while (!stopRequested()) {
            thread()->waitTask();
            while (state() == State::Running) {
                thread()->waitTask(3);

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
    Type type() const override {
        return Video;
    }
private:
    // MediaController
    MediaController *mController = nullptr;

    // Vec<PixelFormat> mFormats;
    VideoRenderer   *mRenderer = nullptr;
    Pad             *mSink;

    std::mutex                   mMutex;
    std::queue<Arc<MediaFrame> > mFrames;
    
    Atomic<size_t> mNumFramesDropped {0};
    Atomic<double> mPosition {0.0}; //< Current time
};
NEKO_REGISTER_ELEMENT(VideoSink, VideoSinkImpl);

NEKO_NS_END