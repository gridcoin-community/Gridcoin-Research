#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# IPC method dispatch lint (multiprocess design section 4.4/4.5): every
# interface method in src/ipc/capnp/*.capnp must declare
# "context :Proxy.Context". Without it the method body runs inline on the IPC
# event-loop thread, where a single contended lock stalls the entire channel.
# Neither capnp nor the generated code warns about the omission.

export LC_ALL=C
exec python3 "$(dirname "$0")/lint-capnp-context.py"
