#define _NEKO_SOURCE
#include "time.hpp"
#include <chrono>

#ifdef _WIN32
    #include <Windows.h>
#elif defined(__linux)

#endif

NEKO_NS_BEGIN

int64_t GetTicks() noexcept {
#ifdef _WIN32
    #if (_WIN32_WINNT >= 0x0600)
        return ::GetTickCount64();
    #else
        return ::GetTickCount();
    #endif
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

NEKO_NS_END