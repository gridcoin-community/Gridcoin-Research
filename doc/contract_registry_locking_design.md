# Contract Registry Locking Design

This document records the locking model for the GRC contract registries —
`BeaconRegistry`, `Whitelist`, `ProtocolRegistry`, `SideStakeRegistry`,
`ScraperRegistry` — and explains the conventions used by the `EXCLUSIVE_LOCKS_REQUIRED`,
`LOCKS_EXCLUDED`, and `GUARDED_BY` annotations across the codebase.

The contract registries follow one of two patterns:

  - **Pattern (a) — `cs_main`-protected.** All access to the registry's internal
    state goes through callers that already hold `cs_main`. The registry has no
    internal mutex. Members are annotated `GUARDED_BY(cs_main)` and methods are
    annotated `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`.

  - **Pattern (b) — leaf-lock via `cs_lock`.** The registry has its own
    `cs_lock` (a `CCriticalSection`) that protects internal data structure
    consistency. Chain-handler entry points acquire both `cs_main` and `cs_lock`
    (canonical order: `cs_main → cs_lock`). Non-chain entry points acquire
    only `cs_lock`. Members are annotated `GUARDED_BY(cs_lock)`.

The choice between (a) and (b) is per-registry, driven by whether any
non-chain-handler code path mutates or reads the registry without holding
`cs_main`.

## Why two patterns?

`cs_main` is the global blockchain consistency lock. It is held by chain
validation, accrual, mining, contract dispatch, and most consensus-touching
code. It is *intentionally coarse*: any operation that needs the chain state
to be stable while it runs takes `cs_main`.

For data structures that are exclusively touched by chain-handler code,
`cs_main` provides all the protection needed. The registry doesn't need its
own mutex because every reader and writer is already serialised by the chain
handler. This is pattern (a). Adding a separate `cs_lock` would just be
redundant ceremony.

For data structures that have non-chain mutators or readers — typically the
Qt GUI thread (responding to user input), the scraper thread (collecting
stats and running convergence on its own schedule), or RPC handlers that
don't logically need a chain-state view — `cs_main` is the wrong granularity.
Forcing those threads to take `cs_main` for a simple read or local-state
update would serialise them against chain activation, which costs throughput
and creates ordering concerns.

For those cases, the registry provides its own `cs_lock`. This is pattern
(b). The leaf-lock invariant (see below) keeps the additional lock safe.

## The leaf-lock invariant

When a registry uses pattern (b), `cs_lock` is a **leaf lock**. While the
lock is held, no other lock may be acquired, and no call may be made into
another subsystem that could itself acquire any lock.

Concretely, while holding `cs_lock`:

  - **Do not acquire any other lock** — not `cs_main`, not `cs_wallet`, not
    another registry's `cs_lock`, not any unrelated `CCriticalSection`.
    Acquiring a second lock while `cs_lock` is held breaks the leaf-lock
    property and re-introduces lock-order obligations.

  - **Do not call into another subsystem.** Methods on other registries,
    wallet operations, networking, the scheduler — all are off-limits. Each
    of those could acquire its own lock; even if it doesn't today, a future
    contributor could add one without realising the constraint.

  - **Do not emit `uiInterface` notifications.** Those re-enter the GUI
    thread and acquire Qt-side locks.

  - **Avoid `LogPrintf` with non-trivial formatters** that allocate or touch
    global state. `LogPrintf` on plain string literals is fine; format
    specifiers that call into `FormatMoney` or similar should be moved
    outside the critical section.

The pattern is: gather what you need before `LOCK(cs_lock)`, mutate or read
the registry's own members inside, defer notifications and cross-subsystem
work to after the scope ends.

Violating any of these makes `cs_lock` no longer a leaf lock and the
canonical `cs_main → cs_lock` order is no longer guaranteed acyclic.

## Isolation levels

The locking choice maps to four levels of consistency, in the same sense as
database transaction isolation:

| Level | Mechanism | Semantic guarantee |
|---|---|---|
| Read Uncommitted | no lock | torn reads, undefined behaviour — never acceptable |
| Read Committed | `LOCK(cs_lock)` around a single method | atomic value at that instant; may be stale when caller acts |
| Repeatable Read | `LOCK(cs_main)` across multiple registry reads | same values seen across all reads while the lock is held |
| Serializable | `LOCK(cs_main)` held across the entire decision-and-action | chain state cannot advance between the read and the side-effect |

A leaf `cs_lock` provides Read Committed only. **Callers that need
Repeatable Read or Serializable must hold `cs_main` separately, in addition
to whatever the registry takes internally.**

Concrete examples in this codebase:

  - **Read Committed is sufficient**: a GUI table displaying current sidestake
    allocations, an RPC reporting a snapshot of the scraper list, a Qt model
    listing whitelisted projects. The user gets a snapshot and refreshes when
    they want a fresher one.

  - **Repeatable Read is needed**: a caller that correlates multiple
    registry reads (e.g. "is this sidestake destination also a registered
    scraper?") must hold `cs_main` so the two views agree.

  - **Serializable is needed**: the miner's reward-claim construction
    reads beacon validity and acts on it (signs a claim). If the beacon
    expires between the read and the signature, the claim is invalid. The
    miner holds `cs_main` across the entire window — read, decision,
    action.

The `cs_lock` annotation only protects mechanical correctness against torn
reads. Holding `cs_main` in addition is what gets you state-machine
consistency with the rest of the chain-driven state.

## Per-registry decisions

### BeaconRegistry — pattern (a)

`BeaconRegistry`'s `cs_lock` was acquired zero times anywhere in the tree.
Every consumer of beacon registry state — chain validation, accrual, mining,
contract dispatch, RPC, the GUI's beacon status display — holds `cs_main`
when reading or mutating. There is no non-`cs_main` path.

Members are `GUARDED_BY(cs_main)`. Public methods are
`EXCLUSIVE_LOCKS_REQUIRED(cs_main)`. The previously-vestigial `cs_lock` is
removed.

If a future contributor adds a non-chain path (e.g. a config-file beacon
registry feature analogous to the sidestake non-contract path), the right
move is to add a `cs_lock` at that point and convert this annotation set
accordingly. Do not take `cs_main` from the new path as a shortcut —
holding `cs_main` from the wrong thread is its own design defect.

### SideStakeRegistry — pattern (b)

Has two state pots:

  - `m_mandatory_sidestake_entries` — chain-state. Mutated by chain-handler
    contracts (under `cs_main`), read by reward computation and RPC.

  - `m_local_sidestake_entries` — user-local config. Mutated by the Qt
    GUI's sidestake editor (`SideStakeTableModel::setData/addRow/removeRows`,
    not under `cs_main`) and by config-file load/save paths. Read by the
    same paths plus the miner.

The two pots share the same registry object. `cs_lock` serialises mutations
between the chain-handler path and the GUI/config path. Without it,
concurrent insertion into adjacent `std::unordered_map` operations would
race.

Members `GUARDED_BY(cs_lock)`. Chain-handler methods
`EXCLUSIVE_LOCKS_REQUIRED(cs_main)` and acquire `cs_lock` internally.
Non-chain entry points acquire only `cs_lock`.

### ScraperRegistry — pattern (b)

Has a real non-`cs_main` reader path: the dedicated scraper thread
(`ThreadScraper`) consumes the registry via `GetScrapersLegacy*()` to
validate manifests without holding `cs_main`. Holding `cs_main` from the
scraper thread would block chain activation during convergence and is the
wrong dependency direction.

Members `GUARDED_BY(cs_lock)`. Same chain-handler vs non-chain method split
as SideStakeRegistry.

### ProtocolRegistry — pattern (b)

Currently has no confirmed non-`cs_main` consumer, but is kept consistent
with the other (b) registries to provide future-proofing. The leaf-lock
scaffold is cheap (one uncontended atomic op per call) and the consistency
across registries reduces cognitive load for future contributors.

### Whitelist — pattern (b), but the rollout is deferred

Whitelist has the same conceptual shape as ProtocolRegistry/ScraperRegistry
(option (b) leaf-lock), but its `Snapshot()` method is currently coupled to
the AutoGreylist refresh path in a way that makes a clean annotation
rollout require additional work.

`Whitelist::Snapshot()` is annotated `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`
because its default `refresh_greylist=true` path triggers
`AutoGreylist::Refresh()`, which requires `cs_main`. Several current
consumers — RPC handlers, Qt model methods, the scraper main loop, the
receiver-side `Quorum::ValidateSuperblock` chain — don't hold `cs_main`
when calling `Snapshot()`. The runtime doesn't assert, and the implicit
refresh has been load-bearing for cross-node greylist convergence.

Adding `GUARDED_BY(cs_lock)` to Whitelist's members while keeping
`Snapshot()`'s current annotation would break CI immediately on those
consumers. Conversely, dropping the `cs_main` annotation requires moving
the AutoGreylist refresh out of `Snapshot()` to chain-handler trigger
points so the consensus mechanism remains explicit and deterministic.

That work is a focused follow-on PR. The design and migration plan is at
`~/GridcoinDev/autogreylist_refresh_redesign/README.md` in the
project-local working tree. Once it lands, Whitelist's annotations follow
the same pattern (b) shape as the other registries.

## Lock ordering

Canonical order across the codebase is:

```
cs_main → <wallet>cs_wallet → registry cs_lock (or other leaf locks)
```

Chain-handler code naturally acquires `cs_main` first (held by the
validation thread). When it calls into a registry mutator like
`SideStakeRegistry::AddDelete`, the registry acquires `cs_lock`
internally. The order is enforced at compile time by the
`EXCLUSIVE_LOCKS_REQUIRED` annotations on the chain-handler methods.

Non-chain code (Qt thread, scraper thread, config-file path) acquires
`cs_lock` directly without `cs_main`. Because `cs_lock` is a leaf lock
(per the invariant above), this can never invert the canonical order.

If a leaf-lock invariant violation is introduced — e.g. a method called
from inside a `LOCK(cs_lock)` block acquires `cs_main` — Clang's
thread-safety analysis catches it as a missing-required-lock error at the
inner method's call site, provided the inner method is annotated
`EXCLUSIVE_LOCKS_REQUIRED(cs_main)`. This is why annotation discipline
matters for the load-bearing chain-handler methods even when they're
already taking `cs_main` "obviously."

## CI gate

The `WERROR_THREAD_SAFETY=ON` CMake option (added in #2947) makes Clang's
thread-safety analysis a hard build error. The CI sanitizers job sets this.
For local development on thread-safety changes, set it in your build dir:

```sh
cmake -B build -DWERROR_THREAD_SAFETY=ON
```

A missing `EXCLUSIVE_LOCKS_REQUIRED` annotation, a member access without
the corresponding `GUARDED_BY` lock held, or a lock-order violation will
fail the build instead of compiling with a warning.

## See also

  - `doc/developer-notes.md` — overall threading and lock-ordering notes,
    including the master list of `cs_*` mutexes and their acquisition order.

  - `doc/automated_greylisting_design_highlights.md` — AutoGreylist design
    background.

  - PR #2980 — initial thread-safety annotation rollout across
    `cs_msMiningErrors`, `cs_addrSeenByPeer`, `cs_ScraperGlobals`, and
    `cs_mapAlreadyAskedFor`.

  - PR #2947 — Clang thread-safety analysis CI gate.

  - Issue #1157 — long-running thread-safety hardening tracking.
