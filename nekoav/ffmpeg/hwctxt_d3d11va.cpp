#ifdef _WIN32

#define _NEKO_SOURCE
#include "../hwcontext/d3d11va.hpp"
#include "../threading.hpp"
#include "../elements.hpp"
#include "../context.hpp"

#ifndef NDEBUG
    #include <wrl/client.h>
    #include "../libc.hpp"
    #include "../log.hpp"
#endif

#include <VersionHelpers.h>

extern "C" {
    #include <libavutil/hwcontext_d3d11va.h>
    #include <libavutil/hwcontext.h>
}

NEKO_NS_BEGIN

D3D11VAContext::D3D11VAContext(AVBufferRef *d3d11context) {
    mFFD3D11Context = Arc<AVBufferRef>(
        d3d11context,
        [](AVBufferRef *ref) {
            av_buffer_unref(&ref);
        }
    );
    auto hwContext = reinterpret_cast<AVHWDeviceContext*>(mFFD3D11Context->data);
    mAVDeviceContext = reinterpret_cast<AVD3D11VADeviceContext*>(hwContext->hwctx);
    mDevice = mAVDeviceContext->device;
    mDeviceContext = mAVDeviceContext->device_context;
    mVideoDevice = mAVDeviceContext->video_device;
    mVideoContext = mAVDeviceContext->video_context;

#ifndef NDEBUG
    // Get adapter for debug
    using Microsoft::WRL::ComPtr;

    ComPtr<IDXGIDevice> device;
    ComPtr<IDXGIAdapter> adapter;
    mDevice->QueryInterface(IID_PPV_ARGS(&device));
    device->GetAdapter(adapter.GetAddressOf());

    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);

    NEKO_LOG("D3D11VA: Create Device, Adapter description {}", libc::to_utf8(desc.Description).c_str());
#endif
}
D3D11VAContext::~D3D11VAContext() {

}
void D3D11VAContext::lock() {
    mAVDeviceContext->lock(mAVDeviceContext->lock_ctx);
}
void D3D11VAContext::unlock() {
    mAVDeviceContext->unlock(mAVDeviceContext->lock_ctx);
}

Arc<D3D11VAContext> D3D11VAContext::create(Context *ctxt) {
    NEKO_ASSERT(ctxt);
    if (auto d3d11ctxt = ctxt->queryObject<D3D11VAContext>(); d3d11ctxt) {
        return d3d11ctxt->shared_from_this();
    }
    auto d3d11ctxt = D3D11VAContext::create();
    ctxt->addObject(d3d11ctxt);
    return d3d11ctxt;
}
Arc<D3D11VAContext> D3D11VAContext::create(Element *element) {
    return D3D11VAContext::create(element->context());
}
Arc<D3D11VAContext> D3D11VAContext::create() {
    AVBufferRef *avdevice = nullptr;
    AVDictionary *opts = nullptr;
#ifndef NDEBUG
    av_dict_set(&opts, "debug", "1", 0);
#endif

    if (av_hwdevice_ctx_create(&avdevice, AV_HWDEVICE_TYPE_D3D11VA, nullptr, opts, 0) < 0) {
        av_dict_free(&opts);
        return nullptr;
    }
    av_dict_free(&opts);

#if !defined(NDEBUG)
    auto thread = Thread::currentThread();
    if (thread) {
        // Emm D3D11 device_ctx_create will change the thread name in my pc, change it back in debug
        thread->setName(thread->name());
    }
#endif

    return MakeShared<D3D11VAContext>(avdevice);
}

NEKO_NS_END

#endif