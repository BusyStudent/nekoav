#pragma once

// Async runtime
#include <ilias/io/error.hpp>
#include <ilias/result.hpp>
#include <ilias/task.hpp>

// Memory management
#include <memory>

#define NEKOAV_API // TODO: Add export macro
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