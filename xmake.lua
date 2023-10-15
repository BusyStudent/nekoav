add_rules("mode.debug", "mode.release")

add_requires("opencl-headers", "vulkan-headers", "ffmpeg", "gtest")
add_packages("opencl-headers", "vulkan-headers")

set_languages("c++17")

target("nekoav")
    set_kind("shared")
    add_packages("ffmpeg")

    if is_plat("windows") then
        add_links("user32")
    end

    if has_package("ffmpeg") then 
        add_files("nekoav/ffmpeg/*.cpp")
    end

    add_files("nekoav/*.cpp")
target_end()

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