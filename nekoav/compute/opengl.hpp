#pragma once

#include "../defs.hpp"
#include "../utils.hpp"

#ifdef _WIN32
    #ifdef _MSC_VER
        #pragma commit(lib, "user32.lib")
    #endif
    
    #include <Windows.h>
#endif

NEKO_NS_BEGIN

namespace OpenGL {

#ifdef _WIN32
class WGLContext {
public:
    WGLContext() {
        registerWindow();
        mHwnd = ::CreateWindowExW(
            0,
            L"NekoComputeGLWindow",
            nullptr,
            0,
            0,
            10,
            10,
            10,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );
        if (!mHwnd) {
            return;
        }
        mDC = GetDC(mHwnd);
        if (!mDC) {
            return;
        }
        mCtxt = wglCreateContext(mDC);
    }
    ~WGLContext() {
        if (mCtxt) {
            wglDeleteContext(mCtxt);
        }
        if (mDC) {
            ReleaseDC(mHwnd, mDC);
        }
        if (mHwnd) {
            DestroyWindow(mHwnd);
        }
    }

    bool makeCurrent() {
        return wglMakeCurrent(mDC, mCtxt);
    }
    bool isNull() const noexcept {
        return !mCtxt || !mDC || !mHwnd;
    }
    void *getProcAddress(const char *name) const noexcept {
        auto addr = wglGetProcAddress(name);
        if (!addr) {
            addr = GetProcAddress((HMODULE) libraryHandle(), name);
        }
        return reinterpret_cast<void*>(addr);
    }
    HDC dc() const noexcept {
        return mDC;
    }
    HGLRC glrc() const noexcept {
        return mCtxt;
    }
private:
    static void registerWindow() {
        static bool inited = false;
        if (inited) {
            return;
        }
        WNDCLASSEXW wx { };
        wx.cbSize = sizeof(wx);
        wx.hInstance = GetModuleHandle(nullptr);
        wx.lpszClassName = L"NekoComputeGLWindow";
        wx.lpfnWndProc = DefWindowProcW;
        ::RegisterClassExW(&wx);
    }

    neko_library_path("opengl32.dll");
    neko_import(wglGetProcAddress);
    neko_import(wglCreateContext);
    neko_import(wglDeleteContext);
    neko_import(wglMakeCurrent);

    HWND mHwnd = nullptr; //< Fake window for OpenGL
    HGLRC mCtxt = nullptr;
    HDC   mDC = nullptr;
};
using GLContext = WGLContext;
#endif

};

using GLComputeContext = OpenGL::GLContext;


NEKO_NS_END