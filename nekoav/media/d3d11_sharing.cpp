#ifdef _WIN32
#define _NEKO_SOURCE
#include "../elements.hpp"
#include "../context.hpp"
#include "../utils.hpp"
#include "sharing.hpp"
#include "frame.hpp"
#include <VersionHelpers.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <mutex>

NEKO_NS_BEGIN

using Microsoft::WRL::ComPtr;

class D3D11FrameImpl final : public MediaFrame {
public:
    D3D11FrameImpl(Arc<D3D11SharedContext> sharedContext, ID3D11Texture2D* texture) : mSharedContext(sharedContext), mTexture(texture) {
        D3D11_TEXTURE2D_DESC desc;
        mTexture->GetDesc(&desc);
        mWidth = desc.Width;
        mHeight = desc.Height;
    }
    int format() const override {
        return int(PixelFormat::D3D11);
    }
    double duration() const override {
        return mDuration;
    }
    double timestamp() const override {
        return mPosition;
    }
    int linesize(int plane) const override {
        return 0;
    }
    void *data(int plane) const override {
        if (plane != 0) {
            return nullptr;
        }
        return mTexture.Get();
    }
    bool makeWritable() override {
        return true;
    }
    int query(Value v) const override {
        switch (v) {
            case Value::Width: return mWidth;
            case Value::Height: return mHeight;
            default: return 0;
        }
    }
    bool set(Value v, const void *d) override {
        switch (v) {
            case Value::Duration: mDuration = *(double*)d; return true;
            case Value::Timestamp: mPosition = *(double*)d; return true;
            default: return false;
        }
    }


    double mPosition = 0.0;
    double mDuration = 0.0;

    int mWidth = 0;
    int mHeight = 0;

    Arc<D3D11SharedContext> mSharedContext;
    ComPtr<ID3D11Texture2D> mTexture;
};

class D3D11SharedContextImpl final : public D3D11SharedContext {
public:
    D3D11SharedContextImpl() {
        UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if !defined(NDEBUG) && defined(_MSC_VER)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        auto hr = d3d11.D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags, //< D2D and video processer
            nullptr,
            0,
            D3D11_SDK_VERSION,
            mDevice.GetAddressOf(),
            nullptr,
            mContext.GetAddressOf()
        );
        ComPtr<ID3D10Multithread> multithread;
        hr = mDevice.As(&multithread);
        if (SUCCEEDED(hr)) {
            hr = multithread->SetMultithreadProtected(true);
        }
        hr = mDevice.As(&mVideoDevice);
        hr = mContext.As(&mVideoContext);
    }
    ~D3D11SharedContextImpl() {

    }
    HRESULT _convertTexture2DFormat(ID3D11Texture2D *dst, ID3D11Texture2D *src) {
        // Get format of dst and src
        D3D11_TEXTURE2D_DESC srcDsec;
        D3D11_TEXTURE2D_DESC dstDsec;
        src->GetDesc(&srcDsec);
        dst->GetDesc(&dstDsec);

        if (!mVideoProcessor) {
            D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
            desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
            desc.InputWidth = srcDsec.Width;
            desc.InputHeight = srcDsec.Height;
            desc.OutputWidth = dstDsec.Width;
            desc.OutputHeight = dstDsec.Height;
            desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

            HRESULT hr = mVideoDevice->CreateVideoProcessorEnumerator(&desc, &mVideoProcessorEnumerator);
            if (FAILED(hr)) {
                return hr;
            }

            // Check format
            UINT flags = 0;
            mVideoProcessorEnumerator->CheckVideoProcessorFormat(srcDsec.Format, &flags);
            if (!(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)) {
                return E_INVALIDARG;
            }
            mVideoProcessorEnumerator->CheckVideoProcessorFormat(dstDsec.Format, &flags);
            if (!(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) {
                return E_INVALIDARG;
            }

            // Create processor
            RECT srcRect {0, 0, LONG(srcDsec.Width), LONG(srcDsec.Height)};
            RECT dstRect {0, 0, LONG(dstDsec.Width), LONG(dstDsec.Height)};

            hr = mVideoDevice->CreateVideoProcessor(mVideoProcessorEnumerator.Get(), 0, &mVideoProcessor);

            mVideoContext->VideoProcessorSetStreamFrameFormat(mVideoProcessor.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
            mVideoContext->VideoProcessorSetStreamOutputRate(mVideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, TRUE, nullptr);
            
            mVideoContext->VideoProcessorSetStreamSourceRect(mVideoProcessor.Get(), 0, TRUE, &srcRect);
            mVideoContext->VideoProcessorSetStreamDestRect(mVideoProcessor.Get(), 0, TRUE, &dstRect);
            mVideoContext->VideoProcessorSetOutputTargetRect(mVideoProcessor.Get(), TRUE, &dstRect);
        }
        ComPtr<ID3D11VideoProcessorInputView> inputView;
        ComPtr<ID3D11VideoProcessorOutputView> outputView;

        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc = {};
        inputDesc.FourCC = 0;
        inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inputDesc.Texture2D.MipSlice = 0;
        // In ArraySlice

        HRESULT hr = mVideoDevice->CreateVideoProcessorInputView(src, mVideoProcessorEnumerator.Get(), &inputDesc, &inputView);

        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = {};
        outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outputDesc.Texture2D.MipSlice = 0;

        hr = mVideoDevice->CreateVideoProcessorOutputView(dst, mVideoProcessorEnumerator.Get(), &outputDesc, &outputView);

        // Begin Process
        D3D11_VIDEO_PROCESSOR_STREAM stream = {};
        stream.Enable = TRUE;
        stream.pInputSurface = inputView.Get();

        hr = mVideoContext->VideoProcessorBlt(mVideoProcessor.Get(), outputView.Get(), 0, 1, &stream);
        return hr;
    }
    HRESULT _copyToCurrentDevice(ComPtr<ID3D11Texture2D> &dst, ID3D11Texture2D *src, intptr_t srcSubIndex) {
        if (!src) {
            return E_INVALIDARG;
        }
        HRESULT hr = S_OK;

        ComPtr<ID3D11Device> srcDevice;
        src->GetDevice(&srcDevice);
        // if (srcDevice == mDevice) {
        //     mContext->CopyResource(cur, src);
        //     return S_OK;
        // }
        ComPtr<ID3D11DeviceContext> srcContext;
        srcDevice->GetImmediateContext(&srcContext);

        // Create a src temp texture with shared attr
        D3D11_TEXTURE2D_DESC desc = {};
        src->GetDesc(&desc);
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        ComPtr<ID3D11Texture2D> srcTemp;
        ComPtr<ID3D11Texture2D> dstTemp;
        hr = srcDevice->CreateTexture2D(&desc, nullptr, srcTemp.GetAddressOf());
        if (FAILED(hr)) {
            return hr;
        }
        srcContext->CopyResource(srcTemp.Get(), src);
        srcContext->Flush();

        // Open Shared resource
        ComPtr<IDXGIResource> dxgi;
        srcTemp.As(&dxgi);
        HANDLE shareHandle = nullptr;

        hr = dxgi->GetSharedHandle(&shareHandle);
        hr = mDevice->OpenSharedResource(shareHandle, IID_PPV_ARGS(&dstTemp));

        // Copy
        desc.MiscFlags = 0;
        desc.ArraySize = 1;
        hr = mDevice->CreateTexture2D(&desc, nullptr, dst.GetAddressOf());

        mContext->CopySubresourceRegion(dst.Get(), 0, 0, 0, 0, dstTemp.Get(), srcSubIndex, nullptr);
        return S_OK;;
    }

    Error convertFrame(Arc<MediaFrame> *frame, int wantedFormat) override {
        if (wantedFormat == 0) {
            wantedFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        if (frame == nullptr) {
            return Error::InvalidArguments;
        }
        if (*frame == nullptr) {
            return Error::InvalidArguments;
        }
        // Currently only support D3D11 
        MediaFrame &src = **frame;
        if (src.pixelFormat() != PixelFormat::D3D11) {
            return Error::InvalidArguments;
        }
        auto frameTex = src.data<ID3D11Texture2D*>(0); 
        auto frameTexIdx = src.data<intptr_t>(1);

        // First Copy to our driver
        ComPtr<ID3D11Texture2D> tex;
        _copyToCurrentDevice(tex, frameTex, frameTexIdx);

        // Then convert
        ComPtr<ID3D11Texture2D> output;
        D3D11_TEXTURE2D_DESC outputDesc = {};
        outputDesc.Width = src.width();
        outputDesc.Height = src.height();
        outputDesc.MipLevels = 1;
        outputDesc.ArraySize = 1;
        outputDesc.Format = DXGI_FORMAT(wantedFormat);
        outputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        outputDesc.SampleDesc.Count = 1;
        outputDesc.SampleDesc.Quality = 0;
        auto hr = mDevice->CreateTexture2D(&outputDesc, nullptr, output.GetAddressOf());
        if (FAILED(hr)) {
            return Error::Unknown;
        }
        _convertTexture2DFormat(output.Get(), tex.Get());

        *frame = MakeShared<D3D11FrameImpl>(shared_from_this(), output.Get());
        return Error::Ok;
    }

    void lock() override {
        mMutex.lock();
    }
    void unlock() override {
        mMutex.unlock();
    }
    ID3D11Device *device() const override {
        return mDevice.Get();
    }
    ID3D11DeviceContext *context() const override {
        return mContext.Get();
    }
private:
    struct {
        neko_library_path("d3d11.dll");
        neko_import(D3D11CreateDevice);
    } d3d11;

    ComPtr<ID3D11Device>        mDevice;
    ComPtr<ID3D11DeviceContext> mContext;
    ComPtr<ID3D11VideoDevice>   mVideoDevice;
    ComPtr<ID3D11VideoContext>  mVideoContext;
    ComPtr<ID3D11Texture2D>     mSharingSurface;

    // Convert Value Here
    ComPtr<ID3D11VideoProcessor> mVideoProcessor;
    ComPtr<ID3D11VideoProcessorEnumerator> mVideoProcessorEnumerator;

    std::recursive_mutex        mMutex;
};

Arc<D3D11SharedContext> GetD3D11SharedContext(Context *context) {
    if (!context || !IsWindows8OrGreater()) {
        return nullptr;
    }
    if (auto ctxt = context->queryObject<D3D11SharedContext>(); ctxt) {
        return ctxt->shared_from_this();
    }
    auto ctxt = MakeShared<D3D11SharedContextImpl>();
    context->addObject<D3D11SharedContext>(ctxt);
    return ctxt;
}
Arc<D3D11SharedContext> GetD3D11SharedContext(Element *element) {
    return GetD3D11SharedContext(element->context());
}

NEKO_NS_END

#endif