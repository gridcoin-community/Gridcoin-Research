#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Run the stake-only autounlock helper's unit tests
# (contrib/wallettools/test_gridcoin_autounlock.py). The helper handles the wallet
# passphrase, so its security-critical behaviour -- the listener-ownership gate and
# its fail-closed paths, loopback classification, log redaction, and the unconditional
# stake-only flag on walletpassphrase -- must not regress silently. The suite is
# stdlib-only Python 3 and runs in well under a second, so it belongs in the same
# always-on gate as the linters rather than in the (build-dependent) functional suite.

export LC_ALL=C

if ! command -v python3 > /dev/null; then
    echo "Skipping the autounlock helper tests since python3 is not installed."
    exit 0
fi

exec python3 "$(dirname "$0")/../../contrib/wallettools/test_gridcoin_autounlock.py"
