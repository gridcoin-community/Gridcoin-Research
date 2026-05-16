# Block file corruption recovery — design notes

Running design document for the four-phase plan in issue #2865. Updated as each phase lands.

## Status

| Phase | Scope | Status |
|---|---|---|
| 1 | Coordination invariant: `blk*.dat` fsync paired with LevelDB WAL fsync barrier; shutdown flush after `StopNode()`; surface `FileCommit` / `CTxDB::Sync` failures | Merged 2026-05-15 (PR #2939) |
| 2 | Startup index-vs-file consistency walk; automatic abandonment-style rewind to last consistent block; registry bookmark clamp; beacon registry rebuild on SB boundary cross (Phase 2 abandonment + standard reorg path); crash harness (Tier 1 + Tier 2 + Tier 3 manual procedure) | In progress |
| 3 | `blk*.dat` tail truncation of partial-record debris | Deferred |
| 4 | User-facing recovery UX messaging | Deferred |

This document focuses on Phase 2 — the recovery code that actually *consumes* Phase 1's coordination invariant. Phase 1 by itself only guarantees that the on-disk LevelDB index never references unfsynced `blk*.dat` data; it doesn't recover from a state that violates that guarantee. Phase 2 is the recovery side.

## Failure model recap (from issue #2865)

Three crash scenarios during block write:

- **Scenario A — crash during flat-file write.** Partial record in `blk*.dat`; LevelDB index not yet updated. Index is consistent, flat file has a garbage tail. Safe for chain logic (Phase 3 truncates the tail) but ugly.
- **Scenario B — crash after flat-file write, before LevelDB commit.** Block bytes durable, no index entry pointing at them. The next append seeks past them; the bytes become harmless dead space.
- **Scenario C — crash during IBD with `FileCommit` skipped, LevelDB commit succeeded.** Index entry durable, pointing at `blk*.dat` data still in OS page cache. On restart, the index promises a block at `[nFile, nBlockPos]` but the actual bytes there are garbage or unrelated. **This is what Phase 2 recovers from.** Phase 1 narrowed the window in which this can happen (LevelDB WAL now fsynced alongside `blk*.dat` at every existing fsync boundary, and at shutdown) but cannot eliminate it entirely without invasive batching.

## Hook point in the init sequence

Phase 2 logic runs between two existing call sites in `src/init.cpp`:

- **`LoadBlockIndex()` at init.cpp:1325-1335** — populates the in-memory `mapBlockIndex` tree and sets `pindexBest`. After this, the chain tree is loaded but no contract state exists in memory.
- **`GRC::Initialize(threads, pindexBest)` at init.cpp:1510** — internally runs `InitializeContracts`, which calls each registry's `Initialize()` to load LevelDB-backed state into memory, then `ApplyContracts` to replay contracts forward from the last bookmark.

The Phase 2 hook slots in this window. Critically: **registries have not been initialized yet**, so we can mutate their LevelDB-stored bookmarks (`height_stored`) without having to invalidate any in-memory state.

## The walk-back

`VerifyChainCoherence(int max_walkback) → CBlockIndex* /* last consistent, or nullptr */`

Walk backward from `pindexBest`, at each step:

1. `ReadBlockFromDisk(block, pindex, Params().GetConsensus())` — read the bytes the index says are at `[pindex->nFile, pindex->nBlockPos]`.
2. Compare `block.GetHash(true) == pindex->GetBlockHash()`.
3. If they match, the on-disk data is coherent with the index for this block. Return `pindex` — recovery target found.
4. If they don't match (read failed, or hash differs), the index has lost contact with reality for this block. Log the mismatch, advance `pindex = pindex->pprev`, repeat.
5. Bound the walk by `max_walkback` to keep startup time predictable. Default is well above Phase 1's 5000-block IBD fsync interval (proposed 10000) to leave headroom. If the bound is exhausted without finding a consistent block, log a fatal "data corruption beyond automatic-recovery window — please `-reindex`" and exit cleanly.

In the common case (no corruption), the very first check on `pindexBest` succeeds and we exit immediately — one `ReadBlockFromDisk` of overhead per startup. The expensive walk only runs when corruption is actually present.

## Abandonment-style rewind (not disconnect-style)

When the walk returns a non-tip `pindex_consistent`, we cannot use the existing `DisconnectBlocksBatch` primitive (`src/main.cpp:933`). That function reads each block from disk to compute the inverse of its effect on the chainstate — but for the blocks we're abandoning, the on-disk data is by definition unreadable or unhashable. Chicken-and-egg.

Instead, Phase 2 does an **abandonment rewind**:

1. Update in-memory chain state: `pindexBest = pindex_consistent`, `nBestHeight = pindex_consistent->nHeight`, `hashBestChain = pindex_consistent->GetBlockHash()`, `g_chain_trust.SetBest(pindex_consistent)`.
2. Persist the new tip to LevelDB: `txdb.WriteHashBestChain(pindex_consistent->GetBlockHash())`.
3. Do **not** call `DisconnectBlock` on the abandoned range. The transactions in those blocks were already absorbed into the wallet's `mapWallet` before the crash. They stay there as unconfirmed entries. When P2P re-supplies the canonical chain past `pindex_consistent`, the normal `AcceptBlock` path runs and re-confirms them. (Wallet behavior is identical to any small reorg; this is well-trodden code.)
4. Leave the `CBlockIndex` entries for abandoned heights in `mapBlockIndex` and in LevelDB. They become unreferenced from `hashBestChain` and get naturally overwritten as P2P re-supplies the blocks. Aggressively erasing them would be more code for no benefit (the next `AddToBlockIndex` for the re-arriving block overwrites the entry at the same key).

## Registry bookmark clamp

This is the Gridcoin-specific complication and the source of subtlety worth documenting carefully.

Each contract registry with LevelDB backing (`BeaconRegistry`, `ProjectRegistry`, `ProtocolRegistry`, `ScraperRegistry`, `SideStakeRegistry` — enumerated at `src/gridcoin/contract/registry.cpp:9-15` as `CONTRACT_TYPES_WITH_REG_DB`) stores a "highest block I've processed" bookmark under the LevelDB key `("<keytype>_db", "height_stored")` (e.g. `("beacon_db", "height_stored")`). Format and access:

- Write: `RegistryDB::StoreDBHeight()` at `src/gridcoin/contract/registry_db.h:414`.
- Read: `RegistryDB::LoadDBHeight()` at `src/gridcoin/contract/registry_db.h:437`.

If we rewind `hashBestChain` without clamping these bookmarks, two failure modes appear:

1. **Phantom in-memory entries.** `RegistryDB::Initialize()` (registry_db.h:76-197) loads *all* entries for its key_type from LevelDB regardless of `height_stored` (the load at line 139 is unfiltered), then populates `m_historical`, `entries`, etc. So entries written by contracts in the abandoned blocks will reload into memory as if they were still part of the chain.

2. **`ApplyContracts` short-circuit.** The skip check at `src/gridcoin/contract/contract.cpp:535-547`:
   ```cpp
   std::optional<int> db_height = db_heights.GetRegistryBlockHeight(contract_type);
   if (db_height && pindex->nHeight < *db_height) {
       skip_apply_contract = true;
       break;
   }
   ```
   With the stale bookmark sitting at a height higher than every block in the re-download window, every contract gets skipped — the replay iterates but does no work.

**The clamp:** for each registry, if `LoadDBHeight()` returns > `pindex_consistent->nHeight`, call `StoreDBHeight(pindex_consistent->nHeight)` to bring it down. **This is what unblocks `ApplyContracts` from short-circuiting re-arriving blocks.**

### What the clamp does *not* do — important distinction

**The clamp does not bound what `RegistryDB::Initialize()` loads from LevelDB.** It is tempting to assume that lowering `height_stored` causes Initialize to skip entries past that height during load. It does not — Initialize's read at line 139 is unconditional. The phantom entries from abandoned blocks *will* load into `m_historical` and active maps.

They are nevertheless harmless, in the order to think about it:

1. **Common case — P2P redelivers the canonical chain.** The same blocks arrive, with the same transaction hashes and same contract payloads. `ApplyContracts` runs (now correctly, because the clamp unblocked it), the contract handlers call the registry inserts, the registry keys by hash, so `m_historical[hash]` already has the entry → idempotent overwrite. `entries[key]` gets reassigned to the same shared_ptr target. The LevelDB record is written at the same key as the existing record. Zero divergence, zero new dead bytes.

2. **Reorg case — a competing branch arrives.** The phantoms become orphaned in `m_historical` (still keyed by their original hashes, but no live entries from the new chain reference them). They sit as inert memory + LevelDB weight. They have no effect on consensus or query results. Eventually a `-clearallregistryhistory` would tidy them out.

The cost of the clamp is therefore a small amount of dead LevelDB weight in the rare reorg case, in exchange for unblocking the re-replay of contracts in the common case. Considered against the alternative — refusing to start and forcing the user to `-reindex` — it's a clear win.

## Beacon registry rebuild on superblock boundary cross

The clamp-and-tolerate-phantoms approach above works cleanly for `ProjectRegistry`, `ProtocolRegistry`, `ScraperRegistry`, and `SideStakeRegistry` because those registries are mutated only by their contract transactions — the in-memory state is a deterministic function of the contracts that have been applied.

`BeaconRegistry` is different. Beacons have a two-step lifecycle:

1. `advertisebeacon` contract → `m_pending[id] = beacon` (status PENDING). Normal contract write, identical to other registries.
2. **At superblock commit**, `BeaconRegistry::ActivatePending(beacon_ids, sb_time, sb_hash, height)` runs (called from the superblock processing path, *not* contract replay):
   - Moves each verified beacon from `m_pending` into `m_beacons` under the **composite hash** `Hash(sb_hash, original_pending_hash)`. This composite hash is what `Deactivate` uses to reverse activations.
   - Clears the prior `m_expired_pending` set (`beacon.cpp:1149`).
   - Recomputes a fresh `m_expired_pending` from pending beacons that aged out by this SB's time. These get persisted to LevelDB as `EXPIRED_PENDING` entries.

The reverse path (`BeaconRegistry::Deactivate`, called from `DisconnectBlocksBatch` when a SB block is disconnected) restores activations to pending and resurrects expired-pending beacons from the current `m_expired_pending`. The hard limitation is spelled out in the comments at `beacon.cpp:1265-1273`:

> Note that making this foolproof in a reorganization across more than one SB boundary means we would have to repopulate the expired pending beacon map from the PREVIOUS set of expired pending beacons. This would require a traversal of the entire leveldb beacon structure for beacons, as it is keyed by beacon hash, not CPID or CKeyID. The expense is not worth it.

So `Deactivate` already silently loses fidelity if a reorg crosses more than one SB. The existing code accepts this as "almost inconceivable in a real fork scenario."

### Why Phase 2 abandonment makes this worse

Abandonment-style rewind doesn't even call `Deactivate` — it just rewinds `hashBestChain` and lets `Initialize` reload from LevelDB. After load:

- `m_beacons` contains the SB-activations from the abandoned chain (their LevelDB entries are still there).
- `m_expired_pending` reflects only the *most recent* SB's expired-pending set (the only one that survives in LevelDB; prior SB sets were cleared at their succeeding SB).
- `m_pending` has contract-level pendings from the abandoned chain that hadn't been activated yet.

If the canonical chain re-supplies the same SBs with the same content, the phantoms self-heal — `ActivatePending` is idempotent under composite hashes, and `m_expired_pending.clear()` runs first thing inside `ActivatePending` so each re-processed SB recomputes its own set. **But** in any reorg-style case where the new chain produces different SBs, phantom activations orphan in `m_beacons` and phantom expired-pendings linger in `m_expired_pending` (and on disk).

### The rebuild

If the rewind (whether abandonment or standard reorg-disconnect) crosses one or more superblock boundaries, we do **not** rely on the clamp-and-tolerate-phantoms approach for beacons. Instead:

1. `BeaconRegistry::ResetInMemory()` — clears `m_beacons`, `m_pending`, `m_expired_pending`, `m_historical`, `m_pending_ownership_proofs`, and the beacon-side `BeaconDB`'s in-memory maps.
2. `BeaconRegistry::ClearLevelDB()` — wraps `m_beacon_db.clear_leveldb()` (`registry_db.h:292`), wiping all `beacon` and `beacon_db` LevelDB entries and re-writing the schema version.
3. Set the beacon `height_stored` bookmark to 0 so `ApplyContracts` will replay beacon contracts from `V11_height` forward.
4. Other registries (project / protocol / scraper / sidestake) get the regular clamp — they have no analogous superblock-level state.

The cost is one full beacon contract replay from `V11_height` (around block 1,144,000) up to the recovered tip. Beacon contracts are sparse in the contract stream — `ApplyContracts` walks the chain but only does meaningful work on blocks that actually contain beacon contracts. On the order of minutes on a modern machine, not hours. Acceptable as the rare-corruption-recovery and rare-multi-SB-reorg cost.

### Detection — single shared primitive

A small helper, probably in `src/gridcoin/quorum.h+cpp` adjacent to `Quorum::PopHistory()`:

```cpp
//! Returns true if any superblock was committed at a height strictly greater
//! than `floor_height` and less than or equal to `ceil_height` (inclusive).
//! Used by both the Phase 2 startup walk-back (#2865) and the standard
//! DisconnectBlocksBatch reorg path to decide whether the BeaconRegistry
//! needs a from-scratch rebuild.
bool Quorum::SuperblockCommittedInRange(int floor_height, int ceil_height);
```

Both the Phase 2 abandonment hook and the standard reorg path call this with `floor_height = pindex_consistent->nHeight` (or `pcommon->nHeight` for the reorg case) and `ceil_height = pre_rewind_pindexBest->nHeight`. If it returns true, the rebuild is triggered.

### Standard fork/reorg also gets this treatment

This rebuild applies on **both** paths:

- **Phase 2 abandonment** (this design doc's main subject): walk-back finds `pindex_consistent`, check for SB-cross, rebuild beacon registry if crossed, then clamp the other four registries normally.
- **Standard reorg** (existing `DisconnectBlocksBatch` / `ReorganizeChain`): after the disconnect loop succeeds, check whether the disconnect range crossed any SB. If yes, trigger the same beacon-registry rebuild. This fixes the latent limitation the beacon.cpp comments already acknowledge ("almost inconceivable") and treats it correctly instead of accepting drift.

Each lives in its own commit on the Phase 2 PR. The rebuild trigger is the same helper function on both paths, so future maintainers can't add a third rewind site that forgets it.

## Alternatives considered (and rejected)

- **Targeted registry reset.** For each registry where `GetDBHeight() > pindex_consistent->nHeight`, call `registry.Reset()` to clear both in-memory maps and LevelDB storage, then let `InitializeContracts` do a full contract replay from `V11_height`. Safe and well-tested (equivalent to `-clearallregistryhistory`), but the full replay grows linearly with chain height and is increasingly expensive — punishing every rare corruption recovery with a from-genesis registry rebuild.
- **Selective revert via `m_previous_hash` chains.** Walk backward through registry entries to undo phantom entries from blocks above the rewound tip. Faster than full replay, but requires writing a new revert path that works without block data (the existing `Revert()` needs `ContractContext` which requires `ReadBlockFromDisk` — the very blocks we cannot read). Significant new code with its own edge cases.

The bookmark clamp + tolerate-phantoms approach above is the lowest-risk middle ground: it uses only existing primitives, requires no new revert path, costs nothing in the common-case recovery (idempotent), and is bounded in the rare-case overhead (orphans, not corruption).

## Bounds and configuration

- `-coherencewalkmax=<n>` (hidden option, default 10000). Caps the backward walk so a deeply-corrupt datadir doesn't spend the whole startup grinding through `ReadBlockFromDisk`. 10000 is well above Phase 1's 5000-block IBD fsync interval, leaving headroom for atypical fsync skips.
- Skipped entirely if `-reindex` is set (the user is already rebuilding; the walk would be redundant).
- Skipped at genesis (no `pprev`).

## Crash harness — shipped with Phase 2

End-to-end recovery validation needs to reproduce the failure modes that motivate Phase 2 in the first place. `kill -9 <pid>` does not drop the OS page cache, so it does not faithfully reproduce Scenario C. The harness ships in three tiers under `contrib/devtools/crash_recovery_harness/`:

- **Tier 1 — process-level interruption**. Bash wrapper that starts a `gridcoinresearch` against a fresh isolated-testnet datadir, lets it begin syncing, and `kill -9`s it at a configurable target height. Restart, parse the debug.log for the expected Phase 2 INFO lines, assert the chain re-syncs to the same tip as the control nodes. Doesn't reproduce Scenario C (page cache survives), but is a fast smoke test that the Phase 2 hook runs and the recovery code paths execute without crashing. Useful for routine validation and quick CI sketches.
- **Tier 2 — `dm-flakey` device-mapper fault injection**. Sets up a dm-flakey device under the datadir's `blk*.dat` directory, configured to drop writes after a byte threshold. Runs IBD against the isolated testnet, dm-flakey drops `blk*.dat` writes mid-sync, the test then resets the dm device (dropping its pending writes effectively) and restarts the wallet. Reproduces Scenario A directly and a meaningful subset of Scenario C. Linux-only, ~50 lines of dm setup, requires root. This is the script most worth running before merge and after any change to Phase 1/Phase 2 code.
- **Tier 3 — VMware VM with abrupt power-off**. Manual procedure documented in `tier3_vmware.md`. Uses an existing VMware Workstation instance on the developer's host (already configured). Snapshot before each test run for repeatability; start IBD inside the guest; from the host, `vmrun stop <vm> hard` (or equivalent) to abruptly drop the guest's page cache. Reboot the guest, observe Phase 2 recovery end-to-end. This is the most faithful reproduction of Scenario C in a real machine setting; left as a documented manual procedure rather than automation because the VMware lifecycle is host-specific.

Each tier has its own README and asserts the same recovery checklist:

1. Wallet boots without manual intervention.
2. Debug.log contains the expected Phase 2 INFO lines: "detected inconsistency at height N", "walked back to height M", "registry bookmarks clamped" (or "beacon registry rebuild triggered (SB crossed)" if applicable).
3. After P2P re-sync, `getblockchaininfo` reaches the chain tip of the control nodes.
4. `listbeacons` / `getbeaconstatus` show consistent beacon state with controls (specifically validates the rebuild path).
5. No `LevelDB Sync failure` / `FileCommit failed` in the restart's debug.log.

## Out of scope for Phase 2

- **`blk*.dat` tail truncation** — Phase 3. The walk-back identifies the last coherent block but leaves any garbage bytes past it in place. The next `AppendBlockFile` seeks to end-of-file, so new block writes go *past* the garbage tail — correct, but the tail accumulates. Phase 3 truncates it.
- **User-facing UX** — Phase 4. Phase 2 logs to debug.log; Phase 4 surfaces friendly messages in the GUI.

## Notes for the implementer

- All Phase 2 logic must run with `cs_main` held (the init thread already does at this point in startup, but verify and assert via `EXCLUSIVE_LOCKS_REQUIRED`).
- The clamp must use `RegistryDB::StoreDBHeight()` directly on a `CTxDB` connection rather than going through any registry method that touches in-memory state — those don't exist yet at this point in init.
- Log every step at INFO level. A user inspecting `debug.log` after a crash recovery should see a clear narrative: "detected mismatch at height N, walked back to height M, clamped registry bookmarks, resumed."
- The in-code comment block alongside the bookmark-clamp call should restate the "Initialize loads everything, clamp only unblocks ApplyContracts skip" distinction so a future reader doesn't fall into the same intuition trap.
