#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Entry-point access classes lint: src/rpc/entry_point_classes.json must give
# every RPC row and every capnp interface method an access class, agree with
# each row's access column, and, for an IPC method that needs a capability,
# name the capability's kind in the method body.

export LC_ALL=C
exec python3 "$(dirname "$0")/lint-entry-point-classes.py" "$@"
