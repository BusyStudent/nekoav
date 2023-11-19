#define _NEKO_SOURCE
#include "../compute/opengl.hpp"

#if   defined(_WIN32)
    #define HAVE_WGL
    #include <windows.h>
#endif

#if __has_include(<EGL/egl.h>)
    #define HAVE_EGL
    #include <EGL/egl.h>
#endif

#if __has_include(<GL/glx.h>)
    #define HAVE_GLX
    #include <GL/glx.h>
#endif

NEKO_NS_BEGIN




NEKO_NS_END