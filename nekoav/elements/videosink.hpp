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
    // FIXME: It seems like Qt6 modified the sizeof stl container in msvc2022, so use view instead
    /**
     * @brief Get the supported pixel format of this Renderer, It should at least support RGBA
     * 
     * @return std::span<const PixelFormat>
     */
    virtual PixelFormatList supportedFormats() = 0;
    /**
     * @brief Set the Context object, render can register it's ext interface into global context
     * 
     * @param ctxt nullptr on remove
     * 
     * @return Error 
     */
    virtual Error setContext(Context *ctxt) = 0;
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

class D2DRenderer : public VideoRenderer {
public:
    virtual ~D2DRenderer() = default;
    /**
     * @brief Move Current 
     * 
     * @param thread The thread to move (nullptr on current thread)
     * 
     * @return Error 
     */
    virtual Error moveToThread(Thread *thread = nullptr) = 0;
    /**
     * @brief Force paint now
     * 
     * @return Error 
     */
    virtual Error paint() = 0;
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

#if defined(_WIN32)
/**
 * @brief Create a Direct2D Renderer only avaiable on win32
 * 
 * @param hwnd 
 * @return Box<VideoRenderer> 
 */
extern NEKO_API Box<D2DRenderer> CreateD2DRenderer(void *hwnd);
#endif

NEKO_NS_END