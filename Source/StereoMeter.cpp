// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "StereoMeter.h"

StereoMeter::StereoMeter(vband::MeterState& data, bool receiver, int channels)
    : source(data), receiving(receiver), lastTick(juce::Time::getMillisecondCounter()) {
    levelDb.fill(-60.0f); peakDb.fill(-60.0f);
    setChannelCount(channels); setPacketCount(0);
    setTitle(receiver ? "Received audio levels" : "Transmitted stream levels");
    setTooltip("Per-channel stream RMS levels in dBFS, with fast peak capture, 750 ms peak hold and a 1 second clip indicator.");
    timerCallback();
    startTimerHz(60);
}
void StereoMeter::setChannelCount(int channels) {
    channels = std::clamp(channels, receiving ? 0 : 1, vband::maxChannels);
    if (channels == channelCount) return;
    channelCount = channels;
    levelDb.fill(-60.0f); peakDb.fill(-60.0f);
    holdSeconds.fill(0.0f); clipSeconds.fill(0.0f);
    repaint();
}
void StereoMeter::setPacketCount(std::uint64_t count) {
    const auto text = juce::String(receiving ? "Packets received: " : "Packets sent: ")
        + juce::String(static_cast<juce::int64>(count));
    if (text != packetText) { packetText = text; repaint(0, 0, getWidth(), 24); }
}
void StereoMeter::timerCallback() {
    const auto now = juce::Time::getMillisecondCounter();
    const auto elapsed = juce::jlimit(0.0f, 1.0f, float(now - lastTick) * 0.001f);
    lastTick = now;
    const auto current = source.read();
    const bool fresh = current.timestamp != 0 && now - current.timestamp <= 150;
    for (std::size_t c = 0; c < static_cast<std::size_t>(channelCount); ++c) {
        const float target = juce::Decibels::gainToDecibels(fresh && int(c) < current.channels ? current.rms[c] : 0.0f, -60.0f);
        const float instantPeak = juce::Decibels::gainToDecibels(fresh && int(c) < current.channels ? current.peak[c] : 0.0f, -60.0f);
        levelDb[c] = std::max(target, levelDb[c] - elapsed * 24.0f);
        holdSeconds[c] = std::max(0.0f, holdSeconds[c] - elapsed);
        clipSeconds[c] = std::max(0.0f, clipSeconds[c] - elapsed);
        if (instantPeak >= peakDb[c] && instantPeak > -60.0f) {
            peakDb[c] = instantPeak; holdSeconds[c] = 0.75f;
        } else if (holdSeconds[c] == 0.0f) {
            peakDb[c] = std::max(levelDb[c], peakDb[c] - elapsed * 20.0f);
        }
        if (fresh && int(c) < current.channels && current.peak[c] >= 1.0f) clipSeconds[c] = 1.0f;
    }
    repaint();
}
void StereoMeter::paint(juce::Graphics& g) {
    const float width = float(getWidth());
    g.setColour(juce::Colour(0xff16252e));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colour(0xffa6bdc9));
    g.drawText(receiving ? "RECEIVED LEVEL  /  dBFS" : "STREAM LEVEL  /  dBFS", 12, 3, 180, 20, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffdcebf1));
    g.drawText(packetText, 198, 3, getWidth()-210, 20, juce::Justification::centredRight);
    if (channelCount == 0) {
        g.setColour(juce::Colour(0xffa6bdc9));
        g.drawText("Waiting for stream - channels detected automatically", 12, 28,
            getWidth() - 24, 24, juce::Justification::centredLeft);
        return;
    }
    const float barX = 32.0f, barWidth = width - 118.0f, barHeight = channelCount <= 2 ? 13.0f : 6.0f;
    const float rowHeight = channelCount <= 2 ? 23.0f : 11.0f;
    g.setFont(juce::FontOptions(channelCount <= 2 ? 12.0f : 10.0f));
    for (std::size_t c = 0; c < static_cast<std::size_t>(channelCount); ++c) {
        const float y = 30.0f + float(c) * rowHeight + (channelCount == 1 ? 8.0f : 0.0f);
        g.setColour(juce::Colour(0xffdcebf1));
        const auto label = channelCount == 2 ? juce::String(c == 0 ? "L" : "R") : (channelCount == 1 ? juce::String("M") : juce::String(int(c) + 1));
        const int textHeight = channelCount <= 2 ? 18 : 12;
        g.drawText(label, 12, int(y)-2, 16, textHeight, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff0b141b));
        g.fillRoundedRectangle(barX, y, barWidth, barHeight, 2.0f);
        const float level = barWidth * position(levelDb[c]);
        const std::array<float, 4> edges {0.0f, barWidth * position(-12.0f), barWidth * position(-3.0f), barWidth};
        const std::array<juce::Colour, 3> colours {juce::Colour(0xff56d69b), juce::Colour(0xffffcf5a), juce::Colour(0xffff6d69)};
        for (std::size_t zone = 0; zone < 3; ++zone) {
            const float end = std::min(level, edges[zone+1]);
            if (end > edges[zone]) { g.setColour(colours[zone]); g.fillRect(barX + edges[zone], y, end - edges[zone], barHeight); }
        }
        g.setColour(juce::Colour(0xff101820).withAlpha(0.55f));
        for (int segment = 1; segment < 40; ++segment) {
            const float x = barX + barWidth * float(segment) / 40.0f;
            g.fillRect(x, y, 1.0f, barHeight);
        }
        if (peakDb[c] > -60.0f) {
            g.setColour(juce::Colour(0xfff0f8fc));
            g.fillRect(barX + std::min(barWidth - 2.0f, barWidth * position(peakDb[c])), y, 2.0f, barHeight);
        }
        g.setColour(clipSeconds[c] > 0.0f ? juce::Colour(0xffff6d69) : juce::Colour(0xff35434d));
        const float dot = channelCount <= 2 ? 7.0f : 5.0f;
        g.fillEllipse(width - 18.0f, y + (barHeight - dot) * 0.5f, dot, dot);
        g.setColour(juce::Colour(0xffdcebf1));
        const auto value = levelDb[c] <= -59.9f ? juce::String("-inf") : juce::String(levelDb[c], 1);
        g.drawText(value, int(width)-79, int(y)-2, 53, textHeight, juce::Justification::centredRight);
    }
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colour(0xff8fa6b3));
    for (int db : {-60, -48, -36, -24, -12, -6, 0}) {
        const int x = int(barX + barWidth * position(float(db)));
        g.drawText(juce::String(db), x-12, getHeight()-19, 24, 15, juce::Justification::centred);
    }
}
