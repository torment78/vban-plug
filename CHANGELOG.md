# Changelog

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
