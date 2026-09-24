// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace vband {
struct MeterSnapshot {
    std::array<float, 2> rms {}, peak {};
    std::uint32_t timestamp = 0;
};

// The audio thread measures samples; the editor only reads lock-free atomics.
class MeterState {
public:
    static_assert(std::atomic<float>::is_always_lock_free, "Meter publication must be lock-free");
    MeterState() noexcept { reset(); }
    void prepare(double rate) noexcept { sampleRate = std::max(1.0, rate); reset(); }
    void reset() noexcept {
        power.fill(0.0);
        for (auto& value : rms) value.store(0.0f, std::memory_order_relaxed);
        for (auto& value : peak) value.store(0.0f, std::memory_order_relaxed);
        timestamp.store(0, std::memory_order_release);
    }
    template<class Sample>
    void measure(const juce::AudioBuffer<Sample>& audio, std::uint32_t now) noexcept {
        const int count = audio.getNumSamples();
        if (count == 0 || audio.getNumChannels() == 0) { reset(); return; }
        // A 50 ms RMS envelope; peak capture below preserves even one-sample transients.
        const double retain = std::exp(-double(count) / (sampleRate * 0.05));
        for (std::size_t c = 0; c < 2; ++c) {
            const auto* input = audio.getReadPointer(std::min(int(c), audio.getNumChannels() - 1));
            double energy = 0.0;
            float maximum = 0.0f;
            for (int i = 0; i < count; ++i) {
                const double raw = static_cast<double>(input[i]);
                const double value = std::isfinite(raw) ? std::min(4.0, std::abs(raw)) : 0.0;
                energy += value * value;
                maximum = std::max(maximum, static_cast<float>(value));
            }
            power[c] = retain * power[c] + (1.0 - retain) * energy / count;
            rms[c].store(static_cast<float>(std::sqrt(power[c])), std::memory_order_relaxed);
            // Bounded retries: the UI may clear a latched peak once between these operations.
            auto previous = peak[c].load(std::memory_order_relaxed);
            for (int attempt = 0; attempt < 2 && maximum > previous; ++attempt)
                if (peak[c].compare_exchange_strong(previous, maximum, std::memory_order_relaxed)) break;
        }
        timestamp.store(now, std::memory_order_release);
    }
    MeterSnapshot read() noexcept {
        MeterSnapshot result;
        result.timestamp = timestamp.load(std::memory_order_acquire);
        for (std::size_t c = 0; c < 2; ++c) {
            result.rms[c] = rms[c].load(std::memory_order_relaxed);
            result.peak[c] = peak[c].exchange(0.0f, std::memory_order_relaxed);
        }
        return result;
    }
private:
    double sampleRate = 48000.0;
    std::array<double, 2> power {};
    std::array<std::atomic<float>, 2> rms {}, peak {};
    std::atomic<std::uint32_t> timestamp {0};
};
}
