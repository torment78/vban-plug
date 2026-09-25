// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

static bool layout(juce::AudioProcessor& processor, int channels) {
    auto buses = processor.getBusesLayout();
    buses.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
    buses.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
    return processor.setBusesLayout(buses);
}
static void channelLayoutTests() {
    for (auto mode : {vband::Mode::send, vband::Mode::receive}) {
        auto processor = std::make_unique<VbanProcessor>(mode);
        check(processor->getTotalNumInputChannels() == 8 && processor->getTotalNumOutputChannels() == 8,
            "Eight host input and output pins are exposed by default");
        for (int channels = 1; channels <= 8; ++channels) {
            check(layout(*processor, channels), "Host can choose every 1-8 channel layout");
            if (mode == vband::Mode::receive)
                check(processor->meterChannels() == 0, "RX never invents meters from the host layout before detection");
            auto discrete = processor->getBusesLayout();
            discrete.inputBuses.set(0, juce::AudioChannelSet::discreteChannels(channels));
            discrete.outputBuses.set(0, juce::AudioChannelSet::discreteChannels(channels));
            check(processor->isBusesLayoutSupported(discrete), "Numbered channel layouts are accepted");
            auto s = processor->settings(); s.channels = channels;
            check(processor->apply(s).isEmpty(), "All stream channel selections are valid");
            juce::MemoryBlock state; processor->getStateInformation(state);
            auto restored = std::make_unique<VbanProcessor>(mode);
            restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            check(restored->settings().channels == channels, "1-8 stream channel setting survives saved state");
        }
        auto invalid = processor->settings(); invalid.channels = 9;
        check(processor->apply(invalid).isNotEmpty(), "Reject a ninth stream channel");
        invalid.channels = 0; check(processor->apply(invalid).isNotEmpty(), "Reject zero stream channels");
        auto unsupported = processor->getBusesLayout();
        unsupported.inputBuses.set(0, juce::AudioChannelSet::discreteChannels(9));
        unsupported.outputBuses.set(0, juce::AudioChannelSet::discreteChannels(9));
        check(!processor->isBusesLayoutSupported(unsupported), "Reject host buses larger than eight");
        unsupported = processor->getBusesLayout();
        unsupported.inputBuses.set(0, juce::AudioChannelSet::disabled());
        check(processor->isBusesLayoutSupported(unsupported) == (mode == vband::Mode::receive),
            "Only RX permits an output-only host layout");
    }
}
static void channelWireTests() {
    juce::DatagramSocket listener;
    check(listener.bindToPort(0), "Bind raw packet inspector");
    auto tx = std::make_unique<VbanProcessor>(vband::Mode::send);
    auto s = tx->settings(); s.port = listener.getBoundPort(); s.enabled = true; s.name = "EightChannels";
    juce::MidiBuffer midi;
    for (int bits : {16, 24}) for (int channels = 1; channels <= 8; ++channels) {
        check(layout(*tx, channels), "Set TX bus for raw wire test");
        s.bits = bits; s.channels = channels;
        check(tx->apply(s).isEmpty(), "Configure raw multichannel TX");
        check(until([&] { return tx->engine().status() == "Ready to send"; }), "Raw TX socket ready");
        tx->prepareToPlay(48000, 1025);
        juce::AudioBuffer<double> audio(channels, 1025), before;
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < audio.getNumSamples(); ++i) audio.setSample(c, i, (c + 1) * 0.05 + (i % 7) * 1.0e-5);
        before.makeCopyOf(audio);
        tx->processBlock(audio, midi);
        for (int c = 0; c < channels; ++c)
            check(std::memcmp(audio.getReadPointer(c), before.getReadPointer(c), 1025 * sizeof(double)) == 0,
                "All multichannel host samples remain bit-exact");
        int frames = 0, packets = 0;
        std::uint32_t sequence = 0;
        check(until([&] {
            while (listener.waitUntilReady(true, 0) == 1) {
                std::array<std::uint8_t, vband::maxDatagram + 1> bytes {};
                const int size = listener.read(bytes.data(), int(bytes.size()), false);
                vband::AudioPacket decoded;
                check(size > 0 && vband::decode(bytes.data(), static_cast<std::size_t>(size), decoded), "Decode captured TX datagram");
                check(decoded.channels == channels && decoded.bits == bits && decoded.name == vband::streamName("EightChannels"),
                    "One stream retains all channel and format metadata");
                check(decoded.frames <= vband::maxFrames(channels, bits) && size <= int(vband::maxDatagram),
                    "Multichannel datagrams stay within the VBAN payload limit");
                if (packets != 0) check(decoded.sequence == sequence + 1, "Multichannel packet sequence increases");
                sequence = decoded.sequence; ++packets;
                check(frames + decoded.frames <= audio.getNumSamples(), "No duplicated frames in the wire stream");
                for (int i = 0; i < decoded.frames; ++i) for (int c = 0; c < channels; ++c)
                    check(std::abs(decoded.samples[static_cast<std::size_t>(i * channels + c)]
                        - audio.getSample(c, frames + i)) < 0.00004, "Channel ordering and partial-packet samples survive PCM encoding");
                frames += decoded.frames;
            }
            return frames == audio.getNumSamples();
        }), "Complete multichannel host block arrives including final partial packet");
        tx->processBlockBypassed(audio, midi);
        for (int c = 0; c < channels; ++c)
            check(std::memcmp(audio.getReadPointer(c), before.getReadPointer(c), 1025 * sizeof(double)) == 0,
                "Multichannel bypass preserves all host channels");
    }
}
static void channelNetworkTests() {
    auto tx = std::make_unique<VbanProcessor>(vband::Mode::send);
    auto rx = std::make_unique<VbanProcessor>(vband::Mode::receive);
    auto rs = rx->settings(); rs.enabled = true; rs.port = freePort(); rs.name = "Channels";
    auto ts = tx->settings(); ts.enabled = true; ts.port = rs.port; ts.name = rs.name;
    check(rx->apply(rs).isEmpty(), "Configure multichannel RX");
    check(until([&] { return rx->engine().status() == "Waiting for stream"; }), "Multichannel RX ready");
    check(rx->meterChannels() == 0, "Enabled RX waits for a matching stream before showing channel meters");
    juce::MidiBuffer midi;
    auto run = [&](int inputs, int streamChannels, int outputs, int bits, int blockSize = 256) {
        check(layout(*tx, inputs) && layout(*rx, outputs), "Select host buses for channel routing case");
        tx->prepareToPlay(48000, blockSize); rx->prepareToPlay(48000, blockSize);
        ts.channels = streamChannels; ts.bits = bits;
        check(tx->apply(ts).isEmpty(), "Change live stream channel count");
        check(until([&] { return tx->engine().status() == "Ready to send"; }), "Channel change applied to sender");
        juce::AudioBuffer<float> input(inputs, blockSize), output(outputs, blockSize);
        for (int c = 0; c < inputs; ++c) for (int i = 0; i < blockSize; ++i) input.setSample(c, i, (c + 1) * 0.05f);
        std::array<float, 8> expectedStream {};
        for (int c = 0; c < streamChannels; ++c) {
            if (streamChannels == 1) expectedStream[0] = 0.05f * (inputs + 1) * 0.5f;
            else if (inputs == 1 && streamChannels == 2) expectedStream[static_cast<std::size_t>(c)] = 0.05f;
            else if (c < inputs) expectedStream[static_cast<std::size_t>(c)] = 0.05f * (c + 1);
        }
        bool heard = false;
        for (int block = 0; block < 50; ++block) {
            tx->processBlock(input, midi); juce::Thread::sleep(5); rx->processBlock(output, midi);
            if (block < 20 || output.getSample(0, 0) == 0.0f || rx->engine().incomingChannels != streamChannels) continue;
            heard = true;
            for (int c = 0; c < outputs; ++c) {
                float expected = c < streamChannels ? expectedStream[static_cast<std::size_t>(c)] : 0.0f;
                if (outputs == 1) {
                    expected = 0.0f;
                    for (int k = 0; k < streamChannels; ++k) expected += expectedStream[static_cast<std::size_t>(k)] / streamChannels;
                } else if (outputs == 2 && streamChannels == 1) expected = expectedStream[0];
                for (int i : {0, blockSize / 2, blockSize - 1}) check(std::abs(output.getSample(c, i) - expected) < 0.0001f,
                    "RX maps every channel correctly and clears unused outputs");
            }
        }
        check(heard && rx->engine().incomingBits == bits, "Live UDP carries all selected channels");
        const auto received = rx->meterState().read(), sent = tx->meterState().read();
        check(received.channels == streamChannels && sent.channels == streamChannels && rx->meterChannels() == streamChannels,
            "TX and RX expose one meter per stream channel");
        for (int c = 0; c < 8; ++c) {
            const float expected = expectedStream[static_cast<std::size_t>(c)];
            check(std::abs(received.peak[static_cast<std::size_t>(c)] - expected) < 0.0001f,
                "RX meters retain every received channel even on a narrower host bus");
            check(std::abs(sent.peak[static_cast<std::size_t>(c)] - expected) < 0.0001f,
                "TX meters measure the actual mono mix, numbering and silent padding");
        }
    };
    for (int bits : {16, 24}) for (int channels = 1; channels <= 8; ++channels) run(channels, channels, channels, bits);
    run(8, 3, 8, 24); run(8, 4, 8, 24); // Meter count follows TX while both host buses stay at eight.
    run(8, 8, 2, 24);
    check(rx->status().contains("Host: 2 out"), "RX warns when the host exposes fewer outputs than the stream");
    run(8, 8, 1, 16); run(2, 2, 8, 24);
    run(1, 1, 2, 24); run(1, 1, 8, 24);
    run(2, 8, 8, 24); run(8, 1, 1, 24);
    run(1, 2, 2, 16);
    run(8, 8, 8, 24, 1025); // Cross scratch-buffer chunks and the final short chunk.
    rs.enabled = false; check(rx->apply(rs).isEmpty(), "Disable detected RX stream");
    check(rx->meterChannels() == 0, "Disabled RX hides the previously detected channel meters");
}
static void monoCancellationMeterTest() {
    auto tx = std::make_unique<VbanProcessor>(vband::Mode::send);
    auto s = tx->settings(); s.channels = 1; tx->apply(s);
    tx->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
    for (int i = 0; i < 256; ++i) { audio.setSample(0, i, 0.5f); audio.setSample(1, i, -0.5f); }
    tx->processBlock(audio, midi);
    const auto meter = tx->meterState().read();
    check(meter.channels == 1 && meter.rms[0] == 0 && meter.peak[0] == 0 && meter.peak[1] == 0,
        "One mono meter reflects phase cancellation in the transmitted mix");
}
static void multichannelTests() {
    channelLayoutTests(); channelWireTests(); channelNetworkTests(); monoCancellationMeterTest();
}
