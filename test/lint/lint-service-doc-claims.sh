#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Keep the phrases listed in lint-service-doc-claims.py out of doc/, contrib/init/ and
# contrib/docker/. The phrases are checked against embedded samples first, so a phrase
# that has stopped matching anything fails the lint.

export LC_ALL=C

if ! command -v python3 > /dev/null; then
    echo "Skipping the service doc claims lint since python3 is not installed."
    exit 0
fi

DIR="$(dirname "$0")"
python3 -I "${DIR}/lint-service-doc-claims.py" --self-test || exit 1
exec python3 -I "${DIR}/lint-service-doc-claims.py"
