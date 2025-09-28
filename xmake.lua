add_rules("mode.debug", "mode.release")
set_languages("c++23")

-- import async runtime
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
add_requires("ilias")

-- import ffmpeg
add_requires("ffmpeg")

-- for test
add_requires("gtest")

-- set project
add_includedirs("include")

-- update the compile_commands.json for clangd
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd", outputdir = ".vscode"})

target("nekoav")
    add_packages("ilias", {public = true})
    add_packages("ffmpeg")
    set_kind("shared")
    add_files("src/*.cpp")
target_end()

target("test_core")
    add_packages("ffmpeg")
    add_packages("gtest")
    set_kind("binary")
    add_deps("nekoav")
    add_files("test/core.cpp")