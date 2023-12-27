#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../utils.hpp"
#include "opengl.hpp"

#if   defined(_WIN32)
    #define HAVE_WGL
    #include <windows.h>
    #include "opengl_macros.hpp"

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

// WGL EXT 
typedef const char *(APIENTRY * PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC hdc);

static std::once_flag wglRegister;

class WGLDisplay final : public GLDisplay {
public:
    Box<GLContext> createContext() override;
private:
    void loadExtensions(GLContext *ctxt, HDC hdc);

    neko_library_path("opengl32.dll");
    neko_import(wglCreateContext);
    neko_import(wglDeleteContext);
    neko_import(wglMakeCurrent);
    neko_import(wglGetCurrentDC);
    neko_import(wglGetCurrentContext);
    neko_import(wglGetProcAddress);
    neko_import(wglShareLists);

    // WGL Ext
    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = nullptr;
    const char                      *mExtensions = nullptr;
    bool                             mExtLoaded = false;
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

        // Load basic pfns
        makeCurrent();
        glGetString = reinterpret_cast<PFNGLGETSTRINGPROC>(getProcAddress("glGetString"));
        glEnable = reinterpret_cast<PFNGLENABLEPROC>(getProcAddress("glEnable"));
        mExtensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));

        if (!mDisplay->mExtLoaded) {
            mDisplay->loadExtensions(this, mDC);
        }
#ifndef NDEBUG
        fprintf(stderr, "WGL: Create a Context at thread %s\n", Thread::currentThread() == nullptr ? "main" : Thread::currentThread()->name().data());
        fprintf(stderr, "WGL: Version %s\n", glGetString(GL_VERSION));
        fprintf(stderr, "WGL: Vendor: %s\n", glGetString(GL_VENDOR));
        fprintf(stderr, "WGL: Renderer: %s\n", glGetString(GL_RENDERER));
        fprintf(stderr, "WGL: Shader: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
#endif
        doneCurrent();
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
    bool hasExtension(const char *name) const override {
        if (::strstr(mExtensions, name)) {
            return true;
        }
        // Check is WGL
        if (mDisplay->mExtensions) {
            return ::strstr(mDisplay->mExtensions, name);
        }
        return false;
    }
private:
    WGLDisplay *mDisplay;
    HWND  mHwnd = nullptr;
    HDC   mDC = nullptr;
    HGLRC mCtxt = nullptr;

    HDC   mprevDC = nullptr;
    HGLRC mprevCtxt = nullptr;
    DWORD mThreadId = ::GetCurrentThreadId();

    // Basic Function
    PFNGLGETSTRINGPROC glGetString = nullptr;
    PFNGLENABLEPROC    glEnable    = nullptr;
    const char        *mExtensions = nullptr;
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
inline void WGLDisplay::loadExtensions(GLContext *ctxt, HDC hdc) {
    mExtLoaded = true;
    wglGetExtensionsStringARB = ctxt->getProcAddress<PFNWGLGETEXTENSIONSSTRINGARBPROC>(
        "wglGetExtensionsStringARB"
    );
    if (wglGetExtensionsStringARB) {
        mExtensions = wglGetExtensionsStringARB(hdc);
#ifndef NDEBUG
        fprintf(stderr, "WGL: Platform Extensions %s\n", mExtensions);
#endif
    }
}

#endif

Box<GLDisplay> CreateGLDisplay() {
#ifdef HAVE_WGL
    return std::make_unique<WGLDisplay>();
#endif

    return nullptr;
}


NEKO_NS_END