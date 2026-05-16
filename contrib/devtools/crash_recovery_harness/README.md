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

## Tier 3 — VMware abrupt power-off (`tier3_vmware.md`)

Documented manual procedure. Uses an existing VMware Workstation guest on
the developer's host (configuration host-specific). Snapshot before each
run for repeatability; start IBD inside the guest; from the host,
`vmrun stop <vm> hard` drops the guest's page cache instantly. Reboot
and observe Phase 2 recovery end-to-end. The most faithful reproduction
of Scenario C available — what an actual power loss looks like to the
guest filesystem and the wallet process running inside it.

Left as a manual procedure because the VMware lifecycle is host-specific
and not worth automating until we want CI integration on a VMware-capable
runner.

## What every tier asserts

After the restart in each tier, the harness checks:

1. **Wallet booted without manual intervention.** No `-reindex` required.
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

## Invocation pattern

Each script accepts configuration via environment variables (sensible
defaults for the on-host isolated testnet are baked in). Override on the
command line:

```bash
WALLET_BIN=/home/jco/GridcoinDev/gridcoinresearchd-<ver>-<branch>-<hash> \
TEST_DATADIR=/tmp/grc_crash_harness_test \
PEER_NETNS=node1 \
PEER_DATADIR=/home/jco/isolated_Gridcoin_testnet/.GridcoinResearch \
TARGET_HEIGHT=100000 \
./tier1_killsync.sh
```

See each script's header comment for the full set of variables and
defaults.

## Out of scope

These scripts test *the recovery code*. They are not a substitute for
the consensus-touching code review and existing unit tests that validate
the Phase 1 + Phase 2 logic itself. They complement, not replace.
