#pragma once
#include "opengl_macros.hpp"
#include "opengl_pfns.hpp"
#include "../defs.hpp"

NEKO_NS_BEGIN

class GLFunctions {
    public:
        template <typename T>
        T *cast() noexcept {
            // Do check
            if (majorVersion > T::requiredMajorVersion || (majorVersion == T::requiredMajorVersion && minorVersion >= T::requiredMinorVersion)) {
                return static_cast<T *>(this);
            }
            return nullptr;
        }
        template <typename T>
        const T *cast() const noexcept {
            // Do check
            if (majorVersion > T::requiredMajorVersion || (majorVersion == T::requiredMajorVersion && minorVersion >= T::requiredMinorVersion)) {
                return static_cast<const T *>(this);
            }
            return nullptr;
        }

        // Memory allocation
        // void *operator new(size_t size) noexcept {
        //     return libc::malloc(size);
        // }
        // void operator delete(void *ptr) noexcept {
        //     return libc::free(ptr);
        // }
    protected:
        constexpr GLFunctions() = default;

        int majorVersion = 0;
        int minorVersion = 0;
};

class GLFunctions_1_0 : public GLFunctions {
    public:
        static constexpr int requiredMajorVersion = 1;
        static constexpr int requiredMinorVersion = 0;

        GLFunctions_1_0() {
            majorVersion = 1;
            minorVersion = 0;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_1_0_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_1_0_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_1_1 : public GLFunctions_1_0 {
    public:
        static constexpr int requiredMajorVersion = 1;
        static constexpr int requiredMinorVersion = 1;

        GLFunctions_1_1() {
            majorVersion = 1;
            minorVersion = 1;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_1_1_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_1_0::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_1_1_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_1_2 : public GLFunctions_1_1 {
    public:
        static constexpr int requiredMajorVersion = 1;
        static constexpr int requiredMinorVersion = 2;

        GLFunctions_1_2() {
            majorVersion = 1;
            minorVersion = 2;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_1_2_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_1_1::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_1_2_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_1_3 : public GLFunctions_1_2 {
    public:
        static constexpr int requiredMajorVersion = 1;
        static constexpr int requiredMinorVersion = 3;

        GLFunctions_1_3() {
            majorVersion = 1;
            minorVersion = 3;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_1_3_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_1_2::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_1_3_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_1_4 : public GLFunctions_1_3 {
    public:
        static constexpr int requiredMajorVersion = 1;
        static constexpr int requiredMinorVersion = 4;

        GLFunctions_1_4() {
            majorVersion = 1;
            minorVersion = 4;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_1_4_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_1_3::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_1_4_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_1_5 : public GLFunctions_1_4 {
    public:
        static constexpr int requiredMajorVersion = 1;
        static constexpr int requiredMinorVersion = 5;

        GLFunctions_1_5() {
            majorVersion = 1;
            minorVersion = 5;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_1_5_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_1_4::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_1_5_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_2_0 : public GLFunctions_1_5 {
    public:
        static constexpr int requiredMajorVersion = 2;
        static constexpr int requiredMinorVersion = 0;

        GLFunctions_2_0() {
            majorVersion = 2;
            minorVersion = 0;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_2_0_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_1_5::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_2_0_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_2_1 : public GLFunctions_2_0 {
    public:
        static constexpr int requiredMajorVersion = 2;
        static constexpr int requiredMinorVersion = 1;

        GLFunctions_2_1() {
            majorVersion = 2;
            minorVersion = 1;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_2_1_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_2_0::load(cb);
            
            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_2_1_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_3_0 : public GLFunctions_2_1 {
    public:
        static constexpr int requiredMajorVersion = 3;
        static constexpr int requiredMinorVersion = 0;

        GLFunctions_3_0() {
            majorVersion = 3;
            minorVersion = 0;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_3_0_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_2_1::load(cb);
            
            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_3_0_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_3_1 : public GLFunctions_3_0 {
    public:
        static constexpr int requiredMajorVersion = 3;
        static constexpr int requiredMinorVersion = 1;

        GLFunctions_3_1() {
            majorVersion = 3;
            minorVersion = 1;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_3_1_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_3_0::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_3_1_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_3_2 : public GLFunctions_3_1 {
    public:
        static constexpr int requiredMajorVersion = 3;
        static constexpr int requiredMinorVersion = 2;

        GLFunctions_3_2() {
            majorVersion = 3;
            minorVersion = 2;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_3_2_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_3_1::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_3_2_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_3_3 : public GLFunctions_3_2 {
    public:
        static constexpr int requiredMajorVersion = 3;
        static constexpr int requiredMinorVersion = 3;

        GLFunctions_3_3() {
            majorVersion = 3;
            minorVersion = 3;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_3_3_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_3_2::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_3_3_PART
            #undef GL_PROCESS
        }
};
class GLFunctions_4_0 : public GLFunctions_3_3 {
    public:
        static constexpr int requiredMajorVersion = 4;
        static constexpr int requiredMinorVersion = 0;

        GLFunctions_4_0() {
            majorVersion = 4;
            minorVersion = 0;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_4_0_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_3_3::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_4_0_PART
            #undef GL_PROCESS
        }
};
class GLFunctions_4_1 : public GLFunctions_4_0 {
    public:
        static constexpr int requiredMajorVersion = 4;
        static constexpr int requiredMinorVersion = 1;

        GLFunctions_4_1() {
            majorVersion = 4;
            minorVersion = 1;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_4_1_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_4_0::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_4_1_PART
            #undef GL_PROCESS
        }
};
class GLFunctions_4_2 : public GLFunctions_4_1 {
    public:
        static constexpr int requiredMajorVersion = 4;
        static constexpr int requiredMinorVersion = 2;

        GLFunctions_4_2() {
            majorVersion = 4;
            minorVersion = 2;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_4_2_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_4_1::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_4_2_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_4_3 : public GLFunctions_4_2 {
    public:
        static constexpr int requiredMajorVersion = 4;
        static constexpr int requiredMinorVersion = 3;

        GLFunctions_4_3() {
            majorVersion = 4;
            minorVersion = 3;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_4_3_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_4_2::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_4_3_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_4_4 : public GLFunctions_4_3 {
    public:
        static constexpr int requiredMajorVersion = 4;
        static constexpr int requiredMinorVersion = 4;

        GLFunctions_4_4() {
            majorVersion = 4;
            minorVersion = 4;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_4_4_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_4_3::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_4_4_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_4_5 : public GLFunctions_4_4 {
    public:
        static constexpr int requiredMajorVersion = 4;
        static constexpr int requiredMinorVersion = 5;

        GLFunctions_4_5() {
            majorVersion = 4;
            minorVersion = 5;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_4_5_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_4_4::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_4_5_PART
            #undef GL_PROCESS
        }
};

class GLFunctions_4_6 : public GLFunctions_4_5 {
    public:
        static constexpr int requiredMajorVersion = 4;
        static constexpr int requiredMinorVersion = 6;

        GLFunctions_4_6() {
            majorVersion = 4;
            minorVersion = 6;
        }

        #define GL_PROCESS(name, pfn) ::pfn name = nullptr;
        GL_FUNCTIONS_4_6_PART
        #undef GL_PROCESS

        template <typename Callable>
        void load(Callable &&cb) {
            GLFunctions_4_5::load(cb);

            #define GL_PROCESS(name, pfn) name = reinterpret_cast<pfn>(cb(#name));
            GL_FUNCTIONS_4_6_PART
            #undef GL_PROCESS
        }
};

/**
 * @brief Loads the appropriate GLFunctions based on the OpenGL version.
 *
 * @param cb The callback function used to retrieve the OpenGL functions.
 *
 * @return A Box object containing the loaded GLFunctions.
 *
 * @throws None
 */
template <typename Callable>
Box<GLFunctions> LoadGLFunctions(Callable &&cb) {
    auto glGetIntegerv = (::PFNGLGETINTEGERVPROC) cb("glGetIntegerv");
    if (!glGetIntegerv) {
        return Box<GLFunctions>();
    }

    GLint majorVersion = 0;
    GLint minorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

    // Load from 4.6 to ... 1.0
    if (majorVersion == 4 && minorVersion >= 6) {
        auto fn = std::make_unique<GLFunctions_4_6>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 4 && minorVersion >= 5) {
        auto fn = std::make_unique<GLFunctions_4_5>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 4 && minorVersion >= 4) {
        auto fn = std::make_unique<GLFunctions_4_4>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 4 && minorVersion >= 3) {
        auto fn = std::make_unique<GLFunctions_4_3>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 4 && minorVersion >= 2) {
        auto fn = std::make_unique<GLFunctions_4_2>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 4 && minorVersion >= 1) {
        auto fn = std::make_unique<GLFunctions_4_1>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 4 && minorVersion >= 0) {
        auto fn = std::make_unique<GLFunctions_4_0>();
        fn->load(cb);
        return fn;
    }
    // 4.0 End 3.3 begin
    if (majorVersion == 3 && minorVersion >= 3) {
        auto fn = std::make_unique<GLFunctions_3_3>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 3 && minorVersion >= 2) {
        auto fn = std::make_unique<GLFunctions_3_2>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 3 && minorVersion >= 1) {
        auto fn = std::make_unique<GLFunctions_3_1>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 3 && minorVersion >= 0) {
        auto fn = std::make_unique<GLFunctions_3_0>();
        fn->load(cb);
        return fn;
    }
    // 3.0 End 2.1 begin
    if (majorVersion == 2 && minorVersion >= 1) {
        auto fn = std::make_unique<GLFunctions_2_1>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 2 && minorVersion >= 0) {
        auto fn = std::make_unique<GLFunctions_2_0>();
        fn->load(cb);
        return fn;
    }
    // 2.0 End 1.5 begin
    if (majorVersion == 1 && minorVersion >= 5) {
        auto fn = std::make_unique<GLFunctions_1_5>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 1 && minorVersion >= 4) {
        auto fn = std::make_unique<GLFunctions_1_4>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 1 && minorVersion >= 3) {
        auto fn = std::make_unique<GLFunctions_1_3>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 1 && minorVersion >= 2) {
        auto fn = std::make_unique<GLFunctions_1_2>();
        fn->load(cb);
        return fn;
    }
    if (majorVersion == 1 && minorVersion >= 1) {
        auto fn = std::make_unique<GLFunctions_1_1>();
        fn->load(cb);
        return fn;
    }
    // 1.5 End 1.0 begin
    if (majorVersion == 1 && minorVersion >= 0) {
        auto fn = std::make_unique<GLFunctions_1_0>();
        fn->load(cb);
        return fn;
    }
    // 1.0 End
    return Box<GLFunctions>();
}

NEKO_NS_END