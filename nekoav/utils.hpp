#pragma once

#include "defs.hpp"

#ifdef _WIN32
    #include <Windows.h>
    #include <string>

    #define NEKO_LoadLibrary(name) ::LoadLibraryA(name)
    #define NEKO_GetProcAddress(handle, name) ::GetProcAddress((HMODULE)handle, name)
    #define NEKO_FreeLibrary(handle) ::FreeLibrary((HMODULE)handle)

    #define NEKO_SetThreadName(name) ::_Neko_SetThreadName(::GetCurrentThread(), name)
    #define NEKO_GetThreadName()     ::_Neko_GetThreadName(::GetCurrentThread()).c_str()

    inline auto _Neko_Utf16ToUtf8(std::wstring_view uv) {
        std::string buf;
        auto n = ::WideCharToMultiByte(CP_UTF8, 0, uv.data(), uv.length(), nullptr, 0, nullptr, nullptr);
        buf.resize(n);
        n = ::WideCharToMultiByte(CP_UTF8, 0, uv.data(), uv.length(), &buf[0], n, nullptr, nullptr);
        return buf;
    }
    inline auto _Neko_Utf8ToUtf16(std::string_view uv) {
        std::wstring buf;
        auto n = ::MultiByteToWideChar(CP_UTF8, 0, uv.data(), uv.length(), nullptr, 0);
        buf.resize(n);
        n = ::MultiByteToWideChar(CP_UTF8, 0, uv.data(), uv.length(), buf.data(), n);
        return buf;
    }

    inline bool _Neko_SetThreadName(HANDLE thread, std::string_view uname) {
        static HRESULT (__stdcall *SetThreadDescription)(HANDLE, LPCWSTR) = nullptr;
        if (SetThreadDescription == nullptr) {
            // Try Get it
            HMODULE kernel = ::GetModuleHandleA("Kernel32.dll");
            SetThreadDescription = (HRESULT (__stdcall *)(HANDLE, LPCWSTR)) ::GetProcAddress(kernel, "SetThreadDescription");
        }
        if (SetThreadDescription == nullptr) {
            return false;
        }
        return SUCCEEDED(SetThreadDescription(thread, _Neko_Utf8ToUtf16(uname).c_str()));
    }
    inline auto _Neko_GetThreadName(HANDLE thread) {
        static HRESULT (__stdcall *GetThreadDescription)(HANDLE, PWSTR *) = nullptr;
        if (GetThreadDescription == nullptr) {
            // Try Get it
            HMODULE kernel = ::GetModuleHandleA("Kernel32.dll");
            GetThreadDescription = (HRESULT (__stdcall *)(HANDLE, PWSTR *)) ::GetProcAddress(kernel, "GetThreadDescription");
        }
        if (GetThreadDescription == nullptr) {
            return std::string();
        }
        wchar_t *name = nullptr;
        if (FAILED(GetThreadDescription(thread, &name))) {
            return std::string();
        }
        auto buf = _Neko_Utf16ToUtf8(name);
        ::LocalFree(name);
        return buf;
    }
#else
    #include <pthread.h>
    #include <dlfcn.h>
    #include <string>
    #define NEKO_LoadLibrary(name) ::dlopen(name, RTLD_LAZY)
    #define NEKO_GetProcAddress(handle, name) ::dlsym(handle, name)
    #define NEKO_FreeLibrary(handle) ::dlclose(handle)

    #define NEKO_SetThreadName(name) ::pthread_setname_np(::pthread_self(), name)
    #define NEKO_GetThreadName()     ::_Neko_GetThreadName(::pthread_self()).data()

    inline auto _Neko_GetThreadName(pthread_t thread) {
        std::array<char, 32> buf {0};
        ::pthread_getname_np(thread, buf.data(), buf.size());
        return buf;
    }
#endif


inline std::string Neko_asprintf(const char *fmt, ...) {
    va_list varg;
    int s;
    
    va_start(varg, fmt);
#ifdef _WIN32
    s = _vscprintf(fmt, varg);
#else
    s = vsnprintf(nullptr, 0, fmt, varg);
#endif
    va_end(varg);

    std::string str;
    str.resize(s);

    va_start(varg, fmt);
    vsprintf(str.data(), fmt, varg);
    va_end(varg);

    return str;
}

#define neko_library_path(path) struct _loader_t {   \
    _loader_t() { handle = NEKO_LoadLibrary(path); } \
    ~_loader_t() { NEKO_FreeLibrary(handle); }       \
    void *handle;                                    \
} _library;                                          \
bool isLoaded() const noexcept {                     \
    return _library.handle != nullptr;               \
}
#define neko_import_symbol_static(var, symbol) \
    static constexpr auto var = ::symbol;
#define neko_import_symbol(type, var, symbol)  \
    using _pfn_##var = type;                   \
    _pfn_##var var = _pfn_##var(NEKO_GetProcAddress(_library.handle, symbol));
#define neko_import4(type, name) neko_import_symbol(type, name, #name)
#define neko_import(func) neko_import_symbol(decltype(::func)*, func, #func)