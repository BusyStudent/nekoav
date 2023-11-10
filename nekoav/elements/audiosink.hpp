#pragma once

#include "../elements.hpp"
#include <functional>

NEKO_NS_BEGIN

class AudioDevice;
class AudioSink : public Element {
public:
    virtual Error setDevice(AudioDevice *device) = 0;
};

NEKO_NS_END