#!/usr/bin/env python3
# Copyright (c) 2026 ElkaSoft
# SPDX-License-Identifier: AGPL-3.0-only
"""Validate the universal ZIP and installer on a disposable GitHub Mac runner."""
import argparse
import json
import os
import pathlib
import platform
import plistlib
import subprocess
import tempfile
from macos import ROOT, VERSION, digest, executable, run, verify

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("release_directory", type=pathlib.Path)
    args = parser.parse_args()
    if platform.system() != "Darwin" or os.environ.get("GITHUB_ACTIONS") != "true":
        raise SystemExit("Installation validation is restricted to disposable GitHub Actions Mac runners.")
    release = args.release_directory.resolve()
    logs = ROOT / "out/mac-validation"
    logs.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="verify-", dir=logs))
    for line in (release / "SHA256-macOS.txt").read_text().splitlines():
        expected, name = line.split("  ", 1)
        if pathlib.Path(name).name != name or digest(release / name) != expected:
            raise RuntimeError("Release checksum mismatch: " + name)
    run("ditto", "-x", "-k", release / ("VBAN-Plug-" + VERSION + "-macOS-Universal.zip"), work)
    payload = work / "VBAN Plug"
    manifest = json.loads((payload / "SHA256.json").read_text())
    for name, expected in manifest.items():
        path = (payload / name).resolve()
        if not path.is_relative_to(payload.resolve()) or digest(path) != expected:
            raise RuntimeError("Portable checksum mismatch: " + name)
    originals = []
    for folder, target, ext in (("VST3", "VST3", "vst3"), ("Audio Units", "Components", "component")):
        for direction in ("TX", "RX"):
            bundle = payload / folder / ("VBAN Plug " + direction + "." + ext)
            verify(bundle)
            with open(bundle / "Contents/Info.plist", "rb") as stream:
                info = plistlib.load(stream)
            if info["CFBundleShortVersionString"] != VERSION:
                raise RuntimeError("Unexpected bundle version")
            destination = pathlib.Path("/Library/Audio/Plug-Ins") / target / bundle.name
            if destination.exists():
                raise RuntimeError("Refusing to replace an existing plug-in on the runner")
            originals.append((bundle, destination))
    installer = release / ("VBAN-Plug-" + VERSION + "-macOS-Universal.pkg")
    run("sudo", "installer", "-pkg", installer, "-target", "/")
    for original, installed in originals:
        verify(installed)
        for file in original.rglob("*"):
            if file.is_file() and digest(file) != digest(installed / file.relative_to(original)):
                raise RuntimeError("Installed payload differs: " + str(file))
    kit = work / "test-kit"
    kit.mkdir()
    run("tar", "-xzf", release / "macos-test-kit.tar.gz", "-C", kit)
    tester = kit / "VBANPlugTests"
    run("lipo", "-verify_arch", "arm64", "x86_64", tester)
    with open(logs / "audio-vst3.txt", "w") as stream:
        run(tester, logs / "editor-previews",
            "/Library/Audio/Plug-Ins/VST3/VBAN Plug TX.vst3",
            "/Library/Audio/Plug-Ins/VST3/VBAN Plug RX.vst3",
            stdout=stream, stderr=subprocess.STDOUT, timeout=180)
    for direction, code in (("TX", "Vbtx"), ("RX", "Vbrx")):
        with open(logs / ("auval-" + direction + ".txt"), "w") as stream:
            run("auval", "-v", "aufx", code, "Elka", stdout=stream, stderr=subprocess.STDOUT, timeout=120)
    result = "PASS on " + platform.machine() + ": universal architectures, ad-hoc signatures, ZIP hashes, installer payload, audio/VST3 host checks, and AU validation.\n"
    (logs / "RESULT.txt").write_text(result)
    print(result)

if __name__ == "__main__":
    main()
