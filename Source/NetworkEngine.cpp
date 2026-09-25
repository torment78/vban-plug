// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "NetworkEngine.h"

namespace vband {
juce::String validate(const Settings& s) {
    if (s.mode != Mode::send && s.mode != Mode::receive) return "Choose Send or Receive.";
    if (!validAddress(s.address.toStdString())) return "Enter a unicast IPv4 address, for example 192.168.1.20.";
    if (s.port < 1 || s.port > 65535) return "UDP port must be between 1 and 65535.";
    if (!validName(s.name.toStdString())) return "Stream name must contain 1 to 16 printable ASCII characters.";
    if (s.bits != 16 && s.bits != 24) return "Choose PCM 16-bit or PCM 24-bit.";
    if (s.channels < 1 || s.channels > maxChannels) return "Choose 1 to 8 stream channels.";
    return {};
}
NetworkEngine::NetworkEngine() : juce::Thread("VBAN network") { startThread(); }
NetworkEngine::~NetworkEngine() { signalThreadShouldExit(); notify(); stopThread(-1); }
Settings NetworkEngine::settings() const { const juce::ScopedLock lock(mutex); return config; }
juce::String NetworkEngine::configure(const Settings& s) {
    if (auto error = validate(s); error.isNotEmpty()) return error;
    const juce::ScopedLock lock(mutex);
    config = s;
    // Normalise dotted-decimal spelling so source-IP comparisons are exact.
    config.address = juce::IPAddress(s.address).toString();
    const auto gen = generation(controlWord.load()) + 1;
    const auto flags = (s.enabled ? 1u : 0u) | (s.mode == Mode::receive ? 2u : 0u)
        | (s.bits == 24 ? 4u : 0u) | (static_cast<unsigned>(s.channels - 1) << 3);
    controlWord.store((std::uint64_t(gen) << 32) | flags, std::memory_order_release);
    message = s.enabled ? "Starting..." : "Disabled";
    notify(); return {};
}
juce::String NetworkEngine::status() const { const juce::ScopedLock lock(mutex); return message; }
void NetworkEngine::setStatus(const juce::String& s) { const juce::ScopedLock lock(mutex); message = s; }
void NetworkEngine::run() {
    std::unique_ptr<juce::DatagramSocket> socket;
    Settings active;
    std::uint32_t activeGeneration = 0, txSequence = 0, rxSequence = 0, lastPacketTime = 0;
    bool haveSequence = false, gapAfterOverflow = false;
    auto name = streamName("VBAN");
    std::array<std::uint8_t, 65536> buffer {};
    while (!threadShouldExit()) {
        const auto word = control();
        if (generation(word) != activeGeneration) {
            const juce::ScopedLock lock(mutex);
            active = config; activeGeneration = generation(control());
            socket.reset(); haveSequence = false; gapAfterOverflow = false;
            incomingRate = 0; incomingBits = 0; incomingChannels = 0; lastReceive = 0;
            sent = 0; received = 0; dropped = 0; missing = 0;
            name = streamName(active.name.toStdString());
            if (active.enabled) {
                socket = std::make_unique<juce::DatagramSocket>();
                socket->setEnablePortReuse(false);
                if (!socket->bindToPort(active.mode == Mode::receive ? active.port : 0)) {
                    socket.reset(); message = "Cannot bind UDP port. Choose a free port and apply again.";
                } else message = active.mode == Mode::send ? "Ready to send" : "Waiting for stream";
            } else message = "Disabled";
        }
        AudioPacket packet;
        // Always drain the outgoing queue, including when disabled or switching mode.
        for (int n = 0; n < 128 && outgoing.pop(packet); ++n) {
            if (!socket || active.mode != Mode::send || packet.generation != activeGeneration) continue;
            if (juce::Time::getMillisecondCounter() - packet.timestamp > 100) { ++dropped; continue; }
            packet.name = name; packet.sequence = txSequence++;
            std::array<std::uint8_t, maxDatagram> wire {};
            const auto bytes = encode(packet, wire);
            if (bytes && socket->waitUntilReady(false, 0) == 1
                && socket->write(active.address, active.port, wire.data(), static_cast<int>(bytes)) == static_cast<int>(bytes)) {
                ++sent; setStatus("Ready to send");
            } else { ++dropped; setStatus("UDP send failed; check address and network"); }
        }
        if (socket && active.mode == Mode::receive) {
            for (int n = 0; n < 128 && socket->waitUntilReady(true, 0) == 1; ++n) {
                juce::String source; int sourcePort = 0;
                const int bytes = socket->read(buffer.data(), static_cast<int>(buffer.size()), false, source, sourcePort);
                if (source != active.address || bytes < 28) continue;
                if (!decode(buffer.data(), static_cast<std::size_t>(bytes), packet)) { ++dropped; continue; }
                if (packet.name != name) continue;
                const auto now = juce::Time::getMillisecondCounter();
                const bool restart = !haveSequence || now - lastPacketTime > 500;
                const auto delta = packet.sequence - rxSequence;
                if (!restart && (delta == 0 || delta >= 0x80000000u)) { ++dropped; continue; }
                packet.discontinuity = restart || delta != 1 || gapAfterOverflow;
                if (!restart && delta > 1) missing.fetch_add(delta - 1);
                haveSequence = true; rxSequence = packet.sequence; lastPacketTime = now;
                packet.generation = activeGeneration; packet.timestamp = now;
                incomingRate = packet.rate; incomingBits = packet.bits; incomingChannels = packet.channels;
                lastReceive = now;
                if (incoming.push(packet)) { ++received; gapAfterOverflow = false; }
                else { ++dropped; gapAfterOverflow = true; }
            }
        }
        wait(active.enabled ? 1 : 100);
    }
}
}
