# Contract Registry Locking Model

## Scope

Gridcoin's contract subsystem stores governance state in singleton registries
reached through global accessors (`GetBeaconRegistry()`, `GetScraperRegistry()`,
`GetProtocolRegistry()`, `GetSideStakeRegistry()`, `GetWhitelist()`). This
document defines the locking model these registries follow, so that Clang's
thread-safety analysis (`-Wthread-safety`) can enforce it. It is normative: the
annotations exist to make the compiler check the model described here.

## Two tiers of consistency

Two distinct locks operate at two different scopes. They are not
interchangeable.

- **`cs_main` — the coherence envelope.** The blockchain's top-level lock. A
  caller that needs the registry coherent *as a whole* — internally consistent
  across several reads, or consistent with the surrounding chain state —
  acquires `cs_main` before it touches the registry. Whether a caller needs
  this is a property of *what the caller is doing*, not of the registry.

- **`cs_lock` — internal access safety.** A private mutex inside each
  pattern-(b) registry. Its only job is to make a single access to the
  registry's own members mechanically safe (no data races, no torn reads) for
  callers that are *not* holding `cs_main`. It is an implementation detail.
  **The outside world has no knowledge that it exists.**

## The rules

1. **`cs_lock` is private and invisible.** No public method carries any
   `cs_lock` capability annotation — not `EXCLUSIVE_LOCKS_REQUIRED(cs_lock)`
   (that would require callers to hold a lock they cannot even name, since it is
   private), and not `LOCKS_EXCLUDED(cs_lock)` (that leaks the lock's existence
   into a contract the caller can neither see nor act on). A public method
   simply takes `cs_lock` internally.

2. **Members are private and `GUARDED_BY(cs_lock)`** in pattern (b). They *must*
   be private: a public `GUARDED_BY(cs_lock)` member is the same incoherence as
   a public method requiring a private lock — no external caller can hold
   `cs_lock` to access it legally. More broadly, the model requires full
   encapsulation: **there is no direct public access to any class member.** All
   access goes through member methods that take the lock internally. (This
   applies equally to pattern (a), where members are private and
   `GUARDED_BY(cs_main)`.) The lock that protects member data from races is
   `cs_lock`, because it is the only lock that *every* access path shares.

3. **Read accessors carry no lock annotation.** They take `cs_lock` internally,
   build a self-contained result, release the lock, and return that result **by
   value**. They must never hand back a reference to a `cs_lock`-guarded member,
   because the reference would outlive the internally-held lock. What they
   return must be **safe to use**; whether it is **correct** to use is the
   caller's concern (see "Safe versus correct").

4. **Chain-handler methods** (`Add`, `Delete`, `Validate`, `BlockValidate`,
   `Revert`, …) carry `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` and take `cs_lock`
   internally. They are state-machine transitions: the registry's contract
   state must advance and rewind atomically with the blocks, so they have
   exactly one valid calling context — under `cs_main`. That is not a caller
   judgment call, so the requirement is expressed and enforced.

   `cs_main` is **necessary but not sufficient** here. The read accessors are
   reachable *without* `cs_main`, so holding `cs_main` does not serialize a
   mutation against those readers; the only lock the two paths share is
   `cs_lock`. A chain handler therefore holds `cs_main` (chain coherence) **and
   still takes `cs_lock` internally** (data-race exclusion against the lock-only
   readers). Canonical order: `cs_main → cs_lock`.

5. **Private helpers that run under an already-held `cs_lock`** carry
   `EXCLUSIVE_LOCKS_REQUIRED(cs_lock)`. This is the *only* place that annotation
   legitimately appears — internal to the class, documenting "my caller already
   holds the lock." Such a helper may touch members or return references
   internally, because the lock is held across the whole operation; the
   no-references rule bites only at the public boundary.

6. **`cs_main` annotations on the public surface are legitimate** — `cs_main` is
   a shared structural lock the caller genuinely holds, and the canonical lock
   order matters. This is the exact opposite of `cs_lock`: a shared, named,
   caller-held lock versus a private, internal, invisible one. The distinction
   between the two is *public-vs-private lock*, and it is the crux of this whole
   model.

## Safe versus correct

These are different guarantees, and the registry owes only the first.

- **Safe** = mechanically sound. The returned value is self-contained; using it
  cannot cause undefined behaviour, a data race, or a dangling reference. The
  registry guarantees this unconditionally.
- **Correct** = semantically appropriate for the caller's purpose. A snapshot
  taken without `cs_main` may be stale by the time the caller acts on it;
  whether that matters is a property of the caller's logic. If it matters, the
  caller takes `cs_main` to pin the state across its read-decide-act window. The
  registry does not, and cannot, decide this.

## Why by-value snapshots are safe: immutable-once-published entries

The registries store `map<key, Entry_ptr>` where `Entry_ptr` is a `shared_ptr`.
A read accessor returns a **copy of the map** under `cs_lock`: the map itself is
duplicated, but its `Entry_ptr` elements still point at the same `Entry` objects
as the live registry. That sharing is safe **only because a published `Entry` is
never mutated in place.**

"Published" means inserted into the current map (and recorded in the registry's
historical store, `m_*_db`). From that point the `Entry` object is effectively
read-only. Every state change produces a *new* `Entry` and re-points the map slot
to it; the prior `Entry` is retained in the historical store, linked by
`m_previous_hash` — a one-way "chainlet" of revisions per key. Nothing ever
writes through an existing `Entry`.

In effect this is a small version-control structure: each `Entry` is an
immutable revision, `m_previous_hash` is its parent link, the historical store
(`m_*_db`) is the object store, and the current map holds the per-key "HEAD."
Roll-forward commits a new revision and advances HEAD; rollback moves HEAD back
to the parent revision. Because revisions are immutable, moving HEAD is the only
thing a reorg ever does — it never edits a revision.

### Reverts and roll-forwards

This matters most at chain reorganisation, which is exactly where you might
expect entries to be edited — and they are not. Roll-forward (`Add`/`Delete`
during block connect) and rollback (`Revert` during block disconnect) both run
under `cs_main` and take `cs_lock` internally, and both operate purely on *map
slots and pointers*, never on `Entry` contents:

- **Roll-forward** constructs a new `Entry`, stores it in the historical store,
  and assigns its `shared_ptr` into the current map slot. The previous entry
  remains in the historical store (reachable via the new entry's
  `m_previous_hash`).
- **Rollback** erases the reverted entry's current slot and, if the reverted
  action had a predecessor, *resurrects* it: it pulls the earlier entry's
  `shared_ptr` back out of the historical store and re-points the slot to it. The
  resurrected entry is byte-for-byte what it was when first published —
  *because* it was never mutated. (That immutability is what makes reorg-time
  reversion cheap: re-point a pointer instead of replaying contracts forward.)

So a snapshot taken by a non-`cs_main` reader is insulated from a concurrent
reorg two independent ways: (1) its map is a separate copy, so slot
re-pointing/erasure in the live map is invisible to it; and (2) `shared_ptr`
ownership keeps every entry it captured alive and unchanged even if that entry
is erased or replaced in the live registry — no use-after-free, no torn read.
The snapshot is therefore always **safe** to read. It may of course be **stale**
— a reorg may have moved the chain out from under it — which is precisely the
"safe ≠ correct" boundary: a reader that must remain coherent across a
roll-forward/rollback holds `cs_main`, which excludes the chain handlers for the
duration of its read-decide-act window.

This invariant is load-bearing and must be preserved. A future change that
mutates a published entry in place — e.g. flipping `m_status` on an entry already
in the map instead of swapping in a fresh one — would silently corrupt every
outstanding snapshot *and* break reorg reversion (the historical chainlet would
no longer reflect what was actually published). Any registry following this
model must verify it on *all* mutation paths, not just the primary add/delete.

## Pattern (a): BeaconRegistry

BeaconRegistry has no non-`cs_main` access path — every consumer (validation,
accrual, mining, contract dispatch, RPC, GUI status) reads or mutates under
`cs_main`. Its members are therefore `GUARDED_BY(cs_main)` and its public
methods are `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`. There is no second lock; the
declared `cs_lock` is vestigial (acquired nowhere) and is removed. If a future
feature adds a genuine non-`cs_main` path, that is the point to introduce a
`cs_lock` and convert to pattern (b) — not to reach for `cs_main` from the new
path.

## Pattern (b): ScraperRegistry, ProtocolRegistry, SideStakeRegistry (and Whitelist)

These have real non-`cs_main` readers:

- **ScraperRegistry** — the scraper thread reads authorization state via
  `GetScrapersLegacy()` without `cs_main` (`scraper.cpp`).
- **ProtocolRegistry / ScraperRegistry** — RPC iterates `ProtocolEntries()` /
  `Scrapers()` without `cs_main` (`rpc/blockchain.cpp`).
- **SideStakeRegistry** — the Qt sidestake editor and the config load/save paths
  read and mutate local entries without `cs_main`.

So their members are `GUARDED_BY(cs_lock)`, their accessors take `cs_lock`
internally, and `cs_main` rides on top as the coherence envelope for the
chain-handler (mutating) path only. Whitelist has the same shape but its rollout
is deferred — its `Snapshot()` is entangled with the AutoGreylist refresh path
and needs separate work before it can be annotated cleanly.

## Concrete consequences

The rollout is split into one PR per registry; this document is the model they
all follow (later PRs reference it). The concrete changes the model implies:

- `Scrapers()` and `ProtocolEntries()` currently return `const ScraperMap&` /
  `const ProtocolEntryMap&` — references into `cs_lock`-guarded members. They
  must return **by value** (a snapshot built under `cs_lock`). The legacy
  getters (`GetScrapersLegacy*`, `GetProtocolEntriesLegacy*`) already return by
  value and are safe.
- Members of the three pattern-(b) registries become `GUARDED_BY(cs_lock)`;
  BeaconRegistry's become `GUARDED_BY(cs_main)` and its dead `cs_lock` is
  removed.
- No public method gains a `cs_lock` annotation. Chain handlers keep
  `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`. Private helpers that run under the held
  lock get `EXCLUSIVE_LOCKS_REQUIRED(cs_lock)`.

## Verification

Clang thread-safety analysis is the enforcement mechanism, and it is
**Clang-only** — under GCC/MSVC the attributes parse but are silent no-ops, so a
non-Clang build can pass while checking nothing. The CI "Thread Safety (Clang)"
job (`.github/workflows/cmake_quality.yml`) sets `WERROR_THREAD_SAFETY=ON` with
Clang. To reproduce locally:

```sh
cmake -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DWERROR_THREAD_SAFETY=ON
```

Confirm the analysis is active before trusting a clean build:
`grep HAVE_CLANG_THREAD_SAFETY build/CMakeCache.txt` should show `=1`. Wipe
`build/` when switching compilers — a reconfigure does not switch the compiler
of an existing build directory.

## Enforcement limitation: polymorphic calls through `IContractHandler`

The registries are reached two ways. Most consumers use the concrete global
accessor (`GetBeaconRegistry()`, `GetProtocolRegistry()`, …) — there the analyzer
sees the concrete method's annotations and enforces them. But the contract
machinery also reaches them **polymorphically** through the `IContractHandler`
base interface: `RegistryBookmarks::GetRegistryWithDB()` returns
`IContractHandler&`, and the contract-replay, block-connect/disconnect, startup
coherence, and passivation paths call `Initialize` / `GetDBHeight` /
`SetDBHeight` / `PassivateDB` through that base reference.

At those base-type call sites the analyzer **cannot** enforce a lock, and the
base virtual declarations are intentionally left unannotated. The reason is
structural: the same virtual has *different* locking contracts across the
registries — pattern (a) (BeaconRegistry) requires `cs_main`, while pattern (b)
takes `cs_lock` internally and requires nothing of the caller. A single
annotation on the base would be wrong for one pattern or the other, so the base
stays unannotated and these polymorphic call sites are statically unchecked.

This is safe in practice: every such call site is a chain-processing path that
already holds `cs_main` (contract replay, block connect/disconnect, startup
coherence recovery, DB passivation) — which satisfies pattern (a) and is
harmless for pattern (b). The gap is purely in *static enforcement*: a future
caller invoking these through the base reference without `cs_main` would not be
flagged by the analyzer. New consumers should therefore either go through the
concrete registry accessor (where annotations are enforced) or hold `cs_main`
explicitly on the chain-processing path.
