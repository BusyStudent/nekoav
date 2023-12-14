#pragma once

#include "../threading.hpp"
#include "../elements.hpp"
#include "../resource.hpp"
#include "../event.hpp"
#include "../time.hpp"
#include "../libc.hpp"
#include "../pad.hpp"
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "../time.hpp"
#include "cxx20.hpp"

NEKO_NS_BEGIN

enum ElementEventType : int {
    UnknownEvent = 0,
    StageBegin = 1,
    StageEnd = 2,
    CustomEventBegin = 114514,
};

/**
 * @brief For listener element run
 * 
 */
class ElementEventSubscriber {
public:
    virtual void received(Element *element,
                  const ElementEventType &event_type, 
                  const std::string &event_msg,
                  const uint64_t timestamp = NekoAV::GetTicks(),
                  const std::source_location local = std::source_location::current(), 
                  const Thread *thread = Thread::currentThread()) = 0;
};

class PrintElementTracer : public ElementEventSubscriber {
public:
    PrintElementTracer(FILE *stream = stderr) : mStream(stream) {}

    inline void received(Element *element,
                         const ElementEventType &event_type, 
                         const std::string &event_msg,
                         const uint64_t timestamp = NekoAV::GetTicks(),
                         const std::source_location local = std::source_location::current(), 
                         const Thread *thread = Thread::currentThread()) override {
        switch (event_type)
        {
        case ElementEventType::UnknownEvent:
            _output("[%u][%s:%u (%s)][%s]: unknow stage event happen, message: %s\n", 
                    timestamp, 
                    local.file_name(), 
                    local.line(), 
                    local.function_name(), 
                    (thread == nullptr ? "mainThread" : thread->name().data()), 
                    event_msg.c_str());
            break;
        case ElementEventType::StageBegin:
            _output("[%u][%s:%u (%s)][%s]: %s stage begin\n", 
                    timestamp, 
                    local.file_name(), 
                    local.line(), 
                    local.function_name(), 
                    (thread == nullptr ? "mainThread" : thread->name().data()), 
                    event_msg.c_str());
            break;
        case ElementEventType::StageEnd:
            _output("[%u][%s:%u (%s)][%s]: %s stage end\n", 
                    timestamp, 
                    local.file_name(), 
                    local.line(), 
                    local.function_name(), 
                    (thread == nullptr ? "mainThread" : thread->name().data()), 
                    event_msg.c_str());
            break;
        default:
            _output("[%u][%s:%u (%s)][%s]: %s\n", 
                    timestamp, 
                    local.file_name(), 
                    local.line(), 
                    local.function_name(), 
                    (thread == nullptr ? "mainThread" : thread->name().data()), 
                    event_msg.c_str());
            break;
        }
    }
private:
    void _output(const char *format, ...) {
        const char *name = "Main";
        auto thread = Thread::currentThread();
        if (thread) {
            name = thread->name().data();
        }
        va_list varg;
        va_start(varg, format);
        ::fprintf(mStream, "TRACE >>> ");
        ::vfprintf(mStream, format, varg);
        va_end(varg);

        ::fflush(mStream);
    }
    FILE *mStream;
};

NEKO_NS_END

#undef NEKO_SOURCE_LOCATION