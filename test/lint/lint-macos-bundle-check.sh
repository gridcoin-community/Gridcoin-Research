#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Runs the tests of contrib/devtools/check-macos-bundle.py, the check the macOS
# CI jobs apply to the deployed bundle. The rules are plain functions, so the
# tests run anywhere; this is the only place they run off a Mac.

export LC_ALL=C
exec python3 -I "$(dirname "$0")/../../contrib/devtools/test-check-macos-bundle.py"
