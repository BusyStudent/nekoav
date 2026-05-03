add_rules("mode.debug", "mode.release")
set_languages("c++23")

-- import async runtime
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
add_requires("ilias", {configs = {coro_trace = true}})

-- import ffmpeg
add_requires("ffmpeg")

-- Import miniaudio
add_requires("miniaudio")

-- for test
add_requires("gtest")

-- set project
add_includedirs("include")

-- update the compile_commands.json for clangd
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd", outputdir = ".vscode"})
set_encodings("utf-8")

target("nekoav")
    add_packages("ilias", {public = true})
    add_packages("ffmpeg", "miniaudio")
    set_kind("shared")
    
    add_includedirs("src")
    add_defines("_NEKOAV_SOURCE")
    add_files("src/**.cpp")
target_end()

target("test_core")
    add_packages("ffmpeg")
    add_packages("gtest")
    set_kind("binary")
    add_deps("nekoav")
    add_files("test/core.cpp")

target("test_elements")
    add_packages("gtest")
    set_kind("binary")
    add_deps("nekoav")
    add_files("test/elements.cpp")

target("player")
    add_rules("qt.widgetapp")
    add_packages("ilias")
    add_deps("nekoav")

    add_files("example/player.cpp")
    add_files("example/player.ui")
    add_files("example/shaders.qrc")
    add_frameworks("QtGui", "QtGuiPrivate")