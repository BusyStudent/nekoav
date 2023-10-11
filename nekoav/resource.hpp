#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

/**
 * @brief All object passed from Element to Element must inherit it, it support RTTI
 * 
 */
class Resource : public Object { };

using ResourceView = View<Resource>;

NEKO_NS_END