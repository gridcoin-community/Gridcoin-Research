#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Refused setting keys lint: share/gridcoin/refused-setting-keys.txt must follow
# its grammar, hold no duplicate, be installed by src/CMakeLists.txt, and be
# matched the same way by Python's fnmatch as by the C++ matcher.

export LC_ALL=C
exec python3 "$(dirname "$0")/lint-refused-setting-keys.py"
