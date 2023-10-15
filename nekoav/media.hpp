#pragma once

#include "elements.hpp"
#include "resource.hpp"

NEKO_NS_BEGIN

class MediaElement : public Element {
public:

};
class MediaFrame : public Resource {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;
};
class MediaPacket : public Resource {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

/**
 * @brief Demuxer Element
 * 
 */
class Demuxer : public MediaElement {
public:
    virtual void setSource(std::string_view url) = 0;
};

class Enmuxer : public MediaElement {
public:

};

class Decoder : public MediaElement {
public:

};

class Encoder : public MediaElement {
public:

};

class AudioResampler : public MediaElement {
public:

};
class VideoResampler : public MediaElement {
public:

};



NEKO_NS_END