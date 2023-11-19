#define _NEKO_SOURCE
#include "context.hpp"

NEKO_NS_BEGIN

Context::Context() {

}
Context::~Context() {

}

bool Context::addObject(std::type_index idx, void *inf) {
    if (inf == nullptr) {
        return false;
    }
    std::lock_guard locker(mMutex);
    mObjects.try_emplace(idx, inf);
    return true;
}
bool Context::removeObject(std::type_index idx, void *p) {
    if (p == nullptr) {
        return false;
    }
    std::lock_guard locker(mMutex);
    mObjects.erase(idx);
    return true;
}
void *Context::queryObject(std::type_index idx) const {
    std::shared_lock locker(mMutex);
    auto it = mObjects.find(idx);
    if (it != mObjects.end()) {
        return it->second;
    }
    return nullptr;
}

NEKO_NS_END