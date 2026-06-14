# Windowed Transaction Table — Architecture

> Status: **as-built** on branch `testnet_v13_checkpoint_removed` at tip `900889be6`, the head of PR5-C — the full windowed-model stack. This is the GUI subsystem that renders the wallet's transaction history: the detailed `TransactionView` `QTableView` and the `OverviewPage` recent-transactions list. This file is the **as-built companion** to `doc/transaction_table_windowed_model.md` (the design-of-record / planning history, parts of which remain aspirational); every claim here is anchored to source on this branch. Where the two disagree, this file describes what the code actually does.

## 1. Motivation

### The old model
Historically the GUI used Bitcoin Core's "full replica + sledgehammer refresh" pattern:

- `TransactionTableModel` (a Qt-main-thread `QAbstractTableModel`) held a complete decomposed replica of every wallet row in `TransactionTablePriv::cachedWallet`.
- A client-side `TransactionFilterProxy` (`QSortFilterProxyModel`, `dynamicSortFilter=true`) did all sort and filter.
- On every chain-tip advance, `updateConfirmations()` emitted `dataChanged` over the *whole* model.

That cost O(N) Qt-thread work per block, kept a full in-process replica, took `TRY_LOCK(cs_main)+TRY_LOCK(cs_wallet)` on the paint thread inside `index()`, ran `LOCK2` on the paint thread inside `describe()`, and froze startup with an O(N) decompose under `LOCK2(cs_main, cs_wallet)`.

A subtle trap in the old container: it was ordered **hash-only** (`TxLessThan: a.hash < b.hash`), not chronologically; the visible newest-first order came entirely from the proxy's display sort, not the underlying container.

### Why it ground to a halt
With a large wallet, every block connection forced a model-wide `dataChanged` (O(N) Qt repaint accounting), and the paint thread itself contended for `cs_main`/`cs_wallet` — exactly the locks the validation thread wanted. Startup paid a single O(N) decompose under both global locks.

### The 122k-tx data point
The endpoint was validated on a 121,979-transaction wallet running on a 32-bit ARMv7 ODROID with 1.9 GiB RAM, with smooth scroll/sort/filter — the most constrained real-world target. Note carefully: the in-process rewrite does **not** reduce GUI memory footprint (the full decomposed record set stays resident in the one process); the footprint win is deferred to the multiprocess split (#2937). The win realized here is responsiveness — the render and detail paths no longer touch the wallet locks, and per-block work is O(viewport)/O(volatile) rather than O(N). One old-model cost is explicitly **not** eliminated by this rewrite: the startup/datetime-toggle O(N) decompose under `LOCK2(cs_main, cs_wallet)` (see §4 `reloadAndSnapshot` and §14).

## 2. High-level architecture

The rewrite is a producer/consumer split. An authoritative, Qt-thread-free `GRC::WalletTxStore` (producer) holds the full decomposed history ordered by `(time DESC, hash ASC, idx ASC)`, maintains per-view `GRC::Cursor` objects (server-side filter+sort over absolute record indices), and pushes position-stamped, seqno-stamped, value-typed events down the MPSC `GRC::WalletEventQueue`. Thin Qt consumer models replay those events and fetch windowed slices.

```
   CORE / NODE SIDE (off the Qt thread)                              GUI / CONSUMER SIDE (Qt main thread)
   ┌───────────────────────────────────────────┐                    ┌──────────────────────────────────────────┐
   │ validation / msghand threads               │                    │                                            │
   │  NotifyTransactionChanged (cs_main+cs_wallet)                    │  WalletModel::drainEventQueue()            │
   │  NotifyBlocksChangedForWallet (cs_main)     │                    │   (500ms timer + requestEventDrainSoon)    │
   │      │ decompose + updateStatus             │                    │      │ pop ≤1024 events                    │
   │      │ + populateDisplayLabel               │                    │      ▼                                      │
   │      ▼  enqueue* (O(1), cs_intake leaf)     │                    │   fan-out (by viewId):                      │
   │  ┌──────────────┐                           │                    │   ├─ VIEW_FULL → TransactionTablePriv      │
   │  │ intake deque │  cs_intake                 │                    │   │     (lockstep replica, no gate)        │
   │  └──────┬───────┘                           │                    │   ├─ VIEW_OVERVIEW → OverviewTxModel       │
   │         ▼  (REVERSE_LOCK: drop cs_intake)   │                    │   │     (served-window slice, seqno gate)  │
   │  ┌──────────────────────────────────┐       │                    │   └─ VIEW_DETAILED → DetailedTxModel       │
   │  │ store-worker thread (workerLoop)  │       │                    │         (WindowCache: slice + virtual cnt) │
   │  │  applyIntake → insert/remove/     │       │                    │              │ structural deltas           │
   │  │  update/addressbook  (cs_store)   │       │                    │              │ + content getRows()         │
   │  └──────┬────────────────────────────┘      │                    │              ▼                              │
   │         │ O(N) ordering maint. + cursor drive│                    │      QTableView / QListView paint           │
   │         ▼                                    │  ── getRows /       │              ▲                              │
   │  ┌──────────────────────────────┐            │     getAllRows /    │      TransactionView (viewport reports,    │
   │  │ WalletTxStore (cs_store leaf) │◄───────────┼──── rowForKey ──────┤        anchor capture/restore, filter UI,   │
   │  │  m_records (time,hash,idx)    │            │     getRowDetail    │        CSV export, detail dialog)           │
   │  │  m_by_hash multimap           │            │   (up-channel,      │                                            │
   │  │  m_fields_cache/m_keys_cache  │            │    by value)        │                                            │
   │  │  m_volatile set               │            │                     │                                            │
   │  │  m_cursors (per-view)         │ ── push ───┼──► WalletEventQueue ─┼──► (drained above)                         │
   │  │  m_view_seqno (high-water)    │  (under     │   MPSC, seqno-      │                                            │
   │  └──────────────────────────────┘   cs_store) │   stamped           │                                            │
   │  applyChainTipRefresh() runs INLINE on the validation thread (under caller's cs_main), NOT on the worker.        │
   └───────────────────────────────────────────┘                    └──────────────────────────────────────────┘
```

The `TransactionFilterProxy` is gone (deleted in PR4). The producer is shaped to become the node side and the consumers the GUI side of the future multiprocess split (#2937), with the value-typed payloads as the wire format.

## 3. The lock model

Canonical order: **`cs_main` → `cs_wallet` → `cs_store` (leaf)**. `cs_store` is declared `mutable Mutex cs_store` at `src/qt/wallettxstore.h:326` and the threading contract is documented at `wallettxstore.h:91-97`. `cs_intake` (`wallettxstore.h:368`) is a *separate, independent* leaf.

| Rule | Where enforced |
|------|----------------|
| `cs_store` is NEVER held while acquiring `cs_main`/`cs_wallet` | `getRowDetail` resolves the part under `cs_store`, RELEASES it, then takes `LOCK2(cs_main, cs_wallet)` — `wallettxstore.cpp:844-864`; the inline warning at `:862-863` forbids hoisting the format under `cs_store` |
| `applyChainTipRefresh` takes the wallet locks *outside-in* | `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` (`wallettxstore.h:165`); body takes `cs_wallet` then `cs_store` last (`wallettxstore.cpp:599-600`) |
| `cs_intake` and `cs_store` are never co-held | `workerLoop` drops `cs_intake` via `REVERSE_LOCK(lock)` before `applyIntake` runs (`wallettxstore.cpp:186-188`); `enqueue*` take only `cs_intake` (`:114-152`) |
| The store-worker NEVER takes `cs_main`/`cs_wallet` | `insert/remove/updateTransaction` take only `cs_store` (`wallettxstore.cpp:229,303,486`); this is what makes `reloadAndSnapshot`'s park-while-holding-both-wallet-locks deadlock-free |

Thread placement:
- **Validation/msghand threads** (producers): `NotifyTransactionChanged` (`walletmodel.cpp:713`, `EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet)`), `NotifyBlocksChangedForWallet` (`walletmodel.cpp:862`, `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`). They decompose + status-stamp under the locks they already hold, then `enqueue*` (O(1)).
- **Store-worker thread** (`m_worker`, `wallettxstore.h:375`): drains the intake FIFO, runs the O(N) maintenance under `cs_store`, pushes events.
- **Validation thread, inline**: `applyChainTipRefresh` runs on the caller's thread (NOT the worker) so it can take `cs_wallet` under `cs_main`.
- **Qt main thread**: `drainEventQueue`, all view models, `registerView`/`setView*`/`getRows`/`getAllRows`/`rowForKey`/`getRowDetail`.

The off-`cs_main` guarantee: after PR5-C the Qt render path is entirely off the wallet locks. `data()`/`index()` are pure slice reads (`transactiontablemodel.cpp:209-219`, `detailedtxmodel.cpp:91-106`), status is computed producer-side, and the only Qt-thread code that still takes `cs_main`/`cs_wallet` is the discrete double-click detail dialog (`getRowDetail`, relocated — see §10) and the rate-limited balance recompute (`checkBalanceChanged`).

The annotations are Clang-only (`HAVE_CLANG_THREAD_SAFETY=1`); under GCC they parse as silent no-ops. The Thread Safety (Clang) CI job compiles them under `WERROR_THREAD_SAFETY=ON`.

### ⚠ In-code comment contradiction — a maintenance hazard
The load-bearing exclusion argument for `reloadAndSnapshot` is that **`cs_main` is held at every producer of an insert/remove, including both `CT_DELETED` sites**. Two comments describe these sites, and they **disagree**:

- `reloadAndSnapshot`'s comment (`wallettxstore.cpp:370-379`) is **correct**: it states that both `CT_DELETED` fire sites — `ReorganizeChain` (`main.cpp`, `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`, ~`main.cpp:1106`) and `ResendWalletTransactions` (`wallet.cpp`, `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`, ~`wallet.cpp:2111`) — hold `cs_main` *even though they do not hold `cs_wallet`* at the fire site. That is exactly why holding `cs_main` across the rebuild + drain fully excludes producers.
- `NotifyTransactionChanged`'s comment (`walletmodel.cpp:764`) is **wrong**: it asserts the `CT_DELETED` callsites "DO NOT hold either lock," and cites stale line numbers (`main.cpp:1290` / `wallet.cpp:1349`; the actual sites are nearer `main.cpp:1280` / `wallet.cpp:2212`).

The correct claim is the `cs_main`-held one in `wallettxstore.cpp`. The `walletmodel.cpp:764` comment over-claims in the safe direction for *its own* purposes (it only needs the `CT_NEW`/`CT_UPDATED`/`CT_UPDATING` branch's locks), so the wrong assertion is currently harmless — but a future editor who trusts it and weakens `reloadAndSnapshot`'s `cs_main` hold would reintroduce a producer/rebuild race. Treat `wallettxstore.cpp:370-379` as the authority and fix the stale comment when next in the file.

## 4. Producer: `WalletTxStore`

`src/qt/wallettxstore.{h,cpp}` — the authoritative, producer-owned ordering store.

### State (`wallettxstore.h:323-377`)
- `m_records` (`:331`, `GUARDED_BY(cs_store)`) — the full decomposed `TransactionRecord` set, kept in `RecordOrder` (the projection of `TxOrderLess`, `wallettxstore.cpp:20-25`).
- `m_by_hash` (`:333`) — `unordered_multimap<uint256, size_t>`; same-hash records are contiguous, so each entry maps a tx to a `[minPos, maxPos]` run.
- `m_fields_cache` / `m_keys_cache` (`:339-340`) — projected filter/sort inputs, kept *exactly parallel* to `m_records` (PR4-fix F).
- `m_volatile` (`:345`) — hashes with ≥1 height-dependent record; bounds the per-tip refresh.
- `m_cursors` (`:351`) — `std::map<int, Cursor>` (std::map for stable references).
- `m_view_seqno` (`:358`) — per-view high-water (the seqno of the last event emitted for each view).
- `m_limit_enabled`/`m_limit_time` (`:361-362`) — the OptionsModel datetime-display cutoff. **Changing the cutoff is not cheap:** `OptionsModel::LimitTxnDisplayChanged` → `TransactionTableModel::refreshWallet` (`transactiontablemodel.cpp:250`, wired at `:234`) → `TransactionTablePriv::refreshWallet`/`loadWallet` (`:82-91`,`:68-77`) → `reloadAndSnapshot` — a **full store rebuild under `LOCK2`** that re-decomposes the whole wallet and re-publishes every cursor `Reset` (same path as construction, below).
- intake state (`:364-376`): `cs_intake`, `m_intake` deque, `m_stop`/`m_rebuilding`/`m_worker_parked` flags, `m_intake_cv`/`m_idle_cv`, `m_worker`, the Qt-only `m_started` guard.

### Records, index, projectors
The store holds **full** decomposed records plus full `TransactionStatus`, not a reduced order-key set. The projectors (`projectFields`, `wallettxstore.cpp:31-38`; `projectKeys`, `:49-57`) compute the Qt-free filter/sort inputs once per record; the cursor reads them back by *const reference* via `projectFieldsAt`/`projectKeysAt` (`:644-652`, annotated `NO_THREAD_SAFETY_ANALYSIS` because they are reached through the cursor's `std::function` indirection). `makeCursorProjectors` (`:654-660`) binds explicit `-> const T&`-returning lambdas — a by-value deduction would copy the key per comparison and defeat fix F. `recomputeCacheAt` (`:662-666`) rebuilds slot `i` after every mutation.

### Read APIs (Qt thread, under one `cs_store` hold)
- `getRows(viewId, first, count)` (`:736-779`) — returns a `RowsResult` (`wallettxstore.h:63-68`) bundling the `[first, first+count)` slice **plus** `total_accepted`, `epoch`, `high_water`, all sampled under the *same* `cs_store` hold. `count < 0` means "all served from `first`". `total_accepted` is saturating-cast to `INT_MAX` for Qt's int `rowCount` (`:755-757`).
- `getAllRows(viewId)` (`:781-805`) — **cap-independent**: iterates the full accepted set (`cur.totalAccepted()`, `:799`), not the served window, so CSV export is never truncated.
- `rowForKey(viewId, hash, idx)` (`:807-832`) — `(hash, idx)` → absolute index via `m_by_hash` → accepted row via `Cursor::positionOf`. `idx < 0` returns the min accepted row across all parts (old hash-only `indexForTxid` semantics). Pure read, no projector calls.
- `getRowDetail(hash, idx)` (`:834-870`) — see §10.

The atomic `RowsResult` sampling closes the **PR4-fix B** race: a consumer must never re-sample metadata in a separate locked call (the deleted `totalAccepted(viewId)` was exactly that bug), or a worker insert/remove between the two locks would misalign the slice against the structural delta stream.

### Reload (`reloadAndSnapshot`, `wallettxstore.cpp:366-482`)
Qt-thread full rebuild. Holds `LOCK2(cs_main, cs_wallet)` (`:380`) across the *entire* rebuild **and** the queue drain — `cs_main` is the load-bearing exclusion lock because every producer of insert/remove holds it (both `CT_DELETED` sites hold `cs_main` even without `cs_wallet`; documented at `:370-379` — and see the §3 comment-contradiction warning). It quiesces the worker (`m_rebuilding` + waits on `m_idle_cv` for `m_worker_parked`, `:388-396`), rescans `mapWallet`, **decomposes + `updateStatus` + `populateDisplayLabel` each visible tx producer-side (`:398-420`)**, sorts (`:421`), swaps `m_records`/index/caches/volatile/cursors under `cs_store` (`:424-450`), drains+discards pre-rebuild events (`:456`), then re-publishes per-view cursor Resets AFTER the drain (recording each into `m_view_seqno`, `:464-469`), then releases the worker (`:474-479`). Ordering is load-bearing: republishing the Resets *before* the drain would discard them.

This is the one **O(N)-under-both-wallet-locks** operation the rewrite does **not** eliminate. It is structurally the old `loadWallet` scan, and it runs (a) at construction — `TransactionTablePriv::loadWallet` (`transactiontablemodel.cpp:68-77`) from the `TransactionTableModel` ctor (`:231`) — and (b) on every datetime-cutoff toggle (above). It is the startup freeze §14 discloses; its deferred remediation is the design-of-record's "step 6" progressive background decompose (§12, §14), which is not yet implemented on this branch.

### Per-tip status refresh (`applyChainTipRefresh`, `wallettxstore.cpp:591-642`)
Runs INLINE on the validation thread under the caller's `cs_main`. Iterates a *copy* of `m_volatile` (it mutates as txs mature), re-runs `updateStatus` per part from `mapWallet`, recomputes the cache, re-drives cursors one part at a time, and drops each hash once all parts are terminal (`updateVolatileForHash`). Volatility is classified by `recordStatusIsVolatile` (`:62-79`): `OpenUntil*`/`Unconfirmed`/`Confirming`/`Immature`/`MaturesWarning` → volatile; `Confirmed`/`Offline`/`Conflicted`/`NotAccepted` → terminal. This makes the per-block cost O(volatile), not O(N).

### Producer admission (`NotifyTransactionChanged`, `walletmodel.cpp:713`)
The producer decides what enters the authoritative record set before any enqueue:

- **Event routing** (`:773-776`, `:842-846`): the switch handles `CT_NEW`, `CT_UPDATED`, **and `CT_UPDATING`** together. `CT_NEW` → `enqueueInsert`; **both `CT_UPDATED` and `CT_UPDATING`** → `enqueueUpsert` — `CT_UPDATING` shares the in-place upsert path (an existing tx is updated and repositioned in any status-sorted cursor). `CT_DELETED` → `enqueueRemove` (`:856-857`). Per part it runs `decomposeTransaction` then `updateStatus` + `populateDisplayLabel` (`:831-838`) under the locks already held, so the off-lock cursors filter/sort by status and label without re-touching the wallet.
- **Transient-orphan-coinstake visibility override** (`:810-821`): visibility is decided by `showTransaction`, with one deliberate override. The coinstake/coinbase of the block *currently being connected* is kept visible even though `showTransaction` transiently reads it as an orphan — the wallet is notified **before** `SetBestChain` advances `pindexBest`, so a block sitting directly on the current tip (`pprev == pindexBest`, not yet `IsInMainChain`) is a split-second from becoming the tip; without the override its own coinstake would get a spurious `enqueueRemove` and never enter the GUI model. A genuine orphan (`pprev != pindexBest`) stays hidden, so `-showorphans` semantics are unchanged, and for a current-era coinstake the orphan check is `showTransaction`'s only false path, so the override cannot un-hide a tx filtered for any other reason. This shapes the authoritative record set.

### Mutation cores
- `insertLocked` (`:233-299`): datetime cutoff → dedup by hash → `lower_bound` insert slot → `shiftIndex` BEFORE the vector splice → splice `m_records` + both caches at the same position → update `m_by_hash`/`m_volatile` → push `VIEW_FULL` `RowsInserted` while holding `cs_store` → drive every cursor.
- `removeLocked` (`:307-364`): resolve `[minPos, maxPos]`, then a **runtime structural guard** (`:336-347`) that bails (LogPrintf, no erase) if the run is non-contiguous/out-of-range. This is an explicit always-on check rather than an `assert`: a wrong erase would swallow foreign rows / corrupt the store in *any* build, so a producer regression must degrade safely (log + skip, leaving a stale row at worst). (The in-code comment justifying this by `-DNDEBUG` is inaccurate — the project builds with `-UNDEBUG`, so `assert` is *not* elided; see §14.)
- `updateTransaction` (`:484-560`): CT_UPDATED/CT_UPDATING in-place upsert. Captures the hash before cutoff erase; empty→`removeLocked`; absent→`insertLocked`; part-count-changed/broken run→remove+insert fallback (`:530-538`); otherwise in-place overwrite (positions stable) + `applyStatusUpdate` per affected row. Emits **no** `VIEW_FULL` event (TTM is no longer rendered post-PR5-C).
- `applyAddressBookChange` (`:562-589`): `cs_store` only (label carried in). Re-snapshots `label` one record at a time, interleaving `recomputeCacheAt` + per-cursor `applyStatusUpdate`, because recomputing all same-address keys first would leave `view_index` transiently unsorted and break `lower_bound` under an Address sort (PR4-fix C review follow-up).

`emitCursorDeltas` (`:872-904`) translates each cursor's served-window `CursorDelta`s into per-view `WalletEvent`s, pushes under `cs_store` so seqno-order == mutation-order, and records the seqno of every emitted event into `m_view_seqno[viewId]` (last-wins).

## 5. The double-queue worker (PR2.5)

The off-lock O(N) maintenance engine sits between the producer threads and the store mutation. PR2 ran `insert/removeTransaction` **synchronously** inside `NotifyTransactionChanged` under `cs_main`+`cs_wallet`, putting the O(N) `std::vector` splice + `shiftIndex` on the validation critical path. PR2.5 splits it.

- **Producer side** (`enqueue*`, `wallettxstore.cpp:114-152`): push one `IntakeItem` (`wallettxstore.h:248-255`, kind ∈ {Insert, Remove, Update, AddressBook}) onto `m_intake` under `cs_intake` ONLY, `notify_one`, return O(1). Decompose happens producer-side; the worker never re-decomposes. `enqueueUpsert` early-returns on empty records (`:134-136`); `enqueueInsert` does not (it no-ops downstream — harmless asymmetry).
- **Worker** (`workerLoop`, `:154-190`): `WAIT_LOCK(cs_intake)` once, then loops on the **explicit while-condition** form (`while (!m_stop && (m_rebuilding || m_intake.empty())) m_intake_cv.wait(lock);`, `:162-171`) — *not* a `wait(predicate)` lambda, because Clang's thread-safety analyzer cannot propagate the held lock into a lambda body but can into the surrounding loop (`:158-161`). On `m_rebuilding` it sets `m_worker_parked` + signals `m_idle_cv`; on `m_stop` it returns; otherwise it pops one item, drops `cs_intake` via `REVERSE_LOCK`, and runs `applyIntake` with no lock held (`:185-188`). `applyIntake` (`:192-204`) dispatches by kind.
- **Park/quiesce barrier**: `reloadAndSnapshot` sets `m_rebuilding` and waits on `m_idle_cv` for the worker to park; the wait is gated on `m_started` (`:392`) so the first ctor-time reload (before `start()` launches the worker) skips it.
- **Shutdown** (`~WalletTxStore`, `:91-101`): set `m_stop` under `cs_intake`, `notify_all`, join. A worker mid-`applyIntake` finishes its current item (then re-evaluates and sees `m_stop`) — never a torn apply.

Because a single worker drains the FIFO and every event is pushed under `cs_store`, consumer-visible seqno order equals store-mutation order across all producer threads. This is the actual cross-thread ordering guarantee — the validation thread (`applyChainTipRefresh`) and the Qt thread (`register`/`setView*`/`reload`) also push events; they are ordered correctly only because `cs_store` linearizes their pushes, not because the worker is the sole pusher.

## 6. Qt-free cores: ordering, filter, cursor

Three TUs encode, with zero Qt headers, the behavior that used to live inside Qt widgets, so they compile straight into the `ENABLE_GUI=OFF` `test_gridcoin` binary CI exercises under ASan/UBSan — closing the blind spot that let PR #2944's heap corruption escape.

### Ordering (`txorder.{h,cpp}`)
`TxOrderLess` (`txorder.cpp:9-14`) is the single source of truth: `(time DESC, hash ASC, idx ASC)`. `hash` ASC clusters a tx's decomposed parts into one contiguous run (the removal path depends on this); `idx` ASC is unique within a tx, so it is a *true total order* (`TxOrderLess(a,a) == false`). `WalletTxStore::RecordOrder` (`wallettxstore.cpp:20-25`) is the only projector to this key.

### Filter/sort (`txfilter.{h,cpp}`)
- `FilterSpec` (`txfilter.h:88-111`) — flat value type (ints, bools, `std::string`, a `uint32_t` bitmask), IPC-marshalable. Defaults reproduce `TransactionFilterProxy`'s initial state. `date_to` defaults to `0xFFFFFFFF` (the original `MAX_DATE`, ~2106), not `INT64_MAX`. `limit_rows` is **not** a per-row predicate — `Accepts` ignores it; it is a served-window cap applied by the cursor.
- `TxFilterFields` (`:119-132`) — the per-row filter inputs (`time`, signed `net_amount`, `type`, `status`, `address`, snapshotted `label`).
- `SortKey` (`:148-155`) — locale-free sort inputs. `status_sort_key` is `rec.status.sortKey`; `type_string` is the `(type, generated_type)` tuple rendered as `"%03d.%03d"` digits (sorts by category, not translated name); the Address column carries `label_string` and `address_string` as **two separate keys**.
- `Accepts` (`txfilter.cpp:46-68`) — exact port of `filterAcceptsRow`: two separate inactive gates (`show_inactive` AND `show_orphans` both mask Conflicted/NotAccepted, `:54-55`), `type_mask` bit test (`(1u << f.type) & s.type_mask`, `:57` — `Type` must stay < 32), inclusive date range, address-OR-label case-insensitive substring, `llabs(net_amount) >= min_amount`.
- `CompareKeys` (`:70-103`) — three-way (`<0/0/>0`); the sign is flipped for `TXSORT_DESC` but the `0` (tie) result is **preserved** so the caller applies its own tie-break. The Address column compares `label_string` then `address_string` (two-level, no separator byte — PR4-fix G). `Less` (`:105-108`) is a thin `CompareKeys(...) < 0` wrapper retained for tests and key-only `lower_bound`.
- `AsciiLower`/`IContains`/`ICompare` (`txfilter.cpp:19-40`) — ASCII-only case folding, a deliberate sub-cosmetic divergence from Qt's Unicode-aware comparison (exact for base58 addresses and ASCII labels), keeping the module locale-free for the off-thread/IPC producer.

### Cursor (`cursor.{h,cpp}`)
One filtered+sorted view: an ordered `std::vector<size_t> m_view_index` of accepted **absolute** record indices (`cursor.h:146`). Holds no records and no Qt types — reads the backing table through `FieldsFn`/`KeysFn` reference-returning projectors (`cursor.h:63-64`). Callers MUST mutate the backing table BEFORE calling the matching `apply*` (`cursor.h:36-38`).

- `lessIndexed` (`cursor.cpp:37-41`): total order = `CompareKeys` for the active column/order, ties broken by native index `a < b` — **always ascending**, never flipped (`view_index` is over `m_records`, which is in `RecordOrder`, so a lower absidx is the earlier native record; this keeps parts contiguous and ties reproducible, replacing the proxy's `stable_sort`).
- `findSlot` (`:52-57`): identity locate by linear `std::find` — for **existing** rows. `lowerBoundSlot` (`:43-50`): sorted insert slot via `lower_bound` — for a row **not** currently present. Never the reverse — this (spec Item 1) is the most error-prone rule in the file: sort keys are non-unique (every Unconfirmed row shares the same `status_sort_key`), so a key-based search could evict a sibling.
- `applyStoreInsert` (`:100-117`): shift existing absidx `+count` FIRST, then insert each new record at its `lowerBoundSlot` (spec Item 4). `applyStoreRemove` (`:119-137`): erase by identity FIRST, then shift survivors `-count`.
- `applyStatusUpdate` (`:139-190`): four membership/reposition cases by identity-locate then erase-then-`lower_bound` (Item 2). Maintains `view_index` for ALL rows incl. off-window — the `!old_vis && !new_vis` branch (`:188`) still moves the index but emits no delta (Item 3).
- `emitInsertAt`/`emitRemoveAt` (`:73-98`): translate a single already-applied `view_index` mutation into served-window deltas (eviction when full / promotion of an off-window row). Their `size_after`/`size_before` args encode that the vector was already mutated.
- `epoch()` (`cursor.h:97`) bumps ONLY on Reset-class ops — `rebuild` (`:67`), `setSort` (`:212`), `setFilter`→`rebuild` (`:216-220`). Incremental insert/remove/status do not bump it. Consumers use epoch for stale-fetch reconciliation.

## 7. The WindowCache

`src/qt/windowcache.h` — the Qt-free consumer-side reconciliation core (templated on `Record` so it unit-tests GUI-OFF). It caches only a contiguous slice `[cacheFirst, cacheFirst+cacheSize)`, reports the full virtual row count, and renders off-window rows as placeholders. Qt-thread-only, no internal locking (`windowcache.h:54`).

Its entire reason for existing is a strict **two-channel split** (`windowcache.h:27-47`):

- **STRUCTURAL channel** — the ordered Reset/Insert/Remove/Change delta stream. It is the **sole** owner of the virtual row count `m_total` and of the model's begin/end row brackets, and advances `m_structural_seqno`. Each apply head-gates on `seqno <= m_structural_seqno` (already reflected): `applyReset:140`, `applyInsert:157`, `applyRemove:188`, `applyChange:228`.
- **CONTENT channel** — `fillContent` (`:265-285`), the scroll-driven slice fetch. It NEVER changes `m_total` and NEVER advances `m_structural_seqno`. It adopts a slice ONLY when `epoch == m_epoch` AND `high_water == m_structural_seqno` **exactly** (`:268-269`) — an exact match in both directions; a fetch staler OR ahead is rejected and re-requested. A third bounds gate (`first > m_total - count`, `:280`, PR5-A review fixup `76fe300aa`) prevents a malformed slice from setting the window past the table end.

Anti-stale invariants:
- `seedInitial` (`:122-130`) sets `m_structural_seqno = high_water` directly and emits no sink signals (the host issues its own reset).
- `applyReset` (`:137-149`) sets the new baseline to `max(high_water, seqno)` — adopting the fetch's high-water when it is ahead of the Reset event's own seqno (PR4-fix B), capturing off-window deltas that raced the Reset.
- `applyInsert` (`:154-181`) has four position regimes — before window (shift base), inside/at-either-edge (splice into the slice so a top-of-window insert renders without a placeholder flash, boundary inclusive at `:171`), after window (only `m_total`). Drops empty/out-of-range inserts without advancing the seqno (`:159-160`).
- `applyRemove` (`:186-213`) erases the overlap and pulls the base down by `removed_before_cache` (rows removed *strictly before* the cache start). Subtraction-form guard `pos > m_total - count` (`:191`) avoids int wrap.
- `applyChange` (`:225-253`) refreshes only the in-cache overlap and advances `m_structural_seqno` **unconditionally** (`:248`, before the `dataChanged` guard) — a Change does not move rows, so the event must be consumed exactly once even when entirely off-window. If `fresh` is shorter than `count` (a contract violation), uncovered rows keep their cached values (no OOB read).

The reconciliation looks asymmetric — `applyReset` takes `max(high_water, seqno)` but `fillContent` requires exact equality — and that is correct: a Reset must *swallow* off-window deltas that raced it, while a content fetch must *reject* if anything moved. Do not unify them.

## 8. The event queue & reconciliation

`src/qt/wallet_event_queue.{h,cpp}` — a single-mutex MPSC deque.

- `push` (`wallet_event_queue.cpp:14-26`) assigns a fresh monotonic `seqno` + `emit_time_us` under `m_mutex` and **returns** the seqno (so the store records it as a view high-water). The hold window is one `push_back` + increment.
- `drain(max_batch)` (`:28-55`) does an O(1) deque swap for a full drain or moves at most `max_batch` elements; the result vector is built *after* releasing the lock, so a producer in `push()` under `cs_wallet` is never blocked behind per-element drain work.
- Payload variants (`wallet_event_queue.h:30-143`): view ids `VIEW_FULL=0`/`VIEW_OVERVIEW=1`/`VIEW_DETAILED=2`. **Every per-view payload carries `viewId` + `epoch`** (load-bearing for the consumer fan-out and the stale-fetch gates):
  - `RowsInsertedPayload` (`:50-56`) — `{position, records, viewId, epoch}`
  - `RowsRemovedPayload` (`:66-72`) — `{position, count, viewId, epoch}`
  - `RowsResetPayload` (`:80-85`) — `{viewId, epoch, total}`
  - `RowCountChangedPayload` (`:94-99`) — `{viewId, epoch, total_accepted}`
  - `RowsChangedPayload` (`:106-112`) — `{viewId, epoch, first, count}`

  The one exception is `ChainTipChangedPayload` (`:131-135`) — `{height, best_time}` — which is a **global** balance/confirmation hint, not per-view, so it carries neither `viewId` nor `epoch`. `WalletEvent` (`:156-161`) = `{seqno, emit_time_us, payload}`.

The reconciliation keys are: the per-event monotonic **seqno** (structural application order), the per-cursor **epoch** (sort/filter generation), and the per-view **high_water** (`m_view_seqno`, seqno of the last event emitted for the view), all sampled atomically with the rows in one `getRows` call.

Consumer (`WalletModel::drainEventQueue`, `walletmodel.cpp:167-252`):
- `m_draining` RAII reentrancy guard (`:174-178`) — a nested drain (from `fetchWindow`, or from `viewReset`→`restoreAnchor`→`setCurrentIndex`) no-ops so the outer drain owns the queue and events apply exactly once.
- Clears `m_event_drain_requested` up front (`:183`), drains ≤ `MODEL_EVENT_DRAIN_MAX_BATCH=1024` (`guiconstants.h:21`), scans for a `ChainTipChanged` (`:205-210`), applies the `VIEW_FULL` stream to `transactionTableModel->applyEventBatch`, emits `walletEventsDrained` to fan the *same* batch to the per-view consumers (`:232`), runs `updateConfirmations` only if the tip advanced (`:225-227`), then `checkBalanceChanged` + `numTransactionsChanged` once per batch, then re-arms via `QTimer::singleShot(0)` if the batch hit the cap (`:249-251`).
- Periodic 500ms tick (`MODEL_EVENT_DRAIN_INTERVAL`, `guiconstants.h:13`) via `eventDrainTimer` (`walletmodel.cpp:51-53`).

`requestEventDrainSoon` (`:254-271`, PR4-fix D) kicks a next-event-loop-turn drain so a user filter/sort lands immediately rather than waiting up to 500ms. `m_event_drain_requested` coalesces a per-keystroke burst because `QTimer::singleShot` does not deduplicate. Both flags are Qt-thread-only with no synchronization (`walletmodel.h:184,192`).

`checkBalanceChanged` (`:108-165`) keeps the rate-limited recompute: `TRY_LOCK(cs_main)+TRY_LOCK(cs_wallet)` bow-out plus a `MODEL_UPDATE_DELAY=4000ms` (`guiconstants.h:7`) stale-time gate stamped on *commit-to-recompute* (`:145`), not only on detected change — so a stable balance does not leave the gate permanently open. This replaced the old 4s `pollBalanceChanged` poll; it is now event-driven off `ChainTipChanged`.

## 9. Consumers

### `DetailedTxModel` (`detailedtxmodel.{h,cpp}`) — `VIEW_DETAILED`
TransactionView's **direct** model (no proxy). A virtual windowed `QAbstractTableModel` over a `WindowCache<TransactionRecord>` (`detailedtxmodel.h:152`).
- ctor (`detailedtxmodel.cpp:46-78`): registers a `VIEW_DETAILED` cursor (Date DESC, "show everything" `FilterSpec`, `-showorphans` read once at `:58`), seeds a **bounded** `kInitialWindow=200` window via `seedInitial` (NOT `count=-1`, which under the unlimited cap would pull the full replica — `:62-70`).
- `rowCount() == m_cache.total()` (`:80-83`) so the scrollbar spans all history. `data()` (`:91-106`) is pure: render `m_cache.at(row)` via `m_ttm->formatRole` or return a placeholder for an off-window row; no fetch on the paint path. `const_cast` because `formatRole` takes a non-const record (`:104-105`).
- `sort`/`setFilter` (`:113-128`) forward to `setViewSort`/`setViewFilter` then `requestEventDrainSoon`.
- Producer-resolved lookups (`:130-160`): `indexForTxid`/`getAllRows`/`rowForKey` delegate to the store (work for any accepted row); `keyAt` reads `(hash, idx)` from the LOCAL cache and returns false off-window.
- `ensureRowCached` (`:162-187`): brings a row into the slice synchronously; covers both the current viewport AND the target so an in/near-viewport ensure never evicts the visible slice (`fillContent` REPLACES it), with a `kMaxFetch`-bounded centred window for a far jump.
- `onViewportChanged`/`onFetchTimeout` (`:189-242`): debounced (`kDebounceMs=100`) content fetch; `onFetchTimeout` re-clamps against the CURRENT total, computes a ~3× viewport window, applies the `kMaxFetch=4096` backstop and an "already cached" hysteresis skip.
- `fetchWindow` (`:244-283`): reentrancy-guarded (`m_fetch_in_progress` + RAII Guard at `:249-252`), loops ≤ `kMaxFetchRetries=4`: `drainEventQueue` FIRST (so structural seqno catches up to high-water), then `getRows` + `m_cache.fillContent`. On sustained churn defers to a debounced retry.
- `applyEventBatch` (`:291-342`): `std::visit` dispatch; ignores any payload whose `viewId != VIEW_DETAILED` and never consumes `RowCountChanged`/`ChainTipChanged`. Reset→bounded refetch + `applyReset` + `viewReset` signal + re-arm; Insert→`applyInsert`; Remove→`applyRemove`; Changed→refetch slice + `applyChange`.

The nested `CacheSink` (`detailedtxmodel.h:138-148`) forwards the cache's structural callbacks to `begin/end{Insert,Remove,Reset}Rows`. It is a **member, not a base**, because `WindowCacheSink::dataChanged(int,int)` would otherwise overload `QAbstractItemModel::dataChanged` — a moc hazard (`:134-137`).

The enum-mirror **drift guards** sit at the top of this TU (`detailedtxmodel.cpp:21-29`): `static_assert`s tying the Qt-free `GRC::TXSTATUS_*` / `GRC::TXCOL_*` mirrors in `txfilter.h` to the authoritative `TransactionStatus::*` / `TransactionTableModel::*` enums (relocated here from the retired `transactionfilterproxy.cpp`). These are the **only** `static_assert`s in the windowed-model code — see §11/§14 on the absence of a marshalability guard.

### `OverviewTxModel` (`overviewtxmodel.{h,cpp}`) — `VIEW_OVERVIEW`
The PR3 windowing testbed backing OverviewPage's recent-tx list. A `QAbstractListModel` holding the cursor's **full served-window slice** (`m_rows`, `overviewtxmodel.h:63`), bounded by `limit_rows`, not a virtual window.
- ctor (`overviewtxmodel.cpp:16-44`): registers `VIEW_OVERVIEW` (Status DESC, `show_inactive=false`, `limit_rows=initialLimit`), seeds synchronously with `getRows(0,-1)` capturing `high_water` into `m_applied_seqno` so the registration Reset is skipped.
- `data()` (`:51-65`) renders the `ToAddress` roles via `formatRole`. `setLimit` (`:67-76`) forwards to `setViewLimit`; `txidAt` (`:78-84`) backs click-through.
- `applyEventBatch` (`:86-154`): all gated on `seqno <= m_applied_seqno` (PR4-fix B). Reset→`getRows(0,-1)` refetch + `max(high_water, seqno)`; Insert/Remove splice/erase with defensive bounds + empty-insert guard; Change→refetch the changed slice.

### Reduced `TransactionTableModel` (`transactiontablemodel.{h,cpp}`)
Post-stack the TTM is primarily the **shared formatter source** plus the `indexForTxid` backer. `formatRole` (`transactiontablemodel.cpp:671-784`) is the single role-rendering core consumed by TTM's `data`, `DetailedTxModel::data`, `OverviewTxModel::data`, and `ExportRowsModel`; it validates the column range up front (`:676`) with an explicit guard (not an `assert`), so an out-of-range column degrades to an empty `QVariant` rather than reading past the column set in any build. `data()` (`:650-664`) resolves the row each call and never trusts `internalPointer` (a `TransactionRecord*` dangles across vector realloc); `index()` passes explicit `nullptr` (`:830`).

`TransactionTablePriv` (`:41-221`) keeps a Qt-thread-exclusive `cachedWallet` replica kept in lockstep with the producer by replaying the position-stamped `VIEW_FULL` stream (`applyRowsInserted`/`applyRowsRemoved`, `:131-195`, no seqno gate — pure lockstep, with explicit defensive bounds-checks so a bad position degrades to a logged skip, never an out-of-bounds vector op). `loadWallet` calls `store.reloadAndSnapshot` (`:76`). `index(int)` (`:209-219`) is now a pure replica read with **no lazy status refresh** — the PR5-C change.

The TTM is largely vestigial: **no view binds it as its model** (the live views are `DetailedTxModel`/`OverviewTxModel`). `updateConfirmations()`'s `dataChanged(0, N-1)` (`:255-273`) is now **observerless** — only `BitcoinGUI` consumes TTM's `rowsInserted`, not `dataChanged` — and is kept only as the model-correct signal should TTM ever be re-attached.

### `TransactionView` (`transactionview.{h,cpp}`)
`setModel` (`:173-222`) creates the `DetailedTxModel` and sets it as the table's direct model; wires `scrollBar valueChanged → reportViewport`, `header sectionClicked → captureAnchor`, `model viewReset → restoreAnchor`. It deliberately has **no `currentChanged` hook** (`:205-207`, PR5-B review #12) — that would call `drainEventQueue` from inside a selection-changed handler, resetting the model mid-notification. The filter widgets (`:268-340`) mutate `m_filterSpec` then call `applyFilter` (`:342-348`, which `captureAnchor`s first); date bounds are seconds-since-epoch via `StartOfDaySecs` (`:228-231`).

## 10. Cross-cutting mechanisms

**Status.** Computed producer-side under the locks already held — `NotifyTransactionChanged` calls `updateStatus` (`walletmodel.cpp:836`) before enqueue, `reloadAndSnapshot` at `:416`, and per-block `applyChainTipRefresh` (`wallettxstore.cpp:591-642`) over the bounded volatile set. Since PR5-C deleted the lazy on-read `index()` refresh, the producer is the *sole* authoritative status source, not a head start. (What is admitted into the status-bearing record set in the first place is the producer-admission decision in §4 — including the transient-orphan-coinstake override.)

**Address labels.** Snapshotted into `TransactionRecord::label` producer-side by `populateDisplayLabel` (`transactionrecord.cpp:504-513`, `EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)`), so the off-`cs_wallet` cursor can sort the Address column and filter by label substring. `updateAddressBook` (`walletmodel.cpp:273-288`) reads the *authoritative* current label from the AddressTableModel (empty after delete) and enqueues an O(1) AddressBook intake item; the worker re-snapshots one record at a time. Distinct from all of the above is the **render-path** label lookup: the column formatter `formatTxToAddress` (`transactiontablemodel.cpp:489-506`) calls `lookupAddress` (`:362-376`), which calls `AddressTableModel::labelForAddress` (`:364`). That reads the Qt-side **in-memory address cache**, NOT `cs_wallet`. So the snapshotted `record.label` (consumed by the *cursor* for off-thread sort/filter) and the live `labelForAddress` lookup (consumed by the *display formatter*) are two distinct paths — do not conflate them.

**Anchor-on-resort.** `captureAnchor` (`transactionview.cpp:571-600`) ensures the selected (else topmost) row is cached, then records the EXACT `(m_anchor_hash, m_anchor_idx)`. `restoreAnchor` (`:602-625`, fired on `viewReset`) re-finds the row via `rowForKey(hash, idx)`, `scrollTo`+`setCurrentIndex`, then `reportViewport` (because `scrollTo` does not fire `valueChanged` if already centred). The `idx` is load-bearing: a tx's parts share the hash but scatter under an Amount/Address sort, so a hash-only anchor restores to the wrong part (the PR5-B fixup `d0b3802d5`).

**Detail dialog.** `getRowDetail` (`wallettxstore.cpp:834-870`) resolves `(hash, idx)` → the part's `vout` under `cs_store` ONLY, **releases** `cs_store`, then formats under `LOCK2(cs_main, cs_wallet)` via `TransactionDesc::toHTML(wallet, wtx, vout)` (`transactiondesc.h:19`) — the same lock pair the deleted `describe()` took, relocated to the producer. `showDetails` (`transactionview.cpp:464-488`) ensures the row, reads `(hash, idx)` from the LOCAL cache via `keyAt`, then calls `getRowDetail`; the dialog (`transactiondescdialog.cpp:10-21`) renders the pre-built HTML with a "Transaction details unavailable." fallback. Resolving by identity (not view-relative row) keeps it drift-free if the drain lags the cursor. **Caveat:** in-process this still takes the wallet locks on the Qt thread — it *relocates* the detail-path stall; only deleting the lazy `index()` refresh took the render path fully off the locks.

**CSV export.** `exportClicked` (`transactionview.cpp:350-381`) wraps `getAllRows()` (the full filtered+sorted set) in a transient `ExportRowsModel` (`:240-265`) that reuses `formatRole` so exported values match the live table. Necessary because the live windowed model caches only the viewport — exporting through it would blank off-window rows. `getAllRows` is cap-independent (`wallettxstore.cpp:797-803`).

## 11. Multiprocess-readiness (#2937)

The whole design is the in-process prototype of the node/GUI split: producer = node side, consumer = GUI side, the queue = the IPC channel. The IPC-shaped contract (design-of-record lines 183-229):
- **Down-channel** (producer → consumer): the value-typed `WalletEvent` payloads become Cap'n Proto messages. They are pointer-free (`uint256`/`qint64`/enum/`std::string`/`int`/`TransactionStatus`).
- **Up-channel** (consumer → producer): `getRows`/`getAllRows`/`rowForKey`/`getRowDetail`/`setViewport`-equivalents/`setSort`/`setFilter` return by value. The call *shape* is final; only the transport swap (the `interfaces::` shell + Cap'n Proto schema) is deferred (design-of-record lines 337-348).

The Qt-free cores (`txfilter`/`cursor`/`windowcache`) and the value-typed `FilterSpec`/`RowsResult` make the swap mechanical. Two asterisks: (1) localization stays GUI-process-side (the cursor carries GUI-supplied type/address sort strings; `RowDetail` is rendered consumer-side); (2) `getRowDetail` becomes a genuine node-side call only at the split — in-process it relocates the stall. Note: the design-of-record (lines 185-188) *claims* a `static_assert` guards `TransactionRecord` against a future Qt-typed field, but **no such `static_assert` exists in the code on this branch** — the only `static_assert`s present are the enum-mirror drift guards in `detailedtxmodel.cpp:21-29`, which check the `TXSTATUS_*`/`TXCOL_*` mirrors, not marshalability. The record is currently marshalable, but the guard against drift is aspirational, not enforced.

## 12. Incremental delivery (the PR ladder)

> **Provenance basis (read first).** This branch (`testnet_v13_checkpoint_removed`) accumulates the windowed-model stack as **cherry-picks**, not merges, so the GitHub PR numbers are in general **not git-derivable here**. The reliable on-branch anchors are (a) the commit **hashes** (all verified on this branch) and (b) the commits' **own self-labels** in their subject lines (`tx-table windowed-model phase 1`, `windowed-model PR1`, `windowed-model step 2`, `PR2.5`, `PR3-A`, `PR4-B`, `PR5-A/B/C` — each referencing issue **#2944**). Only two of these PRs left **merge commits** on this branch: `#3003` (`60fbed289`, source branch `…tx-table-windowed-phase1`) and `#3010` (`223214b8f`, source branch `…tx-table-windowed-phase3-pr1`). Everything from "step 2" onward is present only as cherry-picked commits with no merge commit. The GitHub PR numbers `#3011`/`#3015`/`#3021`/`#3022`/`#3023`/`#3041`/`#3049` below are **sourced from the design-of-record and the PR tracker** — they are not assertable from this branch's git history, and this document does not claim a git-derived mapping for them.

| Rung | Commit self-label | Commit(s) | PR (source) | What |
|------|-------------------|-----------|-------------|------|
| Predecessor | — | (pre-stack) | #2944 (tracker) | MPSC `wallet_event_queue.{h,cpp}`; replaced the 4s `pollBalanceChanged` poll with `ChainTipChanged` |
| Foundation | "tx-table windowed-model phase 1" | `30a544b7c` | #3003 (**merge on-branch** `60fbed289`) | `QList` → `std::vector` + `unordered_multimap`; hash-only `TxLessThan` → composite order (time DESC, hash ASC, idx ASC) |
| PR1 | "windowed-model PR1" | `1a3cdfb80` | #3010 (**merge on-branch** `223214b8f`) | Qt-free `txfilter.{h,cpp}` (`FilterSpec`/`Accepts`/`Less`) + design-of-record doc |
| PR2 | "windowed-model step 2" | `f6588e14d` | #3011 (tracker) | Relocate ordering into producer-owned `WalletTxStore` behind leaf `cs_store` |
| PR2.5 | "PR2.5" | `5cc430b04` | #3015 (tracker) | Double-queue store-worker; O(N) maintenance off `cs_main`/`cs_wallet` |
| PR3 | "windowed-model PR3-A" | `c92f80dea`… | #3021 (tracker) | Qt-free per-view `Cursor` + server-side filter/sort + producer-side status + `OverviewTxModel` |
| PR4 | "windowed-model PR4-B" | `e5a813471`,`edc58798b`,`39ab27f07`,`7207f1815` | #3022 (tracker) | `DetailedTxModel`; **delete `TransactionFilterProxy`**; fixes A–G |
| PR5-A | "windowed-model PR5-A" | `761935dac`,`76fe300aa` | #3023 (tracker) | Qt-free `WindowCache` core + windowed read APIs (`getRows`/`getAllRows`/`rowForKey`) |
| PR5-B | "windowed-model PR5-B" | `d190bf5d4`,`d0b3802d5` | #3041 (tracker) | Window the detailed table on `WindowCache`; anchor-on-resort by exact `(hash, idx)` |
| PR5-C | "windowed-model PR5-C" | `900889be6` | #3049 (tracker) | Retire the lazy render-thread `TRY_LOCK`; producer-side `getRowDetail(hash, idx)` |
| **Step 6 (deferred, NOT landed)** | — | *no code on this branch* | — (design-of-record step 6, doc:247) | Off-Qt-thread **progressive startup decompose**: the O(N) decompose moves to a chunked background core task; the table opens at row count 0 and paints progressively. The deferred remediation for the startup freeze (§14). |

## 13. Design decisions & trade-offs

| Decision | Rationale | Trade-off |
|----------|-----------|-----------|
| Store holds FULL decomposed records + full `TransactionStatus`, not a reduced key set | A reduced set cannot reproduce `updateStatus`'s branches (Offline/Unconfirmed split, OpenUntil*, MaturesWarning, `countsForBalance`) off-window — makes stale-status bugs structurally impossible | No in-process RSS reduction; the footprint win is deferred to #2937 (internals can later swap behind the cursor contract) |
| Double-queue (PR2.5): O(1) producer enqueue, single worker does O(N) maintenance | Block connection must not pay O(N) GUI bookkeeping under `cs_main`/`cs_wallet` | A worker thread + park protocol; GUI is eventually-consistent (events apply on a drain tick) |
| Exactly ONE worker drains the FIFO | Single-writer serialization makes seqno-order == mutation-order for free; clean node-side maintainer | Store maintenance serialized — but it is under `cs_store` anyway, so a pool gains nothing |
| `applyChainTipRefresh` runs INLINE on the validation thread | It needs `cs_wallet`; on the worker it would force the worker to take the wallet locks → deadlock `reloadAndSnapshot`'s park protocol | O(volatile × view_index) reposition cost lands on the validation thread per block |
| Consumer is a VIRTUAL model (full rowCount + placeholder), NOT `canFetchMore`/`fetchMore` | A full rowCount makes `canFetchMore` never fire and the scrollbar correctly sized/draggable to any position | A fast drag into an un-cached region shows placeholders for a frame |
| Producer `VIEW_DETAILED` cursor cap stays UNLIMITED; window lives in the consumer's `WindowCache` | Keeps `getRows(first)` an absolute accepted index (store read path unchanged); confines the tricky reconciliation to a GUI-OFF-testable core | Consumer must implement the two-channel split + synchronous bounded-retry fetch |
| Qt-free cores with value-typed IPC-shaped payloads | GUI-OFF-testable (closes the #2944 blind spot) + makes #2937 a transport swap | Localization stays GUI-side; deliberate ASCII case-folding divergence from Qt |
| Cached projected keys, returned by const ref (PR4-fix F) | A sort comparison reads the cache with no `strprintf`/allocation per compare | Two extra parallel vectors; const-ref lambdas need `NO_THREAD_SAFETY_ANALYSIS` |
| Runtime structural guard (LogPrintf + bail), not assert, in remove/update | A de-clustered erase would corrupt the store in *any* build; an explicit log+bail degrades safely (a stale row at worst), whereas an `assert` is the wrong tool — it aborts the GUI, and the project builds with `-UNDEBUG` so asserts are not even elided | An always-on runtime check; can silently leave a stale row in the (impossible) violated case |
| Detail by identity via producer `getRowDetail` | Drift-free if the drain lags; node-side shape for #2937 | In-process relocates (does not remove) the detail-path wallet-lock stall |
| Startup/datetime-toggle decompose left as O(N) under `LOCK2` | Lowest-risk: it is the proven old `loadWallet` scan, unchanged; the progressive-decompose remediation (step 6) is deferrable without blocking the responsiveness wins | The startup freeze on a large wallet is not yet eliminated (§14) |
| Risk-bounded ladder of independently-correct PRs | Reviewability/bisectability; lands the trickiest logic in the GUI-OFF unit suite CI exercises | One mild staging artifact — PR2's full-passthrough TTM mode, superseded in PR4 |

### Test strategy
Four Qt-free Boost suites compile into `test_gridcoin` (GUI-OFF, run under ASan/UBSan in CI), registered at `src/test/CMakeLists.txt:50-69`: `qt_txorder_tests`, `qt_txfilter_tests`, `qt_cursor_tests`, `qt_windowcache_tests`. Two Qt `QTest` suites (`wallet_event_queue_tests`, `wallettxstore_tests`) need a GUI build and run only in the GUI-ON jobs (Native, ARM64, ARMhf) — the MPSC-queue/store-worker concurrency tests are therefore *not* under ASan/UBSan in CI. Because GUI-OFF CI cannot exercise the model shell or event wiring (the exact blind spot that let #2944's heap corruption escape), every model-touching PR carries a mandatory ASan/UBSan-GUI + isolated-testnet soak (`-DENABLE_DEBUG_LOCKORDER` for the lock-order invariant) plus the 121,979-tx ARMv7 validation.

## 14. Future work & known limitations

- **Startup (and datetime-toggle) O(N) decompose under `LOCK2` is NOT eliminated.** `reloadAndSnapshot` (`wallettxstore.cpp:366-482`) still holds `LOCK2(cs_main, cs_wallet)` (`:380`) across the entire full-wallet rescan + `decomposeTransaction` + `updateStatus` + `populateDisplayLabel` rebuild (`:398-420`). It runs at construction (`TransactionTablePriv::loadWallet`, `transactiontablemodel.cpp:68-77`, from the TTM ctor `:231`) and on every datetime-cutoff change (`OptionsModel::LimitTxnDisplayChanged` → `refreshWallet` → `reloadAndSnapshot`, wired at `:234`). This is exactly the startup freeze §1 flags for the old model — the rewrite reduces *per-block* and *render-path* cost, not this one-time/on-toggle decompose.
- **Deferred remediation: progressive startup decompose (design-of-record "step 6").** The design-of-record (doc:247) specifies an off-Qt-thread chunked background decompose where the table opens at row count 0 and paints progressively, dissolving the freeze above. It is **not implemented on this branch** (no such code exists). It is a real, not-yet-landed future rung — distinct from the *rejected* `canFetchMore`/`fetchMore` idea below.
- **`canFetchMore`/`fetchMore` was rejected outright, not deferred.** There is no PR5-D in the landed ladder. The idiom is self-contradictory under a full `rowCount` (it never fires) and breaks the scrollbar. Server-side windowed fetch (`getRows`) is already live in-process; only the IPC *transport* of `getRows`/`getAllRows`/`getRowDetail`/`rowForKey` is deferred to #2937.
- **No in-process memory win.** The full decomposed record set stays resident in the one process; the footprint reduction materializes only at the multiprocess split, where the store lives node-side.
- **Detail-path stall relocated, not removed.** `getRowDetail` still takes `LOCK2(cs_main, cs_wallet)` on the Qt thread in-process.
- **Asymptotics stay O(N) per op.** `view_index` maintenance and `findSlot`/`positionOf` are O(N) linear; a code path that calls `applyStatusUpdate` for a large fraction of records in one pass is implicitly O(N²). An order-statistics structure for O(log N) is tracked separately. Correctness, not asymptotics, is the contract here.
- **`RowCountChangedPayload` is plumbed but unused.** Both live consumers ignore it (detailed cap stays unlimited; overview shows only its served window). It exists for a future finite-cap detailed view.
- **Sticky type-filter follow-up.** A sticky type-filter bug surfaced during the GUI mesh soak. It is **pre-existing** (not introduced by this stack) and tracked as a separate follow-up; the `qt_txfilter_tests` type-mask case validates the mask predicate in isolation, not the UI widget's persisted-state behavior.
- **The `static_assert` marshalability claim in the design-of-record is aspirational.** No `static_assert` guards `TransactionRecord` against a future Qt-typed member on this branch (the only `static_assert`s are the enum-mirror drift guards at `detailedtxmodel.cpp:21-29`) — a maintainer adding a `QString`/`QVariant` field would silently break IPC-readiness with no compile error.
- **In-code comment contradiction on the `CT_DELETED` lock state.** `walletmodel.cpp:764` wrongly says the `CT_DELETED` sites hold neither lock (with stale line numbers); `wallettxstore.cpp:370-379` correctly says they hold `cs_main`. The correct claim is load-bearing for `reloadAndSnapshot`'s exclusion (see §3). Fix the stale comment to avoid misleading a future editor.
- **Inaccurate `-DNDEBUG` rationale in several in-code comments.** Comments in `removeLocked`, `applyRowsInserted`/`applyRowsRemoved`, and the `formatRole` column guard justify their runtime checks by claiming the deployed build is `-DNDEBUG` (so `assert` is elided). That is wrong: the build system explicitly undefines `NDEBUG` (`-UNDEBUG` / `/U NDEBUG`, `CMakeLists.txt:54,56`), so `assert` is active in every configuration. The runtime guards are still correct and worth keeping — but for the right reason: they **degrade safely** (log + skip, never an out-of-bounds op or a GUI-aborting `assert`) if a producer ever violates the contiguity/position contract. Reword the comments so a future editor does not believe asserts are compiled out.
- **Re-attaching TTM as a view model is a trap.** `updateConfirmations`'s `dataChanged` is observerless and `index()` no longer self-refreshes status; any re-wire that renders TTM must drive status from the producer path (`applyChainTipRefresh`), not a paint-thread refresh, and must restore a `VIEW_FULL` refresh for in-place CT_UPDATED/CT_UPDATING status changes (which currently emit no `VIEW_FULL` event).
- **The height-prefixed Status sort key is load-bearing.** `applyChainTipRefresh` being O(viewport) on a plain tip-advance depends on the Status sort key being prefixed by the confirmed block height (invariant under a tip append). A future change that makes the Status key depend on anything that mutates on a plain tip append would reorder every cursor's `view_index` on every block — the sledgehammer returns in a subtler form.
