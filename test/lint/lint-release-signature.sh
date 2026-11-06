#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Test the signed SHA256SUMS tools in contrib/release, and refuse a release-keys.txt that
# pins the committed TEST-ONLY key. Skipped when python3 or the cryptography module is missing.

export LC_ALL=C

if ! command -v python3 > /dev/null; then
    echo "Skipping the release signature lint since python3 is not installed."
    exit 0
fi
if ! python3 -c 'import cryptography' 2> /dev/null; then
    echo "Skipping the release signature lint since the python cryptography module is not installed."
    exit 0
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REL="${ROOT}/contrib/release"

python3 -B "${REL}/test_release_signature.py" || exit 1

TEST_KEYID="$(awk '!/^#/ && NF {print $1; exit}' "${REL}/testdata/test-key.pub.txt")"
if [ -z "${TEST_KEYID}" ]; then
    echo "The test key id cannot be read from testdata/test-key.pub.txt."
    exit 1
fi
if grep -q "${TEST_KEYID}" "${ROOT}/contrib/windows/release-keys.txt"; then
    echo "contrib/windows/release-keys.txt pins the TEST-ONLY key ${TEST_KEYID}."
    exit 1
fi
echo "lint-release-signature: ok"
