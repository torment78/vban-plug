#!/bin/bash
# Copyright (c) 2026 ElkaSoft
# SPDX-License-Identifier: AGPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != Darwin ]]; then
    echo "Build-Mac.sh must run on macOS with Xcode command-line tools." >&2
    exit 1
fi
cmake --preset macos-universal
cmake --build --preset macos-release --parallel "${VBAN_BUILD_JOBS:-2}"
python3 packaging/macos.py sign build/macos-universal
ctest --preset macos-release
python3 packaging/macos.py package build/macos-universal
