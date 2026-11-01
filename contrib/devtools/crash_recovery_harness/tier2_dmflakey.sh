#!/usr/bin/env bash
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Tier 2 crash harness: dm-flakey whole-volume fault injection.
#
# Wraps a loopback-backed ext4 volume in a dm-flakey device-mapper target,
# mounts it at the victim wallet's testnet datadir, and uses dm-flakey's
# `drop_writes` feature to discard pending writes to that volume mid-sync.
# When combined with a SIGKILL of the wallet process, this is intended to
# reproduce Scenario A (partial flat-file write) and the realistic
# OS-page-cache-loss flavour of Scenario C -- the dm-flakey reset
# discards everything the kernel had buffered but not yet committed to
# the underlying loopback file.
#
# Linux only. Requires root for dmsetup / mount / kpartx. Idempotent setup
# but always wipes the test datadir on entry.
#
# WHAT THIS ACTUALLY VALIDATES (post-Iteration-9 lessons; see
# doc/block_corruption_recovery_design.md "Lessons from Tier 2
# fault-injection validation" and doc/block_corruption_recovery_test_log.md
# Iteration 9):
#
#   dm-flakey wraps the *whole datadir volume*, so its drops and
#   corruptions hit BDB wallet log records, the BDB environment files,
#   the LevelDB MANIFEST, LevelDB SSTs, the LevelDB WAL, AND blk*.dat
#   indiscriminately. Each of those layers has its own integrity check,
#   and in practice those checks fire before Phase 2 ever gets to run:
#
#     - `drop_writes` mode: Phase 1's FileCommit(blk*.dat) before
#       CTxDB::Sync() barrier ordering guarantees the half that gets
#       lost is the LevelDB-tip-commit half (not the blk*.dat half),
#       so LevelDB rolls back one block on restart and Phase 2's
#       coherence check reports "No rewind needed." Phase 2's rewind
#       path is NOT exercised here.
#
#     - `corrupt_bio_byte` mode: BDB's DB_RUNRECOVERY catches its own
#       corruption first and aborts startup. After restoring wallet.dat
#       + database/ from a clean snapshot, LevelDB's MANIFEST CRC
#       catches its own corruption and aborts. Phase 2 still does not
#       run, because random whole-bio corruption does not reliably
#       produce Phase 2's narrow trigger condition (LevelDB healthy
#       AND blk*.dat content hashes to something other than the
#       CBlockIndex hash at the recorded tip position).
#
# This harness therefore validates the *layered defence below Phase 2*
# (Phase 1 barrier ordering, BDB integrity, LevelDB integrity) -- not
# Phase 2's rewind code path itself. Both Iteration 9 sub-iterations
# passed in the "graceful failure" sense: the wallet either
# auto-recovered (drop_writes) or stopped cleanly with operator-actionable
# error messages (corrupt_bio_byte). No silently-advancing corrupt chain.
#
# To exercise Phase 2's rewind code path end-to-end, use Tier 3
# (`tier3_truncate.sh`), which does a surgical `truncate(1)` on the
# active blk*.dat past a specific block index entry. That produces the
# exact corruption pattern Phase 2 is designed to recover from. The
# pre-merge recommendation for Phase 2 changes is Tier 3, with
# TRUNCATE_BYTES swept across the small (no SB cross), medium (1-2 SBs
# crossed -> clamp path), and large (6+ SBs crossed -> Reset +
# DropAboveHeight + side-chain catch-up) sub-cases.

export LC_ALL=C
set -euo pipefail

WALLET_BIN="${WALLET_BIN:?WALLET_BIN must point to gridcoinresearchd}"
TEST_NETNS="${TEST_NETNS:-node3}"
TEST_MOUNTPOINT="${TEST_MOUNTPOINT:-/mnt/grc_crash_harness_t2}"
TEST_DATADIR="${TEST_DATADIR:-${TEST_MOUNTPOINT}/grc_test}"
PEER_NETNS="${PEER_NETNS:-node1}"
PEER_DATADIR="${PEER_DATADIR:-$HOME/isolated_Gridcoin_testnet/.GridcoinResearch}"
TARGET_HEIGHT="${TARGET_HEIGHT:-50000}"
DROP_AT_HEIGHT="${DROP_AT_HEIGHT:-$(( TARGET_HEIGHT - 100 ))}"
CONVERGE_TIMEOUT="${CONVERGE_TIMEOUT:-1800}"
POLL_INTERVAL="${POLL_INTERVAL:-5}"

BACKING_FILE="${BACKING_FILE:-/tmp/grc_crash_harness_t2.img}"
BACKING_SIZE_MB="${BACKING_SIZE_MB:-8192}"  # 8 GiB; resize for larger TARGET_HEIGHT
DM_NAME="${DM_NAME:-grc_crash_harness_t2}"

log() { printf '[%s] %s\n' "$(date +%H:%M:%S)" "$*"; }
fail() { log "FAIL: $*"; cleanup; exit 1; }

cleanup() {
    log "Cleaning up dm-flakey + loopback state..."
    sudo umount "$TEST_MOUNTPOINT" 2>/dev/null || true
    sudo dmsetup remove "$DM_NAME" 2>/dev/null || true
    if [[ -n "${LOOP_DEV:-}" ]]; then
        sudo losetup -d "$LOOP_DEV" 2>/dev/null || true
    fi
}
trap cleanup EXIT

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

# ---------------------------------------------------------------- preflight

if [[ "$EUID" -ne 0 ]] && ! command -v sudo >/dev/null; then
    fail "sudo required for dm-flakey setup."
fi
command -v dmsetup >/dev/null || fail "dmsetup not installed (apt install dmsetup)."
command -v losetup  >/dev/null || fail "losetup not installed (util-linux package)."
[[ -x "$WALLET_BIN" ]] || fail "WALLET_BIN '$WALLET_BIN' is not executable."
sudo ip netns list | grep -q "^${TEST_NETNS}\b" || fail "Test netns '$TEST_NETNS' not found."
sudo ip netns list | grep -q "^${PEER_NETNS}\b" || fail "Peer netns '$PEER_NETNS' not found."
PEER_TIP=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
if [[ -z "$PEER_TIP" ]] || (( PEER_TIP <= TARGET_HEIGHT )); then
    fail "Peer tip ($PEER_TIP) is not above TARGET_HEIGHT ($TARGET_HEIGHT)."
fi

log "Preflight OK. Peer tip = $PEER_TIP; drop-writes at $DROP_AT_HEIGHT; SIGKILL at $TARGET_HEIGHT."

# ---------------------------------------------------------------- dm-flakey setup

log "Creating ${BACKING_SIZE_MB} MiB backing file at $BACKING_FILE..."
sudo rm -f "$BACKING_FILE"
sudo truncate -s "${BACKING_SIZE_MB}M" "$BACKING_FILE"
LOOP_DEV=$(sudo losetup --show -f "$BACKING_FILE")
log "Loopback device: $LOOP_DEV"

SECTORS=$(sudo blockdev --getsz "$LOOP_DEV")
log "Backing size: $SECTORS sectors."

# dm-flakey parameters: <up_interval> <down_interval> <drop_writes>
# Start in "always up, no drop" mode. We'll switch modes mid-sync.
sudo dmsetup create "$DM_NAME" --table "0 $SECTORS flakey $LOOP_DEV 0 0 0"
log "dm-flakey device: /dev/mapper/$DM_NAME"

log "Formatting and mounting..."
sudo mkfs.ext4 -q "/dev/mapper/$DM_NAME"
sudo mkdir -p "$TEST_MOUNTPOINT"
sudo mount "/dev/mapper/$DM_NAME" "$TEST_MOUNTPOINT"
sudo mkdir -p "$TEST_DATADIR"
sudo chown -R "$USER":"$USER" "$TEST_MOUNTPOINT"

# ---------------------------------------------------------------- start wallet

log "Starting victim wallet..."
# shellcheck disable=SC2024  # /tmp path is user-owned; redirect by calling shell is intentional.
sudo ip netns exec "$TEST_NETNS" "$WALLET_BIN" \
    -testnet -datadir="$TEST_DATADIR" -daemon \
    >/tmp/grc_crash_harness_t2_start.log 2>&1

sleep 5
PIDFILE="$TEST_DATADIR/testnet/gridcoinresearchd.pid"
[[ -f "$PIDFILE" ]] || fail "Pidfile not found at $PIDFILE."
VICTIM_PID=$(cat "$PIDFILE")
log "Victim PID = $VICTIM_PID"

# ---------------------------------------------------------------- sync + drop writes

log "Waiting for victim to reach drop-at-height $DROP_AT_HEIGHT..."
wait_for_height "$TEST_NETNS" "$TEST_DATADIR" "$DROP_AT_HEIGHT" 1800 >/dev/null \
    || fail "Victim did not reach $DROP_AT_HEIGHT within 30 min."

log "Switching dm-flakey to drop_writes mode (all writes from this point will be silently dropped)..."
sudo dmsetup suspend "$DM_NAME"
sudo dmsetup load    "$DM_NAME" --table "0 $SECTORS flakey $LOOP_DEV 0 0 0 1 drop_writes"
sudo dmsetup resume  "$DM_NAME"

log "Letting victim sync into the drop window (writes will be lost)..."
wait_for_height "$TEST_NETNS" "$TEST_DATADIR" "$TARGET_HEIGHT" 600 >/dev/null \
    || log "WARN: victim did not reach $TARGET_HEIGHT (likely because writes are being dropped). Continuing anyway."

REACHED=$(get_block_height "$TEST_NETNS" "$TEST_DATADIR")
log "Victim in-memory height after drop window: $REACHED."

# ---------------------------------------------------------------- kill + reset dm-flakey

log "SIGKILL victim..."
sudo kill -9 "$VICTIM_PID" || log "WARN: kill failed (already exited?)"
sleep 3

log "Unmounting and reloading dm-flakey in normal mode (this discards page-cache writes that never reached the loopback file)..."
sudo umount "$TEST_MOUNTPOINT"
sudo dmsetup suspend "$DM_NAME"
sudo dmsetup load    "$DM_NAME" --table "0 $SECTORS flakey $LOOP_DEV 0 0 0"
sudo dmsetup resume  "$DM_NAME"
sudo mount "/dev/mapper/$DM_NAME" "$TEST_MOUNTPOINT"

# ---------------------------------------------------------------- restart + verify

ARCHIVE="$TEST_DATADIR/testnet/debug.log.tier2_prerestart"
sudo mv "$TEST_DATADIR/testnet/debug.log" "$ARCHIVE" 2>/dev/null || true

log "Restarting victim..."
# shellcheck disable=SC2024  # /tmp path is user-owned; redirect by calling shell is intentional.
sudo ip netns exec "$TEST_NETNS" "$WALLET_BIN" \
    -testnet -datadir="$TEST_DATADIR" -daemon \
    >/tmp/grc_crash_harness_t2_restart.log 2>&1

sleep 5
[[ -f "$PIDFILE" ]] || fail "Restart pidfile not found."

# Wait for RPC.
for _ in 1 2 3 4 5 6 7 8; do
    if rpc "$TEST_NETNS" "$TEST_DATADIR" getblockchaininfo >/dev/null 2>&1; then break; fi
    sleep 5
done

DEBUG_LOG="$TEST_DATADIR/testnet/debug.log"

# Assertion 1: Phase 2 hook executed (any of its INFO lines).
if ! sudo grep -qE "RunStartupCoherenceRecovery: (chain tip at height|detected inconsistency|-reindex set)" "$DEBUG_LOG"; then
    fail "Phase 2 hook did NOT execute."
fi
log "PASS: Phase 2 hook executed."

# Assertion 2: Phase 2 actually detected and rewound (more likely in Tier 2 than Tier 1).
if sudo grep -qE "RunStartupCoherenceRecovery: detected inconsistency past height" "$DEBUG_LOG"; then
    REWIND_LINE=$(sudo grep -E "RunStartupCoherenceRecovery: detected inconsistency past height" "$DEBUG_LOG" | head -1)
    log "INFO: Phase 2 detected and rewound: $REWIND_LINE"
else
    log "INFO: Phase 2 ran but found no inconsistency. (dm-flakey writeback timing did not produce a Scenario C; this is acceptable -- re-run with a higher DROP_AT_HEIGHT or sync more between drop and kill.)"
fi

# Assertion 3: no Phase 1 failure signatures.
if sudo grep -qE "(LevelDB Sync failure|FileCommit failed)" "$DEBUG_LOG"; then
    fail "Phase 1 failure signature in post-restart debug.log."
fi
log "PASS: No Phase 1 failure signatures."

# Assertion 4: chain converges with peer.
log "Waiting up to ${CONVERGE_TIMEOUT}s for chain to converge with peer..."
PEER_NOW=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
if ! wait_for_height "$TEST_NETNS" "$TEST_DATADIR" "$(( PEER_NOW - 1 ))" "$CONVERGE_TIMEOUT" >/dev/null; then
    VICTIM_NOW=$(get_block_height "$TEST_NETNS" "$TEST_DATADIR")
    fail "Chain did not converge: victim=$VICTIM_NOW peer=$PEER_NOW after ${CONVERGE_TIMEOUT}s."
fi
log "PASS: Chain converged."

log "Tier 2 OK. Stopping victim."
rpc "$TEST_NETNS" "$TEST_DATADIR" stop >/dev/null 2>&1 || true
