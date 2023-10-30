#pragma once

#include "defs.hpp"
#include "resource.hpp"
#include "message.hpp"
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <list>

NEKO_NS_BEGIN

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