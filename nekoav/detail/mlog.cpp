#include "mlog.hpp"
#include "../defs.hpp"

#include <stdarg.h>

static int log_level = 0;

void NEKO_EXPORT setLogLevel(int level) {
    log_level = level;
}

void NEKO_EXPORT _log_out (
    const int level,
    const char *fmt_prex, // TODO: 做日志前缀解析
    const char * type,
    const char *time_str,
    const char *file,
    const int line,
    const char *func_name,
    const char *log_str,
    FILE *out)
{
    if (log_level > level) {
        return;
    }
#ifdef NDEBUG
    if (level == NEKO_LOG_LEVEL_DEBUG) {
        return;
    }
#endif
    std::string timestr(time_str);
    std::string fmt;
    if (_Neko_HasColorTTY()) {
        switch (level)
        {
        case NEKO_LOG_LEVEL_INFO:
            fmt = "[-- \x1b[90;01m%s\x1b[0m -- [%s][%s:%d](%s)] \x1b[90m%s\x1b[0m\n";
            break;
        case NEKO_LOG_LEVEL_DEBUG:
            fmt = "[-- \x1b[34;01m%s\x1b[0m -- [%s][%s:%d](%s)] \x1b[34m%s\x1b[0m\n";
            break;
        case NEKO_LOG_LEVEL_WARNING:
            fmt = "[-- \x1b[33;01m%s\x1b[0m -- [%s][%s:%d](%s)] \x1b[33m%s\x1b[0m\n";
            break;
        case NEKO_LOG_LEVEL_FATA:
            fmt = "[-- \x1b[31;01m%s\x1b[0m -- [%s][%s:%d](%s)] \x1b[31;01m%s\x1b[0m\n";
            break;
        default:
            fmt = "[-- \x1b[0m%s\x1b[0m -- [%s][%s:%d](%s)] \x1b[0m%s\x1b[0m\n";
        }
    } else {
        fmt = "[-- %s -- [%s][%s:%d](%s)] %s\n";
    }

    fprintf(out, fmt.c_str(), type, timestr.substr(0, timestr.length() - 1).c_str(), file, line, func_name, log_str);
    fflush(out);
    if (level == NEKO_LOG_LEVEL_FATA) {
        NEKO_ASSERT(false);
    }
}