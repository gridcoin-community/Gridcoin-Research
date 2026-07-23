#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Phase 1f GATE for the interfaces:: migration (doc/multiprocess_design.md §7).
# The Phase 1e burn-down drove the old shrinking allowlist to the
# composition-root floor; this is the flip to the terminal hard-fail gate:
#
#   Every src/qt file EXCEPT the composition root must reach core state only
#   through src/interfaces/*.h. A direct include of a core header is a hard
#   failure -- there is NO allowlist to grow.
#
# The composition root (below) is the ONE documented exemption: the GUI-process
# entry points that legitimately wire core start-up/shutdown and the updater.
# They are excluded from the scan, not allowlisted, so no per-header bookkeeping
# survives here.
#
# Surface-area policy (jco): keep the forbidden set BROAD. Even where a core
# header is stateless, minimizing the core-header surface the GUI links against
# directly is the goal -- so a header is NOT released from the forbidden set just
# because it carries no state. Only the genuinely-needed stateless value/utility
# headers the GUI already includes (amount.h, uint256.h, chainparams.h, sync.h,
# netbase.h, protocol.h, util.h, ...) stay outside FORBIDDEN_RE.
#
# Known non-goals: relative-path evasions (../main.h) are not chased; code
# review owns those.

export LC_ALL=C

cd "$(git rev-parse --show-toplevel)" || exit 1

# chainparams.h / chainparamsbase.h / protocol.h were made stateless by the
# fTestNet extraction (#3204); netbase.h / sync.h / util.h by the remaining
# core-global extraction. They expose only stateless declarations the GUI
# already relies on directly, so they stay out of the forbidden set. Everything
# else core-side (including nominally-stateless key.h / util/system.h) stays
# forbidden per the surface-area policy above.
FORBIDDEN_RE='^[[:space:]]*#[[:space:]]*include[[:space:]]*["<](main\.h|validation\.h|init\.h|net\.h|net_processing\.h|miner\.h|txdb\.h|txdb-leveldb\.h|txmempool\.h|banman\.h|alert\.h|keystore\.h|key\.h|chain\.h|psgt\.h|node/[^">]+|util/system\.h|wallet/[^">]+|gridcoin/[^">]+|rpc/[^">]+|policy/[^">]+|script/[^">]+)[">]'

# The composition root: the three GUI-process entry points (app main / model
# wiring, the main window, the standalone updater). They own the process wiring
# -- choosing MakeGridcoinInit()/the IPC Init proxy and driving start-up,
# shutdown and the updater -- so they legitimately see core headers and are the
# ONE documented exemption. Excluded from the scan below. This is NOT an
# allowlist: it is a fixed set of files, not file:header pairs, and it does not
# grow as the GUI evolves.
COMPOSITION_ROOT_RE='^src/qt/(bitcoin|bitcoingui|upgradeqt)\.cpp$'

# git wildmatch '*' crosses '/' in ls-files pathspecs, so these two patterns
# also cover the researcher/, voting/ and test/ subdirectories. The composition
# root is filtered out; every other file is scanned.
VIOLATIONS=$(git ls-files 'src/qt/*.cpp' 'src/qt/*.h' | grep -vE "$COMPOSITION_ROOT_RE" | sort -u | while read -r f; do
    grep -E "$FORBIDDEN_RE" "$f" 2>/dev/null \
        | sed -E 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]([^">]+)[">].*/\1/' \
        | while read -r h; do
            echo "$f:$h"
        done
done | sort -u)

if [ -n "$VIOLATIONS" ]; then
    echo "Direct core header include(s) in src/qt outside the composition root (file:header):"
    echo "$VIOLATIONS"
    echo
    echo "This is the Phase 1f gate: GUI code must consume core state through"
    echo "src/interfaces/*.h (see src/interfaces/README.md and"
    echo "doc/multiprocess_design.md). There is no allowlist -- migrate the file"
    echo "onto an interface, or (only for the process entry points) add it to the"
    echo "composition-root exemption in $(basename "${BASH_SOURCE[0]}")."
    exit 1
fi

exit 0
