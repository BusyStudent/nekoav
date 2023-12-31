#define _NEKO_SOURCE
#include "backtrace.hpp"
#include "utils.hpp"
#include "defs.hpp"
#include "libc.hpp"
#include <csignal>
#include <cstdarg>
#include <string>

#ifdef NEKO_GCC_ABI
    #include <cxxabi.h>
#endif

#if   defined(_WIN32)
    #include <Windows.h>
    #include <io.h>
    #undef min
    #undef max
#elif defined(__linux__)
    #include <sys/stat.h>
    #include <sys/mman.h>
#endif

NEKO_NS_BEGIN

namespace libc {

void *malloc(size_t n) {
    return ::malloc(n);
}

void  free(void *ptr) {
    return ::free(ptr);
}

void  painc(const char *msg, std::source_location loc) {
    // Mark RED

#ifdef _WIN32
    ::_lock_file(stderr);
#endif

    fprintf(stderr, "\033[31m");
    fputs(msg, stderr);
    fprintf(stderr, "FILE: %s:%d FUNCTION: %s\n", loc.file_name(), int(loc.line()), loc.function_name());
    Backtrace();
    fprintf(stderr, "\033[0m");

#ifdef _WIN32
    ::_unlock_file(stderr);
#endif

    // Done
    breakpoint();

    ::abort();
}
void  breakpoint() {
#ifdef _MSC_VER
    __debugbreak();
#elif __GNUC__
    asm volatile("int $3");
#else
    ::raise(SIGTRAP);
#endif
}
void breakpoint_if_debugging() {
    if (is_debugger_present()) {
        breakpoint();
    }
}
bool is_debugger_present() {
#ifdef _WIN32
    return ::IsDebuggerPresent();
#else
    return false;
#endif
}

size_t vsprintf(std::string *buf, const char *fmt, va_list userArgs) {
    va_list varg;
    int s;
    
    va_copy(varg, userArgs);
#ifdef _WIN32
    s = ::_vscprintf(fmt, varg);
#else
    s = ::vsnprintf(nullptr, 0, fmt, varg);
#endif
    va_end(varg);

    int len = buf->length();

#if NEKO_CXX23
    buf->resize_and_overwrite(len + s);
#else
    buf->resize(len + s);
#endif

    va_copy(varg, userArgs);
    ::vsprintf(buf->data() + len, fmt, varg);
    va_end(varg);

    return s;
}
size_t sprintf(std::string *buf, const char *fmt, ...) {
    va_list varg;
    va_start(varg, fmt);
    auto n = vsprintf(buf, fmt, varg);
    va_end(varg);
    return n;
}
std::string asprintf(const char *fmt, ...) {
    std::string buf;
    va_list varg;
    va_start(varg, fmt);
    vsprintf(&buf, fmt, varg);
    va_end(varg);
    return buf;
}
std::string vasprintf(const char *fmt, va_list varg) {
    std::string buf;
    vsprintf(&buf, fmt, varg);
    return buf;
}

const char *typenameof(const std::type_info &info) {
#ifdef NEKO_GCC_ABI
    static thread_local std::string buf;
    auto v = ::abi::__cxa_demangle(info.name(), nullptr, nullptr, nullptr);
    if (!v) {
        return info.name();
    }
    buf = v;
    ::free(v);
    return buf.c_str();
#elif _MSC_VER
    auto name = info.name();
    if (::strstr(name, "class ") == name) {
        return name + 6;
    }
    return name;
#else
    return info.name();
#endif
}

FILE *u8fopen(const char *buffer, const char *mode) {
#ifdef _WIN32
    auto wpath = _Neko_Utf8ToUtf16(buffer);
    auto wmode = _Neko_Utf8ToUtf16(mode);
    return ::_wfopen(wpath.c_str(), wmode.c_str());
#else
    return ::fopen(buffer, mode);
#endif
}

std::span<uint8_t> mmap(const char *path) {
    std::unique_ptr<FILE, int(*)(FILE*)> fp(
        libc::u8fopen(path, "rb"), ::fclose
    );
    if (!fp) {
        return std::span<uint8_t>();
    }
    auto fd = ::fileno(fp.get());
    if (fd == -1) {
        return std::span<uint8_t>();
    }
    struct stat filestat;
    if (::fstat(fd, &filestat) != 0) {
        return std::span<uint8_t>();
    }
#ifdef _WIN32
    auto handle = (HANDLE) ::_get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        return std::span<uint8_t>();
    }
    HANDLE mapping = ::CreateFileMappingW(handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping == nullptr) {
        return std::span<uint8_t>();
    }
    LPVOID buffer = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    ::CloseHandle(mapping);
    return std::span<uint8_t>(static_cast<uint8_t*>(buffer), filestat.st_size);
#elif defined(__linux__)
    // MMAP
    auto addr = (uint8_t*) ::mmap(nullptr, filestat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        return std::span<uint8_t>();
    }
    return std::span<uint8_t>(addr, filestat.st_size);
#else
    auto data = (uint8_t*) ::malloc(filestat.st_size);
    if (data == nullptr) {
        return std::span<uint8_t>();
    }
    if (::fread(data, 1, filestat.st_size, fp.get()) != filestat.st_size) {
        ::free(data);
        return std::span<uint8_t>();
    }
    return std::span<uint8_t>(data, filestat.st_size);
#endif
}

void munmap(std::span<uint8_t> span) {
    if (span.empty()) {
        return;
    }
#ifdef _WIN32
    ::UnmapViewOfFile(span.data());
#elif defined(__linux)
    ::munmap(span.data(), span.size());
#else
    ::free(span.data());
#endif
}

}

NEKO_NS_END