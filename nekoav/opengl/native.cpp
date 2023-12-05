#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../utils.hpp"
#include "opengl.hpp"
#include <mutex>

#if   defined(_WIN32)
    #define HAVE_WGL
    #include <windows.h>

    #pragma comment(lib, "gdi32.lib")
#endif

#if __has_include(<EGL/egl.h>)
    #define HAVE_EGL
    #include <EGL/egl.h>
#endif

#if __has_include(<GL/glx.h>)
    #define HAVE_GLX
    #include <GL/glx.h>
#endif

NEKO_NS_BEGIN

#ifdef HAVE_WGL

static std::once_flag wglRegister;

class WGLDisplay final : public GLDisplay {
public:
    Box<GLContext> createContext() override;
private:
    neko_library_path("opengl32.dll");
    neko_import(wglCreateContext);
    neko_import(wglDeleteContext);
    neko_import(wglMakeCurrent);
    neko_import(wglGetCurrentDC);
    neko_import(wglGetCurrentContext);
    neko_import(wglGetProcAddress);
friend class WGLContext;
};


class WGLContext final : public GLContext {
public:
    WGLContext(WGLDisplay *display, HWND hwnd) : mDisplay(display), mHwnd(hwnd) {
        mDC = ::GetDC(hwnd);

        // TODO SetPixelFormat
        ::PIXELFORMATDESCRIPTOR pfd = {
            .nSize = sizeof(pfd),
            .nVersion = 1,
            .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
            .iPixelType = PFD_TYPE_RGBA,
            .cColorBits = 32,
            .cDepthBits = 24,
            .cStencilBits = 8,
        };

        auto ok = ::SetPixelFormat(mDC, ::ChoosePixelFormat(mDC, &pfd), &pfd);
        NEKO_ASSERT(ok);

        mCtxt = mDisplay->wglCreateContext(mDC);
        NEKO_ASSERT(mCtxt);
    }
    ~WGLContext() {
        NEKO_ASSERT(mThreadId == ::GetCurrentThreadId());
        int v = ::ReleaseDC(mHwnd, mDC);
        NEKO_ASSERT(v == 1);

        auto ok = mDisplay->wglDeleteContext(mCtxt);
        NEKO_ASSERT(ok);

        ok = ::DestroyWindow(mHwnd);
        NEKO_ASSERT(ok);
    }
    bool makeCurrent() override {
        NEKO_ASSERT(mThreadId == ::GetCurrentThreadId());
        mprevDC = mDisplay->wglGetCurrentDC();
        mprevCtxt = mDisplay->wglGetCurrentContext();
        // Is same, do nothing
        if (mprevDC == mDC && mprevCtxt == mCtxt) {
            mprevCtxt = nullptr;
            mprevDC = nullptr;
            return true;
        }
        return mDisplay->wglMakeCurrent(mDC, mCtxt);
    }
    void doneCurrent() override {
        NEKO_ASSERT(mThreadId == ::GetCurrentThreadId());
        if (mprevCtxt) {
            mDisplay->wglMakeCurrent(mprevDC, mprevCtxt);
        }
    }
    void *getProcAddress(const char *name) const override {
        NEKO_ASSERT(mThreadId == ::GetCurrentThreadId());
        auto addr = mDisplay->wglGetProcAddress(name);
        if (!addr) {
            addr = ::GetProcAddress((HMODULE) mDisplay->libraryHandle(), name);
        }
        return reinterpret_cast<void *>(addr);
    }
private:
    WGLDisplay *mDisplay;
    HWND  mHwnd = nullptr;
    HDC   mDC = nullptr;
    HGLRC mCtxt = nullptr;

    HDC   mprevDC = nullptr;
    HGLRC mprevCtxt = nullptr;
    DWORD mThreadId = ::GetCurrentThreadId();
};

inline Box<GLContext> WGLDisplay::createContext() {
    std::call_once(wglRegister, [](){
        ::WNDCLASSEXW wx {};
        wx.cbSize = sizeof(wx);
        wx.lpfnWndProc = DefWindowProcW;
        wx.hInstance = ::GetModuleHandleW(nullptr);
        wx.lpszClassName = L"NekoWGLDisplay";

        if (!RegisterClassExW(&wx)) {
            NEKO_ASSERT(false);
        }
    });
    HWND hwnd = ::CreateWindowExW(
        0,
        L"NekoWGLDisplay",
        nullptr,

        0,
        0,
        0,
        10,
        10,
        nullptr,
        nullptr,
        ::GetModuleHandleW(nullptr),
        nullptr
    );
    if (hwnd == nullptr) {
        return nullptr;
    }
    return std::make_unique<WGLContext>(this, hwnd);
}

Box<GLDisplay> CreateGLDisplay() {
    return std::make_unique<WGLDisplay>();
}


#endif


// GLController
class GLControllerImpl final : public GLController {
public:
    GLControllerImpl() {

    }
    ~GLControllerImpl() {
        NEKO_ASSERT(mThreadRefcount == 0);
        NEKO_ASSERT(mContextRefcount == 0);
        NEKO_ASSERT(mThread == nullptr);
    }
    void setDisplay(Box<GLDisplay> display) override {
        mDisplay = std::move(display);
    }
    Thread *allocThread() override {
        std::lock_guard lock(mMutex);
        mThreadRefcount ++;
        if (mThread == nullptr) {
            mThread = std::make_unique<Thread>();
            mThread->setName("NekoGLThread");
        }
        return mThread.get();
    }
    GLContext *allocContext() override {
        NEKO_ASSERT(Thread::currentThread() == mThread.get());
        std::lock_guard lock(mMutex);
        mContextRefcount ++;
        if (mContext == nullptr) {
            if (!mDisplay) {
                mDisplay = CreateGLDisplay();
            }
            mContext = mDisplay->createContext();
        }
        return mContext.get();
    }
    void freeThread(Thread *thread) override {
        NEKO_ASSERT(thread == mThread.get());
        NEKO_ASSERT(mThreadRefcount > 0);
        NEKO_ASSERT(Thread::currentThread() != mThread.get());

        std::lock_guard lock(mMutex);
        mThreadRefcount --;
        if (mThreadRefcount == 0) {
            mThread = nullptr;
        }
    }
    void freeContext(GLContext *context) override {
        NEKO_ASSERT(context == mContext.get());
        NEKO_ASSERT(mContextRefcount > 0);
        std::lock_guard lock(mMutex);
        mContextRefcount --;
        if (mContextRefcount == 0) {
            mContext = nullptr;
        }
    }
private:
    std::mutex     mMutex;
    Atomic<size_t> mThreadRefcount {0};
    Atomic<size_t> mContextRefcount {0};

    // Managed by GLController
    Box<GLDisplay> mDisplay;
    Box<GLContext> mContext;
    Box<Thread>    mThread;
};

Box<GLController> CreateGLController() {
    return std::make_unique<GLControllerImpl>();
}

NEKO_NS_END