add_rules("mode.debug", "mode.release")

add_requires("opencl-headers", "vulkan-headers", "ffmpeg", "gtest", "miniaudio")
add_packages("opencl-headers", "vulkan-headers", "miniaudio")

set_languages("c++20")

-- Configureable Option
option("qt_test")
   set_default(false)
   set_showmenu(true)
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
    end

    if has_package("miniaudio") then
        add_files("nekoav/audio/miniaudio.cpp")
    end

    add_files("nekoav/*.cpp")
target_end()

-- FFmpeg
-- if has_package("ffmpeg") then 
--     target("nekoav_ffmpeg")
--         set_kind("shared")
--         set_languages("c++20")

--         add_deps("nekoav")
--         add_files("ffmpeg/*.cpp")
--     target_end()
-- end

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


-- Gui Test
-- if has_config("qt_test") then 
--     target("qtest")
--         add_rules("qt.widgetapp")
--         add_deps("nekoav")
--         add_packages("ffmpeg")

--         add_frameworks("QtCore", "QtGui")
--         add_files("tests/qtest.cpp")
--     target_end()
-- end