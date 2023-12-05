#pragma once

#include "defs.hpp"
#include <string>
#include <vector>

NEKO_NS_BEGIN

template <typename T>
using Vec = std::vector<T>;

#ifndef NDEBUG
extern void NEKO_API InstallCrashHandler();
extern void NEKO_API Backtrace();

// Internal API
extern Vec<void *> GetCallstack();
#else
inline void Backtrace() { }
inline void InstallCrashHandler() { }
#endif

NEKO_NS_END