// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "Metering.h"
#include <juce_gui_basics/juce_gui_basics.h>

class StereoMeter final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    StereoMeter(vband::MeterState&, bool receiver);
    ~StereoMeter() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void setPacketCount(std::uint64_t);
private:
    void timerCallback() override;
    static float position(float db) noexcept { return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f); }
    vband::MeterState& source;
    const bool receiving;
    juce::String packetText;
    std::array<float, 2> levelDb {-60.0f, -60.0f}, peakDb {-60.0f, -60.0f};
    std::array<float, 2> holdSeconds {}, clipSeconds {};
    std::uint32_t lastTick = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoMeter)
};
