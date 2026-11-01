#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Enforce the RPC heritage ledger (issue #3069): every RPC registered in
# vRPCCommands carries a heritage bucket + fingerprint baseline as a column on its
# row -- { "name", &impl, cat, &help, heritage_<bucket>, "<fp>" } -- and the
# fingerprinted buckets (pure-upstream/mixed/removed-upstream) must match their
# recorded surface fingerprint (args + result keys). A drift means the RPC's surface
# changed -- re-confirm its heritage bucket and update the row + doc/rpc-heritage.md.

export LC_ALL=C
exec python3 "$(dirname "$0")/lint-rpc-heritage.py"
