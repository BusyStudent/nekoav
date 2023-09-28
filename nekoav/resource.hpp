#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

/**
 * @brief All object passed from Element to Element must inherit it, it support RTTI
 * 
 */
class Resource : public Object { };

/**
 * @brief View of the resource
 * 
 */
class ResourceView {
public:
    template <typename T>
    ResourceView(const Arc<T> &c) : mPtr(c.get()) { }
    ResourceView(Resource *ptr) : mPtr(ptr) { }
    ResourceView(const ResourceView &) = default;
    ~ResourceView() = default;

    template <typename T>
    T *as() const {
        return dynamic_cast<T*>(mPtr);
    }
private:
    Resource *mPtr = nullptr;
};


NEKO_NS_END