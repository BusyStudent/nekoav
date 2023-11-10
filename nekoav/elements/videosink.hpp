#pragma once

#include "../elements.hpp"

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

NEKO_NS_END