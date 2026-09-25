// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ChannelRouting.h"

VbanProcessor::VbanProcessor(vband::Mode selected)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::create7point1(), true)
        .withOutput("Output", juce::AudioChannelSet::create7point1(), true)), mode(selected) {
    auto s = network.settings(); s.mode = mode; network.configure(s);
}
const juce::String VbanProcessor::getName() const { return isReceiver() ? "VBAN Plug RX" : "VBAN Plug TX"; }
void VbanProcessor::prepareToPlay(double rate, int) {
    hostRate = static_cast<int>(std::llround(rate));
    receiveBuffer.reset(); audioGeneration = 0; levels.prepare(rate);
    setLatencySamples(0);
}
void VbanProcessor::releaseResources() { receiveBuffer.reset(); levels.reset(); }
bool VbanProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    if (b.inputBuses.size() != 1 || b.outputBuses.size() != 1) return false;
    const auto out = b.getMainOutputChannelSet(), in = b.getMainInputChannelSet();
    return out.size() >= 1 && out.size() <= vband::maxChannels
        && (in == out || (isReceiver() && in.isDisabled()));
}
int VbanProcessor::meterChannels() const noexcept {
    if (!isReceiver()) return vband::NetworkEngine::streamChannels(network.control());
    const int incoming = network.incomingChannels.load(std::memory_order_relaxed);
    return std::clamp(incoming > 0 ? incoming : getTotalNumOutputChannels(), 1, vband::maxChannels);
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
    const auto now = juce::Time::getMillisecondCounter();
    if (audioGeneration != gen) {
        receiveBuffer.reset(); levels.reset(); audioGeneration = gen; formatBits = formatChannels = 0;
    }
    if (!isReceiver()) {
        const int channels = vband::NetworkEngine::streamChannels(control);
        const auto readSample = [&](int c, int i) {
            return vband::streamSample(audio.getArrayOfReadPointers(), audio.getNumChannels(), channels, c, i);
        };
        levels.measureSamples(channels, audio.getNumSamples(), now, readSample);
        // TX never writes host audio, regardless of stream layout or network state.
        if (!vband::NetworkEngine::enabled(control) || isNonRealtime() || vband::rateIndex(rate) < 0) return;
        vband::AudioPacket packet;
        packet.generation = gen; packet.rate = rate; packet.timestamp = now;
        packet.bits = (control & 4) ? 24 : 16; packet.channels = channels;
        const int framesPerPacket = vband::maxFrames(channels, packet.bits);
        for (int offset = 0; offset < audio.getNumSamples(); offset += framesPerPacket) {
            packet.frames = std::min(framesPerPacket, audio.getNumSamples() - offset);
            for (int i = 0; i < packet.frames; ++i)
                for (int c = 0; c < channels; ++c)
                    packet.samples[static_cast<std::size_t>(i * channels + c)] = readSample(c, offset + i);
            if (!network.send(packet)) ++network.dropped;
        }
        return;
    }
    audio.clear();
    vband::AudioPacket packet;
    const bool active = vband::NetworkEngine::enabled(control) && !isNonRealtime();
    for (int i = 0; i < 128 && network.receive(packet); ++i) {
        if (!active || packet.generation != gen || now - packet.timestamp > 100) continue;
        if (packet.rate != rate) { receiveBuffer.reset(); continue; }
        if (packet.bits != formatBits || packet.channels != formatChannels) {
            receiveBuffer.reset(); formatBits = packet.bits; formatChannels = packet.channels;
        }
        receiveBuffer.append(packet);
    }
    if (!active || now - network.lastReceive.load() > 100 || network.incomingRate.load() != rate || formatChannels == 0) {
        receiveBuffer.reset(); levels.reset(); return;
    }
    // Fixed scratch storage meters every received channel, even with a narrower host bus.
    for (int offset = 0; offset < audio.getNumSamples(); offset += scratchFrames) {
        const int count = std::min(scratchFrames, audio.getNumSamples() - offset);
        receiveScratch.clear();
        receiveBuffer.render(receiveScratch.getArrayOfWritePointers(), formatChannels, count, rate, audio.getNumSamples());
        levels.measureSamples(formatChannels, count, now,
            [&](int c, int i) { return receiveScratch.getSample(c, i); });
        for (int c = 0; c < audio.getNumChannels(); ++c)
            for (int i = 0; i < count; ++i)
                audio.setSample(c, offset + i, static_cast<Sample>(vband::streamSample(
                    receiveScratch.getArrayOfReadPointers(), formatChannels, audio.getNumChannels(), c, i)));
    }
}
void VbanProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) { bypass(audio, midi); }
void VbanProcessor::processBlockBypassed(juce::AudioBuffer<double>& audio, juce::MidiBuffer& midi) { bypass(audio, midi); }

template<class Sample> void VbanProcessor::bypass(juce::AudioBuffer<Sample>& audio, juce::MidiBuffer& midi) {
    midi.clear(); receiveBuffer.reset();
    vband::AudioPacket packet;
    for (int i = 0; i < 128 && network.receive(packet); ++i) {}
    if (isReceiver()) { audio.clear(); levels.reset(); return; }
    const int channels = vband::NetworkEngine::streamChannels(network.control());
    levels.measureSamples(channels, audio.getNumSamples(), juce::Time::getMillisecondCounter(),
        [&](int c, int i) { return vband::streamSample(audio.getArrayOfReadPointers(), audio.getNumChannels(), channels, c, i); });
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
        const int channels = vband::NetworkEngine::streamChannels(c), inputs = getTotalNumInputChannels();
        if (network.sent.load() > 0) {
            auto text = "Sending  |  " + juce::String(rate) + " Hz  |  " + juce::String(channels) + " ch";
            if (channels > inputs && !(inputs == 1 && channels == 2))
                text += "  |  " + juce::String(channels - inputs) + " silent: set host I/O";
            return text;
        }
        return networkStatus;
    }
    const auto incoming = network.incomingRate.load();
    if (!incoming || juce::Time::getMillisecondCounter() - network.lastReceive.load() > 1000) return network.status();
    if (incoming != rate) return "Stream: " + juce::String(incoming) + " Hz. Set the host to this sample rate.";
    const int channels = network.incomingChannels.load(), outputs = getTotalNumOutputChannels();
    auto text = "Receiving  |  " + juce::String(incoming) + " Hz  |  PCM " + juce::String(network.incomingBits.load())
        + "  |  " + juce::String(channels) + " ch";
    if (channels > outputs) text += "  |  Host: " + juce::String(outputs) + " out";
    return text;
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
