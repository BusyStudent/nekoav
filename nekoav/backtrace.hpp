#pragma once

#include "defs.hpp"
#include <string>

NEKO_NS_BEGIN

#ifndef NDEBUG
extern void NEKO_API InstallCrashHandler();
extern void NEKO_API Backtrace();
#else
inline void Backtrace() { }
inline void InstallCrashHandler() { }
#endif

NEKO_NS_END