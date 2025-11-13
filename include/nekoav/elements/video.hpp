#pragma once

#include <nekoav/element.hpp>
#include <nekoav/format.hpp>
#include <nekoav/caps.hpp>

namespace nekoav {

/**
 * @brief The abstract video renderer.
 * 
 */
class VideoRenderer {
public:
    virtual ~VideoRenderer() = default;
    virtual auto render(const Frame &frame) -> IoTask<void> = 0;
    virtual auto init() -> IoTask<void> = 0;
    virtual auto shutdown() -> IoTask<void> = 0;
    virtual auto pixelFormats() const -> std::vector<PixelFormat> = 0;
};

/**
 * @brief Convert the frame's format to make it suitable for the next element.
 * 
 */
class VideoConverter : public Element {
    
};

/**
 * @brief Present the frame to the user. 
 * 
 */
class VideoSink : public Element {
public:
    VideoSink(std::string_view name = {});
    ~VideoSink();
    
    /**
     * @brief Set the Renderer
     * 
     * @param renderer 
     */
    auto setRenderer(std::unique_ptr<VideoRenderer> renderer) -> void;
};

} // namespace nekoav
