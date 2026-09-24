// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"
VbanEditor::VbanEditor(VbanProcessor& p) : AudioProcessorEditor(p), processor(p), meters(p.meterState(), p.isReceiver()) {
    theme.setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff101820));
    theme.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e2b36));
    theme.setColour(juce::TextEditor::textColourId, juce::Colour(0xffedf4f7));
    theme.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff344956));
    theme.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1e2b36));
    theme.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff137e76));
    theme.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff59d6b9));
    setLookAndFeel(&theme);
    for (auto* c : std::initializer_list<juce::Component*> { &title, &subtitle, &addressLabel, &portLabel, &nameLabel,
        &bitsLabel, &channelsLabel, &statusLabel, &note, &errorLabel, &address, &port, &stream, &bits, &channels, &enabled, &applyButton, &meters })
        addAndMakeVisible(c);
    title.setText(p.getName(), juce::dontSendNotification);
    title.setFont(juce::FontOptions(27.0f, juce::Font::bold));
    subtitle.setText(p.isReceiver() ? "One incoming stream. Straight into your host." : "Your audio passes through. A copy goes over VBAN.", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffa6bdc9));
    addressLabel.setText(p.isReceiver() ? "Sender IP address" : "Destination IP address", juce::dontSendNotification);
    portLabel.setText(p.isReceiver() ? "Local UDP port" : "Destination UDP port", juce::dontSendNotification);
    nameLabel.setText("Stream name", juce::dontSendNotification);
    bitsLabel.setText("PCM format", juce::dontSendNotification);
    channelsLabel.setText("Send channels", juce::dontSendNotification);
    bits.addItem("PCM 16-bit", 16); bits.addItem("PCM 24-bit", 24);
    channels.addItem("Mono", 1); channels.addItem("Stereo", 2);
    port.setInputRestrictions(5, "0123456789"); address.setInputRestrictions(15, "0123456789.");
    stream.setInputRestrictions(16);
    address.setTooltip("IPv4 address of the other computer. Use 127.0.0.1 for a local test.");
    stream.setTooltip("Exact case-sensitive VBAN stream name, 1 to 16 ASCII characters.");
    note.setText(p.isReceiver()
        ? "PCM 16/24-bit and mono/stereo are detected automatically.\nMatch the host sample rate to the sender. No signal outputs silence."
        : "Mono sends the average of left and right. Stereo sends both.\nHost audio stays unchanged. The stream uses the host sample rate.", juce::dontSendNotification);
    note.setColour(juce::Label::textColourId, juce::Colour(0xffa6bdc9));
    note.setFont(juce::FontOptions(13.0f));
    errorLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb690));
    errorLabel.setFont(juce::FontOptions(13.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff75e4c8));
    applyButton.onClick = [this] { applySettings(); };
    enabled.onClick = [this] { applySettings(); };
    address.onReturnKey = port.onReturnKey = stream.onReturnKey = [this] { applySettings(); };
    for (auto* c : std::initializer_list<juce::Component*> { &bits, &channels, &bitsLabel, &channelsLabel }) c->setVisible(!p.isReceiver());
    loadSettings();
    setSize(580, p.isReceiver() ? 504 : 574);
    timerCallback();
    startTimerHz(10); // Status/counters; StereoMeter repaints independently at 60 Hz.
}
VbanEditor::~VbanEditor() { stopTimer(); setLookAndFeel(nullptr); }
void VbanEditor::loadSettings() {
    const auto s = processor.settings();
    address.setText(s.address, false); port.setText(juce::String(s.port), false); stream.setText(s.name, false);
    bits.setSelectedId(s.bits, juce::dontSendNotification); channels.setSelectedId(s.channels, juce::dontSendNotification);
    enabled.setToggleState(s.enabled, juce::dontSendNotification);
    shownGeneration = vband::NetworkEngine::generation(processor.engine().control());
}
void VbanEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff101820));
    g.setColour(juce::Colour(0xff263945)); g.drawHorizontalLine(90, 24.0f, float(getWidth() - 24));
    g.setColour(juce::Colour(0xff16252e)); g.fillRoundedRectangle(24.0f, float(getHeight() - 57), float(getWidth() - 48), 34.0f, 6.0f);
}
void VbanEditor::resized() {
    title.setBounds(24, 17, 530, 37); subtitle.setBounds(24, 54, 535, 24);
    addressLabel.setBounds(24, 109, 170, 30); address.setBounds(205, 109, 345, 32);
    portLabel.setBounds(24, 155, 175, 30); port.setBounds(205, 155, 345, 32);
    nameLabel.setBounds(24, 201, 175, 30); stream.setBounds(205, 201, 345, 32);
    int y = 246;
    if (!processor.isReceiver()) {
        bitsLabel.setBounds(24, y, 175, 30); bits.setBounds(205, y, 155, 32);
        channelsLabel.setBounds(24, y+40, 175, 30); channels.setBounds(205, y+40, 155, 32); y += 70;
    }
    enabled.setBounds(24, y, 260, 32); applyButton.setBounds(390, y, 160, 32);
    errorLabel.setBounds(24, y+34, 530, 32);
    note.setBounds(24, y+65, 535, 40);
    meters.setBounds(24, y+108, 532, 90);
    statusLabel.setBounds(32, getHeight()-56, 515, 32);
}
void VbanEditor::applySettings() {
    auto s = processor.settings();
    s.address = address.getText().trim(); s.port = port.getText().getIntValue(); s.name = stream.getText();
    s.bits = bits.getSelectedId(); s.channels = channels.getSelectedId(); s.enabled = enabled.getToggleState();
    const auto error = processor.apply(s);
    errorLabel.setText(error, juce::dontSendNotification);
    if (error.isEmpty()) loadSettings();
    else enabled.setToggleState(processor.settings().enabled, juce::dontSendNotification);
}
void VbanEditor::timerCallback() {
    if (shownGeneration != vband::NetworkEngine::generation(processor.engine().control())) loadSettings();
    statusLabel.setText(processor.status(), juce::dontSendNotification);
    meters.setPacketCount(processor.isReceiver() ? processor.engine().received.load() : processor.engine().sent.load());
}
