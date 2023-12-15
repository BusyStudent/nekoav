#pragma once

#include "../elements.hpp"
#include <functional>
#include <span>

NEKO_NS_BEGIN

class MediaFrame;

class VideoRenderer {
public:
    using PixelFormatList = std::span<const PixelFormat>;
    // FIXME: I donnot known why it crash on return Vec<PixelFormat>
    /**
     * @brief Get the supported pixel format of this Renderer, It should at least support RGBA
     * 
     * @return std::span<const PixelFormat>
     */
    virtual PixelFormatList supportedFormats() = 0;
    /**
     * @brief Set the Frame object, it will be invoked at worker thread
     * 
     * @return Error 
     */
    virtual Error setFrame(View<MediaFrame>) = 0;
protected:
    VideoRenderer() = default;
    ~VideoRenderer() = default;
};

/**
 * @brief A Video Sink output video to VideoRenderer
 * 
 */
class VideoSink : public Element {
public:
    /**
     * @brief Set the Renderer object, It only take it's reference
     * 
     * @return Error 
     */
    virtual Error setRenderer(VideoRenderer *) = 0;
};

/**
 * @brief Test Video sink for debugging
 * 
 */
class TestVideoSink : public Element {
public:
    /**
     * @brief Set the callback on test window was closed
     * 
     * @param cb The callback
     */
    virtual void  setClosedCallback(std::function<void()> &&cb) = 0;
    /**
     * @brief Set the Window title
     * 
     * @param title 
     * @return Error 
     */
    virtual Error setTitle(std::string_view title) = 0;
};

NEKO_NS_END