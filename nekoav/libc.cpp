#define _NEKO_SOURCE
#include "defs.hpp"

NEKO_NS_BEGIN

namespace libc {

void *malloc(size_t n) {
    return std::malloc(n);
}
void  free(void *ptr) {
    return std::free(ptr);
}
void  sleep(int ms) {

}

}

NEKO_NS_END