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
#define NEKO_EXPORT
#define NEKO_IMPORT
#endif

#ifdef _NEKO_SOURCE
#define NEKO_API NEKO_EXPORT
#else
#define NEKO_API NEKO_IMPORT
#endif

#define NEKO_CXX17 (__cplusplus >= 201703L)
#define NEKO_CXX20 (__cplusplus >= 202002L)
#define NEKO_CXX23 (__cplusplus >= 202300L)

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

enum class PixelFormat : int;
enum class SampleFormat : int;

/**
 * @brief All refcounted object
 * 
 */
class Object : public std::enable_shared_from_this<Object> {
public:
    virtual ~Object() = default;

    template <typename T>
    Arc<T> as() {
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }
    template <typename T>
    Arc<const T> as() const {
        return std::dynamic_pointer_cast<const T>(shared_from_this());
    }
    template <typename T>
    Arc<T> unsafeAs() {
        return std::static_pointer_cast<T>(shared_from_this());
    }
    template <typename T>
    Arc<const T> unsafeAs() const {
        return std::static_pointer_cast<const T>(shared_from_this());
    }
protected:
    constexpr Object() = default;
};

template <typename T, typename ...Args>
Arc<T> make_shared(Args &&...args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

NEKO_NS_END