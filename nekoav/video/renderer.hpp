#pragma once

#include "../defs.hpp"
#include <vector>

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
    virtual Error                   init() = 0;
    /**
     * @brief Video end present
     * 
     */
    virtual Error                   teardown() = 0;
    /**
     * @brief draw a frame (it can block or not)
     * 
     */
    virtual Error                   sendFrame(View<MediaFrame>) = 0;
    /**
     * @brief Get the pixelformat of the list
     * 
     * @return std::vector<PixelFormat> 
     */
    virtual std::vector<PixelFormat> supportedFormats() const = 0;
protected:
    VideoRenderer() = default;
    ~VideoRenderer() = default;
};

NEKO_NS_END