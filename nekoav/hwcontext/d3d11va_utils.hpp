#pragma once

#include "d3d11va.hpp"
#include <wrl/client.h>

NEKO_NS_BEGIN

/**
 * @brief A Helper class for Convert DirectX Texture Format by D3D11VA
 * 
 */
class D3D11VAConverter {
public:
    explicit D3D11VAConverter(ID3D11Device *device) : mDevice(device) {
        mDevice->GetImmediateContext(&mContext);
        mDevice.As(&mVideoDevice);
        mContext.As(&mVideoContext);
    }
    D3D11VAConverter(const D3D11VAConverter &) = delete;
    ~D3D11VAConverter() {

    }
    /**
     * @brief Initalize the converter
     * 
     * @param inputWidth 
     * @param inputHeight 
     * @param inputFormat 
     * @param outputWidth 
     * @param outputHeight 
     * @param outputFormat 
     * @return HRESULT 
     */
    HRESULT initialize(UINT inputWidth, UINT inputHeight, DXGI_FORMAT inputFormat, 
                    UINT outputWidth, UINT outputHeight, DXGI_FORMAT outputFormat
    ) {
        auto hr = initializeImpl(inputWidth, inputHeight, inputFormat, outputWidth, outputHeight, outputFormat);
        if (FAILED(hr)) {
            clear();
        }
        return hr;
    }
    
    HRESULT initializeImpl(UINT inputWidth, UINT inputHeight, DXGI_FORMAT inputFormat, 
                        UINT outputWidth, UINT outputHeight, DXGI_FORMAT outputFormat
    ) {
        if (isInitialized()) {
            return E_FAIL;
        }
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc {
            .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
            .InputWidth = inputWidth,
            .InputHeight = inputHeight,
            .OutputWidth = outputWidth,
            .OutputHeight = outputHeight,
            .Usage = mVideoUsage,
        };
        ComPtr<ID3D11VideoProcessorEnumerator> videoProcessorEnumerator;
        auto hr = mVideoDevice->CreateVideoProcessorEnumerator(&processorDesc, &videoProcessorEnumerator);
        if (FAILED(hr)) {
            return hr;
        }
        // Check format
        UINT flags = 0;
        hr = videoProcessorEnumerator->CheckVideoProcessorFormat(inputFormat, &flags);
        if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)) {
            hr = E_FAIL;
            return hr;
        }
        hr = videoProcessorEnumerator->CheckVideoProcessorFormat(outputFormat, &flags);
        if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) {
            hr = E_FAIL;
            return hr;
        }

        // Create Processor
        RECT srcRect {0, 0, LONG(inputWidth), LONG(inputHeight)};
        RECT dstRect {0, 0, LONG(outputWidth), LONG(outputHeight)};
        hr = mVideoDevice->CreateVideoProcessor(videoProcessorEnumerator.Get(), 0, &mVideoProcessor);
        if (FAILED(hr)) {
            return hr;
        }

        mVideoContext->VideoProcessorSetStreamFrameFormat(mVideoProcessor.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
        mVideoContext->VideoProcessorSetStreamOutputRate(mVideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, TRUE, nullptr);
            
        mVideoContext->VideoProcessorSetStreamSourceRect(mVideoProcessor.Get(), 0, TRUE, &srcRect);
        mVideoContext->VideoProcessorSetStreamDestRect(mVideoProcessor.Get(), 0, TRUE, &dstRect);
        mVideoContext->VideoProcessorSetOutputTargetRect(mVideoProcessor.Get(), TRUE, &dstRect);

        // Create done, setup basic in / output texture and view
        D3D11_TEXTURE2D_DESC inputTextureDesc {
            .Width          = inputWidth,
            .Height         = inputHeight,
            .MipLevels      = 1,
            .ArraySize      = 1,
            .Format         = inputFormat,
            .SampleDesc     = { 1, 0 },
            .Usage          = mInputTextureUsage,
            // .BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            .MiscFlags      = mInputTextureMiscFlags
        };
        D3D11_TEXTURE2D_DESC outputTextureDesc {
            .Width          = outputWidth,
            .Height         = outputHeight,
            .MipLevels      = 1,
            .ArraySize      = 1,
            .Format         = outputFormat,
            .SampleDesc     = { 1, 0 },
            .Usage          = mOutputTextureUsage,
            .BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            .MiscFlags      = mOutputTextureMiscFlags,
        };
        hr = mDevice->CreateTexture2D(&outputTextureDesc, nullptr, &mOutputTexture);
        if (FAILED(hr)) {
            return hr;
        }
        hr = mDevice->CreateTexture2D(&inputTextureDesc, nullptr, &mInputTexture);
        if (FAILED(hr)) {
            return hr;
        }

        // Create Output view
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc {};
        outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outputDesc.Texture2D.MipSlice = 0;
        hr = mVideoDevice->CreateVideoProcessorOutputView(
            mOutputTexture.Get(), 
            videoProcessorEnumerator.Get(), 
            &outputDesc, 
            &mOutputView
        );
        if (FAILED(hr)) {
            return hr;
        }
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc {};
        inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inputDesc.Texture2D.MipSlice = 0;
        hr = mVideoDevice->CreateVideoProcessorInputView(
            mInputTexture.Get(),
            videoProcessorEnumerator.Get(),
            &inputDesc,
            &mInputView
        );
        if (FAILED(hr)) {
            return hr;
        }
        return S_OK;
    }
    /**
     * @brief Clear previous data
     * 
     */
    void clear() {
        mVideoProcessor.Reset();
        mInputTexture.Reset();
        mInputView.Reset();
        mOutputTexture.Reset();
        mOutputView.Reset();
    }
    /**
     * @brief Directly do convert, 
     * 
     * @return HRESULT 
     */
    HRESULT convert() {
        if (!isInitialized()) {
            return E_FAIL;
        }
        
        D3D11_VIDEO_PROCESSOR_STREAM stream = {};
        stream.Enable = TRUE;
        stream.pInputSurface = mInputView.Get();
        
        return mVideoContext->VideoProcessorBlt(mVideoProcessor.Get(), mOutputView.Get(), 0, 1, &stream);
    }
    /**
     * @brief Do convert by texture
     * 
     * @param dst 
     * @param dstResource 
     * @param src 
     * @param srcResource 
     * @return HRESULT 
     */
    HRESULT convert(ID3D11Texture2D *dst, UINT dstResource, ID3D11Texture2D *src, UINT srcResource) {
        if (!isInitialized()) {
            return E_FAIL;
        }
        // Copy to temp buffer
        mContext->CopySubresourceRegion(mInputTexture.Get(), 0, 0, 0, 0, src, srcResource, nullptr);
        if (auto hr = convert(); FAILED(hr)) {
            return hr;
        }
        // Copy to dst
        mContext->CopySubresourceRegion(dst, dstResource, 0, 0, 0, mOutputTexture.Get(), 0, nullptr);
        return S_OK;
    }
    /**
     * @brief Set the Input Texture Misc Flags, call it before initalize
     * 
     * @param flags 
     */
    void setInputTextureMiscFlags(UINT flags) noexcept {
        NEKO_ASSERT(!isInitialized());
        mInputTextureMiscFlags = flags;
    }
    /**
     * @brief Set the Input Texture Usage object, call it before initalize
     * 
     * @param usage 
     */
    void setInputTextureUsage(D3D11_USAGE usage) noexcept {
        NEKO_ASSERT(!isInitialized());
        mInputTextureUsage = usage;
    }
    /**
     * @brief Set the Output Texture Misc Flags object, call it before initalize
     * 
     * @param flags 
     */
    void setOutputTextureMiscFlags(UINT flags) noexcept {
        NEKO_ASSERT(!isInitialized());
        mOutputTextureMiscFlags = flags;
    }
    /**
     * @brief Set the Output Texture Usage object， call it before initalize
     * 
     * @param usage 
     */
    void setOutputTextureUsage(D3D11_USAGE usage) noexcept {
        NEKO_ASSERT(!isInitialized());
        mOutputTextureUsage = usage;
    }
    void setVideoUsage(D3D11_VIDEO_USAGE usage) noexcept {
        NEKO_ASSERT(!isInitialized());
        mVideoUsage = usage;
    }

    ID3D11Texture2D *inputTexture() const noexcept {
        return mInputTexture.Get();
    }
    ID3D11Texture2D *outputTexture() const noexcept {
        return mOutputTexture.Get();
    }
    bool isInitialized() const noexcept {
        return mVideoProcessor.Get() != nullptr;
    }

    static Box<D3D11VAConverter> create(ID3D11Device *device) {
        auto cvt = std::make_unique<D3D11VAConverter>(device);
        if (!cvt->mVideoDevice || !cvt->mVideoContext) {
            return nullptr;
        }
        return cvt;
    }
private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    UINT mInputTextureMiscFlags = 0;
    UINT mOutputTextureMiscFlags = 0;
    D3D11_USAGE mInputTextureUsage = D3D11_USAGE_DEFAULT;
    D3D11_USAGE mOutputTextureUsage = D3D11_USAGE_DEFAULT;
    D3D11_VIDEO_USAGE mVideoUsage = D3D11_VIDEO_USAGE_OPTIMAL_QUALITY;

    ComPtr<ID3D11Device>        mDevice;
    ComPtr<ID3D11DeviceContext> mContext;
    ComPtr<ID3D11VideoDevice>   mVideoDevice;
    ComPtr<ID3D11VideoContext> mVideoContext;
    ComPtr<ID3D11VideoProcessor> mVideoProcessor;
    ComPtr<ID3D11Texture2D>    mInputTexture;
    ComPtr<ID3D11Texture2D>    mOutputTexture;
    ComPtr<ID3D11VideoProcessorInputView> mInputView;
    ComPtr<ID3D11VideoProcessorOutputView> mOutputView;
};

NEKO_NS_END