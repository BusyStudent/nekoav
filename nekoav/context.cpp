#define _NEKO_SOURCE
#include "context.hpp"

NEKO_NS_BEGIN

Context::Context() {

}
Context::~Context() {

}

void Context::registerInterface(std::type_index idx, void *inf) {
    std::lock_guard locker(mMutex);
    mInterfaces.insert(std::make_pair(idx, inf));
}
void Context::unregisterInterface(std::type_index idx) {
    std::lock_guard locker(mMutex);
    mInterfaces.erase(idx);
}
void *Context::queryInterface(std::type_index idx) const {
    std::lock_guard locker(mMutex);
    auto it = mInterfaces.find(idx);
    if (it != mInterfaces.end()) {
        return it->second;
    }
    return nullptr;
}

NEKO_NS_END