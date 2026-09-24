// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"

VbanProcessor::VbanProcessor(vband::Mode selected)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)), mode(selected) {
    auto s = network.settings(); s.mode = mode; network.configure(s);
}
const juce::String VbanProcessor::getName() const { return isReceiver() ? "VBAN Plug RX" : "VBAN Plug TX"; }
void VbanProcessor::prepareToPlay(double rate, int) {
    hostRate = static_cast<int>(std::llround(rate));
    receiveBuffer.reset(); audioGeneration = 0; levels.prepare(rate);
    setLatencySamples(0); // TX has zero delay; RX is an external live source.
}
void VbanProcessor::releaseResources() { receiveBuffer.reset(); levels.reset(); }
bool VbanProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    const auto out = b.getMainOutputChannelSet(), in = b.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && (in == out || (isReceiver() && in.isDisabled()));
}
juce::String VbanProcessor::apply(vband::Settings s) {
    s.mode = mode;
    auto result = network.configure(s);
    if (result.isEmpty()) updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
    return result;
}
void VbanProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) { process(audio, midi); }
void VbanProcessor::processBlock(juce::AudioBuffer<double>& audio, juce::MidiBuffer& midi) { process(audio, midi); }
template<class Sample> void VbanProcessor::process(juce::AudioBuffer<Sample>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const auto control = network.control();
    const auto gen = vband::NetworkEngine::generation(control);
    const int rate = hostRate.load(std::memory_order_relaxed);
    if (audioGeneration != gen) { receiveBuffer.reset(); audioGeneration = gen; formatBits = formatChannels = 0; }
    if (!isReceiver()) {
        levels.measure(audio, juce::Time::getMillisecondCounter());
        // TX never writes host audio, even on overload or network failure.
        if (!vband::NetworkEngine::enabled(control) || isNonRealtime() || vband::rateIndex(rate) < 0) return;
        vband::AudioPacket packet;
        packet.generation = gen; packet.rate = rate;
        packet.timestamp = juce::Time::getMillisecondCounter();
        packet.bits = (control & 4) ? 24 : 16; packet.channels = (control & 8) ? 2 : 1;
        const int maxFrames = std::min(256, 1436 / (packet.channels * packet.bits / 8));
        const auto* left = audio.getReadPointer(0);
        const auto* right = audio.getReadPointer(audio.getNumChannels() > 1 ? 1 : 0);
        for (int offset = 0; offset < audio.getNumSamples(); offset += maxFrames) {
            packet.frames = std::min(maxFrames, audio.getNumSamples() - offset);
            for (int i = 0; i < packet.frames; ++i) {
                if (packet.channels == 1) packet.samples[static_cast<std::size_t>(i)] = static_cast<float>(0.5 * left[offset+i] + 0.5 * right[offset+i]);
                else {
                    packet.samples[static_cast<std::size_t>(i*2)] = static_cast<float>(left[offset+i]);
                    packet.samples[static_cast<std::size_t>(i*2+1)] = static_cast<float>(right[offset+i]);
                }
            }
            if (!network.send(packet)) ++network.dropped;
        }
        return;
    }
    audio.clear();
    vband::AudioPacket packet;
    const auto now = juce::Time::getMillisecondCounter();
    const bool active = vband::NetworkEngine::enabled(control) && !isNonRealtime();
    for (int i = 0; i < 128 && network.receive(packet); ++i) {
        if (!active || packet.generation != gen || now - packet.timestamp > 100) continue;
        if (packet.rate != rate) { receiveBuffer.reset(); continue; }
        if (packet.bits != formatBits || packet.channels != formatChannels) {
            receiveBuffer.reset(); formatBits = packet.bits; formatChannels = packet.channels;
        }
        receiveBuffer.append(packet);
    }
    if (!active || now - network.lastReceive.load() > 100 || network.incomingRate.load() != rate) {
        receiveBuffer.reset(); levels.reset(); return;
    }
    receiveBuffer.render(audio.getWritePointer(0), audio.getNumChannels() > 1 ? audio.getWritePointer(1) : nullptr,
        audio.getNumSamples(), rate);
    levels.measure(audio, now);
}
void VbanProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) { bypass(audio, midi); }
void VbanProcessor::processBlockBypassed(juce::AudioBuffer<double>& audio, juce::MidiBuffer& midi) { bypass(audio, midi); }
template<class Sample> void VbanProcessor::bypass(juce::AudioBuffer<Sample>& audio, juce::MidiBuffer& midi) {
    midi.clear(); receiveBuffer.reset();
    vband::AudioPacket packet;
    for (int i = 0; i < 128 && network.receive(packet); ++i) {}
    if (isReceiver()) audio.clear();
    levels.measure(audio, juce::Time::getMillisecondCounter());
}
juce::String VbanProcessor::status() const {
    const auto c = network.control();
    if (!vband::NetworkEngine::enabled(c)) return "Disabled";
    if (isNonRealtime()) return "Live network audio is paused during offline rendering";
    const int rate = hostRate.load();
    if (!isReceiver()) {
        if (vband::rateIndex(rate) < 0) return "Host sample rate is not supported by VBAN";
        const auto networkStatus = network.status();
        if (networkStatus.startsWith("UDP send failed") || networkStatus.startsWith("Cannot bind")) return networkStatus;
        if (network.sent.load() > 0) return "Sending  |  " + juce::String(rate) + " Hz";
        return network.status();
    }
    const auto incoming = network.incomingRate.load();
    if (!incoming || juce::Time::getMillisecondCounter() - network.lastReceive.load() > 1000) return network.status();
    if (incoming != rate) return "Stream: " + juce::String(incoming) + " Hz. Set the host to this sample rate.";
    return "Receiving  |  " + juce::String(incoming) + " Hz  |  PCM " + juce::String(network.incomingBits.load())
        + "  |  " + (network.incomingChannels.load() == 1 ? "Mono" : "Stereo");
}
juce::AudioProcessorEditor* VbanProcessor::createEditor() { return new VbanEditor(*this); }
void VbanProcessor::getStateInformation(juce::MemoryBlock& dest) {
    const auto s = settings();
    juce::XmlElement xml("VBANPlug");
    xml.setAttribute("version", 1); xml.setAttribute("address", s.address); xml.setAttribute("port", s.port);
    xml.setAttribute("name", s.name); xml.setAttribute("bits", s.bits); xml.setAttribute("channels", s.channels);
    xml.setAttribute("enabled", s.enabled);
    copyXmlToBinary(xml, dest);
}
void VbanProcessor::setStateInformation(const void* data, int size) {
    if (size <= 0 || size > 65536) return;
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("VBANPlug") || xml->getIntAttribute("version") != 1) return;
    vband::Settings s; s.mode = mode;
    s.address = xml->getStringAttribute("address", s.address); s.port = xml->getIntAttribute("port", s.port);
    s.name = xml->getStringAttribute("name", s.name); s.bits = xml->getIntAttribute("bits", s.bits);
    s.channels = xml->getIntAttribute("channels", s.channels); s.enabled = xml->getBoolAttribute("enabled", false);
    network.configure(s);
}
