#ifndef _QNEKO_SOURCE
    #define _QNEKO_SOURCE
#endif

#include "qnekoav.hpp"
#include "../elements/videosink.hpp"
#include "../pipeline.hpp"
#include "../factory.hpp"
#include "../player.hpp"
#include "../format.hpp"
#include "../media.hpp"

#if __has_include(<QOpenGLWidget>)
    #define QNEKO_HAS_OPENGL
    #include <QOpenGLWidget>
    #include <QOpenGLContext>
    #include <QOpenGLFunctions>
    #include <QOpenGLFunctions_3_3_Core>

#endif

#include <QPainter>
#include <QPixmap>
#include <QDebug>

namespace QNekoAV {

using namespace NEKO_NAMESPACE;
using NEKO_NAMESPACE::Arc;

class MediaPlayerPrivate {
public:
    Player mPlayer;
    QObject *mVideoOutput = nullptr;
    QUrl     mUrl;
    MediaPlayer::Error mError = MediaPlayer::NoError;
    QString            mErrorString;
};


auto translateError(NEKO_NAMESPACE::Error err) {
    using Err = NEKO_NAMESPACE::Error;
    switch (err) {
        default: return MediaPlayer::UnknownError;
    }
}
auto translateState(NEKO_NAMESPACE::State state) {
    using State = NEKO_NAMESPACE::State;
    switch (state) {
        case State::Running: return MediaPlayer::PlayingState;
        case State::Paused: return MediaPlayer::PausedState;
        case State::Null: return MediaPlayer::StoppedState;
        default: ::abort();
    }
}

MediaPlayer::MediaPlayer(QObject *parent) : QObject(parent) {
    d = new MediaPlayerPrivate;
    d->mPlayer.setPositionCallback([this](double progress) {
        Q_EMIT positionChanged(progress);
    });
    d->mPlayer.setErrorCallback([this](NEKO_NAMESPACE::Error err, std::string_view reason) {
        QMetaObject::invokeMethod(this, [this, err, s = std::string(reason)]() {
            d->mErrorString = QString::fromUtf8(s);
            d->mError = translateError(err);

            Q_EMIT errorChanged();
            Q_EMIT errorOccurred(d->mError, d->mErrorString);
        }, Qt::QueuedConnection);
    });
    d->mPlayer.setStateChangedCallback([this](NEKO_NAMESPACE::State state) {
        if (state == NEKO_NAMESPACE::State::Ready) {
            return;
        }
        Q_EMIT playbackStateChanged(translateState(state));
    });
}
MediaPlayer::~MediaPlayer() {
    delete d;
}
void MediaPlayer::play() {
    d->mPlayer.play();
}
void MediaPlayer::pause() {
    d->mPlayer.pause();
}
void MediaPlayer::stop() {
    d->mPlayer.stop();
}

void MediaPlayer::setPosition(qreal position) {
    d->mPlayer.setPosition(position);
}

void MediaPlayer::setPlaybackRate(qreal rate) {

}
void MediaPlayer::setVideoOutput(QObject *object) {
    if (auto widget = dynamic_cast<VideoWidget*>(object); widget) {
        d->mVideoOutput = object;
        d->mPlayer.setVideoRenderer(static_cast<VideoRenderer*>(widget->videoRenderer()));
    }
}

void MediaPlayer::setSource(const QUrl &source) {
    d->mPlayer.setUrl(source.toLocalFile().toUtf8().constData());
    d->mUrl = source;

    Q_EMIT sourceChanged(source);
}
void MediaPlayer::setSourceDevice(QIODevice *device, const QUrl &sourceUrl) {

}


class VideoWidgetPrivate : public VideoRenderer {
public:
    virtual ~VideoWidgetPrivate() = default;
    virtual QWidget *widget() = 0;
    virtual QSize    videoSize() = 0;
};

class PainterWidget final : public QWidget, public VideoWidgetPrivate {
public:
    PainterWidget(QWidget *parent) : QWidget(parent) {}

    // VideoWidgetPrivate
    QWidget *widget() override {
        return this;
    }
    QSize videoSize() override {
        if (mPixmap.isNull()) {
            return QSize(1, 1);
        }
        return mPixmap.size();
    }

    // VideoRenderer
    Error setFrame(View<MediaFrame> frame) override {
        if (!frame) {
            // Null 
            mFrame = nullptr;
            QMetaObject::invokeMethod(this, [this]() {
                mPixmap = QPixmap();
                update();
            }, Qt::QueuedConnection);
            return Error::Ok;
        }
        mFrame = frame->shared_from_this<MediaFrame>();
        QMetaObject::invokeMethod(this, [f = Weak<MediaFrame>(mFrame), this]() {
            updateFrame(f);
        }, Qt::QueuedConnection);
        return Error::Ok;
    }
    Vec<PixelFormat> supportedFormats() override {
        Vec<PixelFormat> vec;
        vec.push_back(PixelFormat::RGBA);
        return vec;
    }
    void updateFrame(const Weak<MediaFrame> &weakFrame) {
        auto frame = weakFrame.lock();
        if (!frame) {
            return;
        }
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
    QSize videoSize() override {
        if (!mFrame) {
            return QSize(1, 1);
        }
        return QSize(mTextureWidth, mTextureHeight);
    }

    // VideoRenderer
    Error setFrame(View<MediaFrame> frame) override {
        if (!frame) {
            // Null 
            mFrame = nullptr;
            QMetaObject::invokeMethod(this, [this]() {
                makeCurrent();
                cleanupTexture();
                update();
            }, Qt::QueuedConnection);
            return Error::Ok;
        }
        mFrame = frame->shared_from_this<MediaFrame>();
        QMetaObject::invokeMethod(this, [f = Weak<MediaFrame>(mFrame), this]() {
            updateFrame(f);
        }, Qt::QueuedConnection);
        return Error::Ok;
    }
    Vec<PixelFormat> supportedFormats() override {
        Vec<PixelFormat> vec;
        vec.push_back(PixelFormat::RGBA);
        return vec;
    }
    void updateFrame(const Weak<MediaFrame> &weakFrame) {
        auto frame = weakFrame.lock();
        if (!frame) {
            return;
        }
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
        glBindTexture(GL_TEXTURE_2D, mTextures[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame->data(0));
        glBindTexture(GL_TEXTURE_2D, 0);

        if (requestRelayout) {
            static_cast<VideoWidget*>(parent())->internalRelayout();
        }

        update();
    }

    Arc<MediaFrame> mFrame;

    // OpenGL Data
    GLuint mVertexArray = 0;
    GLuint mBuffer = 0;
    GLuint mProgram = 0;
    GLuint mTextures[4] {0};

    GLuint mTextureWidth = 0;
    GLuint mTextureHeight = 0;
};


#endif

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) {
    // Check env
#ifndef QNEKO_HAS_OPENGL
    d = new PainterWidget(this);
#else
    d = new OpenGLWidget(this);
#endif

    // Setup
}
VideoWidget::~VideoWidget() {
    delete d;
}
void VideoWidget::internalRelayout() {
    auto [texWidth, texHeight] = d->videoSize();
    auto [winWidth, winHeight] = size();

    qreal x, y, w, h;

    if(texWidth * winHeight > texHeight * winWidth){
        w = winWidth;
        h = texHeight * winWidth / texWidth;
    }
    else{
        w = texWidth * winHeight / texHeight;
        h = winHeight;
    }
    x = (winWidth - w) / 2;
    y = (winHeight - h) / 2;

    auto dstRect = QRect(x, y, w, h);
    d->widget()->setGeometry(dstRect);
}
void VideoWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    internalRelayout();
}
void VideoWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(0, 0, width(), height(), Qt::black);
}
void *VideoWidget::videoRenderer() {
    return static_cast<VideoRenderer*>(d);
}

qreal MediaPlayer::duration() const { 
    return d->mPlayer.duration();
}
qreal MediaPlayer::position() const {
    return d->mPlayer.position();
}

}  // namespace QNekoAV