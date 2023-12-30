
add_requires("opencl", "vulkan-headers", "ffmpeg", "gtest", "miniaudio")
add_packages("opencl", "vulkan-headers", "miniaudio")

set_languages("c++20")

-- Configureable Option
option("qt_interop")
   set_default(false)
   set_showmenu(true)
   set_description("Qt support for nekoav")
option_end()

option("opengl")
    set_default(false)
    set_showmenu(true)
    set_description("OpenGL support")
option_end()

option("subtitle")
    set_default(false)
    set_showmenu(true)
    set_description("ASS / SSA Subtitle support")
option_end()

if is_plat("windows") then 
    add_cxxflags("cl::/utf-8")
    add_cxxflags("cl::/Zc:__cplusplus")
    add_cxxflags("cl::/permissive-")
end

if has_config("subtitle") then
    add_requires("libass")
end 


-- Core
target("nekoav")
    set_kind("shared")
    set_languages("c++20")
    -- set_warnings("all")
    -- set_warnings("error")

    if is_plat("windows") then
        add_links("user32")
        add_defines("NOMINMAX")
    end

    if is_mode("release") then 
        add_defines("NEKO_NO_LOG")
    end

    if has_package("miniaudio") then
        add_files("elements/audiodev/miniaudio.cpp")
    end

    -- OpenGL
    if has_config("opengl") then 
        add_files("opengl/*.cpp")
    end

    -- FFmpeg
    if has_package("ffmpeg") then 
        add_packages("ffmpeg")
        add_files("ffmpeg/*.cpp")
    end

    -- Subtitle
    if has_config("subtitle") then
        add_packages("libass")
    end 

    -- Elements
    add_files("elements/*.cpp")

    -- Detail
    add_files("detail/*.cpp")

    -- Media
    add_files("media/*.cpp")

    -- Core
    add_files("*.cpp")
target_end()

-- Qt
if has_config("qt_interop") then 
    target("nekoav_qt")
        add_rules("qt.shared")
        set_languages("c++20")

        add_deps("nekoav")
        add_defines("_QNEKO_SOURCE")
        add_frameworks("QtCore", "QtGui", "QtWidgets")

        if is_plat("linux") then 
            add_defines("QNEKO_HAS_OPENGL")
            add_frameworks("QtOpenGL", "QtOpenGLWidgets")
        end

        add_files("interop/qnekoav*.cpp")
        add_files("interop/qnekoav.hpp")
    target_end()
end
