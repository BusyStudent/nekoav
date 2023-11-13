#pragma once

#include "../elements.hpp"
#include <functional>

NEKO_NS_BEGIN

class VideoRenderer;

class D2DVideoSink : public Element {
public:
    virtual Error setHwnd(void *hwnd) = 0;
};
class VideoSink : public Element {
public:
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