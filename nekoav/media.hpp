#pragma once

#include "elements.hpp"
#include "resource.hpp"
#include <functional>
#include <chrono>

NEKO_NS_BEGIN

inline namespace _abiv1 {

class MediaElement : public Element {
public:

};

/**
 * @brief Interface for Media Frame (Audio / Video)
 * 
 */
class MediaFrame : public Resource {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;

    virtual auto format() const -> int = 0;
    virtual auto duration() const -> double = 0;
    virtual auto timestamp() const -> double = 0;
    virtual auto isKeyFrame() const -> bool = 0;

    virtual auto linesize(int plane) const -> int = 0;
    virtual auto data(int plane) const -> void * = 0;

    /**
     * @brief Detail based query
     * 
     */
    enum Query : int {
        Width,
        Height,
        Channels,
        SampleRate,
    };
    virtual auto query(Query q) const -> int = 0;

    inline  auto width() const -> int { return query(Query::Width); }
    inline  auto height() const -> int { return query(Query::Height); }
    inline  auto channels() const -> int { return query(Query::Channels); }
    inline  auto sampleRate() const -> int { return query(Query::SampleRate); }
    inline  auto sampleFormat() const -> SampleFormat { return SampleFormat(format()); }
    inline  auto pixfmt() const -> PixelFormat { return PixelFormat(format()); }
};

class MediaPacket : public Resource {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;
};


class MediaClock {
public:
    enum Type : int {
        Video = 2,
        Audio = 4,
        Subtitle = 1,
        External = 3, //< External clock like system clock
    };

    /**
     * @brief Get Position of the clock
     * 
     * @return double 
     */
    virtual auto position() const -> double = 0;
    virtual auto type() const -> Type = 0;
protected:
    MediaClock() = default;
    ~MediaClock() = default;
};

class MediaController {
public:
    virtual void addClock(MediaClock *clock) = 0;
    virtual void removeClock(MediaClock *clock) = 0;
    virtual auto masterClock() const -> MediaClock * = 0;
protected:
    MediaController() = default;
    ~MediaController() = default;
};
/**
 * @brief Demuxer Element
 * 
 */
class Demuxer : public MediaElement {
public:
    virtual void setSource(std::string_view url) = 0;
    virtual void setLoadedCallback(std::function<void()> &&cb) = 0;
};

class Enmuxer : public MediaElement {
public:

};

class Decoder : public MediaElement {
public:

};

class Encoder : public MediaElement {
public:

};

class AudioConverter : public MediaElement {
public:

};
class VideoConverter : public MediaElement {
public:

};

class AudioPresenter : public MediaElement {
public:

};
class VideoPresenter : public MediaElement {
public:

};
/**
 * @brief Present Video to a hwnd
 * 
 */
class HwndPresenter : public MediaElement {
public:
    virtual void setHwnd(void *hwnd) = 0;
};

/**
 * @brief External Clock for Media
 * 
 */
class NEKO_API ExternalClock final : public MediaClock {
public:
    ExternalClock();
    ~ExternalClock();

    auto position() const -> double override;
    auto type() const -> Type override;
    
    void start();
    void pause();
    void setPosition(double pos);
private:
    Atomic<int64_t> mTicks {0}; //< Started ticks
    Atomic<bool>    mPaused {true};//< Is Paused ?
    Atomic<double>  mCurrent {0.0}; //< Current Position
};

/**
 * @brief Media Pipeline
 * 
 */
class NEKO_API MediaPipeline final : public MediaController {
public:
    MediaPipeline();
    MediaPipeline(const MediaPipeline &) = delete;
    ~MediaPipeline();

    void setGraph(Graph *g);
    void setState(State s);

    void addClock(MediaClock *clock) override;
    void removeClock(MediaClock *clock) override;
    auto masterClock() const -> MediaClock * override;
private:
    Pipeline                  mPipeline;
    std::vector<MediaClock *> mClocks;
    mutable std::mutex        mClockMutex;
    MediaClock               *mMasterClock = nullptr;
    ExternalClock             mExternalClock;
};

extern auto NEKO_API CreateAudioPresenter() -> Box<AudioPresenter>;
extern auto NEKO_API CreateVideoPresenter() -> Box<VideoPresenter>;

}
NEKO_NS_END