#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Lock annotation lint: a lock annotation on a function definition in a .cpp
# file is also on every separate declaration of that function, and
# AssertLockHeld is not used outside src/sync.h and src/sync.cpp. The self-test
# runs first, so a rule that no longer catches its sample fails here too.

export LC_ALL=C
python3 "$(dirname "$0")/lint-lock-annotations.py" --self-test >/dev/null || exit 1
exec python3 "$(dirname "$0")/lint-lock-annotations.py"
