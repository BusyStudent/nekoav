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
    using Ptr = std::shared_ptr<VideoRenderer>;

    virtual ~VideoRenderer() = default;
    virtual auto init() -> IoTask<void> = 0;
    virtual auto render(Frame frame) -> IoTask<void> = 0;
    virtual auto shutdown() -> IoTask<void> = 0;
    virtual auto pixelFormats() const -> std::vector<PixelFormat> = 0;
};

/**
 * @brief The null video renderer. (just drop the frame)
 * 
 */
class NEKOAV_API NullVideoRenderer final : public VideoRenderer {
public:
    NullVideoRenderer() = default;
    ~NullVideoRenderer() = default;

    auto init() -> IoTask<void> override;
    auto render(Frame frame) -> IoTask<void> override;
    auto shutdown() -> IoTask<void> override;
    auto pixelFormats() const -> std::vector<PixelFormat> override;
};

/**
 * @brief The window video renderer. (render to the window, HWND on windows)
 * 
 */
// class NEKOAV_API WindowVideoRenderer final : public VideoRenderer {
// public:
//     WindowVideoRenderer(void *window);
//     ~WindowVideoRenderer();

//     auto init() -> IoTask<void> override;
//     auto render(Frame frame) -> IoTask<void> override;
//     auto shutdown() -> IoTask<void> override;
//     auto pixelFormats() const -> std::vector<PixelFormat> override;
// };

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
    VideoSink(std::string_view name = {});
    ~VideoSink();
    
    /**
     * @brief Set the Renderer
     * 
     * @param renderer 
     */
    auto setRenderer(VideoRenderer::Ptr renderer) -> void;
private:
    auto onPrepare() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;
    auto onPadPush(Pad &, Sample sample) -> IoTask<void>;
    auto onPadQuery(Pad &pad, Query query) -> std::optional<Reply>;

    struct Impl;

    std::unique_ptr<Impl> d;
    VideoRenderer::Ptr    mRenderer;
    Pad                  &mInput;
};

} // namespace nekoav
