add_rules("mode.debug", "mode.release")

add_requires("opencl-headers", "vulkan-headers", "ffmpeg", "gtest", "miniaudio")
add_packages("opencl-headers", "vulkan-headers", "miniaudio")

set_languages("c++20")

-- Configureable Option
option("qt_test")
   set_default(false)
   set_showmenu(true)
option_end()

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

if is_plat("windows") then 
    add_cxxflags("cl::/utf-8")
    add_cxxflags("cl::/Zc:__cplusplus")
    add_cxxflags("cl::/permissive-")
end

-- Core
target("nekoav")
    set_kind("shared")
    set_languages("c++20")

    if is_plat("windows") then
        add_links("user32")
        add_defines("NOMINMAX")
    end

    if has_package("miniaudio") then
        add_files("nekoav/elements/audiodev/miniaudio.cpp")
    end

    -- OpenGL
    if has_config("opengl") then 
        add_files("nekoav/opengl/*.cpp")
    end

    -- FFmpeg
    if has_package("ffmpeg") then 
        add_packages("ffmpeg")
        add_files("nekoav/ffmpeg/*.cpp")
    end

    -- detail
    add_files("nekoav/detail/*.cpp")

    add_files("nekoav/*.cpp")
    add_files("nekoav/elements/*.cpp")
target_end()

-- Qt
if has_config("qt_interop") then 
    target("nekoav_qt")
        add_rules("qt.shared")
        set_languages("c++20")

        add_deps("nekoav")
        add_defines("_QNEKO_SOURCE")
        add_frameworks("QtCore", "QtGui", "QtWidgets")

        add_files("nekoav/interop/qnekoav.cpp")
        add_files("nekoav/interop/qnekoav.hpp")
    target_end()
end

target("utilstest")
    set_kind("binary")
    add_deps("nekoav")

    add_files("tests/utilstest.cpp")
target_end()

target("cltest")
    set_kind("binary")
    add_deps("nekoav")

    add_files("tests/cltest.cpp")
target_end()

target("coretest")
    set_kind("binary")
    add_deps("nekoav")
    add_packages("gtest")

    add_files("tests/coretest.cpp")
target_end()

target("basetest")
    set_kind("binary")
    add_deps("nekoav")
    add_packages("gtest")

    add_files("tests/basetest.cpp")
target_end()

target("elemtest")
    set_kind("binary")
    add_deps("nekoav")
    add_packages("gtest")

    add_files("tests/elemtest.cpp")
target_end()

-- Gui Test
if has_config("qt_test") and has_config("qt_interop") then 
    target("qtest")
        add_rules("qt.widgetapp")
        add_deps("nekoav", "nekoav_qt")
        add_packages("ffmpeg")

        add_frameworks("QtCore", "QtGui")
        add_files("tests/qtest.cpp")
    target_end()
end