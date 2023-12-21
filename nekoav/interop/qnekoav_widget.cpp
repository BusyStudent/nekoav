#ifndef _QNEKO_SOURCE
    #define _QNEKO_SOURCE
#endif

#include "../media.hpp"
#include "../format.hpp"
#include "qnekoav_p.hpp"
#include <QResizeEvent>
#include <QPainter>
#include <QWidget>
#include <QPixmap>
#include <QDebug>

#if __has_include(<QOpenGLWidget>) && defined(QNEKO_HAS_OPENGL)
    #include <QOpenGLWidget>
    #include <QOpenGLContext>
    #include <QOpenGLFunctions>
    #include <QOpenGLFunctions_3_3_Core>
#endif

#if defined(_WIN32)
    #define QNEKO_HAS_D2D1
    #include <d2d1.h>
    #include <d2d1_1.h>
    #include <d2d1_2.h>
    #include <d2d1_3.h>
    #include <d3d11.h>
    #include <wrl/client.h>
#endif

namespace QNekoAV {

using namespace NEKO_NAMESPACE;
using NEKO_NAMESPACE::Arc;


class PainterWidget final : public QWidget, public VideoWidgetPrivate {
public:
    PainterWidget(QWidget *parent) : QWidget(parent) {}

    // VideoWidgetPrivate
    QWidget *widget() override {
        return this;
    }

    // VideoRenderer
    void updateFrame(const Arc<MediaFrame> &frame) override {
        NEKO_ASSERT(frame->pixelFormat() == PixelFormat::RGBA);
        int width = frame->width();
        int height = frame->height();

        bool notifyRelayout = false;
        if (mPixmap.isNull()) {
            notifyRelayout = true;
        }

        QImage image(width, height, QImage::Format_RGBA8888);
        int srcPitch = frame->linesize(0);
        int dstPitch = image.bytesPerLine();
        if (srcPitch != dstPitch) {
            // Update it
            auto dst = image.bits();
            auto pixels = (uint8_t*) frame->data(0);
            for (int y = 0; y < image.height(); y++) {
                for (int x = 0; x < image.width(); x++) {
                    *((uint32_t*)   &dst[y * dstPitch + x * 4]) = *(
                        (uint32_t*) &pixels[y * srcPitch + x * 4]
                    );
                }
            }
        }
        else {
            ::memcpy(image.bits(), frame->data(0), width * height * 4);
        }
        mPixmap = QPixmap::fromImage(std::move(image));
        if (notifyRelayout) {
            static_cast<VideoWidget*>(parent())->internalRelayout();
        }
        update();
    }
    void clearFrame() override {
        mPixmap = QPixmap();
        update();
    }

    // QWidget
    void paintEvent(QPaintEvent *event) override {
        if (!mPixmap.isNull()) {
            QPainter painter(this);
            painter.drawPixmap(0, 0, width(), height(), mPixmap);
        }
    }

    Arc<MediaFrame> mFrame;
    QPixmap         mPixmap;
};

#ifdef QNEKO_HAS_OPENGL

static const char *vertexShaderRGBA = R"(
#version 330 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texcoord;

out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragTexCoord = texcoord;
}
)";
static const char *fragShaderRGBA = R"(
#version 330 core
in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D textureSampler;

void main() {
    fragColor = texture(textureSampler, fragTexCoord);
}
)";

class OpenGLWidget final : public QOpenGLWidget, public VideoWidgetPrivate, public QOpenGLFunctions_3_3_Core {
public:
    OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent) {

    }
    ~OpenGLWidget() {
        QObject::disconnect(
            context(), &QOpenGLContext::aboutToBeDestroyed, 
            this, &OpenGLWidget::cleanupGL
        );
        cleanupGL();
    }
    void initializeGL() override {
        makeCurrent();

        // Init cleanup
        QObject::connect(
            context(), &QOpenGLContext::aboutToBeDestroyed, 
            this, &OpenGLWidget::cleanupGL
        );
        initializeOpenGLFunctions();

#ifndef NDEBUG
        // Enable Debugger layer if
        if (context()->hasExtension(QByteArrayLiteral("GL_ARB_debug_output"))) {
            glEnable(GL_DEBUG_OUTPUT);
            auto glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC) context()->getProcAddress(
                "glDebugMessageCallback"
            );
            if (glDebugMessageCallback) {
                glDebugMessageCallback([](
                    GLenum source, 
                    GLenum type, 
                    GLuint id, 
                    GLenum severity, 
                    GLsizei length, 
                    const GLchar *message, 
                    const void *userParam) 
                {
                    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", 
                        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
                        type, severity, message);
                    fflush(stderr);
                }, nullptr);
            }
        }
#endif

        initShader();

        glGenVertexArrays(1, &mVertexArray);
        glGenBuffers(1, &mBuffer);

        GLfloat vertex [] = {
            // Vertex    
            -1.0f,  1.0f, 
             1.0f,  1.0f,
             1.0f, -1.0f,
            -1.0f, -1.0f,
            // Texture Coord
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f
        };
        // Config Buffer
        glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);

        // Config VertexArray 
        glBindVertexArray(mVertexArray);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, 0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, reinterpret_cast<void*>(8 * sizeof(GLfloat)));
    }
    void resizeGL(int width, int height) override {
        qDebug() << "Resized to" << width << ", " << height;
        makeCurrent();
        glViewport(0, 0, width, height);
    }
    void paintGL() override {
        makeCurrent();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (!mTextures[0]) {
            // No data
            return;
        }
        glUseProgram(mProgram);
        glBindVertexArray(mVertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, mBuffer);

        // Bind All texture if
        for (int i = 0; i < sizeof(mTextures) / sizeof(GLuint); i++) {
            if (!mTextures[i]) {
                continue;
            }
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, mTextures[i]);
        }

        // Draw
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // Cleanup
        for (int i = 0; i < sizeof(mTextures) / sizeof(GLuint); i++) {
            if (!mTextures[i]) {
                continue;
            }
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glUseProgram(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    void cleanupGL() {
        makeCurrent();

        if (mVertexArray) {
            glDeleteVertexArrays(1, &mVertexArray);
            mVertexArray = 0;
        }
        if (mBuffer) {
            glDeleteBuffers(1, &mBuffer);
            mBuffer = 0;
        }
        if (mProgram) {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
        cleanupTexture();
    }
    void cleanupTexture() {
        for (auto &texture : mTextures) {
            if (texture) {
                glDeleteTextures(1, &texture);
                texture = 0;
            }
        }
        mTextureWidth = 0;
        mTextureHeight = 0;
    }
    void initShader() {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vertexShader, 1, &vertexShaderRGBA, nullptr);
        glShaderSource(fragShader, 1, &fragShaderRGBA, nullptr);
        glCompileShader(vertexShader);
        glCompileShader(fragShader);

        mProgram = glCreateProgram();
        glAttachShader(mProgram, vertexShader);
        glAttachShader(mProgram, fragShader);
        glLinkProgram(mProgram);

        // Release shader
        glDeleteShader(vertexShader);
        glDeleteShader(fragShader);

        // Check OK
        GLint status = 0;
        glGetProgramiv(mProgram, GL_LINK_STATUS, &status);
        if (status == GL_FALSE) {
            glDeleteProgram(mProgram);
            mProgram = 0;
            // qDebug() << "Failed to link shader program";
            return;
        }

        // Set frag sampler
        glUseProgram(mProgram);
        glUniform1i(glGetUniformLocation(mProgram, "textureSampler"), 0);
        glUseProgram(0);
    }

    // VideoWidgetPrivate
    QWidget *widget() override {
        return this;
    }

    // VideoRenderer
    void updateFrame(const Arc<MediaFrame> &frame) override {
        NEKO_ASSERT(frame->pixelFormat() == PixelFormat::RGBA);
        makeCurrent();

        int width = frame->width();
        int height = frame->height();
        
        bool requestRelayout = false;
        if (mTextureWidth != width || mTextureHeight != height) {
            cleanupTexture();
        }
        if (mTextures[0] == 0) {
            // New a new texture
            mTextureWidth = width;
            mTextureHeight = height;

            glGenTextures(1, mTextures);
            glBindTexture(GL_TEXTURE_2D, mTextures[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            requestRelayout = true;
        }
        // Update texture
        if (frame->linesize(0) != width * 4) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize(0) / 4);
        }
        glBindTexture(GL_TEXTURE_2D, mTextures[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame->data(0));
        glBindTexture(GL_TEXTURE_2D, 0);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        if (requestRelayout) {
            static_cast<VideoWidget*>(parent())->internalRelayout();
        }

        update();
    }
    void clearFrame() override {
        makeCurrent();
        cleanupTexture();
        update();
    }

    // OpenGL Data
    GLuint mVertexArray = 0;
    GLuint mBuffer = 0;
    GLuint mProgram = 0;
    GLuint mTextures[4] {0};

    GLuint mTextureWidth = 0;
    GLuint mTextureHeight = 0;
};
#endif

#ifdef QNEKO_HAS_D2D1
using Microsoft::WRL::ComPtr;

static bool forceD2D1_0 = true; //< TODO : Because it need improve

// Global shared d2d1factory
typedef HRESULT (__stdcall* PFN_D2D1Create)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS *, void **);
static HMODULE d2d1dll = nullptr;
static PFN_D2D1Create d2d1Create = nullptr;
static ID2D1Factory *d2d1Factory = nullptr;

// D3D11 
using PFN_D3D11Create = decltype(::D3D11CreateDeviceAndSwapChain) *;
static HMODULE d3d11dll = nullptr;
static PFN_D3D11Create d3d11Create = nullptr;

static void DxWidgetInit() {
    if (!d2d1dll) {
        d2d1dll = ::LoadLibraryA("d2d1.dll");
        d3d11dll = ::LoadLibraryA("d3d11.dll");
        if (!d2d1dll) {
            throw 1;
        }
        if (d3d11dll) {
            d3d11Create = reinterpret_cast<PFN_D3D11Create>(::GetProcAddress(d3d11dll, "D3D11CreateDeviceAndSwapChain"));
        }
        d2d1Create = reinterpret_cast<PFN_D2D1Create>(::GetProcAddress(d2d1dll, "D2D1CreateFactory"));
        if (!d2d1Create) {
            throw 1;
        }
        D2D1_FACTORY_OPTIONS options {
            .debugLevel = D2D1_DEBUG_LEVEL_NONE
        };
#if !defined(NDEBUG) && defined(_MSC_VER)
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        if (FAILED(d2d1Create(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), &options, (void**) &d2d1Factory))) {
            throw 1;
        }
    }
    else {
        d2d1Factory->AddRef();
    }
}
static void DxWidgetShutdown() {
    if (d2d1Factory->Release() == 0) {
        d2d1Factory = nullptr;
        d2d1Create = nullptr;
        d3d11Create = nullptr;
        ::FreeLibrary(d2d1dll);
        ::FreeLibrary(d3d11dll);
        d2d1dll = nullptr;
        d3d11dll = nullptr;
    }
}

class D2D1Widget final : public QWidget, public VideoWidgetPrivate {
public:
    D2D1Widget(QWidget *widget) : QWidget(widget) {
        setAttribute(Qt::WA_PaintOnScreen);
        DxWidgetInit();
        // Try init
        initializeD2D1();
    }
    ~D2D1Widget() {
        cleanupD2D1();
        DxWidgetShutdown();
    }
    QWidget *widget() override {
        return this;
    }
    QPaintEngine *paintEngine() const override {
        return nullptr;
    }
    void paintEvent(QPaintEvent *) override {
        if (!mRenderTarget) {
            return;
        }
        mRenderTarget->BeginDraw();
        mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        auto [screenWidth, screenHeight] = mRenderTarget->GetSize();
        if (mBitmap) {
            if (mDeviceContext) {
                // Cubic
                mDeviceContext->DrawBitmap(
                    mBitmap.Get(), 
                    D2D1::RectF(0, 0, screenWidth, screenHeight), 
                    1.0f, D2D1_INTERPOLATION_MODE_CUBIC
                );
            }
            else {
                mRenderTarget->DrawBitmap(mBitmap.Get(), D2D1::RectF(0, 0, screenWidth, screenHeight));
            }
        }
        else if (mImageSource) {
            D3D11_TEXTURE2D_DESC desc {0};
            mYUVTexture->GetDesc(&desc);
            mDeviceContext2->SetTransform(D2D1::Matrix3x2F::Scale(
                float(screenHeight) / desc.Height,
                float(screenWidth) / desc.Width
            ));
            mDeviceContext2->DrawImage(mImageSource.Get());
            mDeviceContext2->SetTransform(D2D1::Matrix3x2F::Identity());
        }
        auto hr = mRenderTarget->EndDraw();
        if (mSwapChain) {
            mSwapChain->Present(1, 0);
        }
        if (FAILED(hr)) {
            if (hr == D2DERR_RECREATE_TARGET) {
                cleanupD2D1();
                initializeD2D1();
            }
        }
    }
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override {
        MSG *msg = static_cast<MSG *>(message);
        switch (msg->message) {
            case WM_SIZE: {
                onResize(HIWORD(msg->lParam), LOWORD(msg->lParam));
                break;
            }
        }
        return false;
    }
    void changeEvent(QEvent *event) override {
        if (event->type() == QEvent::WinIdChange) {
            qDebug() << "WinId changed";
            cleanupD2D1();
            initializeD2D1();
        }
    }
    void onResize(int width, int height) {
        if (mHwndRenderTarget) {
            mHwndRenderTarget->Resize(D2D1::SizeU(width, height));
        }
        else if (mSwapChain) {
            resizeD2D1_3();
        }
    }
    void initializeD2D1() {
        ComPtr<ID2D1Factory3> factory3;
        if (FAILED(d2d1Factory->QueryInterface(factory3.GetAddressOf())) || !d3d11Create || forceD2D1_0) {
            initializeD2D1_0(); //< Use Legacy D2D1.0
        }
        else {
            if (!initializeD2D1_3(factory3.Get())) { //< Use D2D1.3
                // Fallback
                cleanupD2D1();
                initializeD2D1_0();
            }
        }

        // Common Check
        if (SUCCEEDED(mRenderTarget.As(&mDeviceContext))) {
            // Check RGB family
            if (mDeviceContext->IsDxgiFormatSupported(DXGI_FORMAT_B8G8R8A8_UNORM)) {
                mPixelFormats.push_back(PixelFormat::BGRA);
            }
        }
    }
    void initializeD2D1_0() {
        auto hwnd = (HWND)winId();
        if (!hwnd) {
            return;
        }
        auto hr = d2d1Factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            ),
            D2D1::HwndRenderTargetProperties(hwnd),
            mHwndRenderTarget.ReleaseAndGetAddressOf()
        );
        mRenderTarget = mHwndRenderTarget;
    }
    bool initializeD2D1_3(ID2D1Factory3 *factory) {
        auto hwnd = (HWND)winId();
        if (!hwnd) {
            return false;
        }
        RECT rect;
        ::GetClientRect(hwnd, &rect);

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT hr = 0;

#if !defined(NDEBUG) && defined(_MSC_VER)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        DXGI_SWAP_CHAIN_DESC swapChainDesc {};
        ::memset(&swapChainDesc, 0, sizeof(swapChainDesc));
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.OutputWindow = hwnd;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.BufferDesc.Width = rect.right - rect.left;
        swapChainDesc.BufferDesc.Height = rect.bottom - rect.top;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;

        for (auto type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
            hr = d3d11Create(
                nullptr,
                type,
                nullptr,
                flags,
                nullptr,
                0,
                D3D11_SDK_VERSION,
                &swapChainDesc,
                &mSwapChain,
                &mD3D11Device,
                nullptr,
                &mD3D11DeviceContext
            );
            if (SUCCEEDED(hr)) {
                break;
            }
        }
        if (FAILED(hr)) {
            return false;
        }

        // Create D2D1
        ComPtr<IDXGIDevice> dxDevice;
        ComPtr<ID2D1Device2> d2dDevice;
        hr = mD3D11Device.As(&dxDevice);
        hr = factory->CreateDevice(dxDevice.Get(), &d2dDevice);
        if (FAILED(hr)) {
            return false;
        }
        hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &mDeviceContext2);
        if (FAILED(hr)) {
            return false;
        }
        mRenderTarget = mDeviceContext2;
        if (!resizeD2D1_3()) {
            return false;
        }
        mPixelFormats.push_back(PixelFormat::NV12);
        mPixelFormats.push_back(PixelFormat::D3D11);
        return true;
    }
    bool resizeD2D1_3() {
        HRESULT hr = 0;
        mDeviceContext2->SetTarget(nullptr);
        hr = mSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) {
            return false;
        }

        // Get 
        ComPtr<ID3D11Texture2D> backBuffer;
        hr = mSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr)) {
            return false;
        }
        ComPtr<IDXGISurface> dxSurface;
        if (FAILED(backBuffer.As(&dxSurface))) {
            return false;
        }

        // Create Image for this texture
        ComPtr<ID2D1Bitmap1> bitmap;
        hr = mDeviceContext2->CreateBitmapFromDxgiSurface(
            dxSurface.Get(),
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            ),
            &bitmap
        );
        if (FAILED(hr)) {
            return false;
        }
        mDeviceContext2->SetTarget(bitmap.Get());
        return true;
    }
    void cleanupD2D1() {
        // Cleanup YUV Parts
        cleanupD2D1_3_Textures();
        mD3D11DeviceContext.Reset();
        mD3D11Device.Reset();
        mSwapChain.Reset();
        mDeviceContext2.Reset();

        // D2D1.1
        mDeviceContext.Reset();

        // D2D1.0 Parts
        mBitmap.Reset();
        mRenderTarget.Reset();
        mHwndRenderTarget.Reset();
    }
    void cleanupD2D1_3_Textures() {
        mYUVTexture.Reset();
        mImageSource.Reset();
        mSharedHandle = nullptr;
    }

    // VideoRenderer
    void clearFrame() override {
        mBitmap.Reset();
        cleanupD2D1_3_Textures();
        update();
    }
    void updateFrame(const Arc<MediaFrame> &frame) override {
        bool requestRelayout = false;
        switch (frame->pixelFormat()) {
            case PixelFormat::BGRA:
            case PixelFormat::RGBA:
                updateFrameRGBA(frame, &requestRelayout);
                break;
            case PixelFormat::NV12:
            case PixelFormat::NV21:
                updateFrameYUV(frame, &requestRelayout);
                break;
            case PixelFormat::D3D11:
                updateFrameD3D11VA(frame, &requestRelayout);
                break;
            default:
                break;
        }
        
        if (requestRelayout) {
            static_cast<VideoWidget*>(parent())->internalRelayout();
        }
        update();
    }
    void updateFrameRGBA(const Arc<MediaFrame> &frame, bool *requestRelayout) {
        int width = frame->width();
        int height = frame->height();
        if (mBitmap) {
            auto [bwidth, bheight] = mBitmap->GetPixelSize();
            if (bwidth != width || bheight != height) {
                mBitmap.Reset();
            }
        }
        if (!mBitmap) {
            *requestRelayout = true;
            auto hr = mRenderTarget->CreateBitmap(
                D2D1::SizeU(width, height),
                D2D1::BitmapProperties(
                    D2D1::PixelFormat(translateFormat(frame->pixelFormat()), D2D1_ALPHA_MODE_PREMULTIPLIED)
                ),
                mBitmap.ReleaseAndGetAddressOf()
            );
        }
        mBitmap->CopyFromMemory(nullptr, frame->data(0), frame->linesize(0));
    }
    void updateFrameYUV(const Arc<MediaFrame> &frame, bool *requestRelayout) {
        // TODO : YUV need improved
        int width = frame->width();
        int height = frame->height();
        HRESULT hr = 0;
        if (mImageSource) {
            D3D11_TEXTURE2D_DESC desc {0};
            mYUVTexture->GetDesc(&desc);
            if (desc.Width != width || desc.Height != height) {
                cleanupD2D1_3_Textures();
            }
        }
        if (!mImageSource) {
            *requestRelayout = true;
            ComPtr<IDXGISurface> dxSurface;
            D3D11_TEXTURE2D_DESC desc {
                .Width = UINT(width),
                .Height = UINT(height),
                .MipLevels = 1,
                .ArraySize = 1,
                .Format = translateFormat(frame->pixelFormat()),
                .SampleDesc = {1, 0},
                .Usage = D3D11_USAGE_DYNAMIC,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
            };
            hr = mD3D11Device->CreateTexture2D(&desc, nullptr, &mYUVTexture);
            hr = mYUVTexture.As(&dxSurface);
            hr = mDeviceContext2->CreateImageSourceFromDxgi(
                dxSurface.GetAddressOf(),
                1,
                DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601,
                D2D1_IMAGE_SOURCE_FROM_DXGI_OPTIONS_NONE,
                &mImageSource
            );
        }
        // Update here
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = mD3D11DeviceContext->Map(mYUVTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            return;
        }

        // Copy pixels
        BYTE *dst = static_cast<BYTE*>(mapped.pData);
        BYTE *yData = static_cast<BYTE*>(frame->data(0));
        BYTE *uvData = static_cast<BYTE*>(frame->data(1));
        int yPitch = frame->linesize(0);
        int uvPitch = frame->linesize(1);

        for (int i = 0; i < height; i++){
            ::memcpy(dst + mapped.RowPitch * i, yData + yPitch * i, yPitch);
        }
        for (int i = 0; i < height / 2; i++) {
            ::memcpy(dst + mapped.RowPitch * (height + i), uvData + uvPitch * i, uvPitch);
        }

        mD3D11DeviceContext->Unmap(mYUVTexture.Get(), 0);
    }
    void updateFrameD3D11VA(const Arc<MediaFrame> &frame, bool *requestRelayout) {
        ID3D11Texture2D *texture = reinterpret_cast<ID3D11Texture2D*>(frame->data(0));
        intptr_t index = reinterpret_cast<intptr_t>(frame->data(1));

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        ComPtr<ID3D11Device> device;
        texture->GetDevice(&device);
        ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(&context);

        int width = frame->width();
        int height = frame->height();
        HRESULT hr = 0;
        if (mImageSource) {
            D3D11_TEXTURE2D_DESC desc {0};
            mYUVTexture->GetDesc(&desc);
            if (desc.Width != width || desc.Height != height) {
                cleanupD2D1_3_Textures();
            }
        }
        if (!mImageSource) {
            *requestRelayout = true;
                D3D11_TEXTURE2D_DESC texDesc {
                .Width = UINT(width),
                .Height = UINT(height),
                .MipLevels = 1,
                .ArraySize = 1,
                .Format = desc.Format,
                .SampleDesc = {1, 0},
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                .MiscFlags = D3D11_RESOURCE_MISC_SHARED
            };
            hr = mD3D11Device->CreateTexture2D(&texDesc, nullptr, &mYUVTexture);
            if (FAILED(hr)) {
                return;
            }
            ComPtr<IDXGIResource> resource;
            ComPtr<IDXGISurface> dxSurface;
            hr = mYUVTexture.As(&resource);
            hr = mYUVTexture.As(&dxSurface);
            hr = resource->GetSharedHandle(&mSharedHandle);
            if (FAILED(hr)) {
                return;
            }
            hr = mDeviceContext2->CreateImageSourceFromDxgi(
                dxSurface.GetAddressOf(),
                1,
                DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601,
                D2D1_IMAGE_SOURCE_FROM_DXGI_OPTIONS_NONE,
                &mImageSource
            );
            if (FAILED(hr)) {
                return;
            }
        }
        // Update data here
        ComPtr<ID3D11Texture2D> dstTexture;
        hr = device->OpenSharedResource(mSharedHandle, IID_PPV_ARGS(&dstTexture));
        if (FAILED(hr)) {
            return;
        }
        context->CopySubresourceRegion(dstTexture.Get(), 0, 0, 0, 0, texture, index, 0);
        context->Flush();
    }
    DXGI_FORMAT translateFormat(PixelFormat fmt) const {
        switch (fmt) {
            case PixelFormat::RGBA: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case PixelFormat::BGRA: return DXGI_FORMAT_B8G8R8A8_UNORM;
            case PixelFormat::NV12: return DXGI_FORMAT_NV12;
            default: ::abort();
        }
    }
private:
    // D2D1.0
    ComPtr<ID2D1HwndRenderTarget> mHwndRenderTarget;
    ComPtr<ID2D1RenderTarget> mRenderTarget;
    ComPtr<ID2D1Bitmap>       mBitmap;

    // D2D1.1
    ComPtr<ID2D1DeviceContext>    mDeviceContext;

    // D2D1.3 YUV Support
    ComPtr<ID2D1DeviceContext2>   mDeviceContext2;
    ComPtr<ID2D1ImageSource>      mImageSource;

    // D3D11Device
    ComPtr<ID3D11Device>          mD3D11Device;
    ComPtr<ID3D11DeviceContext>   mD3D11DeviceContext;
    ComPtr<IDXGISwapChain>        mSwapChain;
    ComPtr<ID3D11Texture2D>       mYUVTexture;
    HANDLE                        mSharedHandle = nullptr;
};
#endif


VideoWidgetPrivate *CreateVideoWidgetPrivate(VideoWidget *widget) {

#ifdef QNEKO_HAS_D2D1
    try {
        return new D2D1Widget(widget);
    }
    catch (int) {

    }
#endif

#ifdef QNEKO_HAS_OPENGL
    return new OpenGLWidget(widget);
#else
    return new PainterWidget(widget);
#endif
}

}