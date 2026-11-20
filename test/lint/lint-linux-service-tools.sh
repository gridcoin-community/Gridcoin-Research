#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Run the unit tests of the Linux service tools under contrib/init. The autounlock
# helper handles the wallet passphrase, so its decision order and fail-closed paths
# must not regress silently. The suites are stdlib-only Python 3 and run in well
# under a second, so they belong in the same always-on gate as the linters.

export LC_ALL=C

if ! command -v python3 > /dev/null; then
    echo "Skipping the Linux service tool tests since python3 is not installed."
    exit 0
fi

DIR="$(dirname "$0")/../../contrib/init"
EXIT_CODE=0

for test_file in "${DIR}"/test_*.py; do
    [ -e "${test_file}" ] || continue
    if ! python3 -I "${test_file}"; then
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
