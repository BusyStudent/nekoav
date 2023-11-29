#pragma once

#include <QWidget>
#include <QObject>
#include <QUrl>

#ifdef  _MSC_VER
#define QNEKO_ATTRIBUTE(x) __declspec(x)
#elif   __GNUC__
#define QNEKO_ATTRIBUTE(x) __attribute__((x))
#else
#define QNEKO_ATTRIBUTE(x)
#endif

#ifdef  _WIN32
#define QNEKO_EXPORT QNEKO_ATTRIBUTE(dllexport)
#define QNEKO_IMPORT QNEKO_ATTRIBUTE(dllimport)
#else
#define QNEKO_EXPORT QNEKO_ATTRIBUTE(visibility("default"))
#define QNEKO_IMPORT
#endif

#ifdef _QNEKO_SOURCE
#define QNEKO_API QNEKO_EXPORT
#else
#define QNEKO_API QNEKO_IMPORT
#endif


namespace QNekoAV {

class MediaPlayerPrivate;
class VideoWidgetPrivate;

/**
 * @brief Player holder the pipeline for playing video
 * 
 */
class QNEKO_API MediaPlayer : public QObject {
Q_OBJECT
public:
    enum PlaybackState {
        StoppedState,
        PlayingState,
        PausedState
    };
    Q_ENUM(PlaybackState)

    enum MediaStatus {
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        StalledMedia,
        BufferingMedia,
        BufferedMedia,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(MediaStatus)

    enum Error {
        NoError,
        ResourceError,
        FormatError,
        NetworkError,
        AccessDeniedError,
        UnknownError
    };
    Q_ENUM(Error)

    enum Loops {
        Infinite = -1,
        Once = 1
    };
    Q_ENUM(Loops)

    explicit MediaPlayer(QObject *parent = nullptr);
    ~MediaPlayer();

    // QList<MediaMetaData> audioTracks() const;
    // QList<MediaMetaData> videoTracks() const;
    // QList<MediaMetaData> subtitleTracks() const;

    int activeAudioTrack() const;
    int activeVideoTrack() const;
    int activeSubtitleTrack() const;

    void setActiveAudioTrack(int index);
    void setActiveVideoTrack(int index);
    void setActiveSubtitleTrack(int index);

    // void setAudioOutput(AudioOutput *output);
    // AudioOutput *audioOutput() const;

    void setVideoOutput(QObject *);
    QObject *videoOutput() const;

    // void setVideoSink(VideoSink *sink);
    // VideoSink *videoSink() const;

    QUrl source() const;
    const QIODevice *sourceDevice() const;

    PlaybackState playbackState() const;
    MediaStatus mediaStatus() const;

    qreal duration() const;
    qreal position() const;

    bool hasAudio() const;
    bool hasVideo() const;

    float bufferProgress() const;
    qreal bufferedDuration() const;
    // QMediaTimeRange bufferedTimeRange() const;

    bool isSeekable() const;
    qreal playbackRate() const;

    bool isPlaying() const;
    bool isLoaded() const;

    int loops() const;
    void setLoops(int loops);

    Error error() const;
    QString errorString() const;

    bool isAvailable() const;

    // MediaMetaData metaData() const;

    void setOption(const QString &key, const QString &value);
    void clearOptions();

    void setHttpUseragent(const QString &useragent);
    void setHttpReferer(const QString &referer);

    /**
     * @brief Get the pipeline view
     * 
     * @return void* is NekoAV::Pipeline* 
     */
    void *pipeline() const;

    static QStringList supportedMediaTypes();
    static QStringList supportedProtocols();
public Q_SLOTS:
    void play();
    void pause();
    void stop();
    void load();

    void setPosition(qreal position);

    void setPlaybackRate(qreal rate);

    void setSource(const QUrl &source);
    void setSourceDevice(QIODevice *device, const QUrl &sourceUrl = QUrl());
Q_SIGNALS:
    void sourceChanged(const QUrl &media);
    void playbackStateChanged(PlaybackState newState);
    void mediaStatusChanged(MediaStatus status);

    void durationChanged(qreal duration);
    void positionChanged(qreal position);

    void hasAudioChanged(bool available);
    void hasVideoChanged(bool videoAvailable);

    void bufferProgressChanged(float progress);
    void bufferedDurationChanged(qreal duration);

    void seekableChanged(bool seekable);
    void playingChanged(bool playing);
    void playbackRateChanged(qreal rate);
    void loopsChanged();

    void metaDataChanged();
    void videoOutputChanged();
    void audioOutputChanged();

    void tracksChanged();
    void activeTracksChanged();

    void errorChanged();
    void errorOccurred(MediaPlayer::Error error, const QString &errorString);
private:
    MediaPlayerPrivate *d = nullptr;
};

/**
 * @brief Widget for Displaying video
 * 
 */
class QNEKO_API VideoWidget : public QWidget {
Q_OBJECT
public:
    VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    /**
     * @brief Get NekoAV::VideoRenderer *
     * 
     * @return void* 
     */
    void *videoRenderer();
    void  internalRelayout();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
private:
    VideoWidgetPrivate *d = nullptr;
};

}

using QNekoMediaPlayer = QNekoAV::MediaPlayer;
using QNekoVideoWidget = QNekoAV::VideoWidget;