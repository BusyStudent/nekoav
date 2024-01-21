#pragma once

#ifdef _WIN32
    #include "../utils.hpp"
    #include <Unknwn.h>
    #include <wingdi.h>
    typedef unsigned int GLuint;
    typedef unsigned int GLenum;
    typedef int GLint;


    #ifndef WGL_NV_DX_interop
        #define WGL_NV_DX_interop 1
        #define WGL_ACCESS_READ_ONLY_NV           0x00000000
        #define WGL_ACCESS_READ_WRITE_NV          0x00000001
        #define WGL_ACCESS_WRITE_DISCARD_NV       0x00000002
        typedef BOOL (WINAPI * PFNWGLDXSETRESOURCESHAREHANDLENVPROC) (void *dxObject, HANDLE shareHandle);
        typedef HANDLE (WINAPI * PFNWGLDXOPENDEVICENVPROC) (void *dxDevice);
        typedef BOOL (WINAPI * PFNWGLDXCLOSEDEVICENVPROC) (HANDLE hDevice);
        typedef HANDLE (WINAPI * PFNWGLDXREGISTEROBJECTNVPROC) (HANDLE hDevice, void *dxObject, GLuint name, GLenum type, GLenum access);
        typedef BOOL (WINAPI * PFNWGLDXUNREGISTEROBJECTNVPROC) (HANDLE hDevice, HANDLE hObject);
        typedef BOOL (WINAPI * PFNWGLDXOBJECTACCESSNVPROC) (HANDLE hObject, GLenum access);
        typedef BOOL (WINAPI * PFNWGLDXLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE *hObjects);
        typedef BOOL (WINAPI * PFNWGLDXUNLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE *hObjects);
    #endif /* WGL_NV_DX_interop */
#endif

NEKO_NS_BEGIN

#ifdef _WIN32
/**
 * @brief Helper class for Interop with DirectX
 * 
 */
class WGLDXInteropHelper {
public:
    typedef const char *(WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);

    explicit WGLDXInteropHelper() {
        if (!wglGetCurrentContext()) {
            return;
        }
        // Begin load
        wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) 
            wglGetProcAddress("wglGetExtensionsStringARB");
        if (!wglGetExtensionsStringARB) {
            return;
        }
        const char *extensions = wglGetExtensionsStringARB(wglGetCurrentDC());
        if (!extensions || ::strstr(extensions, "WGL_NV_DX_interop2") == nullptr) {
            return;
        }
        // Begin Load Interop func
        wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC) 
            wglGetProcAddress("wglDXSetResourceShareHandleNV");
        wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC) 
            wglGetProcAddress("wglDXOpenDeviceNV");
        wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)
            wglGetProcAddress("wglDXCloseDeviceNV");
        wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)
            wglGetProcAddress("wglDXRegisterObjectNV");
        wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)
            wglGetProcAddress("wglDXUnregisterObjectNV");
        wglDXObjectAccessNV = (PFNWGLDXOBJECTACCESSNVPROC)
            wglGetProcAddress("wglDXObjectAccessNV");
        wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)
            wglGetProcAddress("wglDXLockObjectsNV");
        wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)
            wglGetProcAddress("wglDXUnlockObjectsNV");
        if (wglDXSetResourceShareHandleNV && wglDXOpenDeviceNV && wglDXCloseDeviceNV && 
            wglDXRegisterObjectNV && wglDXUnregisterObjectNV && wglDXObjectAccessNV && 
            wglDXLockObjectsNV && wglDXUnlockObjectsNV
        ) {
            mLoaded = true;
        }
        // End Load Interop func
    }
    WGLDXInteropHelper(const WGLDXInteropHelper &) = delete;
    ~WGLDXInteropHelper() {
        closeDevice();
    }

    bool closeDevice() {
        NEKO_ASSERT(wglGetCurrentContext());
        if (isOpen()) {
            auto v = wglDXCloseDeviceNV(mDXDeviceHandle);
            mDXDeviceHandle = nullptr;
            mDXDevice->Release();
            mDXDevice = nullptr;
            return _checkError(v);
        }
        return true;
    }
    bool openDevice(IUnknown *device) {
        NEKO_ASSERT(wglGetCurrentContext());
        closeDevice();
        if (!device) {
            return false;
        }
        mDXDeviceHandle = wglDXOpenDeviceNV(device);
        if (!mDXDeviceHandle) {
            _printError();
            return false;
        }
        mDXDevice = device;
        mDXDevice->AddRef();
        return true;
    }
    bool isOpen() const noexcept {
        return mDXDeviceHandle;
    }
    /**
     * @brief register a directX object, let it can be access in OpenGL
     * 
     * @param dxObject 
     * @param name 
     * @param type 
     * @param access 
     * @return HANDLE 
     */
    HANDLE registerObject(void *dxObject, GLuint name, GLenum type, GLenum access) {
        NEKO_ASSERT(wglGetCurrentContext());
        HANDLE h = wglDXRegisterObjectNV(mDXDeviceHandle, dxObject, name, type, access);
        if (!h) {
            _printError();
        }
        return h;
    }
    bool   unregisterObject(HANDLE objectHandle) {
        NEKO_ASSERT(wglGetCurrentContext());
        return _checkError(wglDXUnlockObjectsNV(mDXDeviceHandle, 1, &objectHandle));
    }
    bool   unregisterObjects(size_t count, HANDLE *objectHandles) {
        NEKO_ASSERT(wglGetCurrentContext());
        return _checkError(wglDXUnlockObjectsNV(mDXDeviceHandle, count, objectHandles));
    }

    bool   lockObject(HANDLE objectHandle) {
        NEKO_ASSERT(wglGetCurrentContext());
        return _checkError(wglDXLockObjectsNV(mDXDeviceHandle, 1, &objectHandle));
    }
    bool   lockObjects(size_t count, HANDLE *objectHandles) {
        NEKO_ASSERT(wglGetCurrentContext());
        return _checkError(wglDXLockObjectsNV(mDXDeviceHandle, count, objectHandles));
    }
    bool   unlockObject(HANDLE objectHandle) {
        NEKO_ASSERT(wglGetCurrentContext());
        return _checkError(wglDXUnlockObjectsNV(mDXDeviceHandle, 1, &objectHandle));
    }
    bool   unlockObjects(size_t count, HANDLE *objectHandles) {
        NEKO_ASSERT(wglGetCurrentContext());
        return _checkError(wglDXUnlockObjectsNV(mDXDeviceHandle, count, objectHandles));
    }

    static Box<WGLDXInteropHelper> create() {
        auto helper = std::make_unique<WGLDXInteropHelper>();
        if (helper->mLoaded) {
            return helper;
        }
        return nullptr;
    }
    static bool _checkError(bool code) {
        if (!code) {
            _printError();
        }
        return code;
    }
    static void _printError() {
#ifndef NDEBUG
        auto code = ::GetLastError();
#endif
    }
private:
    neko_library_path("opengl32.dll");
    neko_import(wglGetProcAddress);
    neko_import(wglGetCurrentContext);
    neko_import(wglGetCurrentDC);

    PFNWGLDXSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV = nullptr;
    PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV = nullptr;
    PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV = nullptr;
    PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV = nullptr;
    PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV = nullptr;
    PFNWGLDXOBJECTACCESSNVPROC wglDXObjectAccessNV = nullptr;
    PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV = nullptr;
    PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV = nullptr;
    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = nullptr;
    bool      mLoaded = false;

    HANDLE    mDXDeviceHandle = nullptr;
    IUnknown *mDXDevice = nullptr;
};
#endif

NEKO_NS_END