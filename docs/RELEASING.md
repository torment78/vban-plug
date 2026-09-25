# Building a release

Run `./Build-Release.ps1` from the repository, or `./Build.ps1 -Package`. This uses Visual Studio 2026 Insiders, builds Release, runs the audio/UDP/VST3 tests, and then produces the installer and both ZIPs in `out/releases/<version>`:

- `VBAN-Plug-<version>-Windows-x64-Setup.exe`
- `VBAN-Plug-<version>-Windows-x64-Portable.zip` (one top-level `VBAN Plug` folder)
- `VBAN-Plug-<version>-Source.zip` (project plus the exact JUCE source tree)
- `SHA256.txt` and `START HERE.txt`

Inno Setup 6.6 or later is required. It is discovered on PATH or in `Program Files (x86)/Inno Setup 6`. Override its location with `-InnoCompiler`. `-SkipBuild` repackages an already tested Release build; it does not rebuild or rerun audio tests. The version comes from CMakeLists.txt. Intermediate staging directories are kept under ignored `out/staging` so stale files cannot leak into a release.

The Microsoft C++ runtime is linked statically. No redistributable download is needed. Both complete VST3 bundles and third-party notices are included. The public test installer is unsigned. The project uses AGPLv3; LICENSE and NOTICE.md are included in the packages.

## Installation and removal

Normal installation requests administrator access, places the two VST3 bundles directly in `C:\Program Files\Common Files\VST3`, and keeps the guide, artwork, and uninstaller separately in `C:\Program Files\ElkaSoft\VBAN Plug`. The standard path follows [Steinberg's plug-in location specification](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html). The setup wizard shows both this destination and the path to the installer that was opened. Re-running setup updates the same product. Windows Installed apps provides removal. The uninstaller removes only files installed by this setup, leaving unrelated plug-ins and user files alone.

The Donate button opens ElkaSoft's existing `https://ko-fi.com/msffixit` page in the user's browser. It is optional; no page opens automatically. The installer does not install Voicemeeter or change firewall rules. VBAN Plug is by ElkaSoft; VBAN and Voicemeeter are by VB-Audio.

## Isolated package verification

Run `./packaging/Test-Release.ps1` after packaging. It expands the ZIP, verifies its manifest, installs the exact setup executable twice into a fresh temporary folder under `build/package-tests`, compares both installed bundles with the ZIP, loads them in the test host, then uninstalls them. A sentinel file checks that unrelated files are retained. Logs and editor previews remain in the test directory.

The hidden command-line `/CURRENTUSER /DIR=...` mode routes plug-ins to `<DIR>/VST3` for this verification. Normal double-click setup always uses the standard all-users VST3 location. The test requires that no previous current-user test registration exists, so it cannot replace one accidentally. It does not install into the real VST3 folder.

## Artwork for the later GitHub setup

`docs/images` contains vertical and wide PNG masters, each tagged/untagged. Both variants retain the ElkaSoft maker credit; tagged variants also show the Voicemeeter logo and `VBAN protocol by VB-Audio`. The README uses `social-preview-tagged.jpg`. Both social-preview JPGs are 1280 x 640 and below 1 MB, matching [GitHub's recommended preview size](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/customizing-your-repositorys-social-media-preview). The public repository is https://github.com/torment78/vban-plug. See `installer/ASSETS.md` for asset provenance and generation prompts.
