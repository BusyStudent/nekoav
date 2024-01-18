#define _NEKO_SOURCE
#include "../elements/videocvt.hpp"
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../log.hpp"
#include "../pad.hpp"
#include "common.hpp"
#include "ffmpeg.hpp"

#ifdef _WIN32
    #define HAVE_D3D11VA
    #include <VersionHelpers.h>
    #include <wrl/client.h>
    #include <d3d11.h>
#endif


NEKO_NS_BEGIN

namespace FFmpeg {

class FFVideoConverterImpl final : public Template::GetImpl<VideoConverter> {
public:
    FFVideoConverterImpl() {
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
        mSinkPad->setCallback(std::bind(&FFVideoConverterImpl::_processInput, this, std::placeholders::_1));
        mSinkPad->setEventCallback(std::bind(&Pad::pushEvent, mSourcePad, std::placeholders::_1));
    }
    // void setPixelFormat(PixelFormat format) override {
    //     mTargetFormat = format;
    // }
    Error onInitialize() override {
        return Error::Ok;
    }
    Error onTeardown() override {
        if (mCleanup) {
            std::invoke(mCleanup, this);
        }
        sws_freeContext(mCtxt);
        av_frame_free(&mSwFrame);
        mCopybackFormat = AV_PIX_FMT_NONE;
        mSwsFormat = AV_PIX_FMT_NONE;
        mPassthrough = false;
        mCleanup = nullptr;
        mConvert = nullptr;
        mCtxt = nullptr;
        return Error::Ok;
    }
    Error _processInput(ResourceView resourceView) {
        auto frame = resourceView.viewAs<Frame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mSourcePad->isLinked()) {
            return Error::NoLink;
        }
        if (!mConvert && !mPassthrough) {
            if (auto err = _initConvertIf(frame->get()); err != Error::Ok) {
                return err;
            }
        }
        if (mPassthrough) {
            mSourcePad->push(resourceView);
            return Error::Ok;
        }

        // Alloc frame
        auto dstFrame = av_frame_alloc();
        if (auto err = std::invoke(mConvert, this, dstFrame, frame->get()); err != Error::Ok) {
            NEKO_LOG("Failed to convert frame !!!!!!!, err = {}", err);
            av_frame_free(&dstFrame);
            return err;
        }
        return mSourcePad->push(Frame::make(dstFrame, frame->timebase(), AVMEDIA_TYPE_VIDEO).get());
    }
    Error _initConvertIf(AVFrame *f) {
        auto peerSupported = _peerSupportedPixelFormat();
        AVPixelFormat fmt = AV_PIX_FMT_RGBA;
        if (mTargetFormat == PixelFormat::None) {
            // Try get info by pad
            if (peerSupported.empty()) {
                // No format returned, it means it accept all formats, Just Passthrough
                // return Error::InvalidTopology;
                mPassthrough = true;
                return Error::Ok;
            }
            // Check has this format
            auto iter = std::find(peerSupported.begin(), peerSupported.end(), AVPixelFormat(f->format));
            if (iter != peerSupported.end()) {
                // Support this format, Passthrough
                mPassthrough = true;
                return Error::Ok;
            }
            // Select first
            fmt = peerSupported.front();
        }
        else {
            // Already has target format
            fmt = ToAVPixelFormat(mTargetFormat);
        }

        // Check need copyback
        if (IsHardwareAVPixelFormat(AVPixelFormat(f->format))) {
            // Print Copy back format
            Vec<AVPixelFormat> formatsList;
            AVPixelFormat *avFormatsList = nullptr;
            if (av_hwframe_transfer_get_formats(f->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &avFormatsList, 0) >= 0) {
                int i = 0;
                for (i = 0; avFormatsList[i] != AV_PIX_FMT_NONE; i++) {
                    NEKO_LOG("Supported Copy back format: {}", av_pix_fmt_desc_get(avFormatsList[i])->name);
                }
                formatsList.assign(avFormatsList, avFormatsList + i);
                av_free(avFormatsList);
            }

            // Check copyback pixel format supported
            auto iter = std::find_if(peerSupported.begin(), peerSupported.end(), [&](AVPixelFormat fmt) {
                return std::find(formatsList.begin(), formatsList.end(), fmt) != formatsList.end();
            });
            if (iter != peerSupported.end()) {
                mCopybackFormat = *iter;
                mConvert = &FFVideoConverterImpl::_copybackConvert;
                NEKO_LOG("Init Copyback, to format {}", av_pix_fmt_desc_get(mCopybackFormat)->name);
                return Error::Ok;
            }
            // Try copyback and sws_scale
            if (!mSwFrame) {
                mSwFrame = av_frame_alloc();
            }
            if (auto ret = av_hwframe_transfer_data(mSwFrame, f, 0); ret < 0) {
                return ToError(ret);
            }
            mCopybackFormat = AVPixelFormat(mSwFrame->format);
        }

        // D3D11VA here
#ifdef HAVE_D3D11VA
        if (f->format == AV_PIX_FMT_D3D11 && IsWindows8OrGreater()) {
            if (auto err = _initD3D11Convert(f); err == Error::Ok) {
                return err;
            }
        }
#endif
        if (IsHardwareAVPixelFormat(AVPixelFormat(f->format))) {
            // Need copyback,
            NEKO_ASSERT(mSwFrame);
            f = mSwFrame;
        }

        return _initSwsConvert(f, fmt);
    }
    // Init Parts
    Error _initSwsConvert(AVFrame *f, AVPixelFormat targetFormat) {
        // Prepare sws conetxt
        mCtxt = sws_getContext(
            f->width,
            f->height,
            AVPixelFormat(f->format),
            f->width,
            f->height,
            targetFormat,
            SWS_BICUBIC,
            nullptr,
            nullptr,
            nullptr
        );
        if (!mCtxt) {
            return Error::UnsupportedPixelFormat;
        }
        mSwsFormat = targetFormat;
        NEKO_LOG("Init SwrContext from {} to {}", 
            av_pix_fmt_desc_get(AVPixelFormat(f->format))->name,
            av_pix_fmt_desc_get(AVPixelFormat(targetFormat))->name
        );
        mConvert = &FFVideoConverterImpl::_swsConvert;
        return Error::Ok;
    }
    // Only copy back
    Error _copybackConvert(AVFrame *dstFrame, AVFrame *srcFrame) {
        dstFrame->format = mCopybackFormat;
        int ret = av_hwframe_transfer_data(dstFrame, srcFrame, 0);
        if (ret < 0) {
            return ToError(ret);
        }
        av_frame_copy_props(dstFrame, srcFrame);
        return Error::Ok;
    }
    // do Sws scale
    Error _swsConvert(AVFrame *dstFrame, AVFrame *srcFrame) {
        int ret = 0;
        if (IsHardwareAVPixelFormat(AVPixelFormat(srcFrame->format))) {
            // Copy back
            if (!mSwFrame) {
                mSwFrame = av_frame_alloc();
            }
            ret = av_hwframe_transfer_data(mSwFrame, srcFrame, 0);
            if (ret < 0) {
                return ToError(ret);
            }
            av_frame_copy_props(mSwFrame, srcFrame);
            srcFrame = mSwFrame;
        }
        // New version of api
#if     LIBSWSCALE_VERSION_INT > AV_VERSION_INT(6, 1, 100)
        ret = sws_scale_frame(mCtxt, dstFrame, srcFrame);        
#else
        // Use old one, alloc data
        auto n = av_image_get_buffer_size(mSwsFormat, srcFrame->width, srcFrame->height, 32);
        auto buffer = av_buffer_alloc(n);
        ret = av_image_fill_arrays(
            dstFrame->data,
            dstFrame->linesize,
            buffer->data,
            mSwsFormat,
            srcFrame->width,
            srcFrame->height,
            32
        );
        if (ret < 0) {
            av_buffer_unref(&buffer);
            return Error::OutOfMemory;
        }
        dstFrame->buf[0] = buffer;
        ret = sws_scale(
            mCtxt,
            srcFrame->data,
            srcFrame->linesize,
            0,
            srcFrame->height,
            dstFrame->data,
            dstFrame->linesize
        );
        dstFrame->width = srcFrame->width;
        dstFrame->height = srcFrame->height;
        dstFrame->format = mSwsFormat;
#endif
        if (ret < 0) {
            av_frame_free(&dstFrame);
            return Error::OutOfMemory;
        }
        av_frame_copy_props(dstFrame, srcFrame);
        return Error::Ok;
    }
    // Get the supported format of peer
    Vec<AVPixelFormat> _peerSupportedPixelFormat() {
        Vec<AVPixelFormat> formats;
        auto &pixfmt = mSourcePad->next()->property(Properties::PixelFormatList);
        if (pixfmt.isNull()) {
            return formats;
        }
        for (const auto &v : pixfmt.toList()) {
            formats.push_back(ToAVPixelFormat(v.toEnum<PixelFormat>()));
        }
        return formats;
    }

#ifdef HAVE_D3D11VA
    Error _initD3D11Convert(AVFrame *f) {
        auto err = _initD3D11ConvertImpl(f);
        if (err != Error::Ok) {
            _d3d11Cleanup();
        }
        else {
            mConvert = &FFVideoConverterImpl::_d3d11Convert;
            mCleanup = &FFVideoConverterImpl::_d3d11Cleanup;
        }
        return err;
    }
    Error _initD3D11ConvertImpl(AVFrame *f) {
        D3D11_TEXTURE2D_DESC textureDesc {};
        auto texture = reinterpret_cast<ID3D11Texture2D *>(f->data[0]);
        auto textureIndex = reinterpret_cast<intptr_t>(f->data[1]);

        texture->GetDevice(&mD3D11Device);
        texture->GetDesc(&textureDesc);
        mD3D11Device->GetImmediateContext(&mD3D11Context);
        if (FAILED(mD3D11Context.As(&mD3D11VideoContext)) || FAILED(mD3D11Device.As(&mD3D11VideoDevice))) {
            return Error::External;
        }

        // Detect output format and create 
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc {
            .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
            .InputWidth = textureDesc.Width,
            .InputHeight = textureDesc.Height,
            .OutputWidth = textureDesc.Width,
            .OutputHeight = textureDesc.Height,
            .Usage = D3D11_VIDEO_USAGE_OPTIMAL_QUALITY,
        };
        auto hr = mD3D11VideoDevice->CreateVideoProcessorEnumerator(&processorDesc, &mD3D11VideoProcessorEnumerator);
        if (FAILED(hr)) {
            return Error::External;
        }

        // Check Input
        UINT flags = 0;
        hr = mD3D11VideoProcessorEnumerator->CheckVideoProcessorFormat(textureDesc.Format, &flags);
        if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)) {
            return Error::External;
        }
        // Check Output
        DXGI_FORMAT outputFormat = DXGI_FORMAT_UNKNOWN;
        for (auto fmt : _translateToDXGIFormat(_peerSupportedPixelFormat()) ) {
            hr = mD3D11VideoProcessorEnumerator->CheckVideoProcessorFormat(fmt, &flags);
            if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) {
                outputFormat = fmt;
                break;
            }
        }
        if (outputFormat == DXGI_FORMAT_UNKNOWN) {
            return Error::External;
        }
        // Create Processor
        RECT srcRect {0, 0, LONG(textureDesc.Width), LONG(textureDesc.Height)};
        RECT dstRect {0, 0, LONG(textureDesc.Width), LONG(textureDesc.Height)};
        hr = mD3D11VideoDevice->CreateVideoProcessor(mD3D11VideoProcessorEnumerator.Get(), 0, &mD3D11VideoProcessor);
        if (FAILED(hr)) {
            return Error::External;
        }

        mD3D11VideoContext->VideoProcessorSetStreamFrameFormat(mD3D11VideoProcessor.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
        mD3D11VideoContext->VideoProcessorSetStreamOutputRate(mD3D11VideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, TRUE, nullptr);
            
        mD3D11VideoContext->VideoProcessorSetStreamSourceRect(mD3D11VideoProcessor.Get(), 0, TRUE, &srcRect);
        mD3D11VideoContext->VideoProcessorSetStreamDestRect(mD3D11VideoProcessor.Get(), 0, TRUE, &dstRect);
        mD3D11VideoContext->VideoProcessorSetOutputTargetRect(mD3D11VideoProcessor.Get(), TRUE, &dstRect);

        // Create done, setup basic in / output texture and view
        D3D11_TEXTURE2D_DESC inputTextureDesc {
            .Width          = textureDesc.Width,
            .Height         = textureDesc.Height,
            .MipLevels      = 1,
            .ArraySize      = 1,
            .Format         = textureDesc.Format,
            .SampleDesc     = { 1, 0 },
            .Usage          = D3D11_USAGE_DEFAULT,
            // .BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
        };
        D3D11_TEXTURE2D_DESC outputTextureDesc {
            .Width          = textureDesc.Width,
            .Height         = textureDesc.Height,
            .MipLevels      = 1,
            .ArraySize      = 1,
            .Format         = outputFormat,
            .SampleDesc     = { 1, 0 },
            .Usage          = D3D11_USAGE_DEFAULT,
            .BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
        };
        D3D11_TEXTURE2D_DESC stagingTextureDesc {
            .Width          = textureDesc.Width,
            .Height         = textureDesc.Height,
            .MipLevels      = 1,
            .ArraySize      = 1,
            .Format         = outputFormat,
            .SampleDesc     = { 1, 0 },
            .Usage          = D3D11_USAGE_STAGING,
            .CPUAccessFlags = D3D11_CPU_ACCESS_READ,
        };
        hr = mD3D11Device->CreateTexture2D(&outputTextureDesc, nullptr, &mD3D11OutputTexture);
        hr = mD3D11Device->CreateTexture2D(&stagingTextureDesc, nullptr, &mD3D11StagingTexture);
        hr = mD3D11Device->CreateTexture2D(&inputTextureDesc, nullptr, &mD3D11InputTexture);
        if (!mD3D11OutputTexture || !mD3D11StagingTexture || !mD3D11InputTexture) {
            return Error::External;
        }

        // Create Output view
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc {};
        outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outputDesc.Texture2D.MipSlice = 0;
        hr = mD3D11VideoDevice->CreateVideoProcessorOutputView(
            mD3D11OutputTexture.Get(), 
            mD3D11VideoProcessorEnumerator.Get(), 
            &outputDesc, 
            &mD3D11OutputView
        );
        if (FAILED(hr)) {
            return Error::External;
        }
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc {};
        inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inputDesc.Texture2D.MipSlice = 0;
        hr = mD3D11VideoDevice->CreateVideoProcessorInputView(
            mD3D11InputTexture.Get(),
            mD3D11VideoProcessorEnumerator.Get(),
            &inputDesc,
            &mD3D11InputView
        );
        if (FAILED(hr)) {
            return Error::External;
        }
        mD3D11OutputAVFormat = _translateToAVPixelFormat(outputFormat);
        NEKO_LOG("Init D3D11VA Convert, from {} to {}", textureDesc.Format, outputFormat);
        return Error::Ok;
    }
    Error _d3d11Convert(AVFrame *dstFrame, AVFrame *srcFrame) {
        dstFrame->width = srcFrame->width;
        dstFrame->height = srcFrame->height;
        dstFrame->format = mD3D11OutputAVFormat;
        av_frame_get_buffer(dstFrame, 0);
        av_frame_copy_props(dstFrame, srcFrame);

        // Do convert
        auto texture = reinterpret_cast<ID3D11Texture2D *>(srcFrame->data[0]);
        auto textureIndex = reinterpret_cast<intptr_t>(srcFrame->data[1]);
        HRESULT hr = 0;

        mD3D11Context->CopySubresourceRegion(
            mD3D11InputTexture.Get(),
            0,
            0, 0, 0,
            texture,
            textureIndex,
            nullptr
        );

        D3D11_VIDEO_PROCESSOR_STREAM stream = {};
        stream.Enable = TRUE;
        stream.pInputSurface = mD3D11InputView.Get();
        
        hr = mD3D11VideoContext->VideoProcessorBlt(mD3D11VideoProcessor.Get(), mD3D11OutputView.Get(), 0, 1, &stream);
        if (FAILED(hr)) {
            return Error::External;
        }

        // Copy 
        mD3D11Context->CopyResource(mD3D11StagingTexture.Get(), mD3D11OutputTexture.Get());

        // Convert to AVFrame
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = mD3D11Context->Map(mD3D11StagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
        if (FAILED(hr)) {
            return Error::External;
        }
        if (mappedResource.RowPitch != dstFrame->linesize[0]) {
            for (int y = 0; y < dstFrame->height; y++) {
                ::memcpy(
                    dstFrame->data[0] + dstFrame->linesize[0] * y, 
                    (uint8_t*) mappedResource.pData + mappedResource.RowPitch * y, 
                    dstFrame->linesize[0]
                );
            }
        }
        else {
            ::memcpy(dstFrame->data[0], mappedResource.pData, dstFrame->linesize[0] * dstFrame->height);
        }

        mD3D11Context->Unmap(mD3D11StagingTexture.Get(), 0);

        return Error::Ok;
    }
    void _d3d11Cleanup() {
        mD3D11VideoProcessorEnumerator.Reset();
        mD3D11VideoContext.Reset();
        mD3D11VideoDevice.Reset();
        mD3D11VideoProcessor.Reset();
        mD3D11InputTexture.Reset();
        mD3D11OutputTexture.Reset();
        mD3D11StagingTexture.Reset();
        mD3D11InputView.Reset();
        mD3D11OutputView.Reset();
        mD3D11Context.Reset();
        mD3D11Device.Reset();
        mD3D11OutputAVFormat = AV_PIX_FMT_NONE;
    }
    Vec<DXGI_FORMAT> _translateToDXGIFormat(std::span<const AVPixelFormat> formats) {
        Vec<DXGI_FORMAT> dxgiFormats;
        for (auto format : formats) {
            switch (format) {
                case AV_PIX_FMT_RGBA: dxgiFormats.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); break;
                case AV_PIX_FMT_BGRA: dxgiFormats.push_back(DXGI_FORMAT_B8G8R8A8_UNORM); break;
                case AV_PIX_FMT_RGBA64: dxgiFormats.push_back(DXGI_FORMAT_R16G16B16A16_FLOAT); break;
            }
        }
        return dxgiFormats;
    }
    AVPixelFormat _translateToAVPixelFormat(DXGI_FORMAT format) {
        switch (format) {
            case DXGI_FORMAT_R8G8B8A8_UNORM: return AV_PIX_FMT_RGBA;
            case DXGI_FORMAT_B8G8R8A8_UNORM: return AV_PIX_FMT_BGRA;
            case DXGI_FORMAT_R16G16B16A16_FLOAT: return AV_PIX_FMT_RGBA64;
            default: ::abort();
        }
    }
#endif

private:
    SwsContext *mCtxt = nullptr;
    AVFrame    *mSwFrame = nullptr; //< Used when hardware format
    Pad        *mSinkPad = nullptr;
    Pad        *mSourcePad = nullptr;
    bool        mPassthrough = false; //< Directly passthrough
    PixelFormat mTargetFormat = PixelFormat::None; //< If not None, force
    AVPixelFormat mSwsFormat = AV_PIX_FMT_NONE; //< Current target format
    AVPixelFormat mCopybackFormat = AV_PIX_FMT_NONE; //< Format for copyback

    // Method
    Error (FFVideoConverterImpl::*mConvert)(AVFrame *dst, AVFrame *src) = nullptr;
    void  (FFVideoConverterImpl::*mCleanup)() = nullptr;

    // Platform Graphics
#ifdef _WIN32
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3D11Device>        mD3D11Device;
    ComPtr<ID3D11DeviceContext> mD3D11Context;
    ComPtr<ID3D11VideoDevice>   mD3D11VideoDevice;
    ComPtr<ID3D11VideoContext> mD3D11VideoContext;
    ComPtr<ID3D11VideoProcessor> mD3D11VideoProcessor;
    ComPtr<ID3D11Texture2D>    mD3D11InputTexture;
    ComPtr<ID3D11Texture2D>    mD3D11OutputTexture;
    ComPtr<ID3D11Texture2D>    mD3D11StagingTexture; //< Texture for copy back to CPU
    ComPtr<ID3D11VideoProcessorInputView> mD3D11InputView;
    ComPtr<ID3D11VideoProcessorOutputView> mD3D11OutputView;
    ComPtr<ID3D11VideoProcessorEnumerator> mD3D11VideoProcessorEnumerator;
    //< Output format for AVFrame
    AVPixelFormat                          mD3D11OutputAVFormat = AV_PIX_FMT_NONE;
#endif
};

NEKO_REGISTER_ELEMENT(VideoConverter, FFVideoConverterImpl);

}

NEKO_NS_END