#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# No official build may enable the QML front end before the cut-over; see
# lint-qml-release.py for what is checked.

export LC_ALL=C

exec python3 "$(dirname "$0")/lint-qml-release.py"
