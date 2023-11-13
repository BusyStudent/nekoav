#pragma once

#include "defs.hpp"
#include "resource.hpp"
#include "time.hpp"
#include <functional>

NEKO_NS_BEGIN

class Event : public Resource {
public:
    enum Type : uint32_t {
        None,
        StateChanged,  //< Some element state changed
        BusChanged,    //< Element bus was changed
        ConetxtChanged, //< Element's context was changed
        ErrorOccurred, //< Error occurred, require pipeline stop

        PadAdded,
        PadRemoved,

        MediaEndOfFile, //< Media is reached eof
        MediaBuffering, //< Media is buffering
        SeekRequested,  //< Request to seek
        FlushRequested, //< Request to flush internal buffer
        ClockUpdated,   //< The clock was updated

        PipelineWakeup, //< Wakeup Pipeline, internal use only
        User = 10086   //< User Begin
    };
    
    explicit Event(Type type, Element *sender) : mType(type), mSender(sender) { }

    Type type() const noexcept {
        return mType;
    }
    Element *sender() const noexcept {
        return mSender;
    }
    int64_t time() const noexcept {
        return mTime;
    }

    Arc<Event> shared_from_this() {
        return Resource::shared_from_this<Event>();
    }
private:
    Type     mType = None;
    Element *mSender = nullptr;
    int64_t  mTime = GetTicks();
};

using EventType = Event::Type;

class ErrorEvent : public Event {
public:
    ErrorEvent(Error error, Element *sender) : Event(ErrorOccurred, sender), mError(error) { }

    Error error() const noexcept {
        return mError;
    }

    static Arc<ErrorEvent> make(Error error, Element *sender) {
        return std::make_shared<ErrorEvent>(error, sender);
    }
private:
    Error mError;
};

class MediaClock;
class ClockEvent : public Event {
public:
    ClockEvent(Type type, MediaClock *clock, Element *sender) : Event(type, sender), mClock(clock) { };

    MediaClock *clock() const noexcept {
        return mClock;
    }

    static Arc<ClockEvent> make(Type type, MediaClock *clock, Element *sender) {
        return std::make_shared<ClockEvent>(type, clock, sender);
    }
private:
    MediaClock *mClock = nullptr;
};
class SeekEvent : public Event {
public:
    SeekEvent(double targetSeconds) : Event(SeekRequested, nullptr) { }

    double time() const noexcept {
        return mTime;
    }
    static Arc<SeekEvent> make(double targetSeconds) {
        return std::make_shared<SeekEvent>(targetSeconds);
    }
private:
    double mTime;
};

extern NEKO_API Error     DispatchEvent(View<Event> event);
extern NEKO_API EventType RegisterEvent(uint32_t count = 1);

NEKO_NS_END