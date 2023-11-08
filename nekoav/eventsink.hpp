#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

class Event;

class EventSink {
public:
    virtual Error sendEvent(View<Event> event) = 0;
    virtual Error postEvent(View<Event> event) = 0;
};

NEKO_NS_END