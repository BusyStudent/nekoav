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

#define NEKO_SOURCE_LOCATION std::source_location loc = std::source_location::current()

NEKO_NS_BEGIN

class ElementTracer {
public:
    /**
     * @brief Call on change the state begin
     * 
     * @param element 
     * @param change 
     */
    virtual void changeStateBegin(Element *element, StateChange change, NEKO_SOURCE_LOCATION) = 0;
    virtual void changeStateEnd(Element *element, StateChange change, NEKO_SOURCE_LOCATION) = 0;

    virtual void sendEventBegin(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) = 0;
    virtual void sendEventEnd(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) = 0;

    virtual void onEventBegin(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) = 0;
    virtual void onEventEnd(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) = 0;

    virtual void onSinkPushBegin(Element *element, View<Pad> pad ,View<Resource> resource, NEKO_SOURCE_LOCATION) = 0;
    virtual void onSinkPushEnd(Element *element, View<Pad> pad, View<Resource> resource, NEKO_SOURCE_LOCATION) = 0;

    virtual void onSinkEventBegin(Element *element, View<Pad> pad, View<Event> event, NEKO_SOURCE_LOCATION) = 0;
    virtual void onSinkEventEnd(Element *element, View<Pad> pad, View<Event> event, NEKO_SOURCE_LOCATION) = 0;

    virtual void onSourceEventBegin(Element *element, View<Pad> pad, View<Event> event, NEKO_SOURCE_LOCATION) = 0;
    virtual void onSourceEventEnd(Element *element, View<Pad> pad, View<Event> event, NEKO_SOURCE_LOCATION) = 0;

    virtual void pushEventToBegin(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) = 0;
    virtual void pushEventToEnd(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) = 0;

    virtual void pushToBegin(Element *element, View<Pad> targetPad, View<Resource> resource, NEKO_SOURCE_LOCATION) = 0;
    virtual void pushToEnd(Element *element, View<Pad> targetPad, View<Resource> resource, NEKO_SOURCE_LOCATION) = 0;
};
class PrintElementTracer : public ElementTracer {
public:
    PrintElementTracer(FILE *stream = stderr) : mStream(stream) {}

    void changeStateBegin(Element *element, StateChange change, NEKO_SOURCE_LOCATION) override {
        _output("%s change state(%s) begin\n", element->name().c_str(), GetStateChangeString(change));
    }
    void changeStateEnd(Element *element, StateChange change, NEKO_SOURCE_LOCATION) override {
        _output("%s change state(%s) end\n", element->name().c_str(), GetStateChangeString(change));
    }

    void sendEventBegin(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s send event(%p : %s) begin\n", element->name().c_str(), event.get(), libc::typenameof(typeid(*event)));
    }
    void sendEventEnd(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s send event(%p : %s) end\n", element->name().c_str(), event.get(), libc::typenameof(typeid(*event)));
    }
    
    void onEventBegin(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s on event(%p) begin\n", element->name().c_str(), event.get());
    }
    void onEventEnd(Element *element, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s on event(%p) end\n", element->name().c_str(), event.get());
    }

    void onSinkPushBegin(Element *element, View<Pad> targetPad, View<Resource> resource, NEKO_SOURCE_LOCATION) override {
        _output("%s on sink push(%s, %p : %s) begin\n", element->name().c_str(), targetPad->name().data(), resource.get(), libc::typenameof(typeid(*resource)));
    }
    void onSinkPushEnd(Element *element, View<Pad> targetPad, View<Resource> resource, NEKO_SOURCE_LOCATION) override {
        _output("%s on sink push(%s, %p : %s) end\n", element->name().c_str(), targetPad->name().data(), resource.get(), libc::typenameof(typeid(*resource)));
    }

    void onSinkEventBegin(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s on sink event begin\n", element->name().c_str());
    }
    void onSinkEventEnd(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s on sink event end\n", element->name().c_str());
    }

    void onSourceEventBegin(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s on source event begin\n", element->name().c_str());
    }
    void onSourceEventEnd(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s on source event end\n", element->name().c_str());
    }


    void pushEventToBegin(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s push event to begin\n", element->name().c_str());
    }
    void pushEventToEnd(Element *element, View<Pad> targetPad, View<Event> event, NEKO_SOURCE_LOCATION) override {
        _output("%s push event to end\n", element->name().c_str());
    }

    void pushToBegin(Element *element, View<Pad> targetPad, View<Resource> event, NEKO_SOURCE_LOCATION) override {
        _output("%s push to begin\n", element->name().c_str());
    }
    void pushToEnd(Element *element, View<Pad> targetPad, View<Resource> event, NEKO_SOURCE_LOCATION) override {
        _output("%s push to end\n", element->name().c_str());
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
        ::fprintf(mStream, "TRACE: Thread %s, Ticks %" PRId64  " ", name, GetTicks());
        ::vfprintf(mStream, format, varg);
        va_end(varg);

        ::fflush(mStream);
    }
    FILE *mStream;
};

NEKO_NS_END

#undef NEKO_SOURCE_LOCATION