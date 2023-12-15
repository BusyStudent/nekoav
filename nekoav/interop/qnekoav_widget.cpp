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

class D2D1Widget final : public QWidget, public VideoWidgetPrivate {
public:
    D2D1Widget(QWidget *widget) : QWidget(widget) {
        setAttribute(Qt::WA_PaintOnScreen);

        mDll = ::LoadLibraryA("d2d1.dll");
        if (!mDll) {
            throw 1;
        }
        auto create = reinterpret_cast<
            HRESULT (__stdcall*)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS *, void **)
        >(::GetProcAddress(mDll, "D2D1CreateFactory"));
        if (!create) {
            throw 1;
        }

        if (FAILED(create(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, &mD2DFactory))) {
            throw 1;
        }

        // Try init
        initializeD2D1();
    }
    ~D2D1Widget() {
        cleanupD2D1();
        mD2DFactory.Reset();
        ::FreeLibrary(mDll);
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
        if (mBitmap) {
            auto [width, height] = mRenderTarget->GetSize();
            mRenderTarget->DrawBitmap(mBitmap.Get(), D2D1::RectF(0, 0, width, height));
        }
        auto hr = mRenderTarget->EndDraw();
        if (FAILED(hr)) {
            if (hr == D2DERR_RECREATE_TARGET) {
                cleanupD2D1();
                initializeD2D1();
            }
        }
    }
    void resizeEvent(QResizeEvent *event) override {
        if (mRenderTarget) {
            auto hwnd = (HWND)winId();
            RECT rect;
            ::GetClientRect(hwnd, &rect);
            mRenderTarget->Resize(D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top));
        }
    }
    void changeEvent(QEvent *event) override {
        if (event->type() == QEvent::WinIdChange) {
            qDebug() << "WinId changed";
            cleanupD2D1();
            initializeD2D1();
        }
    }

    void initializeD2D1() {
        auto hwnd = (HWND)winId();
        if (!hwnd) {
            return;
        }
        auto hr = mD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd),
            mRenderTarget.ReleaseAndGetAddressOf()
        );

        if (SUCCEEDED(mRenderTarget.As(&mDeviceContext))) {
            if (mDeviceContext->IsDxgiFormatSupported(DXGI_FORMAT_B8G8R8A8_UNORM)) {
                mPixelFormats.push_back(PixelFormat::BGRA);
            }
        }
    }
    void cleanupD2D1() {
        mBitmap.Reset();
        mDeviceContext.Reset();
        mRenderTarget.Reset();
    }

    // VideoRenderer
    void clearFrame() override {
        mBitmap.Reset();
        update();
    }
    void updateFrame(const Arc<MediaFrame> &frame) override {
        int width = frame->width();
        int height = frame->height();
        bool requestRelayout = false;
        if (mBitmap) {
            auto [bwidth, bheight] = mBitmap->GetPixelSize();
            if (bwidth != width || bheight != height) {
                mBitmap.Reset();
                requestRelayout = true;
            }
        }
        if (!mBitmap) {
            auto hr = mRenderTarget->CreateBitmap(
                D2D1::SizeU(width, height),
                D2D1::BitmapProperties(
                    D2D1::PixelFormat(translateFormat(frame->pixelFormat()), D2D1_ALPHA_MODE_PREMULTIPLIED)
                ),
                mBitmap.ReleaseAndGetAddressOf()
            );
        }
        mBitmap->CopyFromMemory(nullptr, frame->data(0), frame->linesize(0));
        
        if (requestRelayout) {
            static_cast<VideoWidget*>(parent())->internalRelayout();
        }
        update();
    }
    DXGI_FORMAT translateFormat(PixelFormat fmt) const {
        switch (fmt) {
            case PixelFormat::RGBA: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case PixelFormat::BGRA: return DXGI_FORMAT_B8G8R8A8_UNORM;
            default: ::abort();
        }
    }
private:
    HMODULE mDll = nullptr;

    ComPtr<ID2D1Factory> mD2DFactory;
    ComPtr<ID2D1HwndRenderTarget> mRenderTarget;
    ComPtr<ID2D1DeviceContext>    mDeviceContext;
    ComPtr<ID2D1Bitmap>           mBitmap;
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