#!/usr/bin/env python3
# Copyright (c) 2026 ElkaSoft
# SPDX-License-Identifier: AGPL-3.0-only
"""Create universal Mac plug-in packages without Developer ID credentials."""
import hashlib
import json
import pathlib
import plistlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
VERSION = re.search(r"project\(VBANPlug VERSION ([\d.]+)", (ROOT / "CMakeLists.txt").read_text()).group(1)
FORMATS = (("VST3", "vst3"), ("AU", "component"))
NOTICES = {
    "JUCE-LICENSE.md": "LICENSE.md",
    "VST3-SDK-LICENSE.txt": "modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt",
    "VST3-interfaces-LICENSE.txt": "modules/juce_audio_processors_headless/format_types/VST3_SDK/pluginterfaces/LICENSE.txt",
    "VST3-base-LICENSE.txt": "modules/juce_audio_processors_headless/format_types/VST3_SDK/base/LICENSE.txt",
    "VST3-public-SDK-LICENSE.txt": "modules/juce_audio_processors_headless/format_types/VST3_SDK/public.sdk/LICENSE.txt",
    "PNG-LICENSE.txt": "modules/juce_graphics/image_formats/pnglib/LICENSE",
    "zlib-LICENSE.txt": "modules/juce_core/zip/zlib/LICENSE",
    "SheenBidi-LICENSE.txt": "modules/juce_graphics/unicode/sheenbidi/LICENSE",
    "JPEG-README.txt": "modules/juce_graphics/image_formats/jpglib/README",
    "HarfBuzz-COPYING.txt": "modules/juce_graphics/fonts/harfbuzz/COPYING",
    "FLAC-LICENSE.txt": "modules/juce_audio_formats/codecs/flac/Flac Licence.txt",
    "Ogg-Vorbis-LICENSE.txt": "modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt",
}

def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)

def digest(path):
    with open(path, "rb") as stream:
        result = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
        return result.hexdigest()

def executable(bundle):
    with open(bundle / "Contents/Info.plist", "rb") as stream:
        return bundle / "Contents/MacOS" / plistlib.load(stream)["CFBundleExecutable"]

def bundles(build):
    for direction in ("TX", "RX"):
        for fmt, ext in FORMATS:
            yield build / ("VBANPlug" + direction + "_artefacts/Release") / fmt / ("VBAN Plug " + direction + "." + ext)

def verify(bundle):
    run("lipo", "-verify_arch", "arm64", "x86_64", executable(bundle))
    run("codesign", "--verify", "--strict", bundle)
    signature = run("codesign", "--display", "--verbose=2", bundle, capture_output=True, text=True).stderr
    if "Signature=adhoc" not in signature:
        raise RuntimeError("Expected local ad-hoc signing only: " + str(bundle))

def sign(build):
    # Apple Silicon requires a valid code signature. Ad-hoc signing needs no
    # certificate, account, identity verification, timestamp, or notarization.
    for bundle in bundles(build):
        run("codesign", "--force", "--sign", "-", "--timestamp=none", bundle)
        verify(bundle)
    tester = build / "VBANPlugTests_artefacts/Release/VBANPlugTests"
    run("codesign", "--force", "--sign", "-", "--timestamp=none", tester)

def package(build):
    output = ROOT / "out/releases" / VERSION / "macos"
    output.mkdir(parents=True, exist_ok=True)
    staging = ROOT / "out/staging"
    staging.mkdir(parents=True, exist_ok=True)
    stage = pathlib.Path(tempfile.mkdtemp(prefix="macos-", dir=staging))
    payload = stage / "portable/VBAN Plug"
    payload.mkdir(parents=True)
    for bundle in bundles(build):
        verify(bundle)
        folder = "VST3" if bundle.suffix == ".vst3" else "Audio Units"
        shutil.copytree(bundle, payload / folder / bundle.name, symlinks=True)
    for name in ("README.md", "LICENSE", "NOTICE.md", "CHANGELOG.md"):
        shutil.copy2(ROOT / name, payload / name)
    shutil.copytree(ROOT / "docs", payload / "docs")
    shutil.copy2(ROOT / "docs/INSTALL-MAC.md", payload / "INSTALL MAC.md")
    cache = (build / "CMakeCache.txt").read_text()
    juce = pathlib.Path(re.search(r"^JUCE_SOURCE_DIR:STATIC=(.+)$", cache, re.M).group(1).strip())
    notice_dir = payload / "Third-party notices"
    notice_dir.mkdir()
    for name, source in NOTICES.items():
        shutil.copy2(juce / source, notice_dir / name)
    commit = run("git", "rev-parse", "HEAD", cwd=ROOT, capture_output=True, text=True).stdout.strip()
    (payload / "BUILD INFO.txt").write_text(
        "VBAN Plug " + VERSION + "\nSource commit: " + commit +
        "\nArchitectures: arm64 + x86_64\nMinimum deployment target: macOS 11.0\n"
        "Signature: ad-hoc only; no Developer ID, no notarization.\n"
        "Source: https://github.com/torment78/vban-plug\n")
    manifest = {str(p.relative_to(payload)): digest(p) for p in sorted(payload.rglob("*")) if p.is_file()}
    (payload / "SHA256.json").write_text(json.dumps(manifest, indent=2) + "\n")
    portable = output / ("VBAN-Plug-" + VERSION + "-macOS-Universal.zip")
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", payload, portable)

    pkgroot = stage / "installer"
    for folder, target in (("VST3", "VST3"), ("Audio Units", "Components")):
        shutil.copytree(payload / folder, pkgroot / "Library/Audio/Plug-Ins" / target, symlinks=True)
    support = pkgroot / "Library/Application Support/ElkaSoft/VBAN Plug"
    support.mkdir(parents=True)
    for name in ("INSTALL MAC.md", "LICENSE", "NOTICE.md", "BUILD INFO.txt"):
        shutil.copy2(payload / name, support / name)
    shutil.copytree(notice_dir, support / "Third-party notices")
    component_list = stage / "components.plist"
    run("pkgbuild", "--analyze", "--root", pkgroot, component_list)
    with open(component_list, "rb") as stream:
        components = plistlib.load(stream)
    for component in components:
        component["BundleIsRelocatable"] = False
    with open(component_list, "wb") as stream:
        plistlib.dump(components, stream)
    installer = output / ("VBAN-Plug-" + VERSION + "-macOS-Universal.pkg")
    run("pkgbuild", "--root", pkgroot, "--component-plist", component_list,
        "--identifier", "com.elkasoft.vbanplug", "--version", VERSION,
        "--install-location", "/", "--ownership", "recommended", installer)

    source = stage / "source/VBAN Plug"
    source.mkdir(parents=True)
    tracked = run("git", "ls-files", "-z", cwd=ROOT, capture_output=True).stdout.decode().split("\0")
    for name in filter(None, tracked):
        relative = pathlib.PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise RuntimeError("Invalid source path: " + name)
        target = source / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, target)
    shutil.copytree(juce, source / "external/JUCE", ignore=shutil.ignore_patterns(".git"), symlinks=True)
    (source / "BUILD FROM SOURCE.txt").write_text(
        "On a Mac with Xcode command-line tools, CMake 4.2+ and Ninja, run:\n"
        "bash Build-Mac.sh\n\nThis source includes the exact JUCE dependency.\n"
        "To create packages, initialize a local repository first if this is an archive:\n"
        "git init && git add . && git -c user.name=Builder -c user.email=builder@localhost commit -m Source\n"
        "Then run bash Build-Mac.sh. The Mac packages use ad-hoc signing only.\n")
    source_zip = output / ("VBAN-Plug-" + VERSION + "-macOS-Source.zip")
    run("ditto", "-c", "-k", "--keepParent", source, source_zip)
    (output / "SHA256-macOS.txt").write_text("".join(digest(p) + "  " + p.name + "\n" for p in (portable, installer, source_zip)))
    # CI uses the same universal executable to test the packaged/installed bundles
    # natively on both processor families. It is not a public release download.
    with tarfile.open(output / "macos-test-kit.tar.gz", "w:gz") as archive:
        archive.add(build / "VBANPlugTests_artefacts/Release/VBANPlugTests", arcname="VBANPlugTests")
    print("Mac packages ready:", output)

if __name__ == "__main__":
    if sys.platform != "darwin":
        raise SystemExit("This packaging script runs on macOS.")
    action, directory = sys.argv[1:]
    build_dir = pathlib.Path(directory).resolve()
    {"sign": sign, "package": package}[action](build_dir)
