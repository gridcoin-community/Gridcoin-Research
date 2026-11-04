#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Staking key access lint: the StakingKeyAccess factories are used only in the
# staking path each serves and the tokens only by the staking paths, the test
# peers only in the unit tests, the friend lists in src/keystore.h are exactly
# the expected ones, and every file that reaches the wallet's keys is listed in
# src/test/data/key_entry_points.json. The self-test runs first, so a rule that
# no longer catches its sample fails here too.

export LC_ALL=C
python3 "$(dirname "$0")/lint-staking-key-access.py" --self-test >/dev/null || exit 1
exec python3 "$(dirname "$0")/lint-staking-key-access.py"
