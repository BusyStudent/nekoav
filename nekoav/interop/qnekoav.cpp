#ifndef _QNEKO_SOURCE
    #define _QNEKO_SOURCE
#endif

#include "qnekoav.hpp"
#include "qnekoav_p.hpp"
#include "../player.hpp"
#include "../property.hpp"

#include <QPainter>
#include <QWidget>
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
    QString  mErrorString;

    MediaPlayer::Error mError = MediaPlayer::NoError;
    MediaPlayer::PlaybackState mState = MediaPlayer::StoppedState;
    MediaPlayer::MediaStatus mStatus = MediaPlayer::NoMedia;
};

static auto translateError(NEKO_NAMESPACE::Error err) {
    using Err = NEKO_NAMESPACE::Error;
    switch (err) {
        case Err::UnsupportedMediaFormat: return MediaPlayer::FormatError;
        default: return MediaPlayer::UnknownError;
    }
}
static auto translateState(NEKO_NAMESPACE::State state) {
    using State = NEKO_NAMESPACE::State;
    switch (state) {
        case State::Running: return MediaPlayer::PlayingState;
        case State::Paused: return MediaPlayer::PausedState;
        case State::Null: return MediaPlayer::StoppedState;
        default: return MediaPlayer::StoppedState;
    }
}

MediaPlayer::MediaPlayer(QObject *parent) : QObject(parent) {
    d = new MediaPlayerPrivate;
    d->mPlayer.setPositionCallback([this](double progress) {
        QMetaObject::invokeMethod(this, [this, progress]() {
            Q_EMIT positionChanged(progress);
        }, Qt::QueuedConnection);
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
        QMetaObject::invokeMethod(this, [this, state]() {
            auto newState = translateState(state);
            if (d->mState != newState) {
                d->mState = newState;
                Q_EMIT playbackStateChanged(d->mState);
            }
        }, Qt::QueuedConnection);
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
    auto r = object->property("videoRenderer");
    if (r.value<void*>()) {
        d->mVideoOutput = object;
        d->mPlayer.setVideoRenderer(static_cast<VideoRenderer*>(r.value<void*>()));

        Q_EMIT videoOutputChanged();
    }
}

void MediaPlayer::setSource(const QUrl &source) {
    if (source.isLocalFile()) {
        d->mPlayer.setUrl(source.toLocalFile().toUtf8().constData());
    }
    else {
        d->mPlayer.setUrl(source.toString().toUtf8().constData());
    }
    d->mUrl = source;

    Q_EMIT sourceChanged(source);
}
void MediaPlayer::setSourceDevice(QIODevice *device, const QUrl &sourceUrl) {

}
void MediaPlayer::setHttpUserAgent(const QString &userAgent) {
    d->mPlayer.setOption(Properties::HttpUserAgent, userAgent.toUtf8().constData());
}
void MediaPlayer::setHttpReferer(const QString &referer) {
    d->mPlayer.setOption(Properties::HttpReferer, referer.toUtf8().constData());
}
void *MediaPlayer::addFilter(const QString &string, const QVariantMap &parameters) {
    return d->mPlayer.addFilter(Filter(string.toUtf8().constData()));
}
void MediaPlayer::removeFilter(void *filter) {
    return d->mPlayer.removeFilter(filter);
}


MediaPlayer::PlaybackState MediaPlayer::playbackState() const {
    return d->mState;
}
MediaPlayer::Error MediaPlayer::error() const {
    return d->mError;
}
QString MediaPlayer::errorString() const {
    return d->mErrorString;
}
bool MediaPlayer::hasVideo() const {
    return d->mPlayer.hasVideo();
}
bool MediaPlayer::hasAudio() const {
    return d->mPlayer.hasAudio();
}
bool MediaPlayer::isPlaying() const {
    return playbackState() == PlayingState;
}
bool MediaPlayer::isAvailable() const {
    return true;
}
QUrl MediaPlayer::source() const {
    return d->mUrl;
}
QObject *MediaPlayer::videoOutput() const {
    return d->mVideoOutput;
}

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) {
    d = CreateVideoWidgetPrivate(this);
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
bool MediaPlayer::isSeekable() const {
    return d->mPlayer.isSeekable();
}

}  // namespace QNekoAV