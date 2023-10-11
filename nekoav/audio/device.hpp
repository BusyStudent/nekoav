#pragma once

#include <functional>
#include "../format.hpp"
#include "../defs.hpp"

NEKO_NS_BEGIN

/**
 * @brief Interface for audio device
 * @details In callback mode
 * 
 */
class AudioDevice {
public:
    virtual ~AudioDevice() = default;
    virtual bool open(SampleFormat fmt, int sampleRate, int channels) = 0;
    virtual bool close() = 0;
    virtual bool pause(bool v) = 0;
    virtual bool isPaused() const = 0;
    virtual void setCallback(std::function<void(void *buffer, int bufferLen)> &&cb) = 0;
};

extern Box<AudioDevice> NEKO_API CreateAudioDevice();

NEKO_NS_END