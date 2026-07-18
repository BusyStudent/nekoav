/**
 * @file test_main.cpp
 * @brief Shared gtest entry for all nekoav test binaries.
 *
 * Installs an Ilias PlatformContext before RUN_ALL_TESTS so ILIAS_TEST
 * coroutines and element IoTask paths have a runtime.
 */

#include <gtest/gtest.h>
#include <ilias/platform.hpp>

auto main(int argc, char **argv) -> int {
    ::ilias::PlatformContext context;
    ::testing::InitGoogleTest(&argc, argv);
    context.install();
    return RUN_ALL_TESTS();
}
