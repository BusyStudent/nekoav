#pragma once

#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <map>

#include "../threading.hpp"
#include "../elements.hpp"
#include "../resource.hpp"
#include "../event.hpp"
#include "../time.hpp"
#include "../libc.hpp"
#include "../pad.hpp"
#include "../time.hpp"
#include "cxx20.hpp"

NEKO_NS_BEGIN

enum ElementEventType : int {
    UnknownEvent = 0,
    StageBegin = 1,
    StageEnd = 2,
    DataReceived = 3,
    DataSend = 4,
    CustomEventBegin = 114514,
};

struct ActionData {
    ActionData( Element *element,
                const ElementEventType &event_type, 
                const std::string &event_msg,
                const uint64_t timestamp = NekoAV::GetTicks(),
                const std::source_location location = std::source_location::current(),
                const Thread *thread = Thread::currentThread()) : 
                    element(element), 
                    event_type(event_type), 
                    event_msg(event_msg),
                    timestamp(timestamp), 
                    location(location), 
                    thread(thread) {}

    const Element *element = nullptr;
    const Pad *pad = nullptr;
    const Resource *resource = nullptr;
    ElementEventType event_type;
    std::string event_msg;
    uint64_t timestamp;
    std::source_location location;
    const Thread *thread = nullptr;
};

/**
 * @brief For listener element run
 * 
 */
class ElementEventSubscriber {
public:
    virtual void received(ActionData actionData) = 0;
};

class PrintElementTracer : public ElementEventSubscriber {
public:
    PrintElementTracer(FILE *stream = stderr);

    inline void received(ActionData actionData);
private:
    void _output(const char *format, ...);
    FILE *mStream;
};

class ElementTCTrack : public ElementEventSubscriber {
public:
    ElementTCTrack(FILE *stream = stderr);

    inline void received(ActionData actionData);
private:
    void _output(const char *format, ...);
    FILE *mStream;
    std::map<const Element *, std::string> elements;
};

NEKO_NS_END

// #undef NEKO_SOURCE_LOCATION