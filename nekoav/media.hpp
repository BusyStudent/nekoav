#pragma once

#include "resource.hpp"
#include <string>
#include <span>

NEKO_NS_BEGIN

class AudioDevice;
class VideoRenderer;

enum class ClockType : int {
    Video = 2,
    Audio = 4,
    Subtitle = 1,
    Unknown  = 0,
    External = 3, //< External clock like system clock
};
enum class StreamType : int {
    Unknown = 0,
    Video = 1,
    Audio = 2,
};

class MediaStream {
public:
    int        index = 0;
    StreamType type = StreamType::Unknown;
};

/**
 * @brief Interface for Media Frame (Audio / Video)
 * 
 */
class MediaFrame : public Resource {
public:
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
    /**
     * @brief Get Position of the clock
     * 
     * @return double 
     */
    virtual auto position() const -> double = 0;
    virtual auto type() const -> ClockType = 0;
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
    /**
     * @brief Get the clock of this element, (can return nullptr)
     * 
     * @return MediaClock* 
     */
    virtual MediaClock *clock() const = 0;
    /**
     * @brief Check all data is processed
     * 
     * @return true 
     * @return false 
     */
    virtual bool isEndOfFile() const = 0;
protected:
    MediaElement() = default;
    ~MediaElement() = default;
};

/**
 * @brief Global controller for media
 * 
 */
class MediaController {
public:
    // Clock
    virtual auto masterClock() const -> MediaClock * = 0;
protected:
    MediaController() = default;
    ~MediaController() = default;
};

/**
 * @brief A Reader directly ready source and output decoded frame (as a wrapper for AVFormatContext + AVCodecContext)
 * 
 */
class MediaReader {
public:
    enum Query : int {
        Duration, // Duration of this Media, in (double), seconds 
        Seekable, // Is Seekable             in (bool)
    };

    virtual ~MediaReader() = default;

    /**
     * @brief Set the current Position
     * 
     * @param position position in seconds
     * 
     * @return Error
     */
    virtual Error setPosition(double position) = 0;
    /**
     * @brief Read a frame from it, (it only produce frame from selected stream)
     * 
     * @param frame output pointer to recving decoded frame
     * @param index output pointer for recving stream index
     * 
     * @return Error
     */
    virtual Error readFrame(Arc<MediaFrame> *frame, int *index = nullptr) = 0;
    /**
     * @brief Open a url to read
     * 
     * @param url The url to open with
     * @param options Dictionary of open flags
     * @return Error 
     */
    virtual Error openUrl(std::string_view url, const Properties *options = nullptr) = 0;
    /**
     * @brief Select a stream, and enable it
     * 
     * @param idx Stream index
     * @param select false on unselect it (default on true)
     * @return Error 
     */
    virtual Error selectStream(int idx, bool select = true) = 0;
    /**
     * @brief Check is end of file
     * 
     * @return true 
     * @return false 
     */
    virtual bool isEndOfFile() const = 0;
    /**
     * @brief Check this stream was selected
     * 
     * @param idx 
     * @return true 
     * @return false 
     */
    virtual bool isStreamSelected(int idx) const = 0;
    /**
     * @brief Do query detail value
     * 
     * @param what 
     * @return Property 
     */
    virtual Property query(int what) const = 0;
    /**
     * @brief Query all streams
     * 
     * @return std::span<MediaStream> 
     */
    virtual std::span<const MediaStream> streams() const = 0;
    /**
     * @brief Create a media reader
     * 
     * @return Box<MediaReader> 
     */
    NEKO_API
    static Box<MediaReader> create();
    /**
     * @brief Register a impl
     * 
     * @tparam T 
     */
    template <typename T>
    static void registers() noexcept {
        _registers(make<T>);
    }
protected:
    MediaReader() = default;
private:
    template <typename T>
    static Box<MediaReader> make() {
        return std::make_unique<T>();
    }
    NEKO_API
    static void _registers(Box<MediaReader> (*fn)()) noexcept;
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
    auto type() const -> ClockType override;
    
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
/**
 * @brief Create a Media Reader object
 * 
 * @return Box<MediaReader> 
 */
inline Box<MediaReader> CreateMediaReader() {
    return MediaReader::create();
}

NEKO_NS_END
