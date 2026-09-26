# Changelog

## 0.3.0-dev — Windows and Mac dev build

- Added universal Apple Silicon/Intel Mac VST3 and Audio Unit builds for TX and RX.
- Separate Windows and Mac installers, portable ZIPs, source archives, and checksums.
- Mac builds use local ad-hoc signatures without Developer ID signing or notarization.
- Added a manually triggered Mac build workflow with native Apple Silicon and Intel validation.
- Stream routing, 1–8 channel selection, and automatic RX metering are unchanged.

## 0.2.1 — RX meters follow the detected stream

- RX shows no channel bars until it detects a matching stream, instead of initially showing the host's channel count.
- Once detected, RX shows exactly one meter per stream channel selected in TX, including when the host keeps eight connections available.
- Disabling RX hides its channel bars; live channel-count changes continue to update automatically.
- Added waiting-state and three/four-channel stream checks with eight-channel host buses.

## 0.2.0 — One stream, up to eight channels

- TX sends 1–8 numbered channels in one VBAN stream, with PCM16 or PCM24.
- Both VST3s advertise eight host inputs and outputs and accept 1–8 channel layouts.
- TX has a Stream channels selector and both editors show the host I/O count.
- Meters follow the stream: one mono bar, stereo L/R, or 3–8 thinner numbered bars.
- RX detects the format/channel count and meters all received channels, including when the host exposes fewer outputs.
- Mono mixing, mono-to-stereo duplication, numbered routing, silent padding, and saved mono/stereo state remain supported.
- Audio tests cover all channel counts, packet boundaries/order, live format changes, host layouts, and float/double pass-through.

## 0.1.1 — Initial public MVP release

- Separate VBAN Plug TX and RX effects for Windows x64 VST3 hosts.
- TX preserves host audio and sends PCM16/24, mono or stereo, to one recipient.
- RX filters one sender and stream, detects PCM format/channels, and plays incoming audio.
- Horizontal L/R meters, peak/clip indicators, and sent/received packet counters.
- Dark ElkaSoft installer with a Donate button and standard VST3 installation.
- Portable bundles, source archive with JUCE, and SHA256 checksums.
- Matching vertical and wide artwork with distinct ElkaSoft and VB-Audio credits.
- AGPLv3 source release, documentation, bug/feature templates, and Ko-fi support links.

Automated audio, VST3 loading, and isolated installer/reinstall/uninstall checks passed.
This initial release is unsigned. RX requires the sender and host to use the same
nominal sample rate; it does not convert between different nominal sample rates.
