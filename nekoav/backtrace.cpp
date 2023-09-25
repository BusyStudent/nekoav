#ifndef NDEBUG

#define _NEKO_SOURCE
#include "backtrace.hpp"
#include "utils.hpp"
#include <array>

#ifdef _WIN32
    #include <DbgEng.h>
    #include <wrl/client.h>
#endif

NEKO_NS_BEGIN

void Backtrace() {
#ifdef _WIN32
    using Microsoft::WRL::ComPtr;

    // Import the dbg engine library code
    static struct {
        neko_library_path("dbgeng.dll");
        neko_import(DebugCreate);
    } dbgeng;
    if (dbgeng.DebugCreate == nullptr) {
        return;
    }

    ComPtr<IDebugClient> client;
    ComPtr<IDebugControl> control;

    // Create the debug interface
    auto hr = dbgeng.DebugCreate(IID_PPV_ARGS(&client));
    if (FAILED(hr)) {
        return;
    }
    hr = client.As(&control);
    if (FAILED(hr)) {
        return;
    }

    // Attach self
    client->AttachProcess(0, GetCurrentProcessId(), DEBUG_ATTACH_NONINVASIVE | DEBUG_ATTACH_NONINVASIVE_NO_SUSPEND);
    control->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE);

    // Get symbols
    ComPtr<IDebugSymbols> symbols;
    hr = client.As(&symbols);
    if (FAILED(hr)) {
        return;
    }

    // Get Callstack
    std::array<void*, 1024> callstack;
    auto nframe = CaptureStackBackTrace(0, callstack.size(), callstack.data(), nullptr);

    char name[256];
    char extraInfo[512];
    ULONG nameLen = 0;

    // Print current thread info
    fprintf(stderr, "---BACKTRACE BEGIN--- ThreadID '%d' ThreadName '%s'\n", int(GetCurrentThreadId()), NEKO_GetThreadName());

    for (int n = 0; n < nframe; n++) {
        ::memset(extraInfo, 0, sizeof(extraInfo));
        ::memset(name, 0, sizeof(name));

        hr = symbols->GetNameByOffset(reinterpret_cast<ULONG64>(callstack[n]), name, sizeof(name), &nameLen, nullptr);
        if (hr == S_OK) {
            name[nameLen] = '\0';
            
            // Get source code path and line
            ULONG line = 0;
            ULONG fileLen = 0;
            char file[256];
            
            symbols->GetLineByOffset(reinterpret_cast<ULONG64>(callstack[n]), &line, file, sizeof(file), &fileLen, nullptr);
            file[fileLen] = '\0';

            ::sprintf(extraInfo, "%s:%ld", file, long(line));
        }
        else {
            ::strcpy(name, "???");

            // Get the module path
        }

        // Check this function is in stl, mini it
        if (::strstr(name, "!std::")) {
            extraInfo[0] = '\0';
        }
        
        fprintf(stderr, "#%d %s [%s]\n", n, name, extraInfo);
    }

    fprintf(stderr, "---BACKTRACE END---\n");
#endif
}

NEKO_NS_END

#endif