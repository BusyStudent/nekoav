#pragma once

#include "../format.hpp"
#include "opengl_macros.hpp"
#include "opengl.hpp"
#include <d3d11.h>
#include <d3d9.h>
#include <wrl/client.h>

//  WGL_NV_DX_INTEROP macros from GLEW

/* --------------------------- WGL_NV_DX_interop --------------------------- */

#ifndef WGL_NV_DX_interop
#define WGL_NV_DX_interop 1

#define WGL_ACCESS_READ_ONLY_NV 0x0000
#define WGL_ACCESS_READ_WRITE_NV 0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002

typedef BOOL (WINAPI * PFNWGLDXCLOSEDEVICENVPROC) (HANDLE hDevice);
typedef BOOL (WINAPI * PFNWGLDXLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
typedef BOOL (WINAPI * PFNWGLDXOBJECTACCESSNVPROC) (HANDLE hObject, GLenum access);
typedef HANDLE (WINAPI * PFNWGLDXOPENDEVICENVPROC) (void* dxDevice);
typedef HANDLE (WINAPI * PFNWGLDXREGISTEROBJECTNVPROC) (HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
typedef BOOL (WINAPI * PFNWGLDXSETRESOURCESHAREHANDLENVPROC) (void* dxObject, HANDLE shareHandle);
typedef BOOL (WINAPI * PFNWGLDXUNLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
typedef BOOL (WINAPI * PFNWGLDXUNREGISTEROBJECTNVPROC) (HANDLE hDevice, HANDLE hObject);

#endif /* WGL_NV_DX_interop */

// Begin our helper
NEKO_NS_BEGIN

using Microsoft::WRL::ComPtr;

class DXGLInterop {
public:
    
    DXGLInterop(GLContext *context) : mContext(context) {
        wglDXCloseDeviceNV = context->getProcAddress<PFNWGLDXCLOSEDEVICENVPROC>("wglDXCloseDeviceNV");
        wglDXLockObjectsNV = context->getProcAddress<PFNWGLDXLOCKOBJECTSNVPROC>("wglDXLockObjectsNV");
        wglDXObjectAccessNV = context->getProcAddress<PFNWGLDXOBJECTACCESSNVPROC>("wglDXObjectAccessNV");
        wglDXOpenDeviceNV = context->getProcAddress<PFNWGLDXOPENDEVICENVPROC>("wglDXOpenDeviceNV");
        wglDXRegisterObjectNV = context->getProcAddress<PFNWGLDXREGISTEROBJECTNVPROC>("wglDXRegisterObjectNV");
        wglDXSetResourceShareHandleNV = context->getProcAddress<PFNWGLDXSETRESOURCESHAREHANDLENVPROC>("wglDXSetResourceShareHandleNV");
        wglDXUnlockObjectsNV = context->getProcAddress<PFNWGLDXUNLOCKOBJECTSNVPROC>("wglDXUnlockObjectsNV");
        wglDXUnregisterObjectNV = context->getProcAddress<PFNWGLDXUNREGISTEROBJECTNVPROC>("wglDXUnregisterObjectNV");
    }
    ~DXGLInterop() {
        cleanup();
    }

    void cleanup() {
        if (mDeviceHandle) {
            wglDXCloseDeviceNV(mDeviceHandle);
            mDeviceHandle = nullptr;
        }
        mVideoProcessor.Reset();
        mDeviceContext.Reset();
        mDevice.Reset();
    }
    bool init(ID3D11Device *device) {
        cleanup();
        device->GetImmediateContext(&mDeviceContext);
    }

    static Box<DXGLInterop> create(GLContext *context) {
        if (context->hasExtension("WGL_NV_DX_interop") && context->hasExtension("WGL_NV_DX_interop2")) {
            return std::make_unique<DXGLInterop>(context);
        }
        return nullptr;
    }
private:
    PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV = nullptr;
    PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV = nullptr;
    PFNWGLDXOBJECTACCESSNVPROC wglDXObjectAccessNV = nullptr;
    PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV = nullptr;
    PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV = nullptr;
    PFNWGLDXSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV = nullptr;
    PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV = nullptr;
    PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV = nullptr;

    GLContext *mContext = nullptr;

    // Share data
    ComPtr<ID3D11Device> mDevice;
    ComPtr<ID3D11DeviceContext> mDeviceContext;
    ComPtr<ID3D11VideoProcessor> mVideoProcessor;
    HANDLE               mDeviceHandle = nullptr;
};

NEKO_NS_END
