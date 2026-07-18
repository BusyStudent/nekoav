/**
 * @file core.cpp
 * @brief Pure unit tests: time bases, pixel format names, Value container.
 *
 * No Element graph, no Ilias async — plain gtest. Depends on FFmpeg only for
 * the internal time::toFFmpeg / fromFFmpeg helpers.
 */

#include <nekoav/caps.hpp>
#include <nekoav/format.hpp>
#include <gtest/gtest.h>
#include "../../src/ffmpeg.hpp"

using namespace nekoav;

// MARK: Time

TEST(TimeTest, Convert) {
    // 1 ms in a 1/1000 time base must round-trip as FFmpeg timestamp 1.
    Rational timeBase {1, 1000};
    AVRational ffTimeBase {timeBase.num, timeBase.den};
    auto input = std::chrono::nanoseconds(1'000'000);

    auto timestamp = time::toFFmpeg(input, ffTimeBase);
    auto output = time::fromFFmpeg(timestamp, ffTimeBase);

    EXPECT_EQ(timestamp, 1);
    EXPECT_EQ(output, input);
}

// MARK: Format

TEST(FormatTest, GetName) {
    // Public toString must match FFmpeg's pix_fmt name (caps / debug / logs).
    EXPECT_EQ(toString(PixelFormat::YUV420P), "yuv420p");
}

// MARK: Value

TEST(ValueTest, StoreValues) {
    auto string = Value {"HelloWorld"};
    auto list = Value::fromList({1, 2, 3, "String"});
    auto map = Value::fromMap({{"Key", 1}});

    EXPECT_NE(string, Value {});
    EXPECT_TRUE(list.isList());
    EXPECT_TRUE(map.isMap());
    EXPECT_EQ(map["Key"], 1);
    EXPECT_TRUE(map["Not a Key"].isNull());
}
