#pragma once

#include "defs.hpp"
#include <functional>
#include <string>
#include <list>

NEKO_NS_BEGIN

class VideoRenderer;
class PlayerPrivate;

class Filter {
public:
    Filter(std::string_view name) : mName(name) { }
    Filter(const std::type_info &info) : mName(info.name()) { }
private:
    std::string mName; //< Filter name
friend class Player;
};

/**
 * @brief Wrapper for easily use Pipeline to play Video
 * 
 */
class NEKO_API Player {
public:
    Player();
    Player(const Player &) = delete;
    ~Player();

    /**
     * @brief Add a filter
     * 
     * @param filter 
     * @return void * The marks to remove the filter (0 on add failure)
     */
    void *addFilter(const Filter &filter);
    /**
     * @brief Remove a filter
     * 
     * @param id 
     */
    void removeFilter(void *id);
    /**
     * @brief Set the Url object
     * 
     * @param url 
     */
    void setUrl(std::string_view url);
    /**
     * @brief Set the Options object for Open Url
     * 
     * @param options The pointer of options, nyllptr on cleaer
     */
    void setOptions(const Properties *options);
    /**
     * @brief Add a Option
     * 
     * @param key 
     * @param value 
     */
    void setOption(std::string_view key, std::string_view value);
    /**
     * @brief Set the Video Renderer object
     * 
     * @param renderer 
     */
    void setVideoRenderer(VideoRenderer *renderer);
    /**
     * @brief Set the Position 
     * 
     * @param position 
     */
    void setPosition(double position);
    /**
     * @brief Check current media has video stream
     * 
     * @return true 
     * @return false 
     */
    bool hasVideo() const noexcept;
    /**
     * @brief Check current media has audio stream
     * 
     * @return true 
     * @return false 
     */
    bool hasAudio() const noexcept;
    /**
     * @brief Check current media is seekable
     * 
     * @return true 
     * @return false 
     */
    bool isSeekable() const noexcept;
    /**
     * @brief Get duration of current media
     * 
     * @return double (in seconds)
     */
    double duration() const noexcept;
    /**
     * @brief Get current position
     * 
     * @return double (in seconds)
     */
    double position() const noexcept;
    /**
     * @brief Get current state
     * 
     * @return State 
     */
    State state() const noexcept;

    void play();
    void pause();
    void stop();

    void setErrorCallback(std::function<void(Error, std::string_view)> &&callback);
    void setPositionCallback(std::function<void(double)> &&callback);
    void setStateChangedCallback(std::function<void(State)> &&callback);
private:
    void _run();
    void _load();
    void _error(Error err, std::string_view msg);
    void _setState(State state);
    void _translateEvent(View<Event>);
    void _buildAudioPart();
    void _buildVideoPart();
    bool _configureSubtitle();

    Box<PlayerPrivate> d;
    Box<Properties> mOptions; //< The options for open
    Atomic<State>  mState { State::Null };
    Thread        *mThread = nullptr; //< Work Thread
    VideoRenderer *mRenderer = nullptr;
    std::string    mUrl; //< The dest to 
    std::string    mSubtitleUrl; //< The Url of the subtitle

    std::list<Filter> mFilters; //< List of filter 

    // Callbacks
    std::function<void(Error, std::string_view)> mErrorCallback;
    std::function<void(double)>           mPositionCallback;
    std::function<void(State)>            mStateChangedCallback;
};

NEKO_NS_END