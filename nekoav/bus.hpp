#pragma once

#include "defs.hpp"
#include "resource.hpp"
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <list>

NEKO_NS_BEGIN

class Message : public Resource {
public:
    enum Type {
        None,
        StateChanged,  //< Some element state changed
        ErrorOccurred, //< Error occurred, require pipeline stop

        MediaEndOfFile, //< Media is reached eof
        MediaBuffering, //< Media is buffering

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

    Arc<Message> shared_from_this() {
        return Resource::shared_from_this<Message>();
    }
private:
    Type  mType = None;
    void *mSender = nullptr;
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


class NEKO_API Bus {
public:
    using Watcher = std::function<void (const Message &, bool &drop)>;
    using Token   = void *;

    Bus();
    Bus(const Bus &) = delete;
    ~Bus();

    /**
     * @brief Post a message into queue
     * 
     * @param message 
     */
    void postMessage(const Arc<Message> &message);
    /**
     * @brief Add watcher into bus (first in, first called)
     * 
     * @details The watcher will be called at different thread
     * 
     * @param watcher 
     * @return Token 
     */
    Token addWatcher(Watcher &&watcher);
    void  delWatcher(Token token);

    bool  waitMessage(Arc<Message> *message, int timeoutMs = -1);
    bool  pollMessage(Arc<Message> *message) {
        return waitMessage(message, 0);
    }
private:
    std::condition_variable mCondition;
    std::mutex              mMutex;
    std::queue<Arc<Message> > mQueue;
    std::list<Watcher>        mHandlers;
};

NEKO_NS_END