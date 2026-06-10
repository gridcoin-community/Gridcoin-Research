# Transaction Table Windowed Model — Design

## Status

In progress. This document describes the full scope of the transaction-table
windowed-model rewrite and the sequence of pull requests that implement it. It
is the design of record for the effort; individual PRs reference it.

The work builds on two landed changes:

- **PR #2944** — the wallet event queue: core threads push pre-decomposed
  transaction events onto an MPSC queue; the Qt main thread drains it. This
  established the producer/consumer split the windowed model formalizes.
- **PR #3003** — the container/ordering swap: `TransactionTablePriv`'s cached
  wallet became a `std::vector<TransactionRecord>` ordered by `TxRecordOrder`
  (`time` DESC, `hash` ASC, `idx` ASC) plus an `unordered_multimap<uint256,
  size_t>` hash index. This is the ordered store the windowed model promotes
  to an authoritative producer-side object.

## Motivation

The transaction GUI inherited a "full replica + sledgehammer refresh" model
from Bitcoin Core:

- `TransactionTableModel` (a Qt-main-thread object) holds a **full** decomposed
  replica of every wallet transaction row.
- On every chain-tip advance, `updateConfirmations()` emits a `dataChanged`
  signal spanning the **entire** model (rows `0 … N-1`) for two columns.
- A client-side `QSortFilterProxyModel` (`TransactionFilterProxy`) with
  `dynamicSortFilter=true` sits between the model and the view.

The dominant cost is the proxy's reaction to the wide `dataChanged`: it re-runs
`filterAcceptsRow()` for **all `N`** source rows, and because `filterAcceptsRow`
reads several roles through `data()` → `TransactionTablePriv::index()`, that
re-evaluation drags the per-row status recompute (`TRY_LOCK cs_main`/`cs_wallet`
+ `updateStatus`), the address-book label lookup, and string searches across
the whole model on every block. On large or dense wallets (heavy stake
accretion, dense sidestakes) this is a per-block hitch and a per-block
accessibility-tree rebuild.

It also forces O(N) decomposition on the Qt main thread at startup (the model
constructor calls `loadWallet()` under `LOCK2(cs_main, cs_wallet)`), freezing
the GUI for seconds on large wallets.

### What this is and is not

This effort delivers, **in-process**:

- Startup freeze elimination — decomposition moves off the Qt main thread; the
  table paints progressively.
- Per-block confirmation refresh reduced from O(N) to O(viewport).
- The Qt thread comes off `cs_main`/`cs_wallet` entirely on the render path.
- The accessibility-tree churn of issue #2952 fixed (viewport-scoped refresh,
  no full-table proxy remap).

It does **not** deliver, in-process, the GUI memory-footprint reduction often
associated with windowed models. The full decomposed record set stays resident
in the one process. That footprint win materializes only at the multiprocess
split (see [Phase 4](#phase-4--multiprocess-deferred)), when the store lives in
the node process and each GUI process holds only a viewport-sized slice. We
accept this trade deliberately: the in-process reshaping is the lowest-risk
path that also yields a value-typed, pointer-free contract the multiprocess
split marshals mechanically.

## Endpoint architecture

A producer-owned authoritative store, per-view server-side cursors, and thin
windowed consumer models. The client `QSortFilterProxyModel` is retired; sort
and filter become server-side operations.

```
   core threads (NotifyTransactionChanged, SetBestChain)
        │  decompose + updateStatus under cs_main/cs_wallet
        ▼
   GRC::WalletTxStore          (authoritative; owns records[] + by_hash; cs_store leaf lock)
        │   per-view Cursor { FilterSpec, SortSpec, view_index, viewport, epoch }
        │
   down-channel: Rows* payloads on the existing WalletEventQueue  ──▶  TransactionTableModel (thin, windowed slice cache)
   up-channel:   setViewport / setSort / setFilter / getRows  ◀──        │
                                                                          ▼
                                                              TransactionView (QTableView)
                                                              OverviewPage   (QListView)
```

### Authoritative store: full decomposed records

`GRC::WalletTxStore` holds the **full** decomposed `std::vector<TransactionRecord>`
in `TxRecordOrder`, the companion `unordered_multimap<uint256, size_t>` hash
index, and a full `TransactionStatus` per record refreshed by calling the
existing `updateStatus(wtx)` under `cs_main`+`cs_wallet`. It is guarded by a
**leaf** mutex `cs_store` with the invariant that `cs_store` never nests
`cs_main`/`cs_wallet` — producers do all wallet work (decompose, `updateStatus`)
*before* taking `cs_store`; where both are needed the order is
`cs_main → cs_wallet → cs_store`.

This describes the store once it *serves reads*. In the relocation step (step 2
below) the store does not yet serve reads — the consumer keeps its own full
replica and the producer only stamps positions — so the store holds **ordering
keys only** (`TxOrderKey` + the hash index), not full records; materializing
records there would merely double resident memory. It grows to the full
`records[]` + `TransactionStatus` form above in the windowing step (step 5),
when the consumer drops its replica for a viewport slice and begins reading the
store. The status-correctness rationale below is precisely what dictates *full
records once the store serves reads*.

Two alternatives were considered and rejected as the primary store:

- **Reduced order-key store + lazy per-window decompose** would yield an
  in-process memory win, but a reduced key set **cannot reproduce
  `updateStatus()`'s branches**: the `Offline`/`Unconfirmed` split,
  `OpenUntilBlock`/`OpenUntilDate` (needs `nLockTime`), `MaturesWarning` (needs
  `GetRequestCount`), and `countsForBalance = IsTrusted` (depends on
  `fConfChange`/`IsFromMe`/dependency confirmation) are **not** functions of
  `(anchor, tip)`. A reduced store would have to re-lock the wallet for the
  window anyway *and* maintain off-window confirmation flags perfectly for the
  `Conflicted`/`NotAccepted` filter — reintroducing exactly the stale-status
  bug class we have already paid to fix (the researcher-status `MarkDirty()` /
  beacon `BeaconChanged()` fixes). Full records make that class structurally
  impossible off-window.
- **Hybrid sidecar** (full records for the window + compact filter fields for
  all rows) shares the same status re-derivation hazard.

Because the wallet stays in-node in both the in-process and multiprocess forms
(per the multiprocess RFC, #2937), the store sits node-side regardless of its
internal shape. The GUI-process footprint win at Phase 4 is therefore identical
for all three store shapes — so choosing full-records forfeits nothing at the
Phase-4 boundary while buying the lowest regression now. If an in-process RSS
win is later judged worthwhile, the store internals can be swapped to a
reduced-key representation behind the unchanged cursor/contract layer.

### Server-side sort and filter

Filter and sort run over `records[]` via a **Qt-free** evaluation core
(`src/qt/txfilter.{h,cpp}`):

- `GRC::Accepts(TxFilterFields, FilterSpec)` — the filter predicate, an exact
  port of `TransactionFilterProxy::filterAcceptsRow`.
- `GRC::Less(SortKey, SortKey, column, order)` — the sort comparison, an exact
  port of the `Qt::EditRole` sort keys.

A cursor's `setFilter` rebuilds its `view_index` (an `O(N)` walk + `O(F log F)`
sort of the `F` accepted rows); `setSort` re-sorts the `view_index`
(`O(F log F)`). Both run off the Qt thread, bump the cursor `epoch`, and emit a
`RowsReset`. Because the core's natural order is already the default user sort
(`time` DESC), most users never trigger a re-sort; the cost fires only on
explicit sort-by-amount/type/address clicks, which are rare.

### Anchor invariance bounds tip refresh

The `Status` sort key is prefixed by the **confirmed block height**, which is
invariant under a tip append. Therefore a plain tip-advance reorders **no**
cursor's `view_index`. A tip-advance only recomputes status for the
viewport-sized set of rows (via `updateStatus` under the locks) and emits one
`RowsRangeChanged` per cursor — replacing the whole-model `dataChanged(0, N-1)`
sledgehammer.

Only two events require a row reposition: **first confirmation** (a row's
height flips from the `INT_MAX` sentinel to a real height) and **reorg**. Both
arrive via `CT_UPDATED` and must reposition the affected row in **every**
cursor's `view_index` (including off-viewport) and flip its
`Conflicted`/`NotAccepted` filter membership. This is the global-correctness
obligation the windowed model must honor and the test plan exercises.

### Consumer is a virtual model, not `canFetchMore`/`fetchMore`

The consumer `TransactionTableModel` reports the **full** filtered row count
from `rowCount()` (a stable accessibility-tree shape), holds only a
viewport-sized slice cache, and on a cache miss in `data()` returns a
placeholder and schedules a `setViewport` + `getRows` fetch; the reply emits
`dataChanged` for the filled range.

This is deliberately **not** the `canFetchMore`/`fetchMore` idiom the earlier
sketch proposed: with a full `rowCount`, `canFetchMore` never fires, so that
idiom is self-contradictory here. The virtual-model pattern (full count +
placeholder-on-miss) is the correct Qt approach and is better for the
accessibility tree (stable row count).

Two consistency disciplines are baked into the contract:

1. **`rowCount()` is event-driven only.** It is mutated solely inside
   `beginInsertRows`/`endInsertRows`/`beginRemoveRows`/`endRemoveRows`/
   `beginResetModel`/`endResetModel` as the model drains events. It never
   forwards to a live store read. `getRows` is a pure data fetch; a reply whose
   `epoch` does not match the model's current epoch is discarded and re-fetched
   after pending events drain. This preserves Qt's "row count changes only
   between begin/end" contract across the synchronous-down / asynchronous-up
   boundary.
2. **Identity is `(hash, idx)`, not `(txid, vout)`.** Selection and
   `focusTransaction` re-resolution use the row's `(hash, idx)`; `internalPointer`
   is never used to carry a `TransactionRecord*` (it dangles across vector
   reallocation — established in PR #3003).

## The contract (IPC-shaped, in-process)

Value-typed and pointer-free across the view↔model boundary, so the
multiprocess split is a mechanical transport swap. `TransactionRecord` is
already marshalable (no `QString`/`QVariant` members); a `static_assert` guards
that against a future Qt-typed field.

### Down-channel (producer → consumer)

Rides the existing `WalletEventQueue` unchanged in transport; only the payload
variant grows. Every payload carries `viewId` + `epoch`:

```cpp
struct RowsResetPayload       { int viewId; int total_count; uint64_t epoch; };
struct RowCountChangedPayload { int viewId; int total_count; uint64_t epoch; };
struct RowsChangedPayload     { int viewId; int first; int last; uint64_t epoch;
                                std::vector<TransactionRecord> records; };
struct RowsInsertedPayload    { int viewId; int position; uint64_t epoch;
                                std::vector<TransactionRecord> records; };
struct RowsRemovedPayload     { int viewId; int position; int count; uint64_t epoch; };
struct ChainTipChangedPayload { int height; int64_t best_time; };  // retained: balance hint only
```

`TxAddedPayload` / `TxRemovedPayload` (the PR #2944 payloads) are retired once
both consumers are migrated.

### Up-channel (consumer → producer)

In-process: direct synchronous methods on `WalletTxStore`, taking `cs_store`,
returning by value. At Phase 4 each becomes a Cap'n Proto request — a mechanical
lift.

```cpp
RowsResult getRows(int viewId, int first, int last) const;   // slice cache fill
RowsResult getAllRows(int viewId) const;                     // CSV export + select-all copy
RowDetail  getRowDetail(int viewId, int row) const;          // replaces describe(); double-click only
void       setViewport(int viewId, int first, int last);     // visible range + prefetch margin; no core locks
void       setSort(int viewId, int column, int order);
void       setFilter(int viewId, FilterSpec spec);
int        rowForKey(int viewId, uint256 hash, int idx) const;  // selection / focus re-resolution
```

`FilterSpec` and the sort/field projections live in the Qt-free
`src/qt/txfilter.h` (see step 1). Localization stays GUI-process-side: the
cursor carries a GUI-supplied `type`/`address` sort string and `RowDetail` is
rendered consumer-side, so the node never localizes — the one place
"mechanical transport swap" carries an asterisk.

## The PR ladder

The windowed model is reached through a sequence of independently-correct,
none-throwaway, reviewable pull requests. The phasing exists for **risk
control** (reviewability and bisectability of battle-tested multithreaded GUI
code), not for intermediate user-facing value — intermediate states are not
expected to run for an appreciable time. Each step keeps the table fully
working on its own.

| PR | Scope | Risk |
|----|-------|------|
| **1** | Qt-free `txfilter.{h,cpp}` (`FilterSpec` / `Accepts` / `Less`, exact ports) + GUI-OFF unit suite; `TransactionFilterProxy` collapses its members into one `FilterSpec` and delegates `filterAcceptsRow` to `Accepts()`. Sort still via Qt `EditRole`. **Zero GUI behavior change.** | Low |
| **2** | Relocate the cached-wallet **ordering** into a producer-owned `GRC::WalletTxStore` (key-only; `cs_store` leaf lock). The producer computes positions and emits `RowsInserted{position,records}`/`RowsRemoved{position,count}`; `TransactionTableModel` keeps its own Qt-thread-exclusive replica and applies the events at the stamped position in lockstep — it never reads the store on the render path, which keeps the unchanged proxy consistent across a multi-insert batch. `rowCount` is the replica's size (event-driven). | Med-high (new lock) |
| **3** | Per-view `Cursor` infrastructure + `Rows*`/`RowsReset` events; producer-side O(viewport) status refresh on tip-advance and the `CT_UPDATED` reposition/membership-flip correctness. Migrate **OverviewPage** off its proxy to a cursor-bound model (the easy consumer — limit ≈ viewport, no scroll path). | Med |
| **4** | Migrate **TransactionView** off its proxy to a cursor-bound model; **delete `TransactionFilterProxy`**; wire header-click sort → `setSort`, the filter widgets → `setFilter`, `focusTransaction` → `rowForKey`, CSV export + select-all → `getAllRows`, selection re-resolution by `(hash, idx)`. Remove the `updateConfirmations` sledgehammer. Still full-materialized. | Med-high |
| **5** | Windowing: the model holds only a slice cache + margin; `data()` on a miss returns a placeholder and schedules `setViewport` + async `getRows`; status refresh moves fully producer-side; delete `index()`'s Qt-thread `TRY_LOCK`+`updateStatus` and `describe()`'s `LOCK2` (replaced by `getRowDetail`). | **High** (the irreducible multithreaded-GUI concentration) |
| **6** | Off-Qt-thread progressive startup decompose: the `O(N)` decompose moves to a chunked background core task; the table opens at row count 0 and paints progressively. Order-flexible after PR 2. | Med |

PR 2's consumer-side replica is the one transient: a full replica fed by
position-stamped events, kept (rather than reading the store directly) so the
unchanged proxy stays consistent during a multi-insert drain. It shrinks to a
viewport slice in the windowing step (PR 5); nothing is discarded, only
narrowed.

## Deliberate divergences

### Bitcoin Core as input, not default

Bitcoin Core emits the same whole-model `dataChanged` sledgehammer in its
`updateConfirmations` equivalent and has never viewport-scoped it; its scaling
story for large wallets is "use the daemon." That does not fit Gridcoin's
researcher-with-a-large-wallet GUI use case, so this windowed model is a
deliberate, documented divergence rather than a port. Where the design keeps
upstream patterns (the lazy `updateStatus` block-hash staleness check; the
decomposition model), it is because they are sound, not by default.

### ASCII case folding in filter and sort string comparison

The Qt-free core compares strings using ASCII case folding rather than Qt's
Unicode-aware `Qt::CaseInsensitive`, in two places:

- `Accepts()` matches the address/label search substring (`IContains`).
- `Less()` orders the string sort columns — `Status`, `Type`, `Address`
  (`ICompare`).

This is identical to Qt for base58 addresses, ASCII labels, and the ASCII
`Status` sort key. It can differ only for case-insensitive handling of
**non-ASCII** characters — i.e. a non-ASCII character in a label search
(`Accepts`), or in the localized `Type` string when sorting by Type in a
locale whose translated type names contain non-ASCII characters (`Less`). The
`Address` column is base58 (ASCII) so its sort never diverges.

The live exposure differs by step. In step 1 only `Accepts()` is wired, so the
only live divergence is the label-search edge; sorting still goes through Qt's
`EditRole` and remains Unicode-aware. `Less()` becomes the live comparator only
when `TransactionView` is migrated off the proxy (step 4). At that point we
revisit whether ASCII folding is acceptable for the `Type` column or whether
the GUI — which already supplies the localized `type`/`address` sort strings,
keeping localization GUI-process-side — should also supply a locale-aware
collation key so the comparator stays Qt-faithful without the core gaining a
Qt dependency. Until then the divergence is unit-test-only for `Less()`.

Centralizing both predicates in one Qt-free, unit-tested module is worth this
documented, sub-cosmetic edge.

## Risks

- **`cs_store` is a new leaf lock** on a path that is Qt-thread-exclusive today.
  Producer mutations and one-time re-sorts under `setSort`/`setFilter` block the
  Qt thread's `getRows` for the hold duration; mitigated by copy-out-only reads
  and audited hold windows. The lock-order invariant
  (`cs_main → cs_wallet → cs_store`, never nested the other way) is mandatory.
- **`Status`-sort global correctness**: first-confirmation and reorg must
  reposition the affected row in every cursor's `view_index` including
  off-viewport and flip `Conflicted`/`NotAccepted` membership. OverviewPage
  defaults to `Status` DESC, so this is exercised constantly.
- **Windowing (PR 5)** concentrates the irreducible risk: precise
  `begin`/`endInsertRows` across the viewport boundary for uncached rows, epoch
  reconciliation between async `getRows` replies and up-channel events, and
  prefetch-margin handling.
- **GUI-OFF CI cannot exercise the model shell or event wiring** — the blind
  spot that let PR #2944's heap corruption escape CI. Every model-touching PR
  carries a mandatory ASan/UBSan-GUI + isolated-testnet validation burden that
  decomposition does not reduce. PR 1 is structured specifically to land the
  trickiest predicate logic in the GUI-OFF configuration CI *can* exercise.

## Test strategy

- **Unit (offline, GUI-OFF)** — the Qt-free core compiles into `test_gridcoin`
  with `ENABLE_GUI=OFF`. PR 1 covers every `Accepts()` predicate and every
  `Less()` column/order with boundary cases. Later PRs add Qt-free tests for
  the cursor maintenance core: `view_index` splice on insert/remove,
  first-confirmation/reorg reposition across cursors (including off-viewport
  membership flips), and limit-row eviction.
- **Isolated-testnet manual** — the heavy stake-accreted and deliberately
  fragmented wallets are the only way to hit the real paths: scroll past the
  prefetch margin, header-click sort on each column, every filter widget +
  search, date-range + display-limit toggles, reorg during view, new-tx insert
  above/below viewport, selection survival across sort/filter reset, CSV export
  + select-all copy, and label edits.
- **ASan/UBSan-GUI smoke** — mandatory because GUI-OFF CI cannot catch the model
  shell. Each model-touching PR (store relocation, proxy removal, windowing) is
  driven through a full scroll/sort/filter/reorg pass on a large wallet under
  the sanitizer GUI build (`RelWithDebInfo`), watching for use-after-free across
  vector reallocation and across the viewport-boundary row operations. Thread-
  safety annotations on `cs_store` are verified under clang with
  `HAVE_CLANG_THREAD_SAFETY=1` (GCC silently no-ops them).

## Phase 4 — multiprocess (deferred)

Not in scope for this effort. The contract above is built strictly IPC-shaped
so the split is a transport swap, not a redesign: the down-channel payloads and
up-channel methods become Cap'n Proto messages, `WalletTxStore` becomes the
node-side service body behind an `interfaces::` abstraction, and each GUI
process holds only its viewport slice — at which point the memory-footprint win
materializes. The `interfaces::` abstract layer and the Cap'n Proto schema are
explicitly deferred to that work (tracked under the multiprocess RFC, #2937);
this effort deliberately does not build the abstraction shell, because what
matters for a clean split is the **shape of the calls underneath**, which this
contract establishes.

---

## Addendum: the per-view cursor `view_index`-maintenance algorithm (PR3)

This pins down the cursor arithmetic before any wiring (the PR1 risk-control move:
get the trickiest logic into the GUI-OFF unit suite first). A cursor is a Qt-free
object (`src/qt/cursor.{h,cpp}`) maintaining one filtered+sorted view over the
producer's record table, using PR1's `GRC::Accepts` / `GRC::Less` / `SortKey` /
`TxFilterFields` / `FilterSpec`. It never holds records; it is parameterized on two
projector callables supplied by the caller (the store in production, a synthetic
table in the unit tests):

- `Fields(absidx) -> TxFilterFields` — the filter inputs for record `absidx`.
- `Keys(absidx)   -> SortKey`        — the sort inputs for record `absidx`.

### State + invariant

```
std::vector<std::size_t> view_index;   // accepted ABSOLUTE indices into the record
                                       // table, kept sorted by Less(Keys(a),Keys(b),col,order)
```

**Invariant (I):** at every observable point, `view_index` contains *exactly* the
absolute indices `i` with `Accepts(Fields(i), filter) == true`, in `Less`-sorted
order for the current `(sort_column, sort_order)`, and every stored value equals the
record's *current* absolute index in the table.

The **served window** the consumer sees is `view_index[0 .. served)`, where
`served = (limit < 0) ? view_index.size() : min(view_index.size(), limit)`. All
emitted `CursorDelta`s are in **served-window-local** coordinates.

```
struct CursorDelta { enum {Reset, Insert, Remove, Change} type; int first; int count; };
```

### ⚠ Item 1 — locate existing rows by identity, never by sort-key

To find an *existing* record's slot in `view_index` (for remove/update), search for
its **absolute index value** (the `(hash,idx)` identity maps 1:1 to it), NOT a
`lower_bound` over `Less`. Sort-keys are **not unique** — every Unconfirmed row
shares the `INT_MAX`-height `status_sort_key`, so a `lower_bound` on a captured key
lands anywhere in that band and can evict/reposition a *sibling*. Locating an
existing row is therefore an identity scan (or a reverse map); `lower_bound`/`Less`
is used **only** to choose the insertion slot for a row not currently present.

### ⚠ Item 2 — reposition = erase, THEN lower_bound

When a row that is present moves (its `SortKey` changed): **erase it from its old
slot first, then compute the insert slot via `lower_bound` over the post-erase
`view_index`.** A `new_pos` computed against the pre-erase vector is off-by-one
whenever `old_pos <= new_pos`. Erase-then-`lower_bound` is exact by construction.

### ⚠ Item 3 — off-window rows are still maintained; only emission is gated

Membership/position changes update `view_index` for **all** rows, in-window or not
(else an off-window row renders out of order when the window later grows/scrolls —
Invariant I must always hold over the *whole* vector). Only the *delta emission* is
gated by the served boundary (below). First confirmations on a Status-DESC Overview
cross the served boundary constantly, so this path is hot.

### ⚠ Item 4 — reindex/splice discipline (composes across a batch drain)

The store-worker drains a batch of intake ops; each must leave Invariant I intact
before the next:

- `applyStoreInsert(P, count)`: the table inserted `count` records at absolute
  `[P, P+count)`, shifting every later absolute index by `+count`. **(a)** for each
  entry `e` in `view_index`, `if (e >= P) e += count` (fixes the stored absolute
  indices); **(b)** for each new `i` in `[P, P+count)`, `if (Accepts(Fields(i)))`
  insert `i` at its `lower_bound`-by-`Less` slot. Shift-before-insert so the new
  entries are placed against an already-consistent vector.
- `applyStoreRemove(P, count)`: the table removed absolute `[P, P+count)`, shifting
  later indices by `-count`. **(a)** erase the entries whose value is in
  `[P, P+count)` (located by identity, Item 1); **(b)** for each remaining entry
  `e`, `if (e >= P+count) e -= count`. Remove-before-shift, mirror of insert.

Both end with Invariant I, so a sequence of ops composes.

### CT_UPDATED / applyStatusUpdate(P) — the membership/reposition cases

Record `P`'s status (and thus `Accepts`/`SortKey`) changed. Let `old_in =` (P found
in `view_index` at `old_pos`, Item 1), `new_in = Accepts(Fields(P))`:

| old_in | new_in | action | emission |
|--------|--------|--------|----------|
| no  | no  | nothing | — |
| no  | yes | `lower_bound`-insert P at `new_pos` | Insert (+ eviction) if `new_pos < served` |
| yes | no  | erase P at `old_pos` | Remove (+ promotion) if `old_pos < served` |
| yes | yes, slot unchanged | none (keys re-read) | Change at `old_pos` if `< served` |
| yes | yes, slot moved | erase `old_pos`, then `lower_bound`-insert (Item 2) | translate (`old_pos`,`new_pos`) below |

### Served-window translation (eviction / promotion at the boundary)

With `served_old`/`served_new` computed before/after, and `cap = (limit<0)?∞:limit`:

- **Insert at `view_index` slot `p`:**
  - `p >= served_old` → off-window, no emission.
  - `p < served_old` → emit `Insert(first=p, 1)`; **if the window was full**
    (`view_index.size()-1 >= cap`, i.e. `served_old == cap`) the old last visible row
    is pushed out → also emit `Remove(first=cap-1 [post-insert: served_new... =cap], 1)`
    (eviction). Net visible count stays `cap`.
- **Remove at slot `p`:**
  - `p >= served_old` → off-window, no emission (served may shrink if unlimited).
  - `p < served_old` → emit `Remove(first=p, 1)`; **if a row existed just past the
    boundary** (`view_index.size() (pre-remove) > cap`, i.e. there was an off-window
    row at `cap`) it promotes into view → also emit `Insert(first=served_new-1, 1)`
    (promotion).
- **Reposition (`old_pos`→`new_pos`, both relative to the same vector state):** apply
  the Remove translation for `old_pos` then the Insert translation for `new_pos`
  against the post-erase vector, collapsing a same-visible-slot move to a `Change`.

### setLimit(new_limit)

Recompute `served`. If it grows by `k`, emit `Insert(first=served_old, k)` (rows
`view_index[served_old .. served_new)` promote in). If it shrinks by `k`, emit
`Remove(first=served_new, k)`. `view_index` itself is untouched (Item 3) — only the
window boundary moved. This is the cheap stairstep-resize op (decision 1).

### setFilter / setSort

A wholesale change to membership or order: rebuild `view_index` from scratch and emit
a single `Reset(total = served_new)`; the consumer bulk-refills via `getRows`. (An
incremental diff is possible but not worth the complexity for an interactive
filter/sort change.)

### Complexity

Per store op / status update: O(N) over `view_index` (the absolute-index shift and/or
the identity scan) — which is exactly why PR2.5 moved this onto the off-`cs_main`
store-worker. An order-statistics structure to reach O(log N) is tracked separately;
the contract here is correctness, not asymptotics.
