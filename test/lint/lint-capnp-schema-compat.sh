#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Cap'n Proto IPC schema backward-compatibility lint (multiprocess design
# section 4.2): the src/ipc/capnp/*.capnp ordinals and file ids define the IPC
# wire format and must stay append-only across releases. This diffs each schema
# against the most recent mainnet release tag and fails on a removed/renamed
# ordinal or a changed file id. New schemas (absent at the baseline release) pass.

export LC_ALL=C
exec python3 "$(dirname "$0")/lint-capnp-schema-compat.py"
