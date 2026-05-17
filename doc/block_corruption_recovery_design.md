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

### Soft-fail in `LoadBlockIndex` so the Phase 2 hook can actually run

Prior to Phase 2, `LoadBlockIndex` aborted hard on the first `ReadBlockFromDisk` failure of the 1000-block startup verification loop (`dbwrapper.cpp:637-638` and `:757-758`). For a Scenario-C-corrupted datadir this short-circuited startup with a modal "Error loading blkindex.dat" before the Phase 2 hook had a chance to run — defeating the entire automatic-recovery purpose.

The behavior is now: on a read error in the verification loop, log a warning, **break out of the loop without flagging the load as failed**, and return success so the init sequence proceeds. The Phase 2 hook then sees the same corruption via `VerifyChainCoherence` and handles it through the normal recovery pipeline. This is one of the four post-implementation bug fixes (see "Bugs caught by isolated-testnet validation" below).

## The walk-back

`VerifyChainCoherence(int max_walkback) → CoherenceResult`

Walk backward from `pindexBest`, at each step:

1. Read the block bytes the index says are at `[pindex->nFile, pindex->nBlockPos]` using the lower-level overload `ReadBlockFromDisk(block, pindex->nFile, pindex->nBlockPos, Params().GetConsensus(), /*fReadTransactions=*/false)`. This overload uses `SER_BLOCKHEADERONLY` so we read only the header — enough to recompute the hash without paying the cost of deserializing transactions. **Note**: we deliberately do NOT use the `ReadBlockFromDisk(block, pindex, ...)` overload with `fReadTransactions=false`, because that overload's no-transactions branch copies the header from the in-memory `pindex` and returns true without touching disk — defeating the entire purpose of a coherence check (caught during Copilot review of an earlier draft).
2. Compute `block.GetHash(true)` and compare against `pindex->GetBlockHash()`.
3. If they match, the on-disk data is coherent with the index for this block. Return `pindex` as `pindex_consistent` — recovery target found.
4. If the read failed (file missing, partial record) or the hashes differ, the index has lost contact with reality for this block. Log the mismatch, record the block's `(nFile, nBlockPos)` and `CBlockIndex*` into the result (downstream cleanup steps need them), advance `pindex = pindex->pprev`, repeat.
5. Bound the walk by `max_walkback` to keep startup time predictable. Default is well above Phase 1's 5000-block IBD fsync interval (proposed 10000) to leave headroom. If the bound is exhausted without finding a consistent block, log a fatal "data corruption beyond automatic-recovery window — please `-reindex`" and exit cleanly.

In the common case (no corruption), the very first check on `pindexBest` succeeds and we exit immediately — one `ReadBlockFromDisk` of overhead per startup. The expensive walk only runs when corruption is actually present.

### Forward walk after the backward walk

The first draft collected `abandoned_indexes` and `abandoned_positions` *during* the backward walk. Isolated-testnet validation surfaced a second corruption class the backward walk alone could not see: **forward ghost state from a prior interrupted Phase 2**.

Specifically: if a previous run's Phase 2 rewound `pindexBest` in memory and persisted `hashBestChain`, but the new tip's `CDiskBlockIndex.hashNext` was *not* updated on disk (it still pointed at the abandoned chain), then on the next startup `LoadBlockIndex` faithfully rebuilds the abandoned forward chain from `hashNext`. The backward walk now finds the on-disk tip block coherent (it really is — that's the rewound tip) and returns immediately with `pindex_consistent == pindexBest`, "no rewind needed." But `pindexBest->pnext` is non-null, pointing at the resurrected ghost chain. The first P2P-delivered block past the tip then trips `assert(!pindexBest->pnext)` in `DisconnectBlocksBatch`, killing the wallet.

The fix: after the backward walk finds `pindex_consistent` (whether by inconsistency or first-block-OK), perform a forward walk:

```
for ghost := pindex_consistent->pnext; ghost != nullptr; ghost = ghost->pnext:
    abandoned_indexes.push_back(ghost)
    abandoned_positions.insert(pack(ghost->nFile, ghost->nBlockPos))
    if ghost->IsSuperblock(): ++sb_cross_count
```

This single forward pass catches:

1. **The corruption-rewind case** — every block past the newly-discovered consistent tip (whether the chain had one stale block or eighty).
2. **The ghost-only case** — when `pindex_consistent == pindexBest` (backward walk found tip coherent), the forward walk still enumerates any leftover ghosts from a prior interrupted Phase 2 and feeds them into the cleanup pipeline.

`RunStartupCoherenceRecovery` no longer short-circuits the "no rewind needed" path when `abandoned_indexes` is non-empty — it logs the ghost-only case distinctly and runs the full cleanup pipeline (AbandonChainTo to re-persist the severed `hashNext`, CleanAbandonedRange to verify chainstate is consistent, PurgeOrphanedBlockIndexEntries to remove the resurrected entries from `mapBlockIndex` and LevelDB).

### Collected state

The walk pair collects two pieces of state for the downstream cleanup steps:

- `abandoned_indexes`: the `CBlockIndex*` of every block past `pindex_consistent` (whether reached via the backward walk's failures or the forward walk's pnext enumeration). Consumed by `PurgeOrphanedBlockIndexEntries`.
- `abandoned_positions`: the set of `(nFile, nBlockPos)` pairs for those blocks, packed into `uint64_t` for fast hash-set lookup. Consumed by `CleanTxdbAbandonedRange`.

Collecting in the forward pass (rather than during the backward walk as originally drafted) avoids double-accounting when both inconsistency and ghosts coexist, and produces a deterministic enumeration order regardless of which corruption class triggered the walk.

## Abandonment-style rewind + surgical chainstate cleanup

When the walk returns a non-tip `pindex_consistent`, we cannot use the existing `DisconnectBlocksBatch` primitive (`src/main.cpp:933`). That function reads each block from disk to compute the inverse of its effect on the chainstate — but for the blocks we're abandoning, the on-disk data is by definition unreadable or unhashable. Chicken-and-egg.

What we *can* do is roll back the chainstate effects of those blocks directly in the `CTxIndex` LevelDB table without needing the block data. Gridcoin's chainstate is much simpler than Bitcoin Core's modern UTXO database — `CTxIndex` (`src/index/txindex.h:12`) is just `{pos, vector<CDiskTxPos> vSpent}` keyed by tx hash, stored under LevelDB key `("tx", hash)`. The entire effect of `ConnectBlock` on chainstate is:

- A new `CTxIndex` entry per tx in the block, with `pos` pointing into the block's `blk*.dat` region.
- Updated `vSpent[i]` markers on each input tx's `CTxIndex`, set to the spending tx's `CDiskTxPos` (which is inside the block's region).

So the inverse is two transformations over one LevelDB iteration:

1. **Delete** `CTxIndex` entries whose `pos` is in the abandoned `(nFile, nBlockPos)` set.
2. **Clear** `vSpent[i]` entries on remaining `CTxIndex` whose value is in the abandoned set.

Both checks use the same primitive — *"is this `CDiskTxPos`'s `(nFile, nBlockPos)` in the abandoned set?"* — which is an `unordered_set<uint64_t>` lookup, O(1). The set is at most `-coherencewalkmax` entries (default 10000), built in one walk of the in-memory `CBlockIndex` chain (already collected during the coherence walk).

The full Phase 2 abandonment sequence:

1. `AbandonChainTo(pindex_consistent, txdb)` — updates in-memory chain globals (`pindexBest`, `nBestHeight`, `hashBestChain`, `g_chain_trust`, sync time), nulls `pindex_consistent->pnext` so the in-memory tree doesn't dangle forward from the new tip, persists the new `hashBestChain` to LevelDB, **and persists `CDiskBlockIndex(pindex_consistent)` so its on-disk `hashNext` is null**. The `WriteBlockIndex` call is the load-bearing durability piece: without it, the next `LoadBlockIndex` reconstructs `pindex_consistent->pnext` from the stale on-disk `hashNext` and the ghost chain resurrects on every subsequent boot. The function also handles the `pindex_consistent == pindexBest` case (no in-memory rewind needed, but the on-disk `hashNext` may still be stale from a prior interrupted Phase 2 — we always persist the severed state).
2. `CleanTxdbAbandonedRange(abandoned_positions, txdb)` — one sequential scan of the `("tx", *)` keyspace in LevelDB. For each entry: delete if its `pos` is in the abandoned set; otherwise if any `vSpent[i]` is in the abandoned set, clear those specific `vSpent[i]` entries and rewrite the modified `CTxIndex`. Atomic via `TxnBegin`/`TxnCommit`. Two-pass (collect-then-apply) to avoid iterator invalidation while writing. **Implementation note**: the inner loop must call `iterator->Next()` on every iteration regardless of whether the current entry was queued for deletion or rewrite — an early `continue` after the queue insert (the bug that shipped in an early draft) loops forever on the first match and OOMs.
3. `PurgeOrphanedBlockIndexEntries(txdb, abandoned_indexes)` — for each abandoned `CBlockIndex*`: `mapBlockIndex.erase(hash)` and `txdb.EraseBlockIndex(hash)`. **DO NOT call `delete` on the `CBlockIndex*`** — see "Block-index pool allocation" below.

Wallet handling: any `mapWallet` entry whose confirming block hash was abandoned will, on the wallet load + rescan that follows in init step 8, detect that its confirming block no longer exists in `mapBlockIndex` and revert the tx to unconfirmed. Same code path as any small reorg's wallet behavior. No explicit mempool resurrection is needed in the abandonment path: in the canonical-chain re-supply case the same txs come back via blocks (re-confirmed at the same heights); in the divergent-reorg case other peers still hold the txs in their mempools and mempool relay supplies them back.

### Cost

The chainstate scan is paid exactly once per actual Scenario C event:

- **Common case** (no corruption): `VerifyChainCoherence` returns OK on the first hash check; no rewind, no scan, no cleanup. Total cost: one `ReadBlockFromDisk` (~5 ms).
- **Corruption detected**: walk-back (a few `ReadBlockFromDisk` calls) + `AbandonChainTo` (one LevelDB write) + `CleanTxdbAbandonedRange` (one full scan of the `("tx", *)` keyspace) + `PurgeOrphanedBlockIndexEntries` (in-memory erases). ~1–30 seconds on SSD, ~10–60 seconds on HDD depending on chain size. Compared with `-reindex` from scratch (10–20 minutes on HDD), a ~30x–60x speedup.
- **Subsequent startups** after recovery: chain is coherent at the rewound tip; first hash check passes; return immediately. No scan.

The scan cost is **O(txdb size), not O(abandoned range)**. A deeper rewind (within `-coherencewalkmax`) doesn't cost meaningfully more than a shallow one; the rewind distance only affects the size of the `unordered_set` we test membership against, and that lookup is O(1).

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

**Important — this "phantoms harmless" framing applies to registry entries only.** It does *not* apply to chainstate (`CTxIndex` / `vSpent` in the txdb). Chainstate phantoms would silently break `ConnectInputs` validation when re-arriving blocks try to spend outputs that the abandoned blocks already marked spent — that's why chainstate gets the surgical `CleanTxdbAbandonedRange` treatment above instead of being left alone. Registry entries don't have that problem because the registry handler's insert is idempotent under the entry's content hash.

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

## Design decisions captured during review

These are the design choices made (and the alternatives explicitly rejected) during the review pass on the first draft of Phase 2. Recorded here so future maintainers don't have to re-derive them from the PR discussion.

### Chainstate rollback: surgical `CTxIndex` cleanup, not `-reindex`

The first draft assumed phantom `CTxIndex` / `vSpent` entries from abandoned blocks were harmless (mirroring the registry "phantoms harmless" argument). Copilot review pointed out that they're *not* harmless — when re-arriving blocks try to spend the same outputs, `ConnectInputs` reads the stale `vSpent[i]` marker and rejects the input as already-spent. The wallet would silently fail to validate the re-supplied chain.

Two correctness-preserving options were considered:

- **(A) Detection only + require `-reindex`**: walk-back becomes a diagnostic layer; on corruption detection, log a clear message and exit. User must restart with `-reindex`. Simple, safe, but `-reindex` on HDD takes 10–20 minutes and Gridcoin has insufficient GUI affordances to help users understand what's happening — a poor user experience for an automatic-recovery feature.
- **(B) Surgical chainstate cleanup**: scan the `("tx", *)` keyspace in LevelDB, delete entries whose `pos` is in the abandoned range, clear `vSpent[i]` entries whose value is in the abandoned range. Atomic via `TxnBegin/TxnCommit`.

**Decision: option B.** Gridcoin's chainstate (`CTxIndex` with inline `vSpent`, no separate UTXO db) is much simpler than Bitcoin Core's, making the surgical scan tractable: one sequential LevelDB scan, ~10M entries on mainnet, ~1–30 seconds total. ~30x–60x faster than `-reindex` on HDD. The trade-off is ~100 LOC of carefully-tested cleanup code.

Bitcoin Core's analogous solution is undo data (`rev*.dat` files written by `ConnectBlock`). Gridcoin doesn't have that. Adding it would be a substantial separate piece of work; option B uses only existing primitives.

### In-memory orphan cleanup: purge `mapBlockIndex` entries

The first draft left abandoned `CBlockIndex` entries in `mapBlockIndex` "to be overwritten by `AddToBlockIndex` when P2P re-supplies the blocks." That works for the canonical-chain case but leaks RAM permanently in the rare divergent-reorg case.

**Decision: purge them.** RAM is more precious than disk; the canonical-supply case re-populates the entries naturally via `AddToBlockIndex` (find-by-hash → in-place update on the new entry); the reorg case is the one we're trying to handle gracefully and leaving permanent RAM debris from it is wrong.

Safety: the erasure runs at a specific point in init (after `LoadBlockIndex`, before `GRC::Initialize`) where no consumer holds references to the abandoned `CBlockIndex` objects. The abandoned entries are all descendants of `pindex_consistent`, so no non-abandoned entry's `pprev` points at them; `AbandonChainTo` already nulled `pindex_consistent->pnext`; Quorum / Tally / wallet / mempool / net are all not yet initialized. Doing this purge at *runtime* (e.g. from a reorg path) would be much harder because of live consumers.

### Block-index pool allocation — never `delete` a `CBlockIndex*`

`CBlockIndex` objects are allocated from `GRC::BlockIndexPool` (`src/gridcoin/block_index.h`), backed by `std::forward_list<std::array<CBlockIndex, CHUNK_SIZE>>` — chunks of 32,768 contiguous objects with no per-object deallocation path. The class comment is explicit (block_index.h:54-55): *"The pool does not provide a way to return discarded objects because the application never removes or destroys block index entries."*

**Hard rule:** `mapBlockIndex.erase(hash)` is safe (only removes the lookup); calling `delete p` on a pool-allocated `CBlockIndex*` is **undefined behavior**. The address is in the middle of a `std::array`, not a heap allocation; `operator delete` corrupts the C++ runtime's free list at that address. The corruption surfaces *much later* — sometimes in a different thread, sometimes after another network round-trip — as `bad_alloc` or assertion failures in unrelated code.

The original Phase 2 PurgeOrphanedBlockIndexEntries shipped with a `delete p` call. The symptom on first isolated-testnet validation: Phase 2 reported clean completion, the wallet caught up to peers, then on the next P2P-delivered block `assert(!pindexBest->pnext)` fired inside `DisconnectBlocksBatch` because heap corruption from the bogus `delete` had overwritten `pindex_consistent->pnext`'s slot with garbage that happened to dereference as a valid-looking `CBlockIndex*`. The fix is removing the `delete` — the pool slot leaks by design (a few hundred bytes per abandoned block, bounded by `-coherencewalkmax`), and the slot becomes inert because nothing in `mapBlockIndex` points at it and the on-disk `CDiskBlockIndex` record is also gone (see next section).

### `CDiskBlockIndex` orphans in LevelDB — erase them

**Decision reversed during testing.** The first draft argued that the disk records were harmless and could be left in place. They are not — they break Phase 2 durability.

The failure mode: on first run, Phase 2 successfully rewinds `pindexBest` in memory and persists `hashBestChain`, but leaves the abandoned blocks' `CDiskBlockIndex` records in LevelDB unchanged. Crucially, those records carry the `hashNext` field from when the chain was longer — pointing into what is now the abandoned range. On the next restart, `LoadBlockIndex` rebuilds the in-memory tree from those `CDiskBlockIndex` records, including the stale `hashNext` linkages. The result: `pindex_consistent->pnext` is non-null again, pointing at the resurrected ghost chain. Phase 2's backward walk finds the tip coherent (it is — the rewound tip's data is intact on disk) and short-circuits with "no rewind needed." The first P2P-delivered block then trips `assert(!pindexBest->pnext)`.

The fix is two-part:

1. `AbandonChainTo` persists `CDiskBlockIndex(pindex_consistent)` after nulling its in-memory `pnext`, so the on-disk `hashNext` is also null on the new tip (closes one end of the ghost linkage at the boundary between surviving and abandoned ranges).
2. `PurgeOrphanedBlockIndexEntries` erases each abandoned block's `CDiskBlockIndex` from LevelDB via a new `CTxDB::EraseBlockIndex(hash)` helper (closes the other end — the abandoned entries themselves are no longer in LevelDB, so `LoadBlockIndex` can't reconstruct them).

Both are required: (1) alone leaves orphaned `CDiskBlockIndex` records that re-populate `mapBlockIndex` even though their `hashNext` chain terminates harmlessly; (2) alone leaves the rewound tip's `hashNext` pointing at a since-erased hash, which `InsertBlockIndex` resolves as nullptr — but only by accident, and only as long as the LevelDB erase actually persisted (which `Sync` should but is not guaranteed under a fresh crash before the next checkpoint).

The reversal is asymmetric with the `mapBlockIndex` pool rule above: in memory we cannot free (pool constraint); on disk we must erase (durability requirement). The bookkeeping for `EraseBlockIndex` is a single LevelDB Delete per abandoned block, called inside the same scope as the existing `CleanTxdbAbandonedRange` write — no extra `TxnCommit` envelope, the cost noise vs. the chainstate scan is rounding error.

### Phase 2 durability rule

The combined design invariant — captured here so future work doesn't accidentally break it again:

> After `RunStartupCoherenceRecovery` returns successfully, a clean restart must report `chain tip ... is coherent. No rewind needed.` and run zero cleanup work. If the second restart re-detects abandoned indexes or rewinds again, **Phase 2 has not converged** and there is a durability bug in either `AbandonChainTo` (failed to persist `hashNext=null` on the new tip) or `PurgeOrphanedBlockIndexEntries` (failed to erase abandoned `CDiskBlockIndex` from LevelDB).

The durability is verified end-to-end by the truncation harness (Tier 3 below): stop wallet, truncate `blk*.dat` tail by 64 KiB, restart and verify Phase 2 rewinds + cleans up, wait for P2P re-sync, then **restart again** and verify the second startup reports "no rewind needed."

### No mempool resurrection in the abandonment path

A normal `DisconnectBlocksBatch` reorg populates `vResurrect` and re-`AcceptToMemoryPool`s each disconnected tx. The abandonment path skips `DisconnectBlock` entirely and thus doesn't resurrect anything.

**Decision: this is acceptable.** In the canonical-supply case the same txs come back inside the re-supplied blocks and re-confirm at the same heights. In the divergent-reorg case other peers in the network still hold those txs in their mempools (mempool relay is independent of block status) and they flow back via standard `getdata` mempool relay. The wallet reaches the correct steady state either way; the network-wide mempool reaches it through standard relay. The "best-effort" semantics of the mempool absorb the temporary gap.

### Coinbase / PoS-specific handling

Gridcoin is PoS, so coinbases are degenerate (value 0, no spendable script). They have no `vSpent` entries to clear and nothing for later txs to reference. The cleanup deletes the coinbase's `CTxIndex` entry via the `pos` check (same as any other tx in an abandoned block); no special case needed.

Coinstakes are the actual reward-bearing txs and have real input spends; they go through the standard non-coinbase path (`CTxIndex` deleted, kernel UTXO's `vSpent[i]` cleared on the parent tx).

### No "MRC registry" — MRCs are just transactions

There is no MRC registry per se. MRCs are transactions with a special contract payload that pays to a beacon-owned address. Their tx-side state lives in `CTxIndex` like any other tx and is handled by the chainstate cleanup. Their contract-side dependency on the beacon registry is covered by the beacon `Reset()` path when an SB is crossed.

### Stake modifiers, Quorum cache, Tally cache, accrual snapshots

All rebuilt inside `GRC::Initialize`, which runs *after* the Phase 2 hook. They see the rewound tip and rebuild from a coherent chain. No special handling needed in the Phase 2 hook itself — the init sequencing protects us.

## Alternatives considered (and rejected) — registry path

These alternatives apply to the four "regular" registries (`ProjectRegistry`, `ProtocolRegistry`, `ScraperRegistry`, `SideStakeRegistry`). `BeaconRegistry` has its own rebuild path described above when an SB boundary is crossed. Chainstate (`CTxIndex` / `vSpent`) is handled by the surgical `CleanTxdbAbandonedRange` scan documented earlier, not by any of these.

- **Targeted registry reset.** For each registry where `GetDBHeight() > pindex_consistent->nHeight`, call `registry.Reset()` to clear both in-memory maps and LevelDB storage, then let `InitializeContracts` do a full contract replay from `V11_height`. Safe and well-tested (equivalent to `-clearallregistryhistory`), but the full replay grows linearly with chain height and is increasingly expensive — punishing every rare corruption recovery with a from-genesis registry rebuild.
- **Selective revert via `m_previous_hash` chains.** Walk backward through registry entries to undo phantom entries from blocks above the rewound tip. Faster than full replay, but requires writing a new revert path that works without block data (the existing `Revert()` needs `ContractContext` which requires `ReadBlockFromDisk` — the very blocks we cannot read). Significant new code with its own edge cases.

The bookmark clamp + tolerate-phantoms approach above is the lowest-risk middle ground for the regular registries: it uses only existing primitives, requires no new revert path, costs nothing in the common-case recovery (idempotent), and is bounded in the rare-case overhead (orphans, not corruption). It is **not** applied to chainstate — see "What the clamp does *not* do" for why chainstate gets surgical treatment instead.

## Bounds and configuration

- `-coherencewalkmax=<n>` (hidden option, default 10000). Caps the backward walk so a deeply-corrupt datadir doesn't spend the whole startup grinding through `ReadBlockFromDisk`. 10000 is well above Phase 1's 5000-block IBD fsync interval, leaving headroom for atypical fsync skips.
- Skipped entirely if `-reindex` is set (the user is already rebuilding; the walk would be redundant).
- Skipped at genesis (no `pprev`).

## Crash harness — shipped with Phase 2

End-to-end recovery validation needs to reproduce the failure modes that motivate Phase 2 in the first place. `kill -9 <pid>` does not drop the OS page cache, so it does not faithfully reproduce Scenario C. The harness ships in three tiers under `contrib/devtools/crash_recovery_harness/`:

- **Tier 1 — process-level interruption**. Bash wrapper that starts a `gridcoinresearch` against a fresh isolated-testnet datadir, lets it begin syncing, and `kill -9`s it at a configurable target height. Restart, parse the debug.log for the expected Phase 2 INFO lines, assert the chain re-syncs to the same tip as the control nodes. Doesn't reproduce Scenario C (page cache survives), but is a fast smoke test that the Phase 2 hook runs and the recovery code paths execute without crashing. Useful for routine validation and quick CI sketches.
- **Tier 2 — `dm-flakey` device-mapper fault injection**. Sets up a dm-flakey device under the datadir's `blk*.dat` directory, configured to drop writes after a byte threshold. Runs IBD against the isolated testnet, dm-flakey drops `blk*.dat` writes mid-sync, the test then resets the dm device (dropping its pending writes effectively) and restarts the wallet. Reproduces Scenario A directly and a meaningful subset of Scenario C. Linux-only, ~50 lines of dm setup, requires root. This is the script most worth running before merge and after any change to Phase 1/Phase 2 code.
- **Tier 3 — deterministic `blk*.dat` tail truncation**. Scripted procedure (`tier3_truncate.sh`). Stop the wallet cleanly so the active `blk*.dat` is closed. Truncate the file tail by a configurable amount (default 64 KiB — picks off roughly 80 blocks at typical Gridcoin densities). Restart and observe Phase 2: the backward walk fails on the truncated tip, walks back until it finds a coherent block, runs the surgical `CleanAbandonedRange` + `PurgeOrphanedBlockIndexEntries` cleanup, then P2P re-supplies the truncated content from peers. After the chain re-syncs, the test does a **second** clean stop/restart and verifies Phase 2 reports "no rewind needed" — proving the durability invariant. Replaces an earlier draft that used VMware abrupt-power-off, which was hard to automate and only validated detection. Truncation gives the same on-disk symptom (block read fails) with the added benefit of being deterministic and parameterizable.

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

## Bugs caught by isolated-testnet validation

The first-draft implementation passed `test_gridcoin` and the Tier 1 smoke harness but failed on the first end-to-end isolated-testnet run (Tier 3 truncation). Four discrete bugs surfaced; each is captured here so future maintainers don't re-derive the lesson:

### 1. `LoadBlockIndex` hard-failed on Scenario C, never reaching the Phase 2 hook

**Symptom:** wallet showed modal "Error loading blkindex.dat" on the corrupted datadir; never ran Phase 2.

**Root cause:** `LoadBlockIndex`'s 1000-block startup verification loop in `dbwrapper.cpp` aborted with a hard failure on the first `ReadBlockFromDisk` error.

**Fix:** soft-fail on read errors in the verification loop — log a warning, break the loop, return success. The Phase 2 hook then sees the same corruption via the deliberate coherence walk and handles it via the recovery pipeline.

### 2. `CleanAbandonedRange` infinite-looped on the first abandoned-tx match

**Symptom:** wallet hung at "scanning CTxIndex" indefinitely; then `std::bad_alloc` as the work queue grew unbounded.

**Root cause:** an early `continue` after `to_erase.push_back(...)` skipped the `iterator->Next()` call, which lived outside the try-block. The iterator stayed on the same key forever.

**Fix:** restructure the inner loop as if/else so `iterator->Next()` is unconditional on every iteration.

The initial misdiagnosis ("system under memory pressure") delayed the catch — the actual disk was fine and the bad_alloc was downstream of the unbounded queue. Always look at iterator semantics before blaming the environment.

### 3. `PurgeOrphanedBlockIndexEntries` called `delete` on pool-allocated objects

**Symptom:** Phase 2 reported clean completion; wallet caught up to peers via P2P; on the first P2P-delivered block past the rewound tip, `assert(!pindexBest->pnext)` fired inside `DisconnectBlocksBatch`.

**Root cause:** `CBlockIndex` objects are allocated from `GRC::BlockIndexPool` (chunked `std::array`-backed; see `src/gridcoin/block_index.h:54-55`). Calling `delete` on a pool slot is undefined behavior — `operator delete` tried to free a heap chunk at the middle of a `std::array`, corrupting the C++ runtime's free list. The corruption surfaced minutes later when subsequent `CBlockIndex` pool slots got back garbage from the corrupted free list, including `pindex_consistent->pnext`.

**Fix:** remove the `delete p` call. Pool slots leak by design (a few hundred bytes per abandoned block, bounded by `-coherencewalkmax`). Future maintainers must NEVER call `delete` on a `CBlockIndex*` — the constraint is documented in `src/gridcoin/block_index.h` and reiterated in the `PurgeOrphanedBlockIndexEntries` doc comment.

### 4. Phase 2 wasn't durable across restarts (ghost-chain resurrection)

**Symptom:** same `assert(!pindexBest->pnext)` again on the second clean restart, after fix #3 supposedly resolved it.

**Root cause:** even with `delete` removed, the first Phase 2 run left stale `CDiskBlockIndex` records in LevelDB for the abandoned blocks, AND left the rewound tip's `CDiskBlockIndex.hashNext` field pointing into the abandoned range. On the next `LoadBlockIndex`, the in-memory tree was rebuilt with the ghost forward linkage. The backward coherence walk saw the tip was coherent (it really was — the rewound tip's block file data was fine) and returned "no rewind needed" — but `pindexBest->pnext` was non-null again, pointing at the resurrected ghost chain.

**Fix:** three coordinated changes for durability:

1. `VerifyChainCoherence` performs a forward `pnext` walk after the backward walk, catching ghost state even when the tip itself is coherent.
2. `AbandonChainTo` persists `CDiskBlockIndex(pindex_consistent)` after nulling its in-memory `pnext`, so the on-disk `hashNext` is also null. Also drops the early-return when `pindex_target == pindexBest` so the ghost-only case still writes the severed state.
3. `PurgeOrphanedBlockIndexEntries` erases each abandoned block's `CDiskBlockIndex` from LevelDB via a new `CTxDB::EraseBlockIndex(hash)` helper, so the entries can't re-populate `mapBlockIndex` on the next startup.

The combined invariant — *"the second clean restart after Phase 2 recovery must report 'No rewind needed.'"* — is what the truncation harness validates.

### Lessons

- `test_gridcoin` validates the in-process happy path. The chain interactions (heap layout under prolonged use, durability across restarts, multi-node convergence) need on-machine validation. The isolated 3-node testnet is the right environment; the truncation harness is the right test driver.
- The four bugs were diagnosed and fixed within a single multi-hour session, with each fix going through build → deploy → repoint → restart on the isolated testnet. The pattern is robust because each fix is a small, focused commit (one bug = one commit) and the test cycle is short enough that the binary diff is easy to reason about.
- The pool-allocation rule (constraint #3) is the kind of architectural fact that's invisible from the call site. The fix added a doc comment to `PurgeOrphanedBlockIndexEntries`, a memory note in the project's cross-session memory, AND the explicit reference in this design doc — three independent surfaces because anyone touching block-index lifecycle in the future needs to hit at least one of them.
