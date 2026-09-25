# Install VBAN Plug TX and RX

Download the installer or portable ZIP from
[GitHub Releases](https://github.com/torment78/vban-plug/releases/tag/v0.2.0).
Version 0.2.0 is an unsigned Windows x64 multichannel test release.

## Installer

1. Close your audio host.
2. Run `VBAN-Plug-0.2.0-Windows-x64-Setup.exe`.
3. Approve Windows' administrator prompt for installation to the standard folder.
4. Review the destination and click Install.
5. Open your host and rescan VST3 plug-ins.

Both complete bundles go to `C:\Program Files\Common Files\VST3`.
The guide, licences, and uninstaller are kept separately in
`C:\Program Files\ElkaSoft\VBAN Plug`.

The dark installer includes the ElkaSoft logo and an optional Donate button.
That button opens https://ko-fi.com/msffixit. It never opens automatically,
and donating is not required.

## Portable ZIP

Extract the ZIP and open its `VBAN Plug` folder. Copy the entire
`VBAN Plug TX.vst3` and `VBAN Plug RX.vst3` directories to
`C:\Program Files\Common Files\VST3`, then rescan in the host.

A VST3 bundle is a folder. Keep its Contents, Resources, moduleinfo.json,
and x86_64-win files together. Do not copy just the inner binary.
Use the installer or manual ZIP copying, with only one copy of each plug-in
in your host's scan paths.

## First test

Load RX on one stereo track: sender IP `127.0.0.1`, port `6980`, stream `VBAN`.
Load TX on a separate track with audio and use the same destination IP, port,
and stream name. Enable network audio in both editors. Use the same nominal
sample rate in the sender and host, for example 48000 Hz.

TX's sent counter and RX's received counter should rise, and the stream meters
should move (one for mono, L/R for stereo). Avoid routing RX's output back into TX.

For two computers, use their actual LAN addresses. Allow the audio host through
Windows Firewall for the selected UDP port. The installer does not change
firewall rules or install Voicemeeter. No additional Microsoft C++ runtime is needed.

## Eight-channel setup

Both plug-ins advertise eight inputs and eight outputs. Configure the track/bus
or plug-in routing pins in your host for eight channels (often called 7.1), then
connect each TX input and RX output. Select **8 channels** in TX and click
**Apply settings**. RX detects the incoming count and displays eight thin meters.

The **Host I/O** row reports the layout selected by the host. The TX stream
drop-down does not change host routing. If the host is still stereo, TX sends
silence for inputs 3–8 and RX can only play channels 1–2, while still metering all
eight received channels. Some hosts support only mono/stereo inserts.

Any count from one to eight is supported. Numbered multichannel audio stays in
one stream. Mono TX averages configured host inputs; mono RX output averages
received channels. A mono stream/input is duplicated when adapting to stereo.

## Update or remove

Close the host and rerun the installer to update an installed copy, then rescan
so the host refreshes the new channel layouts. Existing stream settings remain
compatible; check the host's input/output routing when reopening older projects.
Remove an installer-managed installation through Windows Settings > Apps >
Installed apps > VBAN Plug TX + RX.

For a manual ZIP installation, remove only the two VBAN Plug bundle folders
you copied. Host project settings remain in the host's own project files.

See the [README](../README.md) for all settings and current limitations.
