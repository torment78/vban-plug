// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "VbanProtocol.h"
#include <algorithm>

namespace vband {
// Numbered routing. Preserve the original stereo-to-mono mix and mono-to-stereo copy.
template<class Sample>
float streamSample(const Sample* const* input, int inputChannels, int streamChannels, int channel, int sample) noexcept {
    inputChannels = std::clamp(inputChannels, 0, maxChannels);
    if (inputChannels == 0) return 0.0f;
    if (streamChannels == 1) {
        double sum = 0.0;
        for (int c = 0; c < inputChannels; ++c) sum += static_cast<double>(input[c][sample]) / inputChannels;
        return static_cast<float>(sum);
    }
    if (inputChannels == 1 && streamChannels == 2) return static_cast<float>(input[0][sample]);
    return channel < inputChannels ? static_cast<float>(input[channel][sample]) : 0.0f;
}
}
