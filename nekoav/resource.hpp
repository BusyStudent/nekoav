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
protected:
    Resource() = default;
};

using ResourceView = View<Resource>;

NEKO_NS_END