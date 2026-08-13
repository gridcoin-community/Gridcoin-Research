#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Run contrib/devtools/security-check.py over the executables a build produced.
#
# Usage: check-binary-hardening.sh <build-dir>
#
# This exists so the release workflow has ONE copy of the invocation. The
# previous arrangement pasted the same block into five jobs, and they drifted in
# exactly the ways that matter: one lost the PEP-668 pip fallback, the macOS
# copies named a path that cannot exist for a bundle, and the depends copy listed
# both ELF and PE names so two of the four were always absent.
#
# Discovery, not a fixed list. The daemon and GUI are plain files on Linux, get a
# .exe on the Windows cross build, and on macOS the GUI is a MACOSX_BUNDLE whose
# Mach-O lives inside gridcoinresearch.app/Contents/MacOS/.
#
# Finding nothing is a failure. A hardening gate that inspects zero binaries and
# reports success is the failure mode this whole check was added to remove.

export LC_ALL=C
set -uo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <build-dir>" >&2
    exit 2
fi

BUILD_DIR="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="${BUILD_DIR}/bin"

if [ ! -d "$BIN_DIR" ]; then
    echo "::error::No bin directory at ${BIN_DIR}"
    exit 1
fi

# lief is the only runtime dependency. Newer distributions mark the system
# interpreter as externally managed (PEP 668), so try that form first and fall
# back for older images.
python3 -m pip install --quiet --user 'lief==0.17.5' --break-system-packages 2>/dev/null \
    || python3 -m pip install --quiet --user 'lief==0.17.5'

BINS=()
for name in gridcoinresearchd gridcoinresearch; do
    for candidate in \
        "${BIN_DIR}/${name}" \
        "${BIN_DIR}/${name}.exe" \
        "${BIN_DIR}/${name}.app/Contents/MacOS/${name}"
    do
        # -f rather than -e: a bundle directory is not the Mach-O we want.
        if [ -f "$candidate" ]; then
            BINS+=("$candidate")
        fi
    done
done

if [ "${#BINS[@]}" -eq 0 ]; then
    echo "::error::No Gridcoin executables found under ${BIN_DIR} to check"
    ls -la "$BIN_DIR" || true
    exit 1
fi

echo "Checking ${#BINS[@]} binary(ies):"
printf '  %s\n' "${BINS[@]}"

exec python3 "${SCRIPT_DIR}/security-check.py" "${BINS[@]}"
