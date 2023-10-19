#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

/**
 * @brief All object passed from Element to Element must inherit it, it support RTTI
 * 
 */
class Resource : public std::enable_shared_from_this<Resource> { 
public:
    virtual ~Resource() = default;

    auto shared_from_this() {
        return std::enable_shared_from_this<Resource>::shared_from_this();
    }
    auto shared_from_this() const {
        return std::enable_shared_from_this<Resource>::shared_from_this();
    }
    template <typename T>
    auto shared_from_this() -> Arc<T> {
        return std::static_pointer_cast<T>(shared_from_this());
    }
protected:
    Resource() = default;
};

using ResourceView = View<Resource>;

NEKO_NS_END