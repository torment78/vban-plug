// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "VbanProtocol.h"
#include "SpscQueue.h"
#include <juce_core/juce_core.h>

namespace vband {
enum class Mode { send, receive };
struct Settings {
    Mode mode = Mode::send;
    juce::String address = "127.0.0.1", name = "VBAN";
    int port = 6980, bits = 24, channels = 2;
    bool enabled = false;
};
juce::String validate(const Settings&);
class NetworkEngine final : private juce::Thread {
public:
    NetworkEngine();
    ~NetworkEngine() override;
    Settings settings() const;
    juce::String configure(const Settings&);
    juce::String status() const;
    std::uint64_t control() const noexcept { return controlWord.load(std::memory_order_acquire); }
    bool send(const AudioPacket& p) noexcept { return outgoing.push(p); }
    bool receive(AudioPacket& p) noexcept { return incoming.pop(p); }
    std::atomic<std::uint64_t> sent {0}, received {0}, dropped {0}, missing {0};
    std::atomic<int> incomingRate {0}, incomingBits {0}, incomingChannels {0};
    std::atomic<std::uint32_t> lastReceive {0};
    static std::uint32_t generation(std::uint64_t c) noexcept { return static_cast<std::uint32_t>(c >> 32); }
    static bool enabled(std::uint64_t c) noexcept { return (c & 1) != 0; }
    static bool receiving(std::uint64_t c) noexcept { return (c & 2) != 0; }
private:
    void run() override;
    void setStatus(const juce::String&);
    mutable juce::CriticalSection mutex;
    Settings config;
    juce::String message = "Disabled";
    std::atomic<std::uint64_t> controlWord {std::uint64_t(1) << 32};
    SpscQueue<AudioPacket, 129> outgoing, incoming;
};
}
