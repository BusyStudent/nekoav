add_rules("mode.debug", "mode.release")

set_languages("c++17")

target("nekoav")
    set_kind("shared")

    add_files("nekoav/*.cpp")
target_end()