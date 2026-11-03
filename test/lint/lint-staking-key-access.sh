#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Staking key access lint: the StakingKeyAccess factories are used only in the
# staking path each serves, the test peers only in the unit tests, and the
# friend lists in src/keystore.h are exactly the expected ones.

export LC_ALL=C
exec python3 "$(dirname "$0")/lint-staking-key-access.py"
