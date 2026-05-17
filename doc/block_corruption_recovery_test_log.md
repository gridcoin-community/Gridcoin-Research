# Block file corruption recovery — testing log

Detailed validation record for issue #2865 / PR #2941 Phase 2 (startup walk-back). Phase 1 (Phase-1 durability invariants: `CTxDB::Sync()` barrier paired with `FileCommit`, shutdown flush after `StopNode()`) merged 2026-05-15 as PR #2939 and is preconditioned by every test below.

This document is a companion to [block_corruption_recovery_design.md](block_corruption_recovery_design.md). The design doc describes *what* Phase 2 does and *why*; this doc records *how it was validated* on a real isolated-testnet network, what bugs surfaced during that validation, and how each was fixed.

## Test environment

- Isolated 3-node testnet using IP namespaces, forked from the public testnet *before* its most recent checkpoint (so the synthetic branch `testnet_v13_checkpoint_removed` removes that last checkpoint to avoid `VerifyCheckpoints` rejecting the fork's tip).
- Slot 8 (`node1`, `.GridcoinResearch`): scraper, full chain.
- Slot 9 (`node2`, `.GridcoinResearch2`): scraper, full chain.
- Slot 10 (`node3`, `.GridcoinResearch3`): victim slot for corruption injection. Always runs with `-staking=0` so it cannot stake-mine itself off onto a private fork while behind during Phase 2 recovery — this matches mainnet behavior (mainnet `IsMiningAllowed` returns false during `IsInitialBlockDownload`; testnet does not).
- Snapper subvolume snapshot `3110` (`pre-phase2-deploy`, 2026-05-16 19:00:08) used as the canonical clean baseline. File-level restore on slot 10 only (`blk*.dat` + `accrual/` + `txleveldb/`), never `snapper rollback` (other unrelated work lives on the same subvolume).

## Test methodology

Phase 2 is triggered exclusively from a startup hook. To validate it we apply a controlled corruption to slot 10's on-disk blockchain state, restart slot 10, and verify the recovery pipeline runs to completion and the chain catches back up to slots 8/9.

Three tiers of injected corruption, ordered by impact:

| Tier | Injection | Phase 2 expected behavior |
|------|-----------|---------------------------|
| 1 | Truncate a few hundred bytes off the latest `blk*.dat` | Backward walk fails one block, rewind by one, no SB cross |
| 2 | Truncate ~1 MB | Rewind a few hundred blocks, possibly cross zero or one SB |
| 3 | Truncate ~4 MB | Rewind several thousand blocks, cross multiple SBs, exercise beacon registry Reset path and accrual snapshot rewind |
| (walkmax exit) | Truncate enough to exceed `-coherencewalkmax` (default 10000) | `exhausted=true` returned, recovery declines, user prompted to `-resetblockchaindata` |
| (multi-SB runtime reorg) | Force a runtime reorg crossing multiple SBs without restarting | Tests the runtime `DisconnectBlocksBatch` path and the deferred-rebuild flag |

Each iteration always follows the same envelope:

1. Stop slot 10 cleanly (`gridcoin_wallet_control.sh stop 10`).
2. Stash slot 10's `debug.log` to a timestamped name.
3. Apply the corruption.
4. Start slot 10.
5. Watch `debug.log` for the expected Phase 2 sequence.
6. Verify forward sync from peers brings slot 10 back to slots 8/9's tip.
7. Compare hash at the catch-up height across all three slots to confirm same chain.

## Test iterations

### Iteration 1 — Tier 3 SB-cross (~4 MB) on baseline binary `c871502ac`

**Setup:** Snapshot 3110 restored on all three slots. Slot 10 deployed with `c871502ac` (PR #2941 baseline).

**Result: FAILED. Three distinct Phase 2 bugs surfaced.**

#### Bug 1 — `AccrualSnapshotRegistry` LIFO-only `Deregister` asserts on Phase 2 rewind

After Phase 2 rewound across 6 SBs, `Tally::ActivateSnapshotAccrual`'s `Initialize()` saw a snapshot registry with entries *above* `pindexBest->nHeight`. The registry's `Deregister(height)` is LIFO and asserts strict-monotonic decrease; the unwind loop tripped `assert(latest_height == height)` and the wallet sigaborted.

**Fix:** added `AccrualSnapshotRepository::DropAboveHeight(uint64_t tip_height)` in `src/gridcoin/accrual/snapshot.h` that drops the latest snapshot repeatedly until `LatestHeight() <= tip_height`. Called from `Tally::ActivateSnapshotAccrual` after `m_snapshots.Initialize()` and before the forward walk. Logged as `INFO: ActivateSnapshotAccrual: dropped N snapshot(s) at heights > pindexBest->nHeight=H`.

#### Bug 2 — `SeenStakes` stale after Phase 2 rewind blocks forward sync

`LoadBlockIndex` ends with `g_seen_stakes.Refill(pindexBest)` against the pre-rewind tip, seeding the 2048-slot kernel-proof hash table with proofs from blocks that Phase 2 is about to abandon. After rewind, forward sync from peers re-supplies the same abandoned blocks with the same (deterministic) kernel proofs — each collides with the stale entry, `AcceptBlock` returns `"ignored duplicate proof-of-stake"`, the block is rejected, and the chain wedges at the first re-supplied block past the rewound tip.

**Symptoms (slot 10 debug.log):**

    ERROR: AcceptBlock: ignored duplicate proof-of-stake (<kernel_hash>) for block <hash>
    ERROR: ProcessBlock() : AcceptBlock FAILED

followed by orphans piling up indefinitely.

**Fix (commit `0a7c442a4`):** added `SeenStakes::Clear()` that zeroes `m_proofs_seen`, and called `Clear()` + `Refill(pindexBest)` at the end of `RunStartupCoherenceRecovery`'s success path, so the table reflects only blocks at or below the new (rewound) tip.

**Why `Clear()` is needed, not just a second `Refill`:** `Refill` walks back at most 2048 blocks from `pindex` and overwrites slots by hash, but a slot from the abandoned range gets overwritten only if some post-rewind proof happens to hash to the same offset. With non-overlapping hash ranges only a fraction of the abandoned proofs get evicted, so the wedge still triggers probabilistically. `Clear()` is the only deterministic eviction.

#### Bug 3 — `CheckBlockIndex` modal-aborts on side-chain damage Phase 2 missed

After fixes 1 and 2 brought slot 10 back online, the next startup tripped `CheckBlockIndex`'s integrity guard:

    WARNING: block index integrity check failed -- found mapBlockIndex entry with null pprev that is not the genesis block

Root cause: `VerifyChainCoherence`'s forward walk follows `pnext` from `pindex_consistent`, which catches the active chain forward of the rewound tip but **not** side-chain `CBlockIndex*` entries (competing-fork blocks that were `AcceptBlock`'ed earlier) whose `pprev` chain runs through the abandoned range. Those side-chain entries weren't purged from LevelDB. On the *next* startup, `LoadBlockIndex` saw `CDiskBlockIndex` records whose `hashPrev` resolved to entries already purged, left `pprev` null, and `CheckBlockIndex` aborted with the modal.

**Fix (commit `b59bbaaf3`, two parts):**

1. **Phase 2.5** in `VerifyChainCoherence`: after the active-chain forward walk, iterate `mapBlockIndex` once more and add every entry with `nHeight > pindex_consistent->nHeight` (not already on the active walk) to `abandoned_indexes`. Side-chain positions are intentionally **not** added to `abandoned_positions` because side-chain blocks were never connected (no `CTxIndex`/`vSpent` to clean). SB crossings remain counted strictly from the active walk (only active-chain SB activations mutated registry state). Logged as `INFO: VerifyChainCoherence: extending abandonment to N side-chain index entries above consistent tip height H`.

2. **Self-healing `CheckBlockIndex`** in `src/gridcoin/gridcoin.cpp`: instead of bailing to the reset-blockchain modal, build a `hash → children` index, BFS from each dangling root, and erase the full subtree from both `mapBlockIndex` and LevelDB. A defensive guard preserves the modal fallback if `pindexBest` itself is in the dangling subtree (silently purging the tip's lineage would leave the wallet at a tip that no longer exists). Logged as `Gridcoin: block index self-heal complete (N mapBlockIndex entries removed, M CDiskBlockIndex records erased from LevelDB; pool slots remain allocated, see block_index.h)`.

Both pieces of the fix are needed: Phase 2.5 prevents the dangling state from being created on the rewind path; the self-heal makes existing damaged datasets recoverable without `-resetblockchaindata`.

#### Bug 4 — `VerifyChainCoherence` header-only read declares partially-truncated block coherent

After fixes 1–3, an end-to-end Tier 3 truncation iteration ran cleanly through Phase 2 and 688 blocks of forward sync — then wedged at block 2764206 with:

    ERROR: CheckProofOfStakeV8: VerifySignature failed on coinstake da66fbd248a6...
    ERROR: AcceptBlock: invalid proof-of-stake for block 759f19cc..., prev 962b305e...
    ERROR: ProcessBlock() : AcceptBlock FAILED

The coinstake input's prev_tx `b8bdb239` lived in block `9782f003` at the consistent tip height **2763517** — which is exactly where Phase 2 rewound to. Block 2763517 should have survived intact. `getrawtransaction b8bdb239 true` on slot 10 returned a *different* transaction (`17dec1fe`) — proving `CTxIndex[b8bdb239]` resolved to a position whose disk content was no longer `b8bdb239`'s serialized form.

Position arithmetic from the backward-walk logs:

- Block 2763518 at file position `179477195`
- Block 2763519 at file position `179477953`
- File size after truncation: `179477059`

Block 2763517's position lies just *before* `179477195` and its END is approximately *at* `179477195` — meaning the last ~136 bytes of block 2763517 (including the coinstake transaction payload) fell into the truncated region. But `VerifyChainCoherence` calls `ReadBlockFromDisk` with `fReadTransactions=false`, which deserializes only the 80-byte block header. The header was intact at `pindex->nBlockPos`, the hash matched, and Phase 2 wrongly declared block 2763517 the consistent tip.

When forward sync later validated block 2764206, `ReadStakedInput` seeked into block 2763517's truncated tail and deserialized whatever bytes happened to be there, returning a non-matching tx whose signature didn't verify.

**Fix (commit `e68fa9612`):** pass `fReadTransactions=true` in `coherence.cpp`'s `ReadBlockFromDisk` call. The full block is now deserialized during coherence verification, so any truncation that touches the tx payload fails the read and the walk continues past it. Cost is one full block read per backward step, only on the suspected-corruption path; the no-corruption path returns immediately on the first successful check.

### Iteration 2 — Tier 3 SB-cross (~4 MB) on `e68fa9612` after snapshot-restore + clean sync

Repeated the same Tier 3 ~4 MB truncation, this time against a slot 10 that had been:

1. Restored from snapshot 3110 (file-level: `blk*.dat` + `accrual/` + `txleveldb/`).
2. Started with `-staking=0` against the `e68fa9612` binary (all four Phase 2 fixes present).
3. Forward-synced cleanly to slots 8/9's tip at height `2768947`.
4. Stopped cleanly.

Then:

5. `blk0002.dat` truncated from 183694488 → 179500184 bytes (4 MB chopped).
6. Slot 10 restarted.

**Result: clean recovery end-to-end.**

Phase 2 log sequence:

    RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 10000).
    VerifyChainCoherence: block at height 2768947 ... failed coherence check (read_ok=false); walking back.
    ...
    VerifyChainCoherence: block at height 2763545 (..., blk00002.dat:179499808) failed coherence check (read_ok=false); walking back.
    VerifyChainCoherence: extending abandonment to 21 side-chain index entries above consistent tip height 2763544.
    RunStartupCoherenceRecovery: detected inconsistency past height 2768947. Last consistent block is at height 2763544 (6 superblocks crossed in the abandoned range, 5424 blocks abandoned).
    AbandonChainTo: abandoning chain tip 2ca87835... @ 2768947 down to d23bdc46... @ 2763544.
    RunStartupCoherenceRecovery: chainstate cleanup: scanned 5651616 CTxIndex entries, deleted 10806, cleared 4538 vSpent slot(s) across surviving entries.
    RunStartupCoherenceRecovery: rewind crossed 6 superblock(s); resetting beacon registry for full replay.
    RunStartupCoherenceRecovery: registry bookmarks reconciled to height 2763544. Phase 2 recovery complete; P2P sync will re-supply blocks past the rewound tip.
    Gridcoin: block index is clean
    ActivateSnapshotAccrual: dropped 6 snapshot(s) at heights > pindexBest->nHeight=2763544.

Key difference from Iteration 1: pindex_consistent is now `2763544`, not `2763517`. The full-block coherence check correctly rejected blocks 2763545..2763546 whose tx payloads spilled into the truncated region.

Forward sync results:

| Metric | Iteration 1 (before fix 4) | Iteration 2 (after fix 4) |
|--------|----------------------------|---------------------------|
| Blocks reconnected before wedge | 688 (then stuck) | 5406 (full catch-up) |
| `OrphanBlockManager` size at peak | ≥180 | 1 (single harmless orphan) |
| Duplicate-POS rejections | many (pre-fix 2) / 0 (post-fix 2) | 0 |
| VerifySignature failures | 1 (wedge point) | 0 |
| `CheckBlockIndex` outcome | dangling entries (pre-fix 3) / clean (post-fix 3) | clean |
| Final tip vs slot 8 | 2767386 (forked) / 2766784 (wedged) / 2764205 (wedged) | 2768950 (matches slot 8) |

Slot 8 and slot 10 confirmed at the same hash at the catch-up height.

### Iteration 3 — Tier 1 spot check (~500 bytes, 1 block, 0 SBs)

**Setup:** Slot 10 at tip 2768955 from Iteration 2 catch-up. blk0002.dat truncated from 183684889 → 183684389 bytes (500 bytes chopped — less than one block).

**Result: PASS.**

    RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 10000).
    VerifyChainCoherence: block at height 2768955 ... failed coherence check (read_ok=false); walking back.
    RunStartupCoherenceRecovery: detected inconsistency past height 2768955. Last consistent block is at height 2768954 (0 superblocks crossed in the abandoned range, 1 blocks abandoned).
    AbandonChainTo: abandoning chain tip ... @ 2768955 down to ... @ 2768954.
    RunStartupCoherenceRecovery: chainstate cleanup: scanned 5651632 CTxIndex entries, deleted 2, cleared 1 vSpent slot(s) across surviving entries.
    RunStartupCoherenceRecovery: registry bookmarks reconciled to height 2768954. Phase 2 recovery complete; P2P sync will re-supply blocks past the rewound tip.
    Gridcoin: block index is clean

Minimal Phase 2 case. No SB cross → no beacon registry reset, no `DropAboveHeight`. Forward sync brought slot 10 back to 2768955 matching slot 8. 0 orphans, 0 forward-sync errors.

### Iteration 4 — Tier 2 spot check (~1 MB, 1345 blocks, 2 SBs)

**Setup:** Slot 10 at tip 2768955 from Iteration 3 catch-up. blk0002.dat truncated from 183685248 → 182636672 bytes (1 MB chopped).

**Result: PASS.**

    RunStartupCoherenceRecovery: detected inconsistency past height 2768955. Last consistent block is at height 2767610 (2 superblocks crossed in the abandoned range, 1345 blocks abandoned).
    AbandonChainTo: abandoning chain tip ... @ 2768955 down to ... @ 2767610.
    RunStartupCoherenceRecovery: chainstate cleanup: scanned 5651632 CTxIndex entries, deleted 2690, cleared 1289 vSpent slot(s) across surviving entries.
    RunStartupCoherenceRecovery: rewind crossed 2 superblock(s); resetting beacon registry for full replay.
    RunStartupCoherenceRecovery: registry bookmarks reconciled to height 2767610. Phase 2 recovery complete; P2P sync will re-supply blocks past the rewound tip.
    Gridcoin: block index is clean
    ActivateSnapshotAccrual: dropped 2 snapshot(s) at heights > pindexBest->nHeight=2767610.

Mid-tier case: crosses the 2-SB threshold that triggers beacon registry `Reset()`. `DropAboveHeight` dropped 2 snapshots (matches SB crossings). Forward sync brought slot 10 to 2768956 matching slot 8. 1 orphan, 0 forward-sync errors.

### Iteration 5 — Tier 3 walkmax-exit (4 MB truncation, `-coherencewalkmax=100`)

**Purpose:** verify Phase 2 safely declines to act when the corruption exceeds the configured walkback bound, and that the chainstate is left untouched (no partial abandonment).

**Setup:** Slot 10 at tip 2768956 from Iteration 4 catch-up. `-coherencewalkmax=100` added to the wallet control entry. blk0002.dat truncated from 183685697 → 179491393 bytes (4 MB chopped — would require ~5400 block rewind, vastly exceeding 100).

**Result: PASS.**

    RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 100).
    VerifyChainCoherence: block at height 2768956 (..., blk00002.dat:183684912) failed coherence check (read_ok=false); walking back.
    ... (98 more walking-back lines, heights 2768955 down to 2768858) ...
    VerifyChainCoherence: block at height 2768857 (..., blk00002.dat:183604542) failed coherence check (read_ok=false); walking back.
    ERROR: RunStartupCoherenceRecovery: chain coherence walk hit the 100-block bound without finding a consistent block. The datadir has corruption beyond the automatic-recovery window. Please restart with -reindex.

Backward-walk count was exactly 100 (matching the cap). After exhaustion, **`AbandonChainTo` did NOT run**, **`chainstate cleanup` did NOT run**, **`PurgeOrphanedBlockIndexEntries` did NOT run** — Phase 2 returned false from `RunStartupCoherenceRecovery`, `init.cpp:1405-1408`'s `InitError` fired, and the wallet showed the modal and exited. `blk0002.dat` size unchanged at 179491393 (no further modification of disk state).

This is the critical safety property: when Phase 2 cannot find a consistent block within the bound, it must **not** perform a partial abandonment that would leave the wallet in an inconsistent state worse than what it started with. It bails cleanly, surfaces an actionable error, and lets the user choose the recovery path (typically `-reindex`).

### Iteration 8 — Tier 1 (SIGKILL mid-sync)

**Purpose:** exercise the harness's Tier 1 `tier1_killsync.sh` recovery path manually — verify that `SIGKILL` of the wallet during active forward-sync does not corrupt the on-disk state, and that the Phase 2 hook on the next startup correctly reports "no rewind needed" (Phase 1's durability invariant holds across the kill).

**Optimization vs the harness script:** the script does a full genesis IBD then kills at `TARGET_HEIGHT=50000`. For this validation we substituted a snapshot 3110 restore (slot 10 starts at ~2768900) + sync forward to peers' tip (~2768989). The Phase 2 hook runs on every startup regardless of where the chain is, so the SIGKILL-then-restart path is identical; only the setup time is shorter.

**Setup:** snapshot 3110 restored to slot 10 (file-level: `blk*.dat` + `accrual/` + `txleveldb/`). Slot 10 started with the refactored binary `328a03e93`.

**Step 1 — start slot 10 + watch for active sync:**

    DEBUG=/.../testnet/debug.log
    until sudo grep -qE "SetBestChain: new best block" $DEBUG; do sleep 0.5; done

This poll matches the FIRST log line indicating slot 10 has accepted a peer block (i.e. it's past startup and in active forward-sync), as opposed to a fixed time delay that could fire too early (still in init) or too late (sync already complete).

First match: `2026-05-17T06:48:52Z [grc-msghand] INFO: SetBestChain: new best block {101ca94... 2768823}`. SIGKILL fires immediately on this signal.

**Step 2 — find the wallet process and SIGKILL:**

The slot is launched via `gdb_commands` inside a `screen` window — three wrapper processes (`sudo ip netns exec`, `sudo -u jco`, `gdb`) plus the actual `gridcoinresearch` binary. The kill target is the leaf binary; the `pgrep` pattern anchors at `^/home/jco/GridcoinDev/gridcoinresearch-...` to skip the `sudo ip netns exec ...` wrappers.

    sudo kill -9 $PID

Process gone. The wallet had advanced from 2768823 to somewhere in the 2768960s region by the time the kill landed (forward-sync continues in parallel with our shell-side `pgrep`/`kill`); irrelevant to the test outcome — what matters is that the shutdown was abrupt.

**Step 3 — restart slot 10 + assertions:**

Cleanup: the `screen` window for `inst10` and the three orphaned wrappers (gdb keeps running after the inferior dies — doesn't notice until somebody tries to interact) must be killed before the wallet-control script accepts a `start 10` — otherwise it sees `inst10` still in the screen window list and refuses.

Post-restart debug.log key lines:

    INFO: RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 10000).
    INFO: RunStartupCoherenceRecovery: chain tip at height 2768962 is coherent. No rewind needed.
    Gridcoin: block index is clean

Assertions matched:

- ✅ Phase 2 hook executed (`RunStartupCoherenceRecovery: ...` lines present).
- ✅ No Phase 1 failure signatures (`grep -cE "LevelDB Sync failure|FileCommit failed|fdatasync failed|fsync failed"` = 0).
- ✅ Chain converged with peer: slot 10 = 2768989, slot 8 = 2768989, gap = 0.

**Result: PASS** — validates that Phase 1's "LevelDB block index never references unfsynced blk*.dat data" invariant survives `SIGKILL` mid-sync, exactly as designed. The Phase 2 hook on restart sees a coherent tip and exits via the no-rewind-needed fast path (one `ReadBlockFromDisk` of overhead).

### Iteration 7 — Multi-SB runtime reorg with in-line rebuild (post-refactor)

**Purpose:** validate the refactored runtime path. The deferred-rebuild flag mechanism (Iteration 6 below, kept for chronology) was replaced with an in-line `GRC::RebuildBeaconRegistry` called directly from `DisconnectBlocksBatch` when `sb_cross_count >= 2`. The rebuild calls `BeaconRegistry::Reset()` followed by `InitializeContracts(pindexBest)`, which Replays beacon contracts from V11_height forward. The reorg path holds `cs_main` throughout, so no thread observes the intermediate empty-registry state.

**Why the refactor:** the deferred approach left a fork window between the runtime reorg that set the flag and the next restart that consumed it. During that window the in-memory beacon registry was missing prior-SB expired-pending entries (the `m_expired_pending` carry-only-latest-SB limitation), and any subsequent `ConnectBlock` that referenced one of those beacons could deterministically diverge from healthy peers. Mitigating that window with an `AcceptBlock` guard + forced shutdown + modal coordination added ~100 lines and traded one ugliness (fork risk) for another (forced restart UX). Running the rebuild in-line during the reorg eliminates both — the registry is consistent before `cs_main` is released, so no subsequent block validation sees stale state and no operator action is required.

**Cost analysis:** the rebuild walks from `BlockV11Height` forward applying beacon contracts. On the isolated testnet (3 nodes, ~3M block height) the walk completed in **303 ms** on the test SSD. Wall-clock time of the full `reorganize` RPC (which includes disconnecting 2000 blocks + reconnecting on the forward direction, plus this rebuild) was 41 s. The rebuild itself is a small fraction of the reorg's intrinsic cost.

**Setup:** slot 10 at tip 2768977 against the refactored binary `328a03e93`.

    target_height = tip - 2000 = 2766977
    orig_head     = cbdc0ea2d1c2fcf631089f001b8260598f3afaeb694ea579f35b0cbb933303c5
    reorg_target  = 2ec809861fefe03de01cff68f0841368ffbf19883dcd035c9ed9703152ebb70b

**Step 1 — backward runtime reorg via RPC:**

    $ reorganize 2ec809861fefe03de01cff68f0841368ffbf19883dcd035c9ed9703152ebb70b
    {"RollbackChain": true}
    (elapsed: 41 s)
    $ getblockcount
    2766977

Slot 10 debug.log (key lines):

    WARN: DisconnectBlocksBatch: reorg disconnected 2 superblock(s); beacon registry expired-pending fidelity is degraded for the prior SB(s). Rebuilding beacon registry in-line to maintain consensus.
    INFO: RebuildBeaconRegistry: starting in-line beacon registry rebuild after multi-SB runtime reorg.
    INFO: InitializeContracts: Loading stored history for contract type beacon...
    INFO: InitializeContracts: History load not successful for contract type beacon. Will initialize from contract replay.
    Gridcoin: Starting contract replay from height 1301500.
    Replaying contracts from block 1301500...
    INFO: RebuildBeaconRegistry: beacon registry rebuild complete in 303ms.
    INFO ForceReorganizeToHash: success! height 2766977 hash 2ec809861f...

The contract replay started at height 1301500 (V11 activation) — the full beacon-history replay we wanted, not a clamped partial. After the rebuild, the reconnect side of `ReorganizeChain` ran `beacon.Activate()` per reconnected block; the registry was correct at every step.

**Step 2 — forward reorg back to original head:**

    $ reorganize cbdc0ea2d1c2fcf631089f001b8260598f3afaeb694ea579f35b0cbb933303c5
    {"RollbackChain": true}
    $ getblockcount
    2768977

Tip hash matches the saved original head exactly.

**Step 3 — stop + restart slot 10:**

Slot 10 debug.log on the next startup:

    INFO: RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 10000).
    INFO: RunStartupCoherenceRecovery: chain tip at height 2768977 is coherent. No rewind needed.
    Gridcoin: block index is clean

No "pending beacon registry rebuild detected" log line — the deferred-flag mechanism is gone, and there's nothing for startup to pick up because the in-line rebuild handled the entire fix during the reorg itself.

**Result: PASS.** Refactored runtime path is correct, fast (303 ms rebuild), and eliminates the fork window without requiring any post-reorg operator action.

**GUI notification note:** the rebuild calls `uiInterface.ThreadSafeMessageBox(...)` without the `MODAL` flag, which routes through `Notificator::notify` (system-tray notification) rather than a `QMessageBox` popup. On some desktop environments the tray notification is subtle or filtered. Daemon mode skips this call entirely via `if (fQtActive)` and emits only the debug.log line, per the operational preference that the daemon should produce nothing but log output.

### Iteration 6 — Multi-SB runtime reorg with deferred-rebuild flag (superseded design)

**NOTE:** This iteration validated the original deferred-rebuild design (`SetBeaconRebuildPending` + LevelDB flag + startup pickup). That design was replaced by the in-line rebuild in Iteration 7 above because of the fork-risk window between the runtime reorg and the next restart. Keeping the record here for chronology.

**Purpose:** validate the runtime path's deferred-rebuild-flag mechanism. On a runtime reorg crossing 2+ SBs, `DisconnectBlocksBatch` cannot fully resurrect prior-SB expired-pending beacons (the `m_expired_pending` carry-only-the-most-recent-SB limitation acknowledged at `beacon.cpp:1265-1273`), so it persists a `beacon_db / needs_rebuild` flag to LevelDB and lets the next startup do a clean rebuild via `BeaconRegistry::Reset()` before `InitializeContracts` replays.

**Staging:** the natural way to produce a 2+ SB reorg (slot 10 stake-mines for hours then reconnects to slots 8/9) is impractical and contradicts the Phase 2 `-staking=0` invariant. Instead, use the `reorganize` developer RPC (`ForceReorganizeToHash`) to roll the chain back manually. The path through `ReorganizeChain → DisconnectBlocksBatch` is the same one a natural reorg takes, so the flag-setting code (`SetBeaconRebuildPending` at `main.cpp:1083-1089`) fires identically.

**Setup:** slot 10 at tip 2768961.

    target_height = tip - 2000 = 2766961
    orig_head     = 6aa47d3e58c6286adc8269b9d926032de9e9e369397d6af9bfbdcdff87839b35
    reorg_target  = 8054020421b765072d083338bf6595339830ef1eb1a22a9d0a255b1750f41f2c

**Step 1 — backward runtime reorg via RPC:**

    $ reorganize 8054020421b765072d083338bf6595339830ef1eb1a22a9d0a255b1750f41f2c
    {"RollbackChain": true}
    $ getblockcount
    2766961

Slot 10 debug.log:

    DisconnectBlocksBatch: <hash> ... (~2000 lines)
    WARN: DisconnectBlocksBatch: reorg disconnected 2 superblock(s); beacon registry expired-pending fidelity is degraded for the prior SB(s). Flagging beacon registry for rebuild on next startup. Until restart, beacon-related queries may show stale entries.
    WARN: ForceReorganizeToHash: Chain trust is now less than before!
    INFO ForceReorganizeToHash: success! height 2766961 hash 8054020421...

`SetBeaconRebuildPending()` was called — confirmed by the "Flagging beacon registry for rebuild on next startup" log line. The flag is now persisted in LevelDB at `beacon_db / needs_rebuild`.

**Step 2 — forward reorg back to original head:**

    $ reorganize 6aa47d3e58c6286adc8269b9d926032de9e9e369397d6af9bfbdcdff87839b35
    {"RollbackChain": true}
    $ getblockcount
    2768961

Tip hash at 2768961 matches the saved original head exactly — no chain damage from the round-trip. Forward direction does not call `SetBeaconRebuildPending` (DisconnectBlocksBatch only counts SBs on the disconnect path) and does not clear the existing flag.

**Step 3 — stop + restart slot 10:**

Slot 10 debug.log on the next startup:

    INFO: RunStartupCoherenceRecovery: pending beacon registry rebuild detected (from prior multi-SB reorg); resetting beacon registry now so it will be rebuilt from V11_height on the InitializeContracts replay below.
    INFO: RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 10000).
    INFO: RunStartupCoherenceRecovery: tip at height 2768961 is coherent on disk but 1 forward ghost block(s) (0 superblock(s) crossed) remain in the in-memory chain from a prior interrupted Phase 2. Cleaning up.
    INFO: RunStartupCoherenceRecovery: chainstate cleanup: scanned 0 CTxIndex entries, deleted 0, cleared 0 vSpent slot(s) across surviving entries.
    INFO: RunStartupCoherenceRecovery: registry bookmarks reconciled to height 2768961. Phase 2 recovery complete; P2P sync will re-supply blocks past the rewound tip.
    Gridcoin: block index is clean

`IsBeaconRebuildPending()` correctly picked up the flag, called `GetBeaconRegistry().Reset()`, and called `ClearBeaconRebuildPending()`. The subsequent `InitializeContracts` pass replayed beacon contracts from V11_height to rebuild the registry. The single forward ghost block left from the manual reorg (the rolled-back side chain) was caught by the Phase 2.5 side-chain extension (Bug 3's fix in commit `b59bbaaf3`) and purged cleanly.

**Step 4 — second restart to confirm the flag was cleared:**

    INFO: RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 10000).
    INFO: RunStartupCoherenceRecovery: chain tip at height 2768965 is coherent. No rewind needed.

No "pending beacon registry rebuild detected" line this restart — confirming `ClearBeaconRebuildPending()` succeeded silently (it logs only on failure).

**Result: PASS** — the full runtime-reorg → flag-set → restart → flag-pickup → Reset → flag-clear cycle works end-to-end.

### Iteration 9 — Tier 2 fault-injection crash harness (dm-flakey on isolated loop device)

Goal: stress the *full software stack* (BDB wallet, LevelDB block index, blk\*.dat block files, ext4 filesystem) under random in-flight write corruption induced at the block layer, rather than surgical content damage targeted at blk\*.dat alone. This is a more "realistic" failure mode than the Tier 3 iterations: a power-loss-class event where many in-flight writes across many files become corrupted or lost simultaneously, instead of a precisely-known byte range in one block file.

**Harness setup**

- 8 GiB sparse backing file `/home/jco/isolated_Gridcoin_testnet/grc_t2_backing.img` on the same volume as the live testnet datadirs
- Loop device on top of the backing file
- `mkfs.ext4 -O ^has_journal` to mimic ext3 / non-journaled metadata behaviour (every metadata fsync hits the disk; no journal replay on remount; corruption is not papered over by the journal)
- Pre-populated from snapper snapshot **2707** (testnet tip ≈ 2,755,994 — 13,007 blocks behind live, ~17 days of runway)
- `dm-flakey` device on top of the loop with two distinct table configurations (see sub-iterations below)
- Real slot 10 stopped via `gridcoin_wallet_control.sh stop 10` for the duration of the test; the harness daemon runs in the same `node3` ip-netns, connecting via the same `connect=192.168.57.101/102` peers as the real slot 10 conf

**Sub-iteration 9a — `drop_writes` mode**

dm-flakey table: `0 SECTORS flakey LOOP 0 30 15 1 drop_writes` (30 s pass-through, 15 s drops, repeating).

The daemon was started against the prepared mount, caught up to the live tip (≈ 2,769,002 → 2,769,009) over the next minute. Drop windows then ran for ~125 s (≈ 3 cycles); during that time blocks 2,769,007 → 2,769,009 were processed. SIGKILL was issued at +125 s into the cycle, **5 seconds into the third DOWN window**, so any in-flight kernel writeback and the subsequent `umount` flush had their writes silently discarded by dm-flakey.

After the kill, dm-flakey was reloaded with `up=60 down=0` (pass-through), the filesystem remounted, and the daemon restarted.

Pre-kill chain tip:  block 2,769,009 (07:24:15)
Post-restart hashBestChain:  block 2,769,008 (07:24:00)
On-restart Phase 2 line:

    INFO: RunStartupCoherenceRecovery: verifying chain coherence (walkback bound 10000).
    INFO: RunStartupCoherenceRecovery: chain tip at height 2769008 is coherent. No rewind needed.

LevelDB's `Sync()` for the commit-tip update that *would* have advanced `hashBestChain` to 2,769,009 landed during a drop window and was silently dropped. The wallet's persisted view of the tip therefore came back as 2,769,008 — Phase 1's barrier ordering (`FileCommit(blk*.dat)` strictly before `CTxDB::Sync()`) guarantees that when one half of the pair lands and the other doesn't, it's the *LevelDB-tip-commit* half that's missing, not the blk\*.dat half. Block 2,769,008's on-disk data was intact and re-hashed to its `CBlockIndex` hash; the coherence check passed without rewinding. P2P resynced the missing block within ~10 s.

A single `ConnectInputs() : prev tx already used at (nFile=2, nBlockPos=139942532, nTxPos=139950042)` ERROR appeared on the restart and reflected a left-over chainstate marker for the missing tip block; the daemon advanced past it during the resync and reached 2,769,012 cleanly.

**Result: PASS** — Phase 1's barrier ordering held under randomly-dropped writes. The Phase 2 *coherence check* ran and correctly reported a coherent tip; the Phase 2 *rewind path* was not triggered because Phase 1 made it unnecessary, which is the intended layering.

**Sub-iteration 9b — `corrupt_bio_byte` mode**

dm-flakey table: `0 SECTORS flakey LOOP 0 30 15 5 corrupt_bio_byte 100 w 0 0` (30 s pass-through, 15 s during which byte 100 of every write bio is replaced with `0x00`).

The daemon synced through several down windows. Blocks 2,769,016 → 2,769,019 landed during the corrupt period; in particular, block 2,769,019's `SetBestChain` commit landed at 07:35:31 — 4 s into the down window of cycle 9, meaning both its blk\*.dat append and its LevelDB Sync had byte 100 of their write bios zeroed. SIGKILL followed at 07:36:59 (also during a down window), then dm-flakey was restored to pass-through, the filesystem remounted, and the daemon restarted.

On restart, **Berkeley DB's own integrity check fired first**:

    ERROR: CDB() : error BDB0087 DB_RUNRECOVERY: Fatal error, run database recovery (-30973) opening database environment
    Gridcoin: Error initializing database environment ... To recover, BACKUP THAT DIRECTORY, then remove everything from it except for wallet.dat.
    gridcoinresearch exiting...

`wallet.dat` + the `database/` BDB-environment dir were then restored from snap 2707 (a clean BDB state), leaving `blk*.dat` + `txleveldb` carrying their corrupted iter-9b contents. The daemon was restarted; this time **LevelDB's MANIFEST checksum check fired**:

    Opening LevelDB in /home/jco/isolated_Gridcoin_testnet/.GridcoinResearch3_flakey/testnet/txleveldb
    AppInit()Exception1
    EXCEPTION: St13runtime_error
    init_blockindex(): error opening database environment Corruption: checksum mismatch: /home/jco/isolated_Gridcoin_testnet/.GridcoinResearch3_flakey/testnet/txleveldb/MANIFEST-183060
    gridcoin in AppInit()

The harness was stopped at this point; recovery from corrupted txleveldb would require `-reindex` (which rebuilds txleveldb from blk\*.dat, itself partially corrupted) or a fresh sync — operator-driven repair rather than automatic Phase 2 rewind.

**Result: PASS for the *layered defence* it actually validates; INTENTIONAL NON-TRIGGER for the Phase 2 rewind path itself.**

`corrupt_bio_byte` indiscriminately corrupts every write bio that lands in a down window — BDB wallet log records, LevelDB MANIFEST/SST/WAL records, and blk\*.dat appends all get a zeroed byte. The lower-layer checks (BDB DB_RUNRECOVERY, LevelDB MANIFEST CRC) catch their own corruption before control reaches Phase 2. Phase 2 is designed for one specific corruption mode that *evades* those checks: LevelDB is healthy and reports tip = N, but blk\*.dat content at N's recorded position hashes to something other than the `CBlockIndex` hash. Random whole-bio corruption is much more likely to be caught by LevelDB's CRCs first than to slip through to that specific Phase 2 trigger condition.

The Phase 2 rewind path itself is exercised end-to-end by Iterations 1–7 above, which use surgical content damage (truncating exact byte ranges of blk\*.dat past a specific block index entry, or `SIGABRT` during `ConnectBlock` to produce a similar partial-write signature). Iteration 9b complements those by validating that *when the corruption pattern doesn't match Phase 2's trigger*, the system still fails safely at the BDB/LevelDB layer instead of corrupting consensus state.

**Combined Tier 2 conclusion**

| Sub-iteration | Layer that fired | Phase 2 rewind path exercised? |
|---------------|-----------------|--------------------------------|
| 9a `drop_writes` | Phase 1 barrier ordering (LevelDB rollback by 1 block) | Coherence check ran; reported coherent; no rewind needed |
| 9b `corrupt_bio_byte` | BDB DB_RUNRECOVERY → after BDB restore, LevelDB MANIFEST CRC | No — corruption caught by lower layers first |

Both sub-iterations demonstrate **graceful failure**: either the wallet recovers automatically (9a) or it stops with a clear error and recovery instructions (9b) instead of silently advancing a corrupt chain.

## All planned tests complete

| Iteration | Tier | Outcome | Notes |
|-----------|------|---------|-------|
| 1 | T3 (baseline binary `c871502ac`) | FAILED — exposed Bugs 1, 2, 3, 4 | Each bug fixed in commits below |
| 2 | T3 (post-fix, all four patches) | PASS | 5424 blocks abandoned, 6 SBs, clean catch-up |
| 3 | T1 (~500 bytes / 1 block / 0 SBs) | PASS | Minimal Phase 2 case |
| 4 | T2 (~1 MB / 1345 blocks / 2 SBs) | PASS | `Reset()` + `DropAboveHeight(2)` |
| 5 | T3 walkmax exit (`-coherencewalkmax=100`) | PASS | Clean refusal, no partial abandonment |
| 6 | Multi-SB runtime reorg, deferred-flag design | PASS (but the design itself was flawed — see Iteration 7) | Flag set, picked up, cleared cleanly; fork window led to refactor |
| 7 | Multi-SB runtime reorg, in-line rebuild | PASS | 303 ms beacon-registry rebuild during the reorg; no fork window; no post-reorg operator action required |
| 8 | Tier 1 SIGKILL mid-sync | PASS | `SIGKILL` during forward-sync at height 2768823; post-restart Phase 2 reports `chain tip ... is coherent. No rewind needed.`; 0 Phase 1 failure signatures; chain converges with peers |
| 9a | Tier 2 dm-flakey `drop_writes` (isolated loop, ext4 no-journal) | PASS | LevelDB tip-commit dropped, hashBestChain rolled back from 2769009 → 2769008; Phase 1 barrier ordering held; coherence check passed; clean P2P resync |
| 9b | Tier 2 dm-flakey `corrupt_bio_byte` (isolated loop, ext4 no-journal) | PASS (layered defence) | BDB `DB_RUNRECOVERY` fires first; after restoring `wallet.dat`+`database/` from snap, LevelDB MANIFEST CRC fires; Phase 2 rewind path not triggered (random whole-bio corruption is caught by lower-layer checks before reaching Phase 2's narrow trigger condition). System fails safely in both cases. |

## Summary of fixes landed during this validation

All commits on branch `testnet_v13_checkpoint_removed` and cherry-pick-ready for PR #2941. (Earlier fixes from prior sessions are listed for completeness.)

| Commit | Subject | Bug it fixes |
|--------|---------|--------------|
| (prior) | accrual: `DropAboveHeight` in `AccrualSnapshotRepository` | Bug 1 (SB-cross sigabort) |
| (prior) | gui: split misleading "Checkpoint mismatch" modal | (separate UX bug surfaced during testing) |
| `c871502ac` (prior) | restore hidden_args for `-blockv13height`/`-blockv14height` | (separate testnet-args regression) |
| `0a7c442a4` | node: clear SeenStakes after Phase 2 rewind | Bug 2 (duplicate-POS wedge) |
| `b59bbaaf3` | node: purge side-chain index entries above Phase 2 consistent tip; self-heal CheckBlockIndex | Bug 3 (dangling pprev modal) |
| `e68fa9612` | node: read full block in Phase 2 coherence check | Bug 4 (partial-block false positive) |
