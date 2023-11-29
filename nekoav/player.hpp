#pragma once

#include "elements.hpp"
#include <string>

NEKO_NS_BEGIN

class VideoRenderer;

class Player : public Element {
public:
    /**
     * @brief Set the Url object
     * 
     * @param url 
     */
    virtual void setUrl(std::string_view url) = 0;
    /**
     * @brief Set the Video Renderer object
     * 
     * @param renderer 
     */
    virtual void setVideoRenderer(VideoRenderer *renderer) = 0;
    /**
     * @brief Set the Position 
     * 
     * @param position 
     */
    virtual void setPosition(double position) = 0;
    virtual double duration() const = 0;
    virtual double position() const = 0;

    inline Error play();
    inline Error pause();
    inline Error stop();
};

/**
 * @brief Create a Player object
 * 
 * @return Arc<Player> 
 */
extern NEKO_API Arc<Player> CreatePlayer();

NEKO_NS_END