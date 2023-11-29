#ifndef NDEBUG

#define _NEKO_SOURCE
#include "backtrace.hpp"
#include "utils.hpp"
#include <csignal>
#include <array>

#ifdef _WIN32
    #include <DbgEng.h>
    #include <wrl/client.h>
#elif defined(__linux)
    #include <strings.h>
    #include <unwind.h>
    #include <cxxabi.h>
    #include <unistd.h>
    #include <dlfcn.h>
    #include <cstring>
    #include <vector>
#endif

NEKO_NS_BEGIN

#ifdef _WIN32
static IDebugSymbols *GetDebugSymbols() {
    using Microsoft::WRL::ComPtr;

    // Import the dbg engine library code
    static struct {
        neko_library_path("dbgeng.dll");
        neko_import(DebugCreate);
    } dbgeng;
    static ComPtr<IDebugClient> client;
    static ComPtr<IDebugControl> control;
    static ComPtr<IDebugSymbols> symbols;

    if (dbgeng.DebugCreate == nullptr) {
        return nullptr;
    }

    // Create the debug interface
    auto hr = dbgeng.DebugCreate(IID_PPV_ARGS(&client));
    if (FAILED(hr)) {
        return nullptr;
    }
    hr = client.As(&control);
    if (FAILED(hr)) {
        return nullptr;
    }

    // Attach self
    client->AttachProcess(0, GetCurrentProcessId(), DEBUG_ATTACH_NONINVASIVE | DEBUG_ATTACH_NONINVASIVE_NO_SUSPEND);
    control->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE);

    // Get symbols
    hr = client.As(&symbols);
    if (FAILED(hr)) {
        return nullptr;
    }
    return symbols.Get();
}
#endif

Vec<void *> GetCallstack() {
    Vec<void *> callstack;
#ifdef _WIN32
    callstack.resize(1024);
    auto nframe = CaptureStackBackTrace(0, callstack.size(), callstack.data(), nullptr);
    callstack.resize(nframe);
#else
    ::_Unwind_Backtrace([](_Unwind_Context *ctxt, void *stack) {
        auto p = static_cast<std::vector<void *> *>(stack);
        auto ip = reinterpret_cast<void*>(::_Unwind_GetIP(ctxt));
        if (ip) {
            p->push_back(ip);
        }
        return _URC_NO_REASON;
    }, &callstack);
#endif
    return callstack;
}

void Backtrace() {
#ifdef _WIN32
    // Get Callstack
    static IDebugSymbols *symbols = GetDebugSymbols();
    if (!symbols) {
        return;
    }

    auto callstack = GetCallstack();

    char name[256];
    char extraInfo[512];
    ULONG nameLen = 0;

    // Print current thread info
    fprintf(stderr, "---BACKTRACE BEGIN--- ThreadID '%d' ThreadName '%s'\n", int(GetCurrentThreadId()), NEKO_GetThreadName());

    for (int n = 0; n < callstack.size(); n++) {
        ::memset(extraInfo, 0, sizeof(extraInfo));
        ::memset(name, 0, sizeof(name));

        auto hr = symbols->GetNameByOffset(reinterpret_cast<ULONG64>(callstack[n]), name, sizeof(name), &nameLen, nullptr);
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
#elif defined(__linux)
    auto callstack = GetCallstack();
    fprintf(stderr, "\033[31m---BACKTRACE START--- ThreadID '%d' ThreadName '%s'\033[0m\n", ::getpid(), NEKO_GetThreadName());

    for (size_t n = 0; n < callstack.size(); ++n) {
        size_t demangleBufferSize = 1024;
        char demangleBuffer [1024];
        char lineBuffer [1024] {0};
        const char *name = "???";
        const char *dllname = "???";
        const char *line = "";
        ::Dl_info info;
        if (::dladdr(callstack[n], &info) != 0) {
            if (info.dli_fname) {
                dllname = info.dli_fname;
                dllname = ::rindex(dllname, '/') + 1;
            }
            if (info.dli_sname) {
                name = info.dli_sname;
            }

            auto offset = (char*) callstack[n] - (char*) info.dli_fbase;
            // Call addr2line get number
            char *cmd;
            FILE *proc;
            ::asprintf(&cmd, "addr2line -a -f --exe=%s +%p", info.dli_fname, (void*) offset);
            proc = ::popen(cmd, "r");
            ::free(cmd);

            ::fread(lineBuffer, sizeof(lineBuffer), 1, proc);
            ::fclose(proc);

            // Remove lastest '\n'
            lineBuffer[strlen(lineBuffer) - 1] = '\0';

            auto linePtr = ::rindex(lineBuffer, '\n');
            if (!linePtr) {
                line = "";
            }
            else {
                *linePtr = '\0';
                line = linePtr + 1;

                if (!info.dli_sname) {
                    linePtr = rindex(lineBuffer, '\n');
                    if (linePtr) {
                        name = linePtr + 1;
                    }
                }
            }

                            
            // Try demangle name
            int status;
            auto v = ::abi::__cxa_demangle(name, demangleBuffer, &demangleBufferSize, &status);
            if (v) {
                // Demangle OK
                name = v;
            }
        }
        // #%d %s!%s [%p]
        fprintf(stderr, "#\033[1m%d\033[0m %s!\033[96m%s\033[0m [%s]\n", int(n), dllname, name, line);
    }

    fprintf(stderr, "\033[31m---BACKTRACE END---\033[0m\n");
#endif
}

void InstallCrashHandler() {
#if defined(_WIN32)
    ::SetUnhandledExceptionFilter([](_EXCEPTION_POINTERS *exceptionInfo) -> LONG {
        // Print exception and context
        auto record = exceptionInfo->ExceptionRecord;
        fprintf(stderr, "\033[31m WIN32 EXCEPTION : %08X At %p \033[0m\n", int(record->ExceptionCode), record->ExceptionAddress);
        Backtrace();
        return EXCEPTION_CONTINUE_SEARCH;
    });
#endif

    auto handler = [](int sig) {
        Backtrace();
    };
    ::signal(SIGILL, handler);
    ::signal(SIGSEGV, handler);
    ::signal(SIGABRT, handler);
}

NEKO_NS_END

#endif
