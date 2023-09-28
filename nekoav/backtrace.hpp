#pragma once

#include "defs.hpp"
#include <string>

NEKO_NS_BEGIN

#ifndef NDEBUG
extern void NEKO_API Backtrace();
#else
inline void Backtrace() { }
#endif

NEKO_NS_END