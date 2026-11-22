#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Runs the tests of contrib/devtools/ci-gate.py, the rule each workflow's CI Gate
# job uses to skip a run that would repeat a pull request run on the identical
# tree. The decision takes its API replies as data, so the tests need no network.

export LC_ALL=C
exec python3 -I "$(dirname "$0")/../../contrib/devtools/test-ci-gate.py"
