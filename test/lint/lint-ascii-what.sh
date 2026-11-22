#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Checks that the Windows-only sources print an exception's what() through AsciiWhat (see
# lint-ascii-what.py). The rule is checked against embedded samples first.

export LC_ALL=C

if ! command -v python3 > /dev/null; then
    echo "Skipping the ascii what() lint since python3 is not installed."
    exit 0
fi

DIR="$(dirname "$0")"
python3 -I "${DIR}/lint-ascii-what.py" --self-test || exit 1
exec python3 -I "${DIR}/lint-ascii-what.py"
