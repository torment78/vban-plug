// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cmath>
#include <limits>

static int checks = 0;
static void check(bool ok, const char* description) {
    ++checks; if (!ok) throw std::runtime_error(description);
}
template<class F> static bool until(F f, int timeout = 1500) {
    const auto end = juce::Time::getMillisecondCounter() + static_cast<unsigned>(timeout);
    do { if (f()) return true; juce::Thread::sleep(2); } while (juce::Time::getMillisecondCounter() < end);
    return false;
}
static int freePort() {
    juce::DatagramSocket socket;
    check(socket.bindToPort(0), "Acquire loopback test port"); return socket.getBoundPort();
}
static void protocolTests() {
    check(vband::validName("1234567890123456"), "16-byte name");
    check(!vband::validName("12345678901234567") && !vband::validName(""), "Reject invalid names");
    check(vband::validAddress("127.0.0.1") && !vband::validAddress("300.1.2.3") && !vband::validAddress("224.0.0.1"), "Validate unicast IPv4");
    std::array<std::uint8_t, vband::maxDatagram> wire {};
    for (int bits : {16, 24}) for (int channels = 1; channels <= 8; ++channels) {
        vband::AudioPacket p, decoded;
        p.bits = bits; p.channels = channels; p.frames = std::min(256, 1436 / (channels * bits / 8));
        p.rate = 48000; p.name = vband::streamName("1234567890123456"); p.sequence = 0xfedcba98u;
        const float values[] {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
        for (int i = 0; i < p.frames * channels; ++i) p.samples[static_cast<std::size_t>(i)] = values[i % 5];
        auto size = vband::encode(p, wire);
        check(size > 28 && size <= 1464, "Bounded packet size");
        check(wire[4] == 3 && wire[7] == (bits / 8 - 1) && wire[24] == 0x98, "Wire header fields and little endian");
        check(wire[28] == 0 && wire[29] == (bits == 16 ? 0x80 : 0) && (bits == 16 || wire[30] == 0x80), "Known signed PCM encoding");
        check(vband::decode(wire.data(), size, decoded), "Decode PCM");
        check(decoded.name == p.name && decoded.sequence == p.sequence && decoded.bits == bits && decoded.channels == channels, "Metadata roundtrip");
        const float tolerance = bits == 16 ? 1.0f / 32768 : 1.0f / 8388608;
        for (int i = 0; i < p.frames * channels; ++i)
            check(std::abs(decoded.samples[static_cast<std::size_t>(i)] - p.samples[static_cast<std::size_t>(i)]) <= tolerance, "PCM sample roundtrip");
        check(!vband::decode(wire.data(), size - 1, decoded), "Reject truncated payload");
        auto original = wire[7]; wire[7] |= 0x10;
        check(!vband::decode(wire.data(), size, decoded), "Reject compressed codec");
        wire[7] = original; wire[4] = 31;
        check(!vband::decode(wire.data(), size, decoded), "Reject invalid rate");
    }
    vband::AudioPacket oversized; oversized.frames = 256; oversized.channels = 2; oversized.bits = 24;
    check(vband::encode(oversized, wire) == 0, "Reject oversize 24-bit packet");
    vband::SpscQueue<int, 4> q;
    check(q.push(1) && q.push(2) && q.push(3) && !q.push(4), "Queue overload bounded");
    int n = 0;
    check(q.pop(n) && n == 1 && q.push(4) && q.pop(n) && n == 2 && q.pop(n) && n == 3 && q.pop(n) && n == 4 && !q.pop(n), "Queue wrap preserves order");
}
static void processorTests() {
    auto tx = std::make_unique<VbanProcessor>(vband::Mode::send);
    auto rx = std::make_unique<VbanProcessor>(vband::Mode::receive);
    tx->prepareToPlay(48000, 4096); rx->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(2, 4096), original(2, 4096); juce::MidiBuffer midi;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 4096; ++i)
        audio.setSample(c, i, float(std::sin(i * 0.03 + c) * 1.5));
    original.makeCopyOf(audio);
    auto s = tx->settings(); s.enabled = true; s.port = freePort(); s.bits = 16; s.channels = 1; s.name = "RoundTrip";
    check(tx->apply(s).isEmpty(), "Apply TX");
    for (int n = 0; n < 50; ++n) tx->processBlock(audio, midi);
    check(std::memcmp(audio.getReadPointer(0), original.getReadPointer(0), 4096 * sizeof(float)) == 0
       && std::memcmp(audio.getReadPointer(1), original.getReadPointer(1), 4096 * sizeof(float)) == 0, "Bit-exact TX pass-through under queue overload");
    check(tx->getLatencySamples() == 0, "Zero TX latency");
    juce::AudioBuffer<double> doubles(2, 256), savedDoubles(2, 256);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i)
        doubles.setSample(c, i, 1.0 + (i + c) * 1.0e-12);
    savedDoubles.makeCopyOf(doubles);
    tx->processBlock(doubles, midi);
    check(std::memcmp(doubles.getReadPointer(0), savedDoubles.getReadPointer(0), 256 * sizeof(double)) == 0
        && std::memcmp(doubles.getReadPointer(1), savedDoubles.getReadPointer(1), 256 * sizeof(double)) == 0, "Bit-exact 64-bit TX pass-through");
    tx->processBlockBypassed(doubles, midi);
    check(std::memcmp(doubles.getReadPointer(0), savedDoubles.getReadPointer(0), 256 * sizeof(double)) == 0, "Bypassed TX preserves doubles");
    rx->processBlockBypassed(doubles, midi);
    check(doubles.getMagnitude(0, 256) == 0, "Bypassed RX is silent");
    tx->setNonRealtime(true); tx->processBlock(audio, midi);
    check(tx->status().contains("offline") && std::memcmp(audio.getReadPointer(0), original.getReadPointer(0), 4096 * sizeof(float)) == 0, "Offline TX pass-through without live sending");
    tx->setNonRealtime(false);
    juce::MemoryBlock state; tx->getStateInformation(state);
    rx->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto restored = rx->settings();
    check(restored.name == s.name && restored.bits == 16 && restored.channels == 1 && restored.port == s.port
        && restored.enabled && restored.mode == vband::Mode::receive, "State roundtrip preserves fixed RX role");
    auto invalid = s; invalid.port = 0;
    check(tx->apply(invalid).isNotEmpty() && tx->settings().port == s.port, "Invalid settings preserve working configuration");
    rx->prepareToPlay(44100, 256); audio.clear(); audio.addFrom(0, 0, original, 0, 0, 4096);
    rx->processBlock(audio, midi);
    check(audio.getMagnitude(0, 4096) == 0, "RX never passes local input");
    rx->setStateInformation("bad", 3);
    check(rx->settings().name == s.name, "Malformed saved state ignored");
}
static void meterTests() {
    auto tx = std::make_unique<VbanProcessor>(vband::Mode::send);
    tx->prepareToPlay(48000, 480);
    juce::AudioBuffer<float> signal(2, 480); juce::MidiBuffer midi;
    for (int i = 0; i < 480; ++i) { signal.setSample(0, i, 0.5f); signal.setSample(1, i, 0.125f); }
    for (int block = 0; block < 50; ++block) tx->processBlock(signal, midi);
    auto levels = tx->meterState().read();
    check(std::abs(levels.rms[0] - 0.5f) < 0.0001f && std::abs(levels.rms[1] - 0.125f) < 0.0001f,
        "Independent calibrated L/R RMS levels");
    check(levels.peak[0] == 0.5f && levels.peak[1] == 0.125f, "Independent L/R peaks");
    check(tx->engine().sent == 0 && levels.timestamp != 0, "TX input metering works with network disabled");
    check(tx->meterState().read().peak[0] == 0, "Peak latch resets after UI consumption");
    signal.clear(); signal.setSample(0, 200, 1.25f);
    tx->processBlock(signal, midi);
    signal.clear(); tx->processBlock(signal, midi);
    levels = tx->meterState().read();
    check(levels.peak[0] == 1.25f && levels.peak[1] == 0.0f, "One-sample clip is retained across silent blocks");
    const auto previousRms = levels.rms[0];
    for (int block = 0; block < 100; ++block) tx->processBlock(signal, midi);
    levels = tx->meterState().read();
    check(levels.rms[0] < 0.001f && levels.rms[0] < previousRms, "RMS falls back to silence");
    juce::AudioBuffer<double> invalid(2, 32); invalid.clear();
    invalid.setSample(0, 0, std::numeric_limits<double>::infinity());
    invalid.setSample(1, 0, std::numeric_limits<double>::quiet_NaN());
    tx->processBlock(invalid, midi);
    levels = tx->meterState().read();
    check(std::isfinite(levels.rms[0]) && std::isfinite(levels.rms[1])
        && std::isfinite(levels.peak[0]) && std::isfinite(levels.peak[1]), "Bad samples cannot poison the meter");
    check(std::isinf(invalid.getSample(0, 0)) && std::isnan(invalid.getSample(1, 0)), "Meter does not alter host samples");
    vband::MeterState mono;
    juce::AudioBuffer<float> monoAudio(1, 480);
    for (int i = 0; i < 480; ++i) monoAudio.setSample(0, i, -0.75f);
    mono.measure(monoAudio, juce::Time::getMillisecondCounter());
    levels = mono.read();
    check(levels.channels == 1 && levels.rms[0] > 0 && levels.rms[1] == 0 && levels.peak[0] == 0.75f && levels.peak[1] == 0,
        "Mono input is shown on exactly one meter");
    tx->releaseResources(); levels = tx->meterState().read();
    check(levels.timestamp == 0 && levels.rms[0] == 0 && levels.peak[0] == 0, "Stopping audio clears metering");
}

static void networkTests() {
    auto tx = std::make_unique<VbanProcessor>(vband::Mode::send);
    auto rx = std::make_unique<VbanProcessor>(vband::Mode::receive);
    const int port = freePort();
    auto rs = rx->settings(); rs.enabled = true; rs.port = port; rs.name = "Loopback";
    check(rx->apply(rs).isEmpty(), "Configure RX");
    check(until([&] { return rx->engine().status() == "Waiting for stream"; }), "RX bound");
    auto ts = tx->settings(); ts.enabled = true; ts.port = port; ts.name = rs.name;
    check(tx->apply(ts).isEmpty(), "Configure TX");
    check(until([&] { return tx->engine().status() == "Ready to send"; }), "TX bound");
    tx->prepareToPlay(48000, 256); rx->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> input(2, 256), output(2, 256); juce::MidiBuffer midi;
    for (int bits : {16, 24}) for (int channels : {1, 2}) {
        ts.bits = bits; ts.channels = channels; check(tx->apply(ts).isEmpty(), "Change send format");
        // Changing PCM format must take effect without waiting for a sender restart.
        juce::Thread::sleep(30);
        bool heard = false, meterChecked = false;
        rx->meterState().read();
        for (int n = 0; n < 90; ++n) {
            for (int i = 0; i < 256; ++i) { input.setSample(0, i, 0.25f); input.setSample(1, i, 0.75f); }
            tx->processBlock(input, midi); juce::Thread::sleep(5); rx->processBlock(output, midi);
            // Allow old buffered audio to drain during a live format change.
            if (n > 10 && rx->engine().incomingBits == bits && rx->engine().incomingChannels == channels
                && output.getMagnitude(0, 256) > 0.1f) {
                const float expected = channels == 1 ? 0.5f : 0.25f;
                check(std::abs(output.getSample(0, 0) - expected) < 0.001f, "Received left/mono audio");
                check(std::abs(output.getSample(1, 0) - (channels == 1 ? 0.5f : 0.75f)) < 0.001f, "Received right audio");
                heard = true;
                if (!meterChecked && n > 25) {
                    const auto meter = rx->meterState().read();
                    check(meter.channels == channels && meter.rms[0] > 0.01f && meter.peak[0] > 0.01f && (channels == 1 || (meter.rms[1] > 0.01f && meter.peak[1] > 0.01f)),
                        "RX meter measures decoded network output");
                    meterChecked = true;
                }
            }
        }
        check(heard && meterChecked && rx->engine().incomingBits == bits && rx->engine().incomingChannels == channels, "Real UDP audio and automatic format detection");
    }
    rx->prepareToPlay(44100, 256);
    for (int i = 0; i < 8; ++i) { tx->processBlock(input, midi); juce::Thread::sleep(5); rx->processBlock(output, midi); }
    check(output.getMagnitude(0, 256) == 0 && rx->status().contains("Set the host"), "Sample-rate mismatch is silent and visible");
    rx->prepareToPlay(48000, 256);
    auto occupied = std::make_unique<VbanProcessor>(vband::Mode::receive);
    occupied->apply(rs);
    check(until([&] { return occupied->engine().status().startsWith("Cannot bind"); }), "Port conflict is reported");
    occupied.reset();
    ts.name = "WrongStream"; tx->apply(ts);
    for (int i = 0; i < 30; ++i) { tx->processBlock(input, midi); juce::Thread::sleep(5); rx->processBlock(output, midi); }
    check(output.getMagnitude(0, 256) == 0, "Wrong stream rejected and stale audio silenced");
    check(rx->meterState().read().rms[0] == 0 && rx->engine().received > 0, "Lost stream silences RX meter while received count remains visible");
    rs.address = "127.0.0.2"; rx->apply(rs); ts.name = "Loopback"; tx->apply(ts);
    juce::Thread::sleep(50);
    for (int i = 0; i < 10; ++i) { tx->processBlock(input, midi); juce::Thread::sleep(5); rx->processBlock(output, midi); }
    check(rx->engine().received == 0 && output.getMagnitude(0, 256) == 0, "Source IP filter");
}
#include "MultichannelTests.h"

static void editorSnapshots(const juce::File& folder) {
    folder.createDirectory();
    for (int channels : {1, 2, 8}) {
        auto tx = std::make_unique<VbanProcessor>(vband::Mode::send);
        auto rx = std::make_unique<VbanProcessor>(vband::Mode::receive);
        check(layout(*tx, channels) && layout(*rx, channels), "Preview host layout");
        tx->prepareToPlay(48000, 256); rx->prepareToPlay(48000, 256);
        auto rs = rx->settings(); rs.enabled = true; rs.port = freePort(); rs.name = "MeterPreview";
        auto ts = tx->settings(); ts.enabled = true; ts.port = rs.port; ts.name = rs.name; ts.channels = channels;
        rx->apply(rs); tx->apply(ts);
        check(until([&] { return rx->engine().status() == "Waiting for stream" && tx->engine().status() == "Ready to send"; }), "Preview UDP setup");
        juce::AudioBuffer<float> input(channels, 256), output(channels, 256); juce::MidiBuffer midi;
        for (auto* p : {tx.get(), rx.get()}) {
            // Refresh audio before each snapshot because editor creation can take over 150 ms.
            for (int block = 0; block < 50; ++block) {
                for (int c = 0; c < channels; ++c) for (int i = 0; i < 256; ++i)
                    input.setSample(c, i, (0.25f + 0.075f * c) * std::sin(float(block * 256 + i) * (0.08f + 0.01f * c)));
                tx->processBlock(input, midi); juce::Thread::sleep(5); rx->processBlock(output, midi);
            }
            check(rx->engine().received > 0 && output.getMagnitude(0, 256) > 0.1f, "Preview uses received UDP audio");
            std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
            const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
            const juce::String name = p->isReceiver() ? "RX" : "TX";
            auto file = folder.getChildFile(name + "-" + juce::String(channels) + "ch.png");
            {
                auto stream = file.createOutputStream();
                if (stream) { stream->setPosition(0); stream->truncate(); }
                check(stream != nullptr && juce::PNGImageFormat().writeImageToStream(image, *stream), "Active editor snapshot");
            }
            if (channels == 2) check(file.copyFileTo(folder.getChildFile(name + ".png")), "Stereo preview alias");
        }
    }
}

static void hostTests(const juce::String& path, bool receiver) {
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, path);
    check(descriptions.size() == 1, "VST3 scan");
    juce::String error;
    auto plugin = format.createInstanceFromDescription(*descriptions[0], 48000, 256, error);
    if (!plugin) throw std::runtime_error(error.toStdString());
    check(plugin->getName() == (receiver ? "VBAN Plug RX" : "VBAN Plug TX"), "Separate VST3 product identity");
    // Metadata-only scans omit bus counts; query the instantiated VST3 before any layout change.
    check(plugin->getTotalNumInputChannels() == 8 && plugin->getTotalNumOutputChannels() == 8,
        "Loaded VST3 exposes eight input and output connections by default");
    check(plugin->supportsDoublePrecisionProcessing(), "VST3 advertises double precision");
    juce::MidiBuffer midi;
    for (int channels = 1; channels <= 8; ++channels) {
        check(layout(*plugin, channels), "Loaded VST3 accepts host-selected 1-8 input/output connections");
        check(plugin->getTotalNumInputChannels() == channels && plugin->getTotalNumOutputChannels() == channels,
            "Loaded VST3 reports the negotiated connection count");
        plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        plugin->prepareToPlay(48000, 256);
        juce::AudioBuffer<float> buffer(channels, 256), before;
        for (int c = 0; c < channels; ++c) for (int i = 0; i < 256; ++i) buffer.setSample(c, i, (c + 1) * 0.075f + i * 0.00001f);
        before.makeCopyOf(buffer);
        plugin->processBlock(buffer, midi);
        for (int c = 0; c < channels; ++c)
            check(receiver ? buffer.getMagnitude(c, 0, 256) == 0
                : std::memcmp(buffer.getReadPointer(c), before.getReadPointer(c), 256 * sizeof(float)) == 0,
                "Loaded VST3 processes every float channel correctly");
        plugin->releaseResources();
        plugin->setProcessingPrecision(juce::AudioProcessor::doublePrecision);
        plugin->prepareToPlay(48000, 256);
        juce::AudioBuffer<double> doubles(channels, 256), original;
        for (int c = 0; c < channels; ++c) for (int i = 0; i < 256; ++i) doubles.setSample(c, i, 0.1 * (c + 1) + i * 1.0e-12);
        original.makeCopyOf(doubles);
        plugin->processBlock(doubles, midi);
        for (int c = 0; c < channels; ++c)
            check(receiver ? doubles.getMagnitude(c, 0, 256) == 0
                : std::memcmp(doubles.getReadPointer(c), original.getReadPointer(c), 256 * sizeof(double)) == 0,
                "Loaded VST3 processes every double channel correctly");
        plugin->releaseResources();
    }
    if (receiver) {
        auto buses = plugin->getBusesLayout();
        buses.inputBuses.set(0, juce::AudioChannelSet::disabled());
        check(plugin->setBusesLayout(buses) && plugin->getTotalNumInputChannels() == 0
            && plugin->getTotalNumOutputChannels() == 8, "Loaded RX permits an eight-output layout with no input bus");
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());
    check(editor != nullptr, "Loaded VST3 editor");
    editor.reset();
}
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    try {
        protocolTests(); processorTests(); meterTests(); networkTests(); multichannelTests();
        if (argc > 1) editorSnapshots(juce::File(juce::String::fromUTF8(argv[1])));
        if (argc > 3) { hostTests(juce::String::fromUTF8(argv[2]), false); hostTests(juce::String::fromUTF8(argv[3]), true); }
        std::cout << "PASS: " << checks << " checks (PCM, pass-through, state, UDP, filters, formats, metering";
        if (argc > 3) std::cout << ", VST3 scan/load/editor";
        std::cout << ")\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL after " << checks << " checks: " << e.what() << "\n"; return 1; }
}
