// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "VbanProtocol.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace vband {
int rateIndex(int rate) noexcept {
    for (int i = 0; i < static_cast<int>(sampleRates.size()); ++i)
        if (sampleRates[static_cast<std::size_t>(i)] == rate) return i;
    return -1;
}
bool validName(const std::string& name) noexcept {
    return !name.empty() && name.size() <= 16 && std::all_of(name.begin(), name.end(),
        [](unsigned char c) { return c >= 32 && c <= 126; });
}
bool validAddress(const std::string& ip) noexcept {
    unsigned octets[4] {}, part = 0, digits = 0;
    for (char c : ip) {
        if (c == '.') { if (!digits || part == 3) return false; ++part; digits = 0; }
        else if (c >= '0' && c <= '9') {
            if (++digits > 3) return false;
            octets[part] = octets[part] * 10 + static_cast<unsigned>(c - '0');
            if (octets[part] > 255) return false;
        } else return false;
    }
    return part == 3 && digits && octets[0] > 0 && octets[0] < 224;
}
std::array<char, 16> streamName(const std::string& name) noexcept {
    std::array<char, 16> result {};
    std::copy_n(name.data(), std::min(name.size(), result.size()), result.data());
    return result;
}
std::size_t encode(const AudioPacket& p, std::array<std::uint8_t, maxDatagram>& out) noexcept {
    const auto index = rateIndex(p.rate);
    if (index < 0 || p.frames < 1 || p.frames > 256 || p.channels < 1 || p.channels > maxChannels
        || (p.bits != 16 && p.bits != 24)) return 0;
    const int width = p.bits / 8;
    const auto size = headerSize + static_cast<std::size_t>(p.frames * p.channels * width);
    if (size > out.size()) return 0;
    std::memcpy(out.data(), "VBAN", 4);
    out[4] = static_cast<std::uint8_t>(index);
    out[5] = static_cast<std::uint8_t>(p.frames - 1);
    out[6] = static_cast<std::uint8_t>(p.channels - 1);
    out[7] = static_cast<std::uint8_t>(width - 1);
    std::memcpy(out.data() + 8, p.name.data(), 16);
    for (int b = 0; b < 4; ++b) out[24 + b] = static_cast<std::uint8_t>(p.sequence >> (b * 8));
    auto* dest = out.data() + headerSize;
    const double scale = p.bits == 16 ? 32768.0 : 8388608.0;
    for (int i = 0; i < p.frames * p.channels; ++i) {
        const double sample = std::isfinite(p.samples[static_cast<std::size_t>(i)])
            ? p.samples[static_cast<std::size_t>(i)] : 0.0;
        const auto value = static_cast<std::int32_t>(std::llround(std::clamp(sample, -1.0, 1.0 - 1.0 / scale) * scale));
        const auto raw = static_cast<std::uint32_t>(value);
        for (int b = 0; b < width; ++b) *dest++ = static_cast<std::uint8_t>(raw >> (b * 8));
    }
    return size;
}
bool decode(const std::uint8_t* data, std::size_t size, AudioPacket& out) noexcept {
    if (size < headerSize || size > maxDatagram || std::memcmp(data, "VBAN", 4) != 0
        || data[4] >= sampleRates.size() || (data[7] != 1 && data[7] != 2) || data[6] >= maxChannels) return false;
    const int frames = data[5] + 1, channels = data[6] + 1, width = data[7] + 1;
    if (size != headerSize + static_cast<std::size_t>(frames * channels * width)) return false;
    out.rate = sampleRates[data[4]]; out.frames = frames; out.channels = channels; out.bits = width * 8;
    std::memcpy(out.name.data(), data + 8, 16);
    // Stream names end at the first NUL; ignore padding from other implementations.
    bool ended = false;
    for (auto& c : out.name) { ended = ended || c == 0; if (ended) c = 0; }
    out.sequence = 0;
    for (int b = 0; b < 4; ++b) out.sequence |= std::uint32_t(data[24 + b]) << (b * 8);
    const auto* src = data + headerSize;
    const auto sign = std::uint32_t(1) << (width * 8 - 1);
    for (int i = 0; i < frames * channels; ++i) {
        std::uint32_t raw = 0;
        for (int b = 0; b < width; ++b) raw |= std::uint32_t(*src++) << (b * 8);
        const auto value = (raw & sign) ? static_cast<std::int32_t>(std::int64_t(raw) - (std::int64_t(1) << (width * 8)))
                                        : static_cast<std::int32_t>(raw);
        out.samples[static_cast<std::size_t>(i)] = static_cast<float>(value) / static_cast<float>(sign);
    }
    return true;
}
}
