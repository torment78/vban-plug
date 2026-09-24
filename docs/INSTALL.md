# Install VBAN Plug TX and RX

Download the installer or portable ZIP from
[GitHub Releases](https://github.com/torment78/vban-plug/releases/tag/v0.1.1).
Version 0.1.1 is an unsigned Windows x64 MVP test release.

## Installer

1. Close your audio host.
2. Run `VBAN-Plug-0.1.1-Windows-x64-Setup.exe`.
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

TX's sent counter and RX's received counter should rise, and the L/R meters
should move. Avoid routing RX's output back into TX.

For two computers, use their actual LAN addresses. Allow the audio host through
Windows Firewall for the selected UDP port. The installer does not change
firewall rules or install Voicemeeter. No additional Microsoft C++ runtime is needed.

## Update or remove

Close the host and rerun the installer to update an installed copy.
Remove an installer-managed installation through Windows Settings > Apps >
Installed apps > VBAN Plug TX + RX.

For a manual ZIP installation, remove only the two VBAN Plug bundle folders
you copied. Host project settings remain in the host's own project files.

See the [README](../README.md) for all settings and current limitations.
