#pragma once

#include "defs.hpp"
#include "time.hpp"
#include "resource.hpp"
#include <functional>
#include <string>

NEKO_NS_BEGIN

/**
 * @brief Event Base class
 * 
 */
class Event : public std::enable_shared_from_this<Event> {
public:
    enum Type : uint32_t {
        None,
        StateChanged,  //< Some element state changed
        BusChanged,    //< Element bus was changed
        ConetxtChanged, //< Element's context was changed
        ErrorOccurred, //< Error occurred, require pipeline stop

        PadAdded,
        PadRemoved,
        PadLinked,
        PadUnlinked,

        PlaybackPause,
        PlaybackResume,

        MediaEndOfFile, //< Media is reached eof
        MediaBuffering, //< Media is buffering
        SeekRequested,  //< Request to seek
        FlushRequested, //< Request to flush internal buffer
        ClockUpdated,   //< The clock was updated

        PipelineWakeup, //< Wakeup Pipeline, internal use only
        User = 10086   //< User Begin
    };
    
    explicit Event(Type type, Element *sender) : mType(type), mSender(sender) { }
    Event(const Event &) = default;
    virtual ~Event() = default;

    Type type() const noexcept {
        return mType;
    }
    Element *sender() const noexcept {
        return mSender;
    }
    int64_t timestamp() const noexcept {
        return mTime;
    }
    template <typename T>
    T *as() {
        return static_cast<T *>(this);
    }

    /**
     * @brief Create a event object 
     * 
     * @param type 
     * @param sender 
     * @return Arc<Event> 
     */
    static Arc<Event> make(Type type, Element *sender) {
        return std::make_shared<Event>(type, sender);
    }
private:
    Type     mType = None;
    Element *mSender = nullptr;
    int64_t  mTime = GetTicks();
};

using EventType = Event::Type;

/**
 * @brief Generic Error event
 * 
 */
class ErrorEvent : public Event {
public:
    ErrorEvent(Error error, Element *sender) : 
        Event(ErrorOccurred, sender), mError(error) { }
    ErrorEvent(Error error, std::string_view message, Element *sender) : 
        Event(ErrorOccurred, sender), mError(error), mMessage(message) { }

    Error error() const noexcept {
        return mError;
    }
    std::string_view message() const noexcept {
        return mMessage;
    }

    static Arc<ErrorEvent> make(Error error, Element *sender) {
        return std::make_shared<ErrorEvent>(error, sender);
    }
    static Arc<ErrorEvent> make(Error error, std::string_view message, Element *sender) {
        return std::make_shared<ErrorEvent>(error, message, sender);
    }
private:
    Error mError;
    std::string mMessage;
};

class MediaClock;
/**
 * @brief Media Clock
 * 
 */
class ClockEvent : public Event {
public:
    ClockEvent(Type type, double position, Element *sender) : Event(type, sender), mPosition(position) { };

    double position() const noexcept {
        return mPosition;
    }

    static Arc<ClockEvent> make(Type type, double position, Element *sender) {
        return MakeShared<ClockEvent>(type, position, sender);
    }
private:
    double mPosition;
};
/**
 * @brief Media Seek 
 * 
 */
class SeekEvent : public Event {
public:
    SeekEvent(double targetSeconds) : Event(SeekRequested, nullptr), mPosition(targetSeconds) { }

    double position() const noexcept {
        return mPosition;
    }
    static Arc<SeekEvent> make(double targetSeconds) {
        return MakeShared<SeekEvent>(targetSeconds);
    }
private:
    double mPosition;
};
/**
 * @brief Media Bufferinf 
 * 
 */
class BufferingEvent : public Event {
public:
    BufferingEvent(int progress, Element *sender) : Event(MediaBuffering, sender), mProgress(progress) {
        NEKO_ASSERT(progress >= 0 && progress <= 100);
    }

    int progress() const noexcept {
        return mProgress;
    }
    bool isFinished() const noexcept {
        return mProgress == 100;
    }
    bool isStarted() const noexcept {
        return mProgress == 0;
    }
    static Arc<BufferingEvent> make(int progress, Element *sender) {
        return MakeShared<BufferingEvent>(progress, sender);
    }
private:
    int mProgress;
};

/**
 * @brief Register an event type
 * 
 * @param count 
 * @return EventType 
 */
extern NEKO_API EventType RegisterEvent(uint32_t count = 1);

NEKO_NS_END