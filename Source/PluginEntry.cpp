// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VbanProcessor(VBAND_IS_RX ? vband::Mode::receive : vband::Mode::send);
}
