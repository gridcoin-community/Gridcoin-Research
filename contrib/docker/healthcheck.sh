#!/usr/bin/env bash

export LC_ALL=C

# Docker runs the check as the image's user, root. The client asks as the
# account the daemon runs as (entrypoint.sh), with the daemon's own data
# directory: as root it would find no RPC credentials, and it refuses an RPC
# listener another account owns.
gosu gridcoin:gridcoin env HOME=/home/gridcoin \
    gridcoinresearchd -datadir=/home/gridcoin/.GridcoinResearch getblockchaininfo > /dev/null 2>&1 || exit 1
