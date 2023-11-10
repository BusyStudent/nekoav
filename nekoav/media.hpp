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
        SampleCount,  //< Number of samples, per channel
    };
    virtual auto query(Query q) const -> int = 0;

    inline  auto width() const -> int { return query(Query::Width); }
    inline  auto height() const -> int { return query(Query::Height); }
    inline  auto channels() const -> int { return query(Query::Channels); }
    inline  auto sampleRate() const -> int { return query(Query::SampleRate); }
    inline  auto sampleCount() const -> int { return query(Query::SampleCount); }
    inline  auto sampleFormat() const -> SampleFormat { return SampleFormat(format()); }
    inline  auto pixelFormat() const -> PixelFormat { return PixelFormat(format()); }
};

class AudioFrame : public MediaFrame {
public:
    virtual void setTimestamp(double timestamp) = 0;
    virtual void setSampleRate(int sampleRate) = 0;
};

class MediaPacket : public Resource {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;

    virtual auto size() const -> int64_t = 0;
    virtual auto data() const -> void * = 0;
    virtual auto duration() const -> double = 0;
    virtual auto timestamp() const -> double = 0;
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
// /**
//  * @brief Demuxer Element
//  * 
//  */

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

// /**
//  * @brief Media Pipeline
//  * 
//  */
// class NEKO_API MediaPipeline final : public Pipeline, public MediaController {
// public:
//     MediaPipeline();
//     MediaPipeline(const MediaPipeline &) = delete;
//     ~MediaPipeline();

//     void setGraph(View<Graph> graph) override;

//     void addClock(MediaClock *clock) override;
//     void removeClock(MediaClock *clock) override;
//     auto masterClock() const -> MediaClock * override;
//     auto position() const noexcept {
//         return mPosition.load();
//     }

//     void seek(double pos);
// protected:
//     void stateChanged(State newState) override;
// private:
//     void _run();
//     std::vector<MediaClock *> mClocks;
//     mutable std::mutex        mClockMutex;
//     MediaClock               *mMasterClock = nullptr;
//     ExternalClock             mExternalClock;
//     Thread                   *mThread = nullptr;
//     Atomic<double>            mPosition {0}; //< Current media playing position
// };


/**
 * @brief Create a Audio Frame object
 * 
 * @param fmt The format type
 * @param channels Numbe of channels
 * @param samples Number of samples in a single channel
 * @return Arc<MediaFrame> 
 */
extern NEKO_API Arc<AudioFrame> CreateAudioFrame(SampleFormat fmt, int channels, int samples);

NEKO_NS_END
