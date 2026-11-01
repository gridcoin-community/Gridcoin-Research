#!/usr/bin/env bash
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Tier 3 crash harness: deterministic blk*.dat truncation.
#
# Reproduces Scenario C (LevelDB index ahead of blk*.dat data, from
# issue #2865) by clean-stopping a synced wallet, truncating the active
# blk*.dat file by a configurable number of bytes, and restarting. The
# LevelDB block index still references the now-missing block bytes --
# exactly the state a real power loss would leave when the kernel's
# writeback of blk*.dat hadn't caught up to LevelDB's WAL commit.
#
# Compared to a true power-pull (formerly Tier 3 via VMware): less
# faithful to the kernel-side corruption-creation race but FAR more
# useful for validating the recovery code, because:
#   - Deterministic: exact rewind depth == exact truncation depth.
#   - No root for the truncate step itself (only for the netns'd wallet).
#   - No VMware / KVM / host-specific lifecycle.
#   - Trivially parametric: sweep TRUNCATE_BYTES through values that
#     cross 0, 1, or 2+ superblock boundaries to exercise each
#     registry-handling code path.
#
# What this tier does NOT cover: mid-sector torn writes (some bytes
# corrupted rather than missing), LevelDB SST file corruption, or
# filesystem-journal artifacts. For Phase 2's recovery code those don't
# matter -- the walk-back's hash compare treats "read failed (EOF)" and
# "read returned garbage" identically -- but they would matter for any
# future filesystem-layer recovery work. The Tier 2 dm-flakey script
# remains the better tool for exercising the corruption-creation side.

export LC_ALL=C
set -euo pipefail

WALLET_BIN="${WALLET_BIN:?WALLET_BIN must point to gridcoinresearchd}"
TEST_NETNS="${TEST_NETNS:-node3}"
TEST_DATADIR="${TEST_DATADIR:-/tmp/grc_crash_harness_t3_datadir}"
PEER_NETNS="${PEER_NETNS:-node1}"
PEER_DATADIR="${PEER_DATADIR:-$HOME/isolated_Gridcoin_testnet/.GridcoinResearch}"
TARGET_HEIGHT="${TARGET_HEIGHT:-50000}"
CONVERGE_TIMEOUT="${CONVERGE_TIMEOUT:-1800}"
POLL_INTERVAL="${POLL_INTERVAL:-5}"

# Bytes to chop from the end of the active (highest-numbered) blk*.dat
# file. Default 65536 (~64 KiB) chops a few dozen blocks on testnet
# without crossing a superblock. Override for scenario sweeps:
#   65536       -- small, no SB cross (registry clamp only)
#   1048576     -- ~1 MiB, likely 1 SB cross (single-SB beacon path)
#   10485760    -- ~10 MiB, likely 2+ SB cross (beacon Reset path)
#   268435456   -- ~256 MiB, beyond default -coherencewalkmax
#                  (set EXPECT_REINDEX_REQUEST=1 to invert assertions)
TRUNCATE_BYTES="${TRUNCATE_BYTES:-65536}"

# When set to 1, assert that Phase 2 refused to start and logged the
# "please -reindex" message instead of recovering. Used for scenarios
# where the truncation is deeper than -coherencewalkmax allows.
EXPECT_REINDEX_REQUEST="${EXPECT_REINDEX_REQUEST:-0}"

log()  { printf '[%s] %s\n' "$(date +%H:%M:%S)" "$*"; }
fail() { log "FAIL: $*"; exit 1; }

rpc() {
    local netns="$1" datadir="$2"; shift 2
    sudo ip netns exec "$netns" "$WALLET_BIN" -testnet -datadir="$datadir" "$@"
}

get_block_height() {
    local netns="$1" datadir="$2"
    rpc "$netns" "$datadir" getblockchaininfo 2>/dev/null \
        | grep -oP '"blocks":\s*\K[0-9]+' || echo 0
}

wait_for_height() {
    local netns="$1" datadir="$2" target="$3" timeout="$4"
    local deadline=$(( SECONDS + timeout ))
    while (( SECONDS < deadline )); do
        local h
        h=$(get_block_height "$netns" "$datadir")
        if [[ -n "$h" ]] && (( h >= target )); then
            echo "$h"; return 0
        fi
        sleep "$POLL_INTERVAL"
    done
    return 1
}

wait_for_pid_exit() {
    local pid="$1" timeout="$2"
    local deadline=$(( SECONDS + timeout ))
    while (( SECONDS < deadline )); do
        if ! sudo kill -0 "$pid" 2>/dev/null; then return 0; fi
        sleep 1
    done
    return 1
}

# ---------------------------------------------------------------- preflight

[[ -x "$WALLET_BIN" ]] || fail "WALLET_BIN '$WALLET_BIN' is not executable."
sudo ip netns list | grep -q "^${TEST_NETNS}\b" || fail "Test netns '$TEST_NETNS' not found."
sudo ip netns list | grep -q "^${PEER_NETNS}\b" || fail "Peer netns '$PEER_NETNS' not found."
[[ -d "$PEER_DATADIR" ]] || fail "Peer datadir '$PEER_DATADIR' does not exist."

PEER_TIP=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
if [[ -z "$PEER_TIP" ]] || (( PEER_TIP <= TARGET_HEIGHT )); then
    fail "Peer tip ($PEER_TIP) is not above TARGET_HEIGHT ($TARGET_HEIGHT)."
fi

log "Preflight OK. Peer tip = $PEER_TIP; victim will sync to $TARGET_HEIGHT then chop $TRUNCATE_BYTES bytes."

# ---------------------------------------------------------------- wipe + sync

log "Wiping test datadir at $TEST_DATADIR..."
sudo rm -rf "$TEST_DATADIR"
sudo mkdir -p "$TEST_DATADIR"
sudo chown "$USER":"$USER" "$TEST_DATADIR"

log "Starting victim wallet..."
# shellcheck disable=SC2024  # /tmp path is user-owned; redirect by calling shell is intentional.
sudo ip netns exec "$TEST_NETNS" "$WALLET_BIN" \
    -testnet -datadir="$TEST_DATADIR" -daemon \
    >/tmp/grc_crash_harness_t3_start.log 2>&1

sleep 5
PIDFILE="$TEST_DATADIR/testnet/gridcoinresearchd.pid"
[[ -f "$PIDFILE" ]] || fail "Pidfile not found at $PIDFILE after 5s."
VICTIM_PID=$(cat "$PIDFILE")
log "Victim PID = $VICTIM_PID"

log "Waiting for victim to reach height $TARGET_HEIGHT (timeout 30 min)..."
if ! REACHED=$(wait_for_height "$TEST_NETNS" "$TEST_DATADIR" "$TARGET_HEIGHT" 1800); then
    fail "Victim did not reach height $TARGET_HEIGHT within 30 min."
fi
log "Victim reached height $REACHED."

# ---------------------------------------------------------------- clean stop

# Use RPC stop, not kill -9. We want the wallet to flush everything
# cleanly so that the ONLY source of post-restart inconsistency is the
# truncation we are about to do -- not residual page-cache loss.
log "Issuing RPC stop and waiting for clean exit..."
rpc "$TEST_NETNS" "$TEST_DATADIR" stop >/dev/null 2>&1 || true

if ! wait_for_pid_exit "$VICTIM_PID" 60; then
    fail "Victim PID $VICTIM_PID did not exit within 60s of RPC stop."
fi
log "Victim exited cleanly."

# ---------------------------------------------------------------- truncate

BLOCKS_DIR="$TEST_DATADIR/testnet/blocks"
[[ -d "$BLOCKS_DIR" ]] || fail "Blocks dir '$BLOCKS_DIR' not found after clean stop."

# Find the active blk*.dat (highest-numbered). sort -V handles 1, 2, ..., 10 correctly.
ACTIVE_BLK=$(sudo find "$BLOCKS_DIR" -maxdepth 1 -name 'blk[0-9]*.dat' -printf '%f\n' | sort -V | tail -1)
[[ -n "$ACTIVE_BLK" ]] || fail "No blk*.dat found in $BLOCKS_DIR."
ACTIVE_BLK_PATH="$BLOCKS_DIR/$ACTIVE_BLK"

ACTIVE_SIZE=$(sudo stat -c %s "$ACTIVE_BLK_PATH")
log "Active block file: $ACTIVE_BLK ($ACTIVE_SIZE bytes)."

if (( TRUNCATE_BYTES <= 0 )); then
    fail "TRUNCATE_BYTES must be positive (got $TRUNCATE_BYTES)."
fi
if (( TRUNCATE_BYTES >= ACTIVE_SIZE )); then
    fail "TRUNCATE_BYTES=$TRUNCATE_BYTES >= active file size $ACTIVE_SIZE; would empty the file. Pick a smaller value or a later TARGET_HEIGHT."
fi

NEW_SIZE=$(( ACTIVE_SIZE - TRUNCATE_BYTES ))
log "Truncating $ACTIVE_BLK from $ACTIVE_SIZE to $NEW_SIZE bytes (dropping $TRUNCATE_BYTES bytes)..."
sudo truncate -s "$NEW_SIZE" "$ACTIVE_BLK_PATH"

CONFIRMED_SIZE=$(sudo stat -c %s "$ACTIVE_BLK_PATH")
(( CONFIRMED_SIZE == NEW_SIZE )) || fail "Truncate did not take effect: size now $CONFIRMED_SIZE, expected $NEW_SIZE."

# ---------------------------------------------------------------- restart + verify

# Archive the pre-restart debug.log so the post-restart slice is clean.
ARCHIVE="$TEST_DATADIR/testnet/debug.log.tier3_prerestart"
sudo mv "$TEST_DATADIR/testnet/debug.log" "$ARCHIVE" 2>/dev/null || true

log "Restarting victim..."
# shellcheck disable=SC2024  # /tmp path is user-owned; redirect by calling shell is intentional.
sudo ip netns exec "$TEST_NETNS" "$WALLET_BIN" \
    -testnet -datadir="$TEST_DATADIR" -daemon \
    >/tmp/grc_crash_harness_t3_restart.log 2>&1 || true  # may exit non-zero in the EXPECT_REINDEX_REQUEST case

sleep 5

DEBUG_LOG="$TEST_DATADIR/testnet/debug.log"

if (( EXPECT_REINDEX_REQUEST == 1 )); then
    # Scenario: truncation exceeded -coherencewalkmax. Phase 2 should
    # have logged the "please -reindex" ERROR and the wallet should
    # have exited (or refused to come up on RPC).
    log "EXPECT_REINDEX_REQUEST=1 -- asserting Phase 2 bailed out cleanly."

    if ! sudo grep -qE "RunStartupCoherenceRecovery:.*beyond the automatic-recovery window.*please restart with -reindex" "$DEBUG_LOG" 2>/dev/null; then
        fail "Phase 2 did NOT log the expected '-reindex required' message in $DEBUG_LOG."
    fi
    log "PASS: Phase 2 logged the -reindex-required message as expected."

    # The wallet should NOT be answering RPC (the init aborted).
    if rpc "$TEST_NETNS" "$TEST_DATADIR" getblockchaininfo >/dev/null 2>&1; then
        fail "Wallet is answering RPC despite EXPECT_REINDEX_REQUEST=1."
    fi
    log "PASS: Wallet correctly refused to come up."

    log "Tier 3 OK (EXPECT_REINDEX_REQUEST path)."
    exit 0
fi

# Normal recovery path: wait for RPC.
for _ in 1 2 3 4 5 6 7 8; do
    if rpc "$TEST_NETNS" "$TEST_DATADIR" getblockchaininfo >/dev/null 2>&1; then
        break
    fi
    sleep 5
done

if ! rpc "$TEST_NETNS" "$TEST_DATADIR" getblockchaininfo >/dev/null 2>&1; then
    fail "Wallet did not bring up RPC within 40s after restart."
fi

# Assertion 1: Phase 2 hook executed (look for any of its INFO lines).
if ! sudo grep -qE "RunStartupCoherenceRecovery: (chain tip at height|detected inconsistency|-reindex set)" "$DEBUG_LOG"; then
    fail "Phase 2 hook did NOT execute (no RunStartupCoherenceRecovery INFO lines in debug.log)."
fi
log "PASS: Phase 2 hook executed."

# Assertion 2: Phase 2 ACTUALLY detected the inconsistency and rewound.
# Unlike Tier 1, this tier is deterministic -- the truncation guarantees
# the index will reference missing data, so a "tip is coherent, no rewind
# needed" outcome would be a real bug.
if ! sudo grep -qE "RunStartupCoherenceRecovery: detected inconsistency past height" "$DEBUG_LOG"; then
    fail "Phase 2 did NOT detect inconsistency despite a $TRUNCATE_BYTES-byte truncation. Either VerifyChainCoherence regressed back to the no-op overload or the truncation is in dead space (try a larger TRUNCATE_BYTES)."
fi
REWIND_LINE=$(sudo grep -E "RunStartupCoherenceRecovery: detected inconsistency past height" "$DEBUG_LOG" | head -1)
log "PASS: Phase 2 detected inconsistency: $REWIND_LINE"

# Assertion 3: CleanAbandonedRange ran and reported its scan stats.
# (Proves the surgical chainstate cleanup path actually executed --
# regressing this back to "leave phantoms" would silently break
# ConnectInputs on the re-supplied blocks.)
if ! sudo grep -qE "CleanAbandonedRange: scanned [0-9]+ CTxIndex entries" "$DEBUG_LOG"; then
    fail "CleanAbandonedRange did NOT run (no scan-stats log line). The surgical chainstate cleanup is silently skipped."
fi
log "PASS: CleanAbandonedRange ran and logged scan stats."

# Assertion 4: PurgeOrphanedBlockIndexEntries ran.
if ! sudo grep -qE "PurgeOrphanedBlockIndexEntries: purged [0-9]+ orphaned CBlockIndex" "$DEBUG_LOG"; then
    fail "PurgeOrphanedBlockIndexEntries did NOT run. The in-memory mapBlockIndex purge is silently skipped."
fi
log "PASS: PurgeOrphanedBlockIndexEntries ran."

# Assertion 5: No Phase 1 failure signatures.
if sudo grep -qE "(LevelDB Sync failure|FileCommit failed|fdatasync failed|fsync failed)" "$DEBUG_LOG"; then
    fail "Phase 1 failure signature detected in post-restart debug.log."
fi
log "PASS: No Phase 1 failure signatures."

# Assertion 6: Chain converges with peer.
log "Waiting up to ${CONVERGE_TIMEOUT}s for chain to converge with peer..."
PEER_NOW=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
if ! wait_for_height "$TEST_NETNS" "$TEST_DATADIR" "$(( PEER_NOW - 1 ))" "$CONVERGE_TIMEOUT" >/dev/null; then
    VICTIM_NOW=$(get_block_height "$TEST_NETNS" "$TEST_DATADIR")
    fail "Chain did not converge: victim=$VICTIM_NOW peer=$PEER_NOW after ${CONVERGE_TIMEOUT}s."
fi
VICTIM_FINAL=$(get_block_height "$TEST_NETNS" "$TEST_DATADIR")
PEER_FINAL=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
log "PASS: Chain converged. victim=$VICTIM_FINAL peer=$PEER_FINAL."

log "Tier 3 OK. Stopping victim."
rpc "$TEST_NETNS" "$TEST_DATADIR" stop >/dev/null 2>&1 || true
