#define _NEKO_SOURCE
#include "defs.hpp"

#if NEKO_GCC_ABI
    #include <cxxabi.h>
#endif

NEKO_NS_BEGIN

namespace libc {

void *malloc(size_t n) {
    return ::malloc(n);
}
void  free(void *ptr) {
    return ::free(ptr);
}

#ifdef NEKO_GCC_ABI
char *demangle(const std::type_info &type) {    
    return ::abi::__cxa_demangle(info.name(), nullptr, nullptr, &status);
}
#endif

}

NEKO_NS_END