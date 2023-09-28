add_rules("mode.debug", "mode.release")

set_languages("c++17")

target("nekoav")
    set_kind("shared")

    if is_plat("windows") then
        add_links("user32")
    end

    add_files("nekoav/*.cpp")
target_end()

target("utilstest")
    set_kind("binary")
    add_deps("nekoav")

    add_files("tests/utilstest.cpp")
target_end()