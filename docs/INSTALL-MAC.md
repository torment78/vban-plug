# Install VBAN Plug on a Mac

The Mac build contains VBAN Plug TX and RX in VST3 and Audio Unit (AU) formats.
Each bundle includes Apple Silicon (arm64) and Intel (x86_64) code. The build
targets macOS 11 or later; the release notes identify the systems actually tested.
Use AU for Logic Pro, or VST3 in a host that supports that format.

## Installer

Close the audio host. Open the macOS Universal .pkg installer, approve the
administrator prompt, and follow the installer. It places the plug-ins in:

- VST3: /Library/Audio/Plug-Ins/VST3
- AU: /Library/Audio/Plug-Ins/Components

The guide and licences go to /Library/Application Support/ElkaSoft/VBAN Plug.
The Mac installer uses Apple's normal installer interface.

## ZIP / manual installation

Extract the macOS Universal ZIP on the Mac. Inside its VBAN Plug folder:

- Copy both complete bundles from VST3 into ~/Library/Audio/Plug-Ins/VST3.
- For Logic, copy both complete bundles from Audio Units into
  ~/Library/Audio/Plug-Ins/Components instead.

In Finder, choose Go > Go to Folder to open these paths; create the final
folder if needed. The tilde means your home folder. Keep one installed copy
of each format, using either the installer or manual installation.

Copy whole .vst3/.component bundles, not individual files from inside Contents.
Reopen the host and rescan plug-ins. An AU host may need a logout/login to
refresh its component list.

## Approval for this test build

This build has no Developer ID signature and has not been notarized by Apple.
The binaries carry a local ad-hoc signature so Apple Silicon can load them;
this does not identify the developer or provide Apple approval.

macOS may block the installer or plug-in on first use. For a copy you obtained
from this project's release, use System Settings > Privacy & Security >
Open Anyway for the blocked item, then retry the installer or host scan.
Do not disable Gatekeeper globally.

Apple's instructions:
https://support.apple.com/102445

If your Mac or host still refuses it, record the exact message and the macOS
and host versions so the cause can be checked. A successful build/test does
not imply every host will accept a non-notarized plug-in.

## First audio test

Load TX and RX in your host, use 127.0.0.1 for both addresses, set the same UDP
port and stream name, and enable both. Use the same sample rate, such as
48000 Hz. Pick 1-8 Stream channels in TX and click Apply settings. RX detects
that count and shows that many meters.

For eight separate channels, configure the host's track/bus routing for
eight inputs/outputs. The available host connections and stream meter count
are independent. Avoid routing RX back into TX.

For another computer, use its LAN address. If macOS asks, allow your audio
host to access the local network. Check its Local Network and Firewall
permissions if loopback works but LAN audio does not.

TX preserves host audio. RX produces the received stream; missing signal is
silent. Network audio is for real-time playback/recording, not offline export.

## Remove

Close the audio host and remove only the VBAN Plug TX and RX bundles you
installed, from the matching VST3 and/or Components folders. For a .pkg
installation, the support files can also be removed from
/Library/Application Support/ElkaSoft/VBAN Plug.

Project/source: https://github.com/torment78/vban-plug
Optional donations: https://ko-fi.com/msffixit
VBAN Plug is by ElkaSoft; VBAN and Voicemeeter are by VB-Audio.
GNU AGPL v3; see LICENSE and NOTICE.md.
