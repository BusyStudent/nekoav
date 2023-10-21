#pragma once

#include "elements.hpp"
#include "resource.hpp"
#include <functional>
#include <chrono>
#include <span>

NEKO_NS_BEGIN

inline namespace _abiv1 {

/**
 * @brief Add extra method for Property
 * 
 */
class MediaProperty final : public Property {
public:
    static constexpr const char *PixelFormatList = "pixelFormatList";
    static constexpr const char *PixelFormat = "pixelFormat";
    static constexpr const char *Width = "width";
    static constexpr const char *Height = "height";
    static constexpr const char *Channels = "channels";
    static constexpr const char *SampleRate = "sampleRate";
    static constexpr const char *SampleFormat = "sampleFormat";
    static constexpr const char *Duration = "duration";

private:
    MediaProperty() = delete;
};

/**
 * @brief Add extra method for Pad
 * 
 */
class MediaPad final : public Pad {
public:
    auto isVideo() const -> bool {
        return name().starts_with("video");
    }
    auto isAudio() const -> bool {
        return name().starts_with("audio");
    }
    auto isSubtitle() const -> bool {
        return name().starts_with("subtitle");
    }
    int  videoIndex() const {
        int idx = -1;
        ::sscanf(name().data(), "video%d", &idx);
        return idx;
    }
    int  audioIndex() const {
        int idx = -1;
        ::sscanf(name().data(), "audio%d", &idx);
        return idx;
    }
    int  subtitleIndex() const {
        int idx = -1;
        ::sscanf(name().data(), "subtitle%d", &idx);
        return idx;
    }
private:
    MediaPad() = delete;
};


class MediaElement : public Element {
public:
    auto inputs() -> std::span<MediaPad*> {
        auto &vec = Element::inputs();
        return {
            reinterpret_cast<MediaPad**>(vec.data()),
            vec.size()
        };
    }
    auto outputs() -> std::span<MediaPad*> {
        auto &vec = Element::outputs();
        return {
            reinterpret_cast<MediaPad**>(vec.data()),
            vec.size()
        };
    }
    auto findInput(std::string_view name) -> MediaPad * {
        return static_cast<MediaPad*>(Element::findInput(name));
    };
    auto findOutput(std::string_view name) -> MediaPad * {
        return static_cast<MediaPad*>(Element::findOutput(name));
    };
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
    inline  auto pixelFormat() const -> PixelFormat { return PixelFormat(format()); }
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

/**
 * @brief Cache Resource, and start a new thread
 * 
 */
class MediaQueue : public MediaElement {
public:
    virtual void setCapicity(size_t size) = 0;
    virtual auto size() const -> size_t = 0;
};

class AudioConverter : public MediaElement {
public:
    virtual void setSampleFormat(SampleFormat fmt) = 0;
};
class VideoConverter : public MediaElement {
public:
    /**
     * @brief Force the converter to convert video to {fmt}, passthrough is disabled
     * 
     * @param fmt 
     */
    virtual void setPixelFormat(PixelFormat fmt) = 0;

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

class AppSink       : public MediaElement {
public:
    virtual bool wait(Arc<Resource> *v, int timeout = -1) = 0;
    virtual auto size() const -> size_t = 0;
};

class AppSource     : public MediaElement {
public:
    virtual auto push(const Arc<Resource> &res) -> Error = 0;
    virtual auto size() const -> size_t = 0;
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
extern auto NEKO_API CreateMediaQueue() -> Box<MediaQueue>;
extern auto NEKO_API CreateAppSource() -> Box<AppSource>;
extern auto NEKO_API CreateAppSink() -> Box<AppSink>;

}
NEKO_NS_END