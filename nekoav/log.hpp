#pragma once
#include "detail/mlog.hpp"
#include <ctime>

#if defined(NEKO_NO_LOG)
#define NEKO_LOG_DEBUG(...)
#define NEKO_LOG_INFO(...)
#define NEKO_LOG_WARNING(...)
#define NEKO_LOG_FATA(...)
inline void setLogLevel(int) { }
#else
extern void setLogLevel(int level);
extern void _log_out(const int level, const char *fmt_prex, const char * type, const char *time_str, const char *file, const int line, const char *func_name, const char *log_str, FILE *out);
#define  _NEKO_LOG_PREFIX(TYPE, ...)                                                                                                            \
{                                                                                                                                               \
    time_t _now = ::time(nullptr);                                                                                                                \
    ::_log_out(NEKO_LOG_LEVEL_##TYPE, "", #TYPE, ::ctime(&_now), __FILE__, __LINE__, __FUNCTION__, ::_Neko_FormatLog(__VA_ARGS__).c_str(), stderr); \
}

#define NEKO_LOG_DEBUG(fmt, ...)            \
    _NEKO_LOG_PREFIX(DEBUG, fmt, __VA_ARGS__)

#define NEKO_LOG_INFO(fmt, ...)            \
    _NEKO_LOG_PREFIX(INFO, fmt, __VA_ARGS__)

#define NEKO_LOG_WARNING(fmt, ...)            \
    _NEKO_LOG_PREFIX(WARNING, fmt, __VA_ARGS__)

#define NEKO_LOG_FATA(fmt, ...)            \
    _NEKO_LOG_PREFIX(FATA, fmt, __VA_ARGS__)

#endif