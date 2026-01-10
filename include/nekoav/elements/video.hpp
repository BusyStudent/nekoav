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
class NEKOAV_API VideoConverter final : public Element {
public:
    VideoConverter(std::string_view name = {});
    ~VideoConverter();
private:
    // auto onPrepare() -> IoTask<void> override;
    // auto onStop() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;
    auto onPush(Pad &, Sample sample) -> IoTask<void>;
    auto init(Frame *frame) -> IoResult<void>;

    struct Impl;
    std::unique_ptr<Impl> d;
    Pad                  &mInput;
    Pad                  &mOutput;
};

/**
 * @brief Present the frame to the user. 
 * 
 */
class NEKOAV_API VideoSink final : public Element {
public:
    // VideoSink(std::string_view name = {});
    // ~VideoSink();
    
    /**
     * @brief Set the Renderer
     * 
     * @param renderer 
     */
    // auto setRenderer(std::unique_ptr<VideoRenderer> renderer) -> void;
};

} // namespace nekoav
