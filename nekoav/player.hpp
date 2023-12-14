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
    double duration() const noexcept;
    double position() const noexcept;
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
    Atomic<State> mState { State::Null };
    Thread       *mThread = nullptr; //< Work Thread
    VideoRenderer *mRenderer = nullptr;
    std::string   mUrl; //< The dest to 

    // Callbacks
    std::function<void(Error, std::string_view)> mErrorCallback;
    std::function<void(double)>           mPositionCallback;
    std::function<void(State)>            mStateChangedCallback;
};

NEKO_NS_END