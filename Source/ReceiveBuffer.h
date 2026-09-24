// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "VbanProtocol.h"
#include <algorithm>
#include <array>

namespace vband {
// Audio-thread-owned elastic buffer. Nominal sender and host sample rates must match.
class ReceiveBuffer {
public:
    void reset() noexcept { read = write = 0; fraction = 0; playing = false; correction = 0; }
    void append(const AudioPacket& p) noexcept {
        if (p.discontinuity || write - read + static_cast<unsigned>(p.frames) >= capacity) reset();
        for (int i = 0; i < p.frames; ++i) {
            auto& frame = samples[(write++) % capacity];
            frame[0] = p.samples[static_cast<std::size_t>(i * p.channels)];
            frame[1] = p.samples[static_cast<std::size_t>(i * p.channels + p.channels - 1)];
        }
    }
    template<class Sample> bool render(Sample* left, Sample* right, int count, int rate) noexcept {
        const auto target = static_cast<unsigned>(std::clamp(std::max(rate / 50, count * 2), 512, 8192));
        if (!playing && write - read >= target + 2) playing = true;
        if (!playing) return false;
        if (write - read > target * 3) { read = write - target; fraction = 0; }
        const double error = (double(write - read) - double(target)) / double(target);
        correction += 0.002 * (std::clamp(error * 0.002, -0.002, 0.002) - correction);
        for (int i = 0; i < count; ++i) {
            if (write - read < 2) { reset(); return false; }
            const auto& a = samples[read % capacity];
            const auto& b = samples[(read + 1) % capacity];
            const auto f = static_cast<float>(fraction);
            const float l = a[0] + (b[0] - a[0]) * f, r = a[1] + (b[1] - a[1]) * f;
            left[i] = right ? l : (l + r) * 0.5f;
            if (right) right[i] = r;
            fraction += 1.0 + correction;
            const auto step = static_cast<unsigned>(fraction);
            read += step; fraction -= step;
        }
        return true;
    }
private:
    static constexpr unsigned capacity = 32768;
    std::array<std::array<float, 2>, capacity> samples {};
    std::uint64_t read = 0, write = 0;
    double fraction = 0, correction = 0;
    bool playing = false;
};
}
