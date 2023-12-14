#define _NEKO_SOURCE
#include "time.hpp"
#include <chrono>
#include <thread>

#ifdef _WIN32
    #include <Windows.h>
    
    #ifdef _MSC_VER
        #pragma comment(lib, "winmm.lib")
    #endif
#elif defined(__linux)
    #include <sys/time.h>
    #include <ctime>
    #include <unistd.h>
#endif

NEKO_NS_BEGIN

int64_t GetTicks() noexcept {
#ifdef _WIN32
    LARGE_INTEGER counter;
    LARGE_INTEGER freq;
    if (::QueryPerformanceCounter(&counter)) {
        if (::QueryPerformanceFrequency(&freq)) {
            return counter.QuadPart  * 1000 / freq.QuadPart;
        }
    }

    #if (_WIN32_WINNT >= 0x0600)
        return ::GetTickCount64();
    #else
        return ::GetTickCount();
    #endif
#elif defined(__linux)
    ::timespec ts;
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

int64_t SleepFor(int64_t ms) noexcept {
    if (ms < 0) {
        return 0;
    }
    auto ticks = GetTicks();
#ifdef _WIN32
    ::timeBeginPeriod(3);
    ::Sleep(ms);
    ::timeEndPeriod(3);
#elif defined(__linux)
    ::usleep(ms * 1000);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
    // Calc offset
    return GetTicks() - (ticks + ms);
}

NEKO_NS_END