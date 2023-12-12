#pragma once

#include "resource.hpp"

NEKO_NS_BEGIN

class AudioDevice;
class VideoRenderer;

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
    // virtual auto isKeyFrame() const -> bool = 0;

    virtual auto linesize(int plane) const -> int = 0;
    virtual auto data(int plane) const -> void * = 0;

    virtual auto makeWritable() -> bool = 0;
    
    /**
     * @brief Detail based Prop
     * 
     */
    enum class Value : int {
        Width,
        Height,
        Channels, 
        SampleRate,
        SampleCount,  //< Number of samples, per channel
        Timestamp,    //< Only for set
        Duration,     //< Only for set
    };
    virtual auto query(Value q) const -> int = 0;
    virtual auto set(Value q, const void * v) -> bool = 0;

    inline  auto width() const -> int { return query(Value::Width); }
    inline  auto height() const -> int { return query(Value::Height); }
    inline  auto channels() const -> int { return query(Value::Channels); }
    inline  auto sampleRate() const -> int { return query(Value::SampleRate); }
    inline  auto sampleCount() const -> int { return query(Value::SampleCount); }
    inline  auto sampleFormat() const -> SampleFormat { return SampleFormat(format()); }
    inline  auto pixelFormat() const -> PixelFormat { return PixelFormat(format()); }

    inline  auto setSampleRate(int sampleRate) -> bool { return set(Value::SampleRate, &sampleRate); }
    inline  auto setTimestamp(double timestamp) -> bool { return set(Value::Timestamp, &timestamp); }
    inline  auto setDuration(double duration) -> bool { return set(Value::Duration, &duration); }
};

/**
 * @brief Interface for Raw Compressed Data
 * 
 */
class MediaPacket : public Resource {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;

    virtual auto size() const -> int64_t = 0;
    virtual auto data() const -> void * = 0;
    virtual auto duration() const -> double = 0;
    virtual auto timestamp() const -> double = 0;
};

/**
 * @brief Clock interface for get time
 * 
 */
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

/**
 * @brief Interface for Sink and Source
 * 
 */
class MediaElement {
public:
    virtual bool isEndOfFile() const = 0;
};

/**
 * @brief Global controller for media
 * 
 */
class MediaController {
public:
    // Clock
    virtual void addClock(MediaClock *clock) = 0;
    virtual void removeClock(MediaClock *clock) = 0;
    virtual auto masterClock() const -> MediaClock * = 0;

    // Media Element
    virtual void addElement(MediaElement *element) = 0;
    virtual void removeElement(MediaElement *element) = 0;

    template <typename T>
    inline  void addObject(T *);

    template <typename T>
    inline  void removeObject(T *);
protected:
    MediaController() = default;
    ~MediaController() = default;
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
 * @brief Create a Audio Frame object
 * 
 * @param fmt The format type
 * @param channels Numbe of channels
 * @param samples Number of samples in a single channel
 * @return Arc<MediaFrame> 
 */
extern NEKO_API Arc<MediaFrame> CreateAudioFrame(SampleFormat fmt, int channels, int samples);
/**
 * @brief Create a Video Frame object
 * 
 * @param fmt 
 * @param width 
 * @param height 
 * @return Arc<MediaFrame> 
 */
extern NEKO_API Arc<MediaFrame> CreateVideoFrame(PixelFormat fmt, int width, int height);
/**
 * @brief Get the Media Controller object with object
 * 
 * @param element 
 * @return MediaController* 
 */
extern NEKO_API MediaController *GetMediaController(View<Element> element);


// Impl for MediaController
template <typename T>
inline void MediaController::addObject(T *object) {
    constexpr bool hasMediaClock = std::is_base_of_v<MediaClock, T>;
    constexpr bool hasMediaElement = std::is_base_of_v<MediaElement, T>;

    static_assert(hasMediaClock || hasMediaElement, "Object must be derived from MediaClock or MediaElement");

    if constexpr (hasMediaClock) {
        addClock(object);
    }
    if constexpr (hasMediaElement) {
        addElement(object);
    }
}
template <typename T>
inline void MediaController::removeObject(T *object) {
    constexpr bool hasMediaClock = std::is_base_of_v<MediaClock, T>;
    constexpr bool hasMediaElement = std::is_base_of_v<MediaElement, T>;

    static_assert(hasMediaClock || hasMediaElement, "Object must be derived from MediaClock or MediaElement");

    if constexpr (hasMediaClock) {
        removeClock(object);
    }
    if constexpr (hasMediaElement) {
        removeElement(object);
    }
}

NEKO_NS_END
