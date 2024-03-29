#pragma once

#ifndef NEKO_NAMESPACE 
#define NEKO_NAMESPACE NekoAV
#endif

#define NEKO_USING(x) using Neko##x = NEKO_NAMESPACE::x
#define NEKO_NS_BEGIN namespace NEKO_NAMESPACE {
#define NEKO_NS_END   }

#ifdef  _MSC_VER
#define NEKO_ATTRIBUTE(x) __declspec(x)
#elif   __GNUC__
#define NEKO_ATTRIBUTE(x) __attribute__((x))
#else
#define NEKO_ATTRIBUTE(x)
#endif

#ifdef  _WIN32
#define NEKO_EXPORT NEKO_ATTRIBUTE(dllexport)
#define NEKO_IMPORT NEKO_ATTRIBUTE(dllimport)
#else
#define NEKO_EXPORT NEKO_ATTRIBUTE(visibility("default"))
#define NEKO_IMPORT
#endif

#ifdef _NEKO_SOURCE
#define NEKO_API NEKO_EXPORT
#else
#define NEKO_API NEKO_IMPORT
#endif

#if defined(_WIN32) && defined(__GNUC__) && !defined(__clang__)
#define NEKO_MINGW
#endif

#if defined(__GNUC__)
#define NEKO_CONSTRUCTOR(name) static void __attribute__((constructor)) name() noexcept
#else
#define NEKO_CONSTRUCTOR(name) static void name() noexcept;          \
    static const NEKO_NAMESPACE::ConstructHelper name##__ctr {name}; \
    static void name() noexcept
#endif

#if defined(__GNUC__) && __has_include(<cxxabi.h>)
#define NEKO_GCC_ABI
#endif

#if !defined(NDEBUG)
#define NEKO_BREAKPOINT() ::NEKO_NAMESPACE::libc::breakpoint_if_debugging();
#define NEKO_ASSERT(x) if (!(x)) { ::NEKO_NAMESPACE::libc::painc("Assertion failed: " #x); }
#define NEKO_PANIC(x) ::NEKO_NAMESPACE::libc::painc(#x);
#else
#define NEKO_BREAKPOINT()
#define NEKO_ASSERT(x)
#define NEKO_PANIC(x) ::abort()
#endif

#if !defined(NDEBUG)
#define NEKO_IMPL_BEGIN
#define NEKO_IMPL_END
#else
#define NEKO_IMPL_BEGIN namespace {
#define NEKO_IMPL_END }
#endif

#define NEKO_CXX17 (__cplusplus >= 201703L)
#define NEKO_CXX20 (__cplusplus >= 202002L)
#define NEKO_CXX23 (__cplusplus >= 202300L)

#include "detail/cxx20.hpp"
#include <typeinfo>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <memory>
#include <atomic>

NEKO_NS_BEGIN

template <typename T>
using Arc = std::shared_ptr<T>; ///< Arc = Atomic refcounter
template <typename T>
using Weak = std::weak_ptr<T>;
template <typename T>
using Box = std::unique_ptr<T>; ///< 
template <typename T>
using Atomic = std::atomic<T>;

using microseconds = int64_t;

enum class PixelFormat : int;
enum class SampleFormat : int;
enum class Error : int;
enum class State : int;
enum class StateChange : int;

class Pad;
class Event;
class EventSink;
class Resource;
class Element;
class ElementFactory;
class Context;
class Container;
class Properties;
class Property;
class Pipeline;
class Thread;

/**
 * @brief Wrapper for RAW Pointer, implict cast from Arc and RAW Pointer
 * 
 * @tparam T 
 */
template <typename T>
class View {
public:
    View() = default;
    template <typename U>
    View(const Arc<U> &ptr) : mPtr(ptr.get()) { }
    template <typename U>
    View(const Box<U> &ptr) : mPtr(ptr.get()) { }
    View(T *ptr) : mPtr(ptr) { }
    View(const View &) = default;
    ~View() = default;

    T *get() const noexcept {
        return mPtr;
    }
    bool empty() const noexcept {
        return mPtr == nullptr;
    }
    
    template <typename U>
    U *viewAs() const {
        return dynamic_cast<U*>(mPtr);
    }
    T *operator ->() const noexcept {
        return mPtr;
    }
    T  &operator *() const noexcept {
        return *mPtr;
    }
    operator bool() const noexcept {
        return mPtr != nullptr;
    }
protected:
    T *mPtr = nullptr;
};

/**
 * @brief A RAII Helper for cleanup
 * 
 * @tparam T 
 */
template <typename T>
class ScopeExit {
public:
    ScopeExit(T &&callback) : mCallback(std::move(callback)) { }
    ScopeExit(const ScopeExit &) = delete;
    ~ScopeExit() {
        if (!mReleased) {
            mCallback();
        }
    }

    void release() noexcept {
        mReleased = true;
    }
private:
    T mCallback;
    bool mReleased = false;
};

/**
 * @brief Helper for NEKO_CONSTRUCTOR
 * 
 * @tparam T 
 */
class ConstructHelper {
public:
    ConstructHelper(void (*fn)()) noexcept {
        fn();
    }
};

/**
 * @brief Create a Arc of giving type
 * 
 * @tparam T 
 * @tparam Args 
 * @param args 
 * @return Arc<T> 
 */
template <typename T, typename ...Args>
Arc<T> make_shared(Args &&...args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}
/**
 * @brief Create a Arc of giving type
 * 
 * @tparam T 
 * @tparam Args 
 * @param args 
 * @return Arc<T> 
 */
template <typename T, typename ...Args>
Arc<T> MakeShared(Args &&...args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// Some useful function
namespace libc {
    /**
     * @brief Allocate memory
     * 
     * @param n 
     * @return void* 
     */
    extern NEKO_API void *malloc(size_t n);
    /**
     * @brief Free memory
     * 
     * @param ptr 
     */
    extern NEKO_API void free(void *ptr);
    /**
     * @brief Trigger debugger to break
     * 
     */
    extern NEKO_API void breakpoint();
    /**
     * @brief Trigger debugger to break if is debugging
     * 
     */
    extern NEKO_API void breakpoint_if_debugging();
    /**
     * @brief Check is debugger attach this process
     * 
     * @return True on debugging
     */
    extern NEKO_API bool is_debugger_present();
    /**
     * @brief Called on assert failed
     * 
     * @param msg 
     * @param loc 
     * @return NEKO_API 
     */
    [[noreturn]]
    extern NEKO_API void painc(const char *msg, std::source_location loc = std::source_location::current());
}

NEKO_NS_END