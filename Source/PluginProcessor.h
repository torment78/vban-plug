// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "NetworkEngine.h"
#include "ReceiveBuffer.h"
#include "Metering.h"
#include <juce_audio_processors/juce_audio_processors.h>

class VbanProcessor final : public juce::AudioProcessor {
public:
    explicit VbanProcessor(vband::Mode);
    ~VbanProcessor() override = default;
    const juce::String getName() const override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    vband::Settings settings() const { return network.settings(); }
    juce::String apply(vband::Settings);
    juce::String status() const;
    int meterChannels() const noexcept;
    bool isReceiver() const noexcept { return mode == vband::Mode::receive; }
    vband::NetworkEngine& engine() noexcept { return network; }
    vband::MeterState& meterState() noexcept { return levels; }
private:
    template<class Sample> void process(juce::AudioBuffer<Sample>&, juce::MidiBuffer&);
    template<class Sample> void bypass(juce::AudioBuffer<Sample>&, juce::MidiBuffer&);
    const vband::Mode mode;
    vband::NetworkEngine network;
    vband::ReceiveBuffer receiveBuffer;
    vband::MeterState levels;
    static constexpr int scratchFrames = 256;
    juce::AudioBuffer<float> receiveScratch {vband::maxChannels, scratchFrames};
    std::uint32_t audioGeneration = 0;
    std::atomic<int> hostRate {48000};
    int formatBits = 0, formatChannels = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VbanProcessor)
};
