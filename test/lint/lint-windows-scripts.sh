#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Static checks of the Windows scripts in contrib/windows and the NSIS template (see
# lint-windows-scripts.py). The rules are checked against embedded bad samples first,
# so a rule that has stopped detecting anything fails the lint.

export LC_ALL=C

if ! command -v python3 > /dev/null; then
    echo "Skipping the Windows scripts lint since python3 is not installed."
    exit 0
fi

DIR="$(dirname "$0")"
python3 -I "${DIR}/lint-windows-scripts.py" --self-test || exit 1
exec python3 -I "${DIR}/lint-windows-scripts.py"
