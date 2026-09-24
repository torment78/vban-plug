// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "PluginProcessor.h"
#include "StereoMeter.h"
#include <juce_gui_basics/juce_gui_basics.h>
class VbanEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VbanEditor(VbanProcessor&);
    ~VbanEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void applySettings();
    void loadSettings();
    VbanProcessor& processor;
    juce::LookAndFeel_V4 theme;
    StereoMeter meters;
    juce::Label title, subtitle, addressLabel, portLabel, nameLabel, bitsLabel, channelsLabel, statusLabel, note, errorLabel;
    juce::TextEditor address, port, stream;
    juce::ComboBox bits, channels;
    juce::ToggleButton enabled {"Enable network audio"};
    juce::TextButton applyButton {"Apply settings"};
    std::uint32_t shownGeneration = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VbanEditor)
};
