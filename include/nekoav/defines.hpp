#pragma once

// Async runtime
#include <ilias/io/error.hpp>
#include <ilias/result.hpp>
#include <ilias/task.hpp>

// Memory management
#include <memory>

#if defined(_MSC_VER)
    #define NEKOAV_EXPORT __declspec(dllexport)
    #define NEKOAV_IMPORT __declspec(dllimport)
#else
    #define NEKOAV_EXPORT __attribute__((visibility("default")))
    #define NEKOAV_IMPORT __attribute__((visibility("default")))
#endif // _MSC_VER

#if defined(_NEKOAV_SOURCE)
    #define NEKOAV_API NEKOAV_EXPORT
#else
    #define NEKOAV_API NEKOAV_IMPORT
#endif // _NEKOAV_SOURCE

#define KEKOAV_THROW(exp) throw exp

namespace nekoav {

// Re-import ilias types
using ilias::IoResult;
using ilias::IoTask;
using ilias::Task;
using ilias::Err;

// Utiks
template <typename ...Ts>
struct Overloads : Ts... { using Ts::operator()...; };

} // namespace nekoav