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
src/qt/addresstablemodel.cpp:util.h
src/qt/addresstablemodel.cpp:wallet/wallet.h
src/qt/addresstablemodel.h:wallet/ismine.h
src/qt/bitcoin.cpp:chainparamsbase.h
src/qt/bitcoin.cpp:chainparams.h
src/qt/bitcoin.cpp:gridcoin/gridcoin.h
src/qt/bitcoin.cpp:gridcoin/upgrade.h
src/qt/bitcoin.cpp:init.h
src/qt/bitcoin.cpp:node/ui_interface.h
src/qt/bitcoin.cpp:policy/fees.h
src/qt/bitcoin.cpp:txdb.h
src/qt/bitcoin.cpp:util.h
src/qt/bitcoin.cpp:validation.h
src/qt/bitcoingui.cpp:gridcoin/backup.h
src/qt/bitcoingui.cpp:gridcoin/staking/difficulty.h
src/qt/bitcoingui.cpp:init.h
src/qt/bitcoingui.cpp:main.h
src/qt/bitcoingui.cpp:node/psgt_pool.h
src/qt/bitcoingui.cpp:script/standard.h
src/qt/bitcoingui.cpp:util.h
src/qt/bitcoingui.cpp:wallet/wallet.h
src/qt/coincontroldialog.cpp:policy/fees.h
src/qt/coincontroldialog.cpp:policy/policy.h
src/qt/coincontroldialog.cpp:validation.h
src/qt/coincontroldialog.cpp:wallet/coincontrol.h
src/qt/coincontroldialog.cpp:wallet/wallet.h
src/qt/consolidateunspentdialog.cpp:util.h
src/qt/consolidateunspentwizard.cpp:util.h
src/qt/consolidateunspentwizardselectdestinationpage.cpp:util.h
src/qt/consolidateunspentwizardselectinputspage.cpp:policy/fees.h
src/qt/consolidateunspentwizardselectinputspage.cpp:policy/policy.h
src/qt/consolidateunspentwizardselectinputspage.cpp:validation.h
src/qt/consolidateunspentwizardselectinputspage.cpp:wallet/coincontrol.h
src/qt/consolidateunspentwizardselectinputspage.cpp:wallet/wallet.h
src/qt/consolidateunspentwizardsendpage.cpp:util.h
src/qt/detailedtxmodel.cpp:util/system.h
src/qt/diagnosticsdialog.h:sync.h
src/qt/diagnosticsdialog.h:wallet/diagnose.h
src/qt/guiutil.cpp:protocol.h
src/qt/guiutil.cpp:util.h
src/qt/intro.cpp:chainparams.h
src/qt/intro.cpp:util.h
src/qt/mrcmodel.cpp:gridcoin/contract/contract.h
src/qt/mrcmodel.cpp:gridcoin/contract/message.h
src/qt/mrcmodel.cpp:main.h
src/qt/mrcmodel.cpp:node/ui_interface.h
src/qt/mrcmodel.cpp:wallet/wallet.h
src/qt/mrcmodel.h:gridcoin/mrc.h
src/qt/multisigndialog.cpp:chainparams.h
src/qt/multisigndialog.cpp:main.h
src/qt/multisigndialog.cpp:net_processing.h
src/qt/multisigndialog.cpp:node/psgt_pool.h
src/qt/multisigndialog.cpp:psgt.h
src/qt/multisigndialog.cpp:script/interpreter.h
src/qt/multisigndialog.cpp:script/standard.h
src/qt/multisigndialog.cpp:sync.h
src/qt/multisigndialog.cpp:util.h
src/qt/multisigndialog.cpp:wallet/wallet.h
src/qt/multisigndialog.h:psgt.h
src/qt/optionsdialog.cpp:miner.h
src/qt/optionsdialog.cpp:netbase.h
src/qt/optionsmodel.cpp:init.h
src/qt/optionsmodel.cpp:miner.h
src/qt/peertablemodel.cpp:net.h
src/qt/peertablemodel.cpp:sync.h
src/qt/peertablemodel.h:net.h
src/qt/psgtpoolpage.cpp:chainparams.h
src/qt/psgtpoolpage.cpp:main.h
src/qt/psgtpoolpage.cpp:node/psgt_pool.h
src/qt/psgtpoolpage.cpp:sync.h
src/qt/psgtpooltablemodel.cpp:node/psgt_pool.h
src/qt/psgtpooltablemodel.cpp:psgt.h
src/qt/psgtpooltablemodel.cpp:script/standard.h
src/qt/psgtpooltablemodel.cpp:util.h
src/qt/psgtpooltablemodel.cpp:wallet/wallet.h
src/qt/psgtpooltablemodel.h:node/psgt_pool.h
src/qt/qtipcserver.cpp:node/ui_interface.h
src/qt/qtipcserver.cpp:util.h
src/qt/researcher/projecttablemodel.cpp:gridcoin/researcher.h
src/qt/researcher/researchermodel.cpp:chainparams.h
src/qt/researcher/researchermodel.cpp:gridcoin/beacon.h
src/qt/researcher/researchermodel.cpp:gridcoin/boinc.h
src/qt/researcher/researchermodel.cpp:gridcoin/magnitude.h
src/qt/researcher/researchermodel.cpp:gridcoin/project.h
src/qt/researcher/researchermodel.cpp:gridcoin/quorum.h
src/qt/researcher/researchermodel.cpp:gridcoin/researcher.h
src/qt/researcher/researchermodel.cpp:gridcoin/scraper/scraper.h
src/qt/researcher/researchermodel.cpp:gridcoin/superblock.h
src/qt/researcher/researchermodel.cpp:gridcoin/support/xml.h
src/qt/researcher/researchermodel.cpp:main.h
src/qt/researcher/researchermodel.cpp:node/ui_interface.h
src/qt/researcher/researcherwizardpoolpage.cpp:key.h
src/qt/rpcconsole.cpp:banman.h
src/qt/rpcconsole.cpp:rpc/client.h
src/qt/rpcconsole.cpp:rpc/protocol.h
src/qt/rpcconsole.cpp:rpc/server.h
src/qt/rpcconsole.h:net.h
src/qt/sendcoinsdialog.cpp:wallet/coincontrol.h
src/qt/sidestaketablemodel.cpp:gridcoin/support/enumbytes.h
src/qt/sidestaketablemodel.cpp:node/ui_interface.h
src/qt/sidestaketablemodel.h:gridcoin/sidestake.h
src/qt/signverifymessagedialog.cpp:init.h
src/qt/signverifymessagedialog.cpp:main.h
src/qt/signverifymessagedialog.cpp:wallet/wallet.h
src/qt/transactiondesc.cpp:gridcoin/tx_message.h
src/qt/transactiondesc.cpp:main.h
src/qt/transactiondesc.cpp:txdb.h
src/qt/transactiondesc.cpp:util.h
src/qt/transactiondesc.cpp:wallet/wallet.h
src/qt/transactiondesc.h:wallet/ismine.h
src/qt/transactionrecord.cpp:wallet/wallet.h
src/qt/transactionrecord.h:wallet/generated_type.h
src/qt/transactionrecord.h:wallet/ismine.h
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
src/qt/voting/votingmodel.cpp:chainparams.h
src/qt/voting/votingmodel.cpp:gridcoin/contract/contract.h
src/qt/voting/votingmodel.cpp:gridcoin/project.h
src/qt/voting/votingmodel.cpp:gridcoin/voting/builders.h
src/qt/voting/votingmodel.cpp:gridcoin/voting/payloads.h
src/qt/voting/votingmodel.cpp:gridcoin/voting/poll.h
src/qt/voting/votingmodel.cpp:gridcoin/voting/registry.h
src/qt/voting/votingmodel.cpp:gridcoin/voting/result.h
src/qt/voting/votingmodel.cpp:main.h
src/qt/voting/votingmodel.cpp:node/ui_interface.h
src/qt/voting/votingmodel.cpp:sync.h
src/qt/voting/votingmodel.h:gridcoin/voting/filter.h
src/qt/voting/votingmodel.h:gridcoin/voting/poll.h
src/qt/voting/votingmodel.h:gridcoin/voting/result.h
src/qt/walletmodel.cpp:gridcoin/tx_message.h
src/qt/walletmodel.cpp:main.h
src/qt/walletmodel.cpp:node/ui_interface.h
src/qt/walletmodel.cpp:util.h
src/qt/walletmodel.cpp:wallet/coincontrol.h
src/qt/walletmodel.cpp:wallet/wallet.h
src/qt/walletmodel.h:wallet/ismine.h
src/qt/wallettxstore.cpp:main.h
src/qt/wallettxstore.cpp:wallet/wallet.h
src/qt/wallettxstore.h:sync.h
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
