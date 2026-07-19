#pragma once

/**
 * @file sample_factory.hpp
 * @brief Helpers to build Sample values for tests without a real demuxer.
 */

#include <nekoav/sample.hpp>
#include <new>

extern "C" {
    #include <libavcodec/packet.h>
}

namespace nekoav::testing {

// TODO: Maybe we put it to the public Packet API?

/**
 * @brief Allocate an empty AVPacket-backed Sample with @p pts (time base 1/1000).
 *
 * Used by Queue integration tests where only ordering / PTS matter, not payload.
 */
inline auto makePacketSample(Timestamp pts) -> Sample {
    auto packet = av_packet_alloc();
    if (!packet) {
        throw std::bad_alloc{};
    }
    Sample sample {
        Packet {packet, Rational{1, 1000}}
    };
    sample.setPts(pts);
    return sample;
}

} // namespace nekoav::testing
