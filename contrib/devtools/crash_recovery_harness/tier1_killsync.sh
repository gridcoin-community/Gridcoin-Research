#!/usr/bin/env bash
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Tier 1 crash harness: process-level interruption.
#
# Starts a fresh-datadir wallet against the on-host isolated testnet,
# lets it sync to TARGET_HEIGHT, sends SIGKILL, restarts, and verifies
# that the Phase 2 startup chain-coherence recovery hook ran and the
# chain re-converges. See README.md and
# doc/block_corruption_recovery_design.md for context.
#
# `kill -9` does not drop the OS page cache, so this tier does not
# reproduce Scenario C faithfully -- the restart almost always finds
# coherent on-disk state. Use Tier 2 (dm-flakey) or Tier 3 (VMware
# abrupt power-off) for the real corruption-recovery validation.
# What Tier 1 reliably catches: regressions that crash the Phase 2 hook
# itself, or that break the no-rewind-needed fast path.

set -euo pipefail

WALLET_BIN="${WALLET_BIN:?WALLET_BIN must point to gridcoinresearchd}"
TEST_NETNS="${TEST_NETNS:-node3}"
TEST_DATADIR="${TEST_DATADIR:-/tmp/grc_crash_harness_t1_datadir}"
PEER_NETNS="${PEER_NETNS:-node1}"
PEER_DATADIR="${PEER_DATADIR:-/home/jco/isolated_Gridcoin_testnet/.GridcoinResearch}"
TARGET_HEIGHT="${TARGET_HEIGHT:-50000}"
CONVERGE_TIMEOUT="${CONVERGE_TIMEOUT:-1800}"  # seconds to wait for re-sync after restart
POLL_INTERVAL="${POLL_INTERVAL:-5}"

log() { printf '[%s] %s\n' "$(date +%H:%M:%S)" "$*"; }
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

# ---------------------------------------------------------------- preflight

[[ -x "$WALLET_BIN" ]] || fail "WALLET_BIN '$WALLET_BIN' is not executable."
sudo ip netns list | grep -q "^${TEST_NETNS}\b"  || fail "Test netns '$TEST_NETNS' not found."
sudo ip netns list | grep -q "^${PEER_NETNS}\b"  || fail "Peer netns '$PEER_NETNS' not found."
[[ -d "$PEER_DATADIR" ]] || fail "Peer datadir '$PEER_DATADIR' does not exist."

PEER_TIP=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
if [[ -z "$PEER_TIP" ]] || (( PEER_TIP <= TARGET_HEIGHT )); then
    fail "Peer tip ($PEER_TIP) is not above TARGET_HEIGHT ($TARGET_HEIGHT). Pick a lower target or wait for peer to sync."
fi

log "Preflight OK. Peer tip = $PEER_TIP; will kill victim at $TARGET_HEIGHT."

# ---------------------------------------------------------------- wipe + start

log "Wiping test datadir at $TEST_DATADIR..."
sudo rm -rf "$TEST_DATADIR"
sudo mkdir -p "$TEST_DATADIR"
sudo chown "$USER":"$USER" "$TEST_DATADIR"

log "Starting victim wallet in netns $TEST_NETNS, datadir $TEST_DATADIR..."
# shellcheck disable=SC2024  # /tmp path is user-owned; redirect by calling shell is intentional.
sudo ip netns exec "$TEST_NETNS" "$WALLET_BIN" \
    -testnet -datadir="$TEST_DATADIR" -daemon \
    >/tmp/grc_crash_harness_t1_start.log 2>&1

# Find PID inside the netns. `pgrep` won't see netns-scoped processes from
# outside, but the wallet writes a pid file.
sleep 5
PIDFILE="$TEST_DATADIR/testnet/gridcoinresearchd.pid"
[[ -f "$PIDFILE" ]] || fail "Victim pidfile not found at $PIDFILE after 5s."
VICTIM_PID=$(cat "$PIDFILE")
log "Victim wallet PID = $VICTIM_PID"

# ---------------------------------------------------------------- sync to target

log "Waiting for victim to reach height $TARGET_HEIGHT (timeout 30 min)..."
if ! REACHED=$(wait_for_height "$TEST_NETNS" "$TEST_DATADIR" "$TARGET_HEIGHT" 1800); then
    fail "Victim did not reach height $TARGET_HEIGHT within 30 min."
fi
log "Victim reached height $REACHED. Sending SIGKILL."

# ---------------------------------------------------------------- kill

sudo kill -9 "$VICTIM_PID" || fail "kill -9 of PID $VICTIM_PID failed (already exited?)"
sleep 3

# ---------------------------------------------------------------- restart + verify

# Archive the pre-restart debug.log so the post-restart slice is clean.
ARCHIVE="$TEST_DATADIR/testnet/debug.log.tier1_prerestart"
sudo mv "$TEST_DATADIR/testnet/debug.log" "$ARCHIVE" 2>/dev/null || true

log "Restarting victim..."
# shellcheck disable=SC2024  # /tmp path is user-owned; redirect by calling shell is intentional.
sudo ip netns exec "$TEST_NETNS" "$WALLET_BIN" \
    -testnet -datadir="$TEST_DATADIR" -daemon \
    >/tmp/grc_crash_harness_t1_restart.log 2>&1

sleep 5
[[ -f "$PIDFILE" ]] || fail "Restart pidfile not found after 5s."
NEW_PID=$(cat "$PIDFILE")
log "Restarted wallet PID = $NEW_PID"

# Wait for RPC to come up.
for _ in 1 2 3 4 5 6; do
    if rpc "$TEST_NETNS" "$TEST_DATADIR" getblockchaininfo >/dev/null 2>&1; then
        break
    fi
    sleep 5
done

# Assertion 1: Phase 2 hook executed (look for any of its INFO lines).
DEBUG_LOG="$TEST_DATADIR/testnet/debug.log"
if ! sudo grep -qE "RunStartupCoherenceRecovery: (chain tip at height|detected inconsistency|-reindex set)" "$DEBUG_LOG"; then
    fail "Phase 2 hook did NOT execute (no RunStartupCoherenceRecovery INFO lines in debug.log)."
fi
log "PASS: Phase 2 hook executed."

# Assertion 2: No Phase 1 failure signatures.
if sudo grep -qE "(LevelDB Sync failure|FileCommit failed|fdatasync failed|fsync failed)" "$DEBUG_LOG"; then
    fail "Phase 1 failure signature detected in post-restart debug.log."
fi
log "PASS: No Phase 1 failure signatures."

# Assertion 3: Chain converges with peer.
log "Waiting up to ${CONVERGE_TIMEOUT}s for chain to converge with peer (current peer tip $(get_block_height "$PEER_NETNS" "$PEER_DATADIR"))..."
PEER_NOW=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
if ! wait_for_height "$TEST_NETNS" "$TEST_DATADIR" "$(( PEER_NOW - 1 ))" "$CONVERGE_TIMEOUT" >/dev/null; then
    VICTIM_NOW=$(get_block_height "$TEST_NETNS" "$TEST_DATADIR")
    fail "Chain did not converge: victim=$VICTIM_NOW peer=$PEER_NOW after ${CONVERGE_TIMEOUT}s."
fi
VICTIM_FINAL=$(get_block_height "$TEST_NETNS" "$TEST_DATADIR")
PEER_FINAL=$(get_block_height "$PEER_NETNS" "$PEER_DATADIR")
log "PASS: Chain converged. victim=$VICTIM_FINAL peer=$PEER_FINAL."

log "Tier 1 OK. Stopping victim."
rpc "$TEST_NETNS" "$TEST_DATADIR" stop >/dev/null 2>&1 || true
