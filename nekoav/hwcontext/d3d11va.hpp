#pragma once

#include <d3d11.h>
#include "../defs.hpp"

struct AVBufferRef;
struct AVD3D11VADeviceContext;

NEKO_NS_BEGIN

/**
 * @brief A Shared D3D11VAContext
 * 
 */
class NEKO_API D3D11VAContext final : public std::enable_shared_from_this<D3D11VAContext> {
public:
    explicit D3D11VAContext(AVBufferRef *d3d11context);
    D3D11VAContext(const D3D11VAContext &) = delete;
    ~D3D11VAContext();

    void lock();
    void unlock();

    AVBufferRef *ffd3d11context() const { return mFFD3D11Context.get(); }
    ID3D11Device *device() const { return mDevice; }
    ID3D11DeviceContext *deviceContext() const { return mDeviceContext; }

    /**
     * @brief Get a context in Element context, if this context doesnot have it, it will create and put it into
     * 
     * @param element 
     * @return Arc<D3D11VAContext> 
     */
    static Arc<D3D11VAContext> create(Element *element);
    static Arc<D3D11VAContext> create(Context *context);
    static Arc<D3D11VAContext> create();
private:
    Arc<AVBufferRef> mFFD3D11Context = nullptr; //< AVBufferRef * created by it
    AVD3D11VADeviceContext *mAVDeviceContext = nullptr;

    ID3D11Device *mDevice;
    ID3D11DeviceContext *mDeviceContext;
    ID3D11VideoContext *mVideoContext;
    ID3D11VideoDevice *mVideoDevice;
};

NEKO_NS_END