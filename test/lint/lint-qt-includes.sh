#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Ratchet for the interfaces:: migration (doc/multiprocess_design.md §7):
# src/qt must not grow NEW direct includes of core headers -- new GUI code
# goes through src/interfaces/*.h instead. The allowlist below records the
# offenders that existed when the ratchet was introduced; entries may only
# ever be REMOVED (the lint fails on stale entries so the list shrinks as
# Phase 1 migrates each file). Flipping this from ratchet to empty-allowlist
# hard-fail is the gate to Phase 2.
#
# Known non-goals: relative-path evasions (../main.h) are not chased; code
# review owns those.

export LC_ALL=C

cd "$(git rev-parse --show-toplevel)" || exit 1

# chainparams.h / chainparamsbase.h / protocol.h were made stateless by the
# fTestNet extraction (#3204); netbase.h / sync.h / util.h by the remaining
# core-global extraction. They expose only stateless declarations now, so the
# GUI may include them directly and they are no longer forbidden here.
FORBIDDEN_RE='^[[:space:]]*#[[:space:]]*include[[:space:]]*["<](main\.h|validation\.h|init\.h|net\.h|net_processing\.h|miner\.h|txdb\.h|txdb-leveldb\.h|txmempool\.h|banman\.h|alert\.h|keystore\.h|key\.h|chain\.h|psgt\.h|node/[^">]+|util/system\.h|wallet/[^">]+|gridcoin/[^">]+|rpc/[^">]+|policy/[^">]+|script/[^">]+)[">]'

# Remaining file:header pairs. The Phase 1e burn-down has driven this down to
# the composition-root floor: every entry now lives in one of the three process
# entry points -- bitcoin.cpp (the app main / model wiring), bitcoingui.cpp, and
# upgradeqt.cpp (the standalone updater) -- which legitimately drive core
# start-up, shutdown and the updater and so are not themselves migrated onto
# src/interfaces/*.h. Flipping the ratchet to a hard-fail against exactly this
# floor is the Phase 1f gate.
#
# DO NOT ADD ENTRIES -- migrate the file onto src/interfaces/*.h instead. (Sole
# exception: an entry may be added when a migration surfaces PRE-EXISTING
# coupling that was previously hidden behind a transitive include, e.g.
# rpcconsole.cpp:banman.h -- the coupling is old, only the include is new.)
ALLOWLIST="\
src/qt/bitcoin.cpp:gridcoin/gridcoin.h
src/qt/bitcoin.cpp:gridcoin/upgrade.h
src/qt/bitcoin.cpp:init.h
src/qt/bitcoin.cpp:node/shutdown.h
src/qt/bitcoin.cpp:node/ui_interface.h
src/qt/bitcoin.cpp:policy/fees.h
src/qt/bitcoin.cpp:txdb.h
src/qt/bitcoin.cpp:validation.h
src/qt/bitcoingui.cpp:init.h
src/qt/upgradeqt.cpp:gridcoin/upgrade.h
"

EXIT_CODE=0

# git wildmatch '*' crosses '/' in ls-files pathspecs, so these two patterns
# also cover the researcher/, voting/ and test/ subdirectories.
CURRENT=$(git ls-files 'src/qt/*.cpp' 'src/qt/*.h' | sort -u | while read -r f; do
    grep -E "$FORBIDDEN_RE" "$f" 2>/dev/null \
        | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]([^">]+)[">].*/\1/' \
        | while read -r h; do
            echo "$f:$h"
        done
done | sort -u)

NEW_VIOLATIONS=$(comm -13 <(echo "$ALLOWLIST" | grep -v '^$' | sort -u) <(echo "$CURRENT" | grep -v '^$'))
STALE_ENTRIES=$(comm -23 <(echo "$ALLOWLIST" | grep -v '^$' | sort -u) <(echo "$CURRENT" | grep -v '^$'))

if [ -n "$NEW_VIOLATIONS" ]; then
    echo "New direct core include(s) in src/qt (file:header):"
    echo "$NEW_VIOLATIONS"
    echo
    echo "GUI code must consume core state through src/interfaces/*.h (see"
    echo "src/interfaces/README.md and doc/multiprocess_design.md). Do not add"
    echo "these includes to the allowlist in $(basename "${BASH_SOURCE[0]}")."
    EXIT_CODE=1
fi

if [ -n "$STALE_ENTRIES" ]; then
    echo "Stale allowlist entr(y/ies) in $(basename "${BASH_SOURCE[0]}") -- the include is gone; remove the entry so the ratchet tightens:"
    echo "$STALE_ENTRIES"
    EXIT_CODE=1
fi

exit $EXIT_CODE
