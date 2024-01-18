-- Test Option

set_languages("c++20")

if has_config("test") then
    add_requires("gtest")

    target("utilstest")
        set_kind("binary")
        add_deps("nekoav")

        add_files("utilstest.cpp")
    target_end()

    target("cltest")
        set_kind("binary")
        add_deps("nekoav")
        add_packages("opencl")

        add_files("cltest.cpp")
    target_end()

    target("coretest")
        set_kind("binary")
        add_deps("nekoav")
        add_packages("gtest")

        add_files("coretest.cpp")
    target_end()

    target("basetest")
        set_kind("binary")
        add_deps("nekoav")
        add_packages("gtest")

        add_files("basetest.cpp")
    target_end()

    target("elemtest")
        set_kind("binary")
        add_deps("nekoav")
        add_packages("gtest")

        add_files("elemtest.cpp")
    target_end()

    -- Gui Test
    if has_config("qt_interop") then 
        target("qtest")
            add_rules("qt.widgetapp")
            add_deps("nekoav", "nekoav_qt")

            add_frameworks("QtCore", "QtGui")
            add_files("qtest.cpp")
        target_end()
    end

    -- Gui Win32
    if is_plat("windows") then 
        target("winplay")
            set_kind("binary")
            add_deps("nekoav")

            add_files("winplay.cpp")
            add_links("user32", "shcore", "gdi32", "Comdlg32")
        target_end()
    end
end
