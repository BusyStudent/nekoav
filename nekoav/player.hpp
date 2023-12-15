#pragma once

#include "elements.hpp"
#include <functional>
#include <string>

NEKO_NS_BEGIN

class VideoRenderer;
class PlayerPrivate;

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
     * @brief Set the Url object
     * 
     * @param url 
     */
    void setUrl(std::string_view url);
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

    Box<PlayerPrivate> d;
    Atomic<State>  mState { State::Null };
    Thread        *mThread = nullptr; //< Work Thread
    VideoRenderer *mRenderer = nullptr;
    std::string    mUrl; //< The dest to 

    // Callbacks
    std::function<void(Error, std::string_view)> mErrorCallback;
    std::function<void(double)>           mPositionCallback;
    std::function<void(State)>            mStateChangedCallback;
};

NEKO_NS_END