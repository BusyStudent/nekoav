#pragma once

#include "defs.hpp"
#include "resource.hpp"
#include "time.hpp"

NEKO_NS_BEGIN

class Message : public Resource {
public:
    enum Type {
        None,
        StateChanged,  //< Some element state changed
        ErrorOccurred, //< Error occurred, require pipeline stop

        MediaEndOfFile, //< Media is reached eof
        MediaBuffering, //< Media is buffering
        SeekRequested,  //< Request to seek
        FlushRequested, //< Request to flush internal buffer

        ClockUpdated,   //< Media Clock time was updated

        PipelineWakeup, //< Wakeup Pipeline, internal use only
        User = 10086   //< User Begin
    };
    
    explicit Message(Type type, void *sender) : mType(type), mSender(sender) { }
    virtual ~Message() = default;

    Type type() const noexcept {
        return mType;
    }
    void *sender() const noexcept {
        return mSender;
    }
    int64_t time() const noexcept {
        return mTime;
    }

    Arc<Message> shared_from_this() {
        return Resource::shared_from_this<Message>();
    }
private:
    Type  mType = None;
    void *mSender = nullptr;
    int64_t mTime = GetTicks();
};
class ErrorMessage : public Message {
public:
    static Arc<ErrorMessage> make(Error err, Element *element) {
        auto msg = make_shared<ErrorMessage>(element);
        msg->mError = err;
        return msg;
    }
    static Arc<ErrorMessage> make(Error err, Pipeline *pipeline) {
        auto msg = make_shared<ErrorMessage>(pipeline);
        msg->mError = err;
        return msg;
    }

    ErrorMessage(void *sender) : Message(ErrorOccurred, sender) { }

    Error error() const noexcept {
        return mError;
    }
private:
    Error mError { };
};


// Media Layer Message

class SeekMessage : public Message {
public:
    static Arc<SeekMessage> make(double seekTime) {
        auto msg = make_shared<SeekMessage>(nullptr);
        msg->mPosition = seekTime;
        return msg;
    }

    SeekMessage(void *sender) : Message(SeekRequested, sender) { }

    double position() const noexcept {
        return mPosition;
    }
private:
    double mPosition = 0.0;
};



NEKO_NS_END