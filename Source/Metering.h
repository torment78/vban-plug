// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "VbanProtocol.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace vband {
struct MeterSnapshot {
    std::array<float, maxChannels> rms {}, peak {};
    std::uint32_t timestamp = 0;
    int channels = 0;
};

// Audio-thread-owned envelope; the editor reads lock-free publications only.
class MeterState {
public:
    static_assert(std::atomic<float>::is_always_lock_free, "Meter publication must be lock-free");
    MeterState() noexcept { reset(); }
    void prepare(double rate) noexcept { sampleRate = std::max(1.0, rate); reset(); }
    void reset() noexcept {
        power.fill(0.0);
        for (auto& value : rms) value.store(0.0f, std::memory_order_relaxed);
        for (auto& value : peak) value.store(0.0f, std::memory_order_relaxed);
        activeChannels.store(0, std::memory_order_relaxed);
        timestamp.store(0, std::memory_order_release);
    }
    template<class Reader>
    void measureSamples(int channels, int count, std::uint32_t now, Reader sample) noexcept {
        channels = std::clamp(channels, 0, maxChannels);
        if (count <= 0 || channels == 0) { reset(); return; }
        if (channels != activeChannels.load(std::memory_order_relaxed)) reset();
        const double retain = std::exp(-double(count) / (sampleRate * 0.05));
        for (int c = 0; c < channels; ++c) {
            const auto index = static_cast<std::size_t>(c);
            double energy = 0.0;
            float maximum = 0.0f;
            for (int i = 0; i < count; ++i) {
                const double raw = static_cast<double>(sample(c, i));
                const double value = std::isfinite(raw) ? std::min(4.0, std::abs(raw)) : 0.0;
                energy += value * value;
                maximum = std::max(maximum, static_cast<float>(value));
            }
            power[index] = retain * power[index] + (1.0 - retain) * energy / count;
            rms[index].store(static_cast<float>(std::sqrt(power[index])), std::memory_order_relaxed);
            auto previous = peak[index].load(std::memory_order_relaxed);
            for (int attempt = 0; attempt < 2 && maximum > previous; ++attempt)
                if (peak[index].compare_exchange_strong(previous, maximum, std::memory_order_relaxed)) break;
        }
        activeChannels.store(channels, std::memory_order_relaxed);
        timestamp.store(now, std::memory_order_release);
    }
    template<class Sample>
    void measure(const juce::AudioBuffer<Sample>& audio, std::uint32_t now) noexcept {
        measureSamples(audio.getNumChannels(), audio.getNumSamples(), now,
            [&audio](int c, int i) { return audio.getSample(c, i); });
    }
    MeterSnapshot read() noexcept {
        MeterSnapshot result;
        result.timestamp = timestamp.load(std::memory_order_acquire);
        result.channels = activeChannels.load(std::memory_order_relaxed);
        for (std::size_t c = 0; c < maxChannels; ++c) {
            result.rms[c] = rms[c].load(std::memory_order_relaxed);
            result.peak[c] = peak[c].exchange(0.0f, std::memory_order_relaxed);
        }
        return result;
    }
private:
    double sampleRate = 48000.0;
    std::array<double, maxChannels> power {};
    std::array<std::atomic<float>, maxChannels> rms {}, peak {};
    std::atomic<std::uint32_t> timestamp {0};
    std::atomic<int> activeChannels {0};
};
}
