#define _NEKO_SOURCE

#include "bus.hpp"
#include <algorithm>

NEKO_NS_BEGIN

Bus::Bus() {

}
Bus::~Bus() {

}
auto Bus::addWatcher(Watcher &&watcher) -> Token {
    std::lock_guard locker(mMutex);
    return &mHandlers.emplace_back(std::move(watcher));
}
void Bus::delWatcher(Token token) {
    if (!token) {
        return;
    }
    std::lock_guard locker(mMutex);
    auto iter = std::find_if(mHandlers.begin(), mHandlers.end(), [token](const Watcher &watcher) {
        return &watcher == token;
    });
    if (iter != mHandlers.end()) {
        mHandlers.erase(iter);
    }
}
void Bus::postMessage(const Arc<Message> &message) {
    if (!message) {
        return;
    }
    std::lock_guard lockerMtx(mMutex);

    // For all watcher first
    bool drop = false;
    for (const auto &w : mHandlers) {
        w(*message, drop);
        if (drop) {
            return;
        }
    }

    mQueue.emplace(message);
    mCondition.notify_one();
}
bool Bus::waitMessage(Arc<Message> *msg, int timeout) {
    if (!msg) {
        return false;
    }
    std::unique_lock locker(mMutex);
    while (mQueue.empty()) {
        if (timeout == 0) {
            // No timeout Poll mode
            return false;
        }
        if (timeout == -1) {
            mCondition.wait(locker);
        }
        else {
            mCondition.wait_for(locker, std::chrono::milliseconds(timeout));
        }
    }

    // Got once message
    *msg = std::move(mQueue.front());
    mQueue.pop();
    return true;
 }

NEKO_NS_END