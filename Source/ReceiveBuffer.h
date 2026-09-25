// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "VbanProtocol.h"
#include <algorithm>
#include <array>
#include <memory>

namespace vband {
// Audio-thread-owned elastic buffer. All channels share the same read clock.
class ReceiveBuffer {
public:
    void reset() noexcept { read = write = 0; fraction = 0; playing = false; correction = 0; channelCount = 0; }
    void append(const AudioPacket& p) noexcept {
        if (p.channels < 1 || p.channels > maxChannels || p.frames < 1 || p.frames > maxFrames(p.channels, p.bits)) return;
        if (p.discontinuity || p.channels != channelCount || write - read + static_cast<unsigned>(p.frames) >= capacity) reset();
        channelCount = p.channels;
        for (int i = 0; i < p.frames; ++i) {
            auto& frame = samples[(write++) % capacity];
            frame.fill(0.0f);
            for (int c = 0; c < p.channels; ++c)
                frame[static_cast<std::size_t>(c)] = p.samples[static_cast<std::size_t>(i * p.channels + c)];
        }
    }
    // The caller clears outputs first, so startup/underrun tails remain silent.
    // hostBlockSize keeps the latency target independent of the scratch-buffer chunk size.
    bool render(float* const* outputs, int channels, int count, int rate, int hostBlockSize) noexcept {
        const auto target = static_cast<unsigned>(std::clamp(std::max(rate / 50, hostBlockSize * 2), 512, 8192));
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
            for (int c = 0; c < std::min(channels, maxChannels); ++c)
                outputs[c][i] = a[static_cast<std::size_t>(c)] + (b[static_cast<std::size_t>(c)] - a[static_cast<std::size_t>(c)]) * f;
            fraction += 1.0 + correction;
            const auto step = static_cast<unsigned>(fraction);
            read += step; fraction -= step;
        }
        return true;
    }
private:
    static constexpr unsigned capacity = 32768;
    using Frame = std::array<float, maxChannels>;
    // Allocate once when the processor is created, never during audio processing.
    std::unique_ptr<Frame[]> samples {std::make_unique<Frame[]>(capacity)};
    std::uint64_t read = 0, write = 0;
    double fraction = 0, correction = 0;
    bool playing = false;
    int channelCount = 0;
};
}
