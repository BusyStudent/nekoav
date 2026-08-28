-- Test targets for nekoav.
--
-- Layout (see README.md):
--   support/     shared main + ProbeElement fixtures
--   unit/        offline pure / runtime unit tests
--   integration/ offline media + Queue
--   network/     optional public-network smoke (opt-in)
--
-- Each binary reuses support/test_main.cpp (Ilias PlatformContext + gtest).
-- rundir is the project root so paths like test/fixtures/av_1s.mkv resolve.

local function add_nekoav_test_target(name, group, timeout)
    target(name)
        set_kind("binary")
        set_default(false)
        set_group("tests")
        set_rundir(os.projectdir())
        add_deps("nekoav")
        add_packages("gtest")
        add_includedirs("./")
        add_files("support/test_main.cpp")
        add_tests("default", {group = group, timeout = timeout})
    target_end()
end

add_nekoav_test_target("test_unit_core", "unit", 10)
target("test_unit_core")
    add_packages("ffmpeg")
    add_files("unit/core.cpp")

add_nekoav_test_target("test_unit_runtime", "unit", 10)
target("test_unit_runtime")
    add_packages("ffmpeg")
    add_files("unit/runtime.cpp")

add_nekoav_test_target("test_integration", "integration", 30)
target("test_integration")
    add_packages("ffmpeg")
    add_files("integration/*.cpp")

if has_config("network_tests") then
    add_nekoav_test_target("test_network", "network", 90)
    target("test_network")
        -- Relative to this file (test/), not project root.
        add_files("network/*.cpp")
end
