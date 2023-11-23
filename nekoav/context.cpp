#define _NEKO_SOURCE
#include "context.hpp"

NEKO_NS_BEGIN

Context::Context() {

}
Context::~Context() {
    for (auto &it : mObjects) {
        if (it.second.cleanup) {
            it.second.cleanup();
        }
    }
}

bool Context::addObject(std::type_index idx, void *inf, std::function<void()> &&cleanup) {
    if (inf == nullptr) {
        return false;
    }
    std::lock_guard locker(mMutex);
    mObjects.try_emplace(idx, inf, std::move(cleanup));
    return true;
}
bool Context::removeObject(std::type_index idx, void *p) {
    if (p == nullptr) {
        return false;
    }
    std::lock_guard locker(mMutex);
    auto iter = mObjects.find(idx);
    if (iter == mObjects.end()) {
        return false;
    }
    if (iter->second.value != p) {
        return false;
    }
    if (iter->second.cleanup) {
        iter->second.cleanup();
    }
    mObjects.erase(iter);
    return true;
}
void *Context::queryObject(std::type_index idx) const {
    std::shared_lock locker(mMutex);
    auto it = mObjects.find(idx);
    if (it != mObjects.end()) {
        return it->second.value;
    }
    return nullptr;
}

NEKO_NS_END