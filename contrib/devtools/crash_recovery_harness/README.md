# Block file corruption recovery — crash harness

Validation harness for the Phase 2 startup chain-coherence recovery path
(issue #2865). Three tiers, increasing in faithfulness to the real failure
mode (hard power loss during IBD) and increasing in setup cost.

## Tier 1 — process-level interruption (`tier1_killsync.sh`)

Smoke test. Starts a fresh-datadir wallet, lets it sync to a configurable
target height, sends `SIGKILL`, restarts, and verifies that the Phase 2
recovery code path runs and the chain re-converges with peers.

`kill -9` does **not** drop the OS page cache, so this tier does **not**
reproduce Scenario C (LevelDB-durable-but-blk-volatile) faithfully — in
practice the kernel's writeback completes between processes and the
restart finds a coherent on-disk state. What this tier *does* validate is
that the Phase 2 hook itself runs without crashing, the no-rewind-needed
fast path executes correctly, and the wallet boots cleanly after a hard
process kill. Useful as a fast regression check in CI or after any change
to the coherence module.

## Tier 2 — `dm-flakey` selective fault injection (`tier2_dmflakey.sh`)

Linux-only, root required. Wraps the wallet's `blocks/` directory in a
`dm-flakey` device-mapper target configured to drop writes after a
threshold. Runs IBD, switches dm-flakey into drop-writes mode mid-sync,
sends `SIGKILL`, then resets dm-flakey (which discards the volatile
pending writes from page cache). Restarts; the on-disk state matches the
post-power-loss case where some blk*.dat data never reached durable
storage but its LevelDB index entry did.

Reproduces Scenario A (partial flat-file write) faithfully and Scenario C
(index ahead of data) partially. This is the most useful tier for
routine validation of Phase 1 + Phase 2 together, especially after any
change to the coordination invariant in `WriteBlockToDisk` or to the
Phase 2 walk-back / rewind / rebuild logic.

## Tier 3 — deterministic blk\*.dat truncation (`tier3_truncate.sh`)

Sync the wallet cleanly to a target height, stop it via RPC, truncate
the active `blk*.dat` file by `TRUNCATE_BYTES` bytes from the end, and
restart. The LevelDB block index still references the now-missing
bytes — exactly the post-power-loss state Scenario C describes, but
produced deterministically by file truncation rather than by racing
against the kernel writeback.

Unique value vs the other tiers:

- **Deterministic.** Exact rewind depth == exact truncation depth.
  Repeatable across runs and machines.
- **Parametric.** Sweep `TRUNCATE_BYTES` through small / medium / large
  values to exercise zero-SB-cross (registry clamp), 1-SB-cross
  (single-SB beacon handling), and 2+-SB-cross (`Reset()` + replay)
  code paths in turn. The script's header comment lists suggested values.
- **Exits the recovery window cleanly.** Set
  `EXPECT_REINDEX_REQUEST=1` with a large `TRUNCATE_BYTES` to verify
  Phase 2 correctly logs the "please `-reindex`" message and refuses
  to start when the truncation exceeds `-coherencewalkmax`.
- **Strict assertions.** Unlike Tier 1 (where "no rewind needed" is the
  expected common case) and Tier 2 (where the dm-flakey timing can land
  on either outcome), Tier 3 *guarantees* an inconsistency. The script
  fails if `VerifyChainCoherence` doesn't detect it, if
  `CleanAbandonedRange` doesn't log its scan stats, or if
  `PurgeOrphanedBlockIndexEntries` doesn't run — turning any silent
  regression of those code paths into a hard failure.

What this tier does **not** cover: mid-sector torn writes, LevelDB SST
file corruption, or filesystem-journal artifacts. For Phase 2's
recovery code those don't matter, but if you need to exercise the
whole stack including the kernel page cache and the block device's
write cache, see [`appendix_vmware_powerpull.md`](appendix_vmware_powerpull.md)
for the original manual VMware power-pull procedure.

## What every tier asserts

After the restart in each tier, the harness checks:

1. **Wallet booted without manual intervention.** No `-reindex`
   required (unless `EXPECT_REINDEX_REQUEST=1` in Tier 3, which inverts).
2. **Phase 2 hook executed.** debug.log contains one of:
   - `INFO: RunStartupCoherenceRecovery: chain tip at height N is coherent. No rewind needed.`
     (no corruption was detected — common in Tier 1, expected when fsync caught up)
   - `INFO: RunStartupCoherenceRecovery: detected inconsistency past height N. Last consistent block is at height M (K superblocks crossed in the abandoned range).`
     (corruption detected, rewind taken — expected in Tier 2 / Tier 3)
3. **No Phase 1 failure signatures.** No `LevelDB Sync failure` or
   `FileCommit failed` in the restart log.
4. **Chain converges with peers.** `getblockchaininfo` after a sync
   timeout shows blocks within 1 of the peer node's tip.
5. **Beacon state consistent with peers** (when an SB was crossed and
   the rebuild fired). `listbeacons` row count matches a control node's
   to within reorg-fork tolerance.

Tier 3 additionally asserts that `CleanAbandonedRange` and
`PurgeOrphanedBlockIndexEntries` both ran (proves the surgical
chainstate cleanup path is wired in; a regression to the old
"leave phantoms" behaviour fails the test).

## Invocation pattern

Each script accepts configuration via environment variables (sensible
defaults for the on-host isolated testnet are baked in). Override on the
command line:

```bash
WALLET_BIN=$HOME/path/to/gridcoinresearchd \
TEST_DATADIR=/tmp/grc_crash_harness_test \
PEER_NETNS=node1 \
PEER_DATADIR=$HOME/isolated_Gridcoin_testnet/.GridcoinResearch \
TARGET_HEIGHT=100000 \
./tier1_killsync.sh
```

See each script's header comment for the full set of variables and
defaults.

## Out of scope

These scripts test *the recovery code*. They are not a substitute for
the consensus-touching code review and existing unit tests that validate
the Phase 1 + Phase 2 logic itself. They complement, not replace.
