#pragma once

#include "defs.hpp"

#ifdef _WIN32
    #include <Windows.h>
    #define NEKO_LoadLibrary(name) ::LoadLibraryA(name)
    #define NEKO_GetProcAddress(handle, name) ::GetProcAddress((HMODULE)handle, name)
    #define NEKO_FreeLibrary(handle) ::FreeLibrary((HMODULE)handle)

    #define NEKO_SetThreadName(name) ::_Neko_SetThreadName(L name);

    inline bool _Neko_SetThreadName(const wchar_t *name) {
        static HRESULT (__stdcall *SetThreadDescription)(HANDLE, LPCWSTR) = nullptr;
        if (SetThreadDescription == nullptr) {
            // Try Get it
            HMODULE kernel = ::GetModuleHandleA("Kernel32.dll");
            SetThreadDescription = (HRESULT (__stdcall *)(HANDLE, LPCWSTR)) ::GetProcAddress(kernel, "SetThreadDescription");
        }
        return SUCCEEDED(SetThreadDescription(::GetCurrentThread(), name));
    }
#else
    #include <dlfcn.h>
    #define NEKO_LoadLibrary(name) ::dlopen(name, RTLD_LAZY)
    #define NEKO_GetProcAddress(handle, name) ::dlsym(handle, name)
    #define NEKO_FreeLibrary(handle) ::dlclose(handle)
#endif

#define neko_library_path(path) struct _loader_t {   \
    _loader_t() { handle = NEKO_LoadLibrary(path); } \
    ~_loader_t() { NEKO_FreeLibrary(handle); }       \
    void *handle;                                    \
} _library;
#define neko_import_symbol(type, var, symbol)  \
    using _pfn_##var = type;                   \
    _pfn_##var var = (_pfn_##var) NEKO_GetProcAddress(_library.handle, symbol);
#define neko_import(func) neko_import_symbol(decltype(::func)*, func, #func)