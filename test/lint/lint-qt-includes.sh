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

FORBIDDEN_RE='^[[:space:]]*#[[:space:]]*include[[:space:]]*["<](main\.h|validation\.h|init\.h|net\.h|net_processing\.h|netbase\.h|miner\.h|txdb\.h|txdb-leveldb\.h|txmempool\.h|banman\.h|alert\.h|keystore\.h|key\.h|chain\.h|chainparams\.h|chainparamsbase\.h|protocol\.h|psgt\.h|sync\.h|util\.h|node/[^">]+|util/system\.h|wallet/[^">]+|gridcoin/[^">]+|rpc/[^">]+|policy/[^">]+|script/[^">]+)[">]'

# file:header pairs present when the ratchet was introduced. DO NOT ADD
# ENTRIES -- migrate the file onto src/interfaces/*.h instead. (Sole
# exception: an entry may be added when a migration surfaces PRE-EXISTING
# coupling that was previously hidden behind a transitive include, e.g.
# rpcconsole.cpp:banman.h -- the coupling is old, only the include is new.)
ALLOWLIST="\
src/qt/aboutdialog.cpp:util.h
src/qt/bitcoin.cpp:chainparamsbase.h
src/qt/bitcoin.cpp:chainparams.h
src/qt/bitcoin.cpp:gridcoin/gridcoin.h
src/qt/bitcoin.cpp:gridcoin/upgrade.h
src/qt/bitcoin.cpp:init.h
src/qt/bitcoin.cpp:node/shutdown.h
src/qt/bitcoin.cpp:node/ui_interface.h
src/qt/bitcoin.cpp:policy/fees.h
src/qt/bitcoin.cpp:txdb.h
src/qt/bitcoin.cpp:util.h
src/qt/bitcoin.cpp:validation.h
src/qt/bitcoingui.cpp:init.h
src/qt/bitcoingui.cpp:util.h
src/qt/consolidateunspentdialog.cpp:util.h
src/qt/consolidateunspentwizard.cpp:util.h
src/qt/consolidateunspentwizardselectdestinationpage.cpp:util.h
src/qt/consolidateunspentwizardsendpage.cpp:util.h
src/qt/detailedtxmodel.cpp:util/system.h
src/qt/diagnosticsdialog.h:sync.h
src/qt/diagnosticsdialog.h:wallet/diagnose.h
src/qt/guiutil.cpp:protocol.h
src/qt/guiutil.cpp:util.h
src/qt/intro.cpp:chainparams.h
src/qt/intro.cpp:util.h
src/qt/optionsdialog.cpp:miner.h
src/qt/optionsdialog.cpp:netbase.h
src/qt/optionsmodel.cpp:miner.h
src/qt/qtipcserver.cpp:node/ui_interface.h
src/qt/qtipcserver.cpp:util.h
src/qt/researcher/researcherwizardpoolpage.cpp:key.h
src/qt/transactiontablemodel.cpp:util.h
src/qt/transactionview.cpp:util/system.h
src/qt/updatedialog.h:gridcoin/upgrade.h
src/qt/upgradeqt.cpp:gridcoin/upgrade.h
src/qt/upgradeqt.cpp:util.h
src/qt/voting/polltab.h:gridcoin/voting/filter.h
src/qt/voting/polltablemodel.cpp:util.h
src/qt/voting/polltablemodel.h:gridcoin/voting/filter.h
src/qt/voting/poll_types.cpp:gridcoin/voting/poll.h
src/qt/voting/pollwizarddetailspage.cpp:main.h
src/qt/voting/votingmodel.cpp:gridcoin/voting/poll.h
src/qt/voting/votingmodel.h:gridcoin/voting/filter.h
src/qt/voting/votingmodel.h:gridcoin/voting/poll.h
src/qt/walletmodel.cpp:node/ui_interface.h
src/qt/walletmodel.cpp:util.h
src/qt/winshutdownmonitor.cpp:init.h
src/qt/winshutdownmonitor.cpp:util.h
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
