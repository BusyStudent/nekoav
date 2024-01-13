
add_requires("opencl", "vulkan-headers", "ffmpeg", "gtest", "miniaudio")
add_packages("opencl", "vulkan-headers", "miniaudio")

set_languages("c++20")

-- Configureable Option
option("qt_interop")
   set_default(false)
   set_showmenu(true)
   set_description("Qt support for nekoav")
option_end()

option("qt_opengl")
    set_default(true)
    set_showmenu(true)
    set_description("Use OpenGL for video output")
option_end()

option("openmp")
    set_default(false)
    set_showmenu(true)
    set_description("OpenMP support")
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
        add_links("user32", "gdi32")
        add_defines("NOMINMAX")
    end

    if is_mode("release") then 
        add_defines("NEKO_NO_LOG")
    end

    if has_package("miniaudio") then
        add_files("elements/audiodev/miniaudio.cpp")
    end

    -- OpenMP
    if has_config("openmp") then 
        add_rules("c++.openmp")
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

    -- Compute
    add_files("compute/*.cpp")

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

        if has_config("qt_opengl") then 
            add_defines("QNEKO_HAS_OPENGL")
            add_frameworks("QtOpenGL", "QtOpenGLWidgets")
        end

        add_files("interop/qnekoav*.cpp")
        add_files("interop/qnekoav.hpp")
    target_end()
end
