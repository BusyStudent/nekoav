#pragma once

#include "resource.hpp"
#include "defs.hpp"

NEKO_NS_BEGIN

class MediaBuffer : public Resource {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

class MediaFrame : public Resource {
public:
    virtual void lock()   = 0;
    virtual void unlock() = 0;
    virtual int  format() = 0;
};

NEKO_NS_END