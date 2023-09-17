#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

class MediaBuffer : public Object {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

class MediaFrame : public Object {
public:
    virtual void lock()   = 0;
    virtual void unlock() = 0;
};

NEKO_NS_END