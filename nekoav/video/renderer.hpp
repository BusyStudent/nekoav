#pragma once

#include "../defs.hpp"
#include <span>

NEKO_NS_BEGIN

inline namespace _abiv1 {
    class MediaFrame;
}

/**
 * @brief Interface for draw the video frame
 * 
 */
class VideoRenderer {
public:
    /**
     * @brief Video start present
     * 
     */
    virtual void                   init() = 0;
    /**
     * @brief Video end present
     * 
     */
    virtual void                   teardown() = 0;
    /**
     * @brief draw a frame (it can block or not)
     * 
     */
    virtual void                   sendFrame(View<MediaFrame>) = 0;
    /**
     * @brief Get the pixelformat of the list
     * 
     * @return std::span<PixelFormat> 
     */
    virtual std::span<PixelFormat> supportedFormats() const = 0;
protected:
    VideoRenderer() = default;
    ~VideoRenderer() = default;
};

NEKO_NS_END