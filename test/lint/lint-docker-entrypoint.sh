#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Run the tests of the Docker entrypoint (contrib/docker/test_entrypoint.sh): the
# subnet check and the RPC lines it writes into a new configuration file. They need
# no container and run in well under a second.

export LC_ALL=C

exec "$(dirname "$0")/../../contrib/docker/test_entrypoint.sh"
