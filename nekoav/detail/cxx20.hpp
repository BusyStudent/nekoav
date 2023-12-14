#pragma once
#include <version>

#ifdef __cpp_lib_latch
    #include <latch>
#else
    #include <condition_variable>
    #include <atomic>
    #include <mutex>
#endif

#ifdef __cpp_lib_source_location
    #include <source_location>
#else
    #include <strings.h>
    #include <unwind.h>
    #include <cxxabi.h>
    #include <unistd.h>
    #include <dlfcn.h>
    #include <cstring>
    #include <vector>
#endif

namespace std {
inline namespace _neko_fallback {

#ifndef __cpp_lib_latch
class latch final {
public:
    explicit latch(ptrdiff_t count) noexcept : mCount(count) {

    }
    latch(const latch&) = delete;
    ~latch() = default;

    void arrive_and_wait(ptrdiff_t count = 1) noexcept {
        std::unique_lock lock(mMutex);
        auto current = mCount.fetch_sub(count) - count;
        if (current == 0) {
            mCondition.notify_all();
        }
        else {
            mCondition.wait(lock);
        }
    }
    void count_down(ptrdiff_t count = 1) noexcept {
        auto current = mCount.fetch_sub(count) - count;
        if (current == 0) {
            mCondition.notify_all();
        }
    }
    void wait() const noexcept {
        std::unique_lock lock(mMutex);
        while (mCount.load() != 0) {
            mCondition.wait(lock);
        }
    }
    bool try_wait() noexcept {
        return mCount.load() == 0;
    }
private:
    mutable condition_variable mCondition;
    mutable mutex mMutex;
    atomic<ptrdiff_t> mCount;
};
#endif

#ifndef __cpp_lib_source_location
#if defined(__linux)
struct source_location {
    static source_location current() noexcept {
        return source_location();
    }

    inline source_location() noexcept {
        std::vector<void *> callstack;
        ::_Unwind_Backtrace([](_Unwind_Context *ctxt, void *stack) {
            auto p = static_cast<std::vector<void *> *>(stack);
            auto ip = reinterpret_cast<void*>(::_Unwind_GetIP(ctxt));
            if (ip) {
                p->push_back(ip);
            }
            return _URC_NO_REASON;
        }, &callstack);

        if (callstack.size() > 2) {
            size_t demangleBufferSize = 1024;
            char demangleBuffer [1024];
            char lineBuffer [1024] {0};
            const char *line = "???:0";

            ::Dl_info info;

            if (::dladdr(callstack[2], &info) != 0) {
                if (info.dli_fname) {
                    mDllname = info.dli_fname;
                    mDllname = ::rindex(mDllname, '/') + 1;
                }
                if (info.dli_sname) {
                    mName = info.dli_sname;
                }

                auto offset = (char*) callstack[2] - (char*) info.dli_fbase;
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
                if (linePtr) {
                    *linePtr = '\0';
                    line = linePtr + 1;

                    if (!info.dli_sname) {
                        linePtr = rindex(lineBuffer, '\n');
                        if (linePtr) {
                            mName = linePtr + 1;
                        }
                    }
                }

                                
                // Try demangle name
                int status;
                auto v = ::abi::__cxa_demangle(mName, demangleBuffer, &demangleBufferSize, &status);
                if (v) {
                    // Demangle OK
                    mName = v;
                }
            }

            auto mstr = std::string(line);
            int separate = mstr.find_last_of(':');
            if (!mstr.empty() && separate < mstr.length()) {
                mFileName = mstr.substr(0, separate);
                mLine = std::stoi(mstr.substr(separate + 1, mstr.length() - separate));
            }
        }
    };

    inline  uint_least32_t line() const noexcept {
        return mLine;
    }
    inline  uint_least32_t column() const noexcept {
        return 0;
    }
    inline  const char* file_name() const noexcept {
        return mFileName.c_str();
    }
    inline const char* function_name() const noexcept {
        return mName;
    }
private:
    const char *mName = "???";
    const char *mDllname = "???";
    int mLine = 0;
    std::string mFileName;
};
#elif
struct source_location {
    static source_location current() noexcept {
        return source_location();
    }

    constexpr source_location() noexcept = default;

    constexpr uint_least32_t line() const noexcept {
        return 0;
    }
    constexpr uint_least32_t column() const noexcept {
        return 0;
    }
    constexpr const char* file_name() const noexcept {
        return "";
    }
    constexpr const char* function_name() const noexcept {
        return "";
    }
};
#endif
#endif

}
}