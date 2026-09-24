// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <string>

namespace vband {
inline constexpr std::size_t headerSize = 28, maxDatagram = 1464;
inline constexpr std::array<int, 21> sampleRates {
    6000,12000,24000,48000,96000,192000,384000,
    8000,16000,32000,64000,128000,256000,512000,
    11025,22050,44100,88200,176400,352800,705600 };
struct AudioPacket {
    std::array<float, 512> samples {}; // interleaved, up to 256 stereo frames
    std::array<char, 16> name {};
    std::uint32_t sequence = 0, generation = 0, timestamp = 0;
    int rate = 48000, frames = 0, channels = 2, bits = 24;
    bool discontinuity = false;
};
int rateIndex(int rate) noexcept;
bool validName(const std::string& name) noexcept;
bool validAddress(const std::string& ip) noexcept;
std::array<char, 16> streamName(const std::string& name) noexcept;
std::size_t encode(const AudioPacket&, std::array<std::uint8_t, maxDatagram>&) noexcept;
bool decode(const std::uint8_t*, std::size_t, AudioPacket&) noexcept;
}
