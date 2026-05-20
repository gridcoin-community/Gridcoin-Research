# GUI wallet event-queue — design rationale

This document records the design decisions behind the Qt wallet GUI's
producer→consumer event-queue redesign, and why the redesign is deliberately
shaped as preparatory work for the GUI/node process separation discussed in
Discussion #2937.

It is the *design* companion to `gui_event_queue_report.md`, which is the
testing report — the bug analysis, the isolated-testnet validation runs, and
the measured results. Read that one for "does it work and how do we know";
read this one for "why is it built this way."

## 1. The problem, in one paragraph

The legacy Qt transaction-model update path ran
`TransactionTablePriv::updateWallet` on the Qt main thread while holding
`LOCK2(cs_main, wallet->cs_wallet)`, doing O(N) `QList` work per
notification. On a large wallet with dense per-block notifications the GUI
became the `cs_main` serialization bottleneck for the message-handling
thread — seconds per block-connect, minutes per disconnect during a reorg.
The full analysis is in `gui_event_queue_report.md` §1.

## 2. The architecture

The redesign replaces the synchronous, lock-bound notification chain with an
asynchronous producer→consumer event queue:

```
   core threads (msghand, stakeminer, RPC, init)
     │  CWallet::NotifyTransactionChanged / uiInterface.NotifyBlocksChanged
     ▼
   producer  — decomposes the tx under the locks the core thread already
               holds; pushes a self-contained WalletEvent
     │
     ▼
   WalletEventQueue  — MPSC: std::mutex + std::deque; a monotonic uint64_t
                       seqno is assigned under the queue lock at push time
     │
     ▼
   consumer  — WalletModel::drainEventQueue(), Qt main thread, 500 ms timer;
               drains in bounded batches and applies them to the model with
               NO cs_main / cs_wallet acquired
```

The single behavioural invariant: **the consumer never acquires a core lock
and never reads core state.** Everything it needs to mutate the transaction
model arrives inside the events.

## 3. Design choices

Each choice is given with the alternative that was considered and the reason
the alternative was rejected.

### 3.1 An event queue, not a faster `updateWallet`

*Alternative:* keep the synchronous chain and attack the symptom — replace
`QList` with an O(log N) container, coalesce notifications, throttle the
poll timers.

*Rejected because:* every one of those is throwaway the moment #2937 lands.
A separate-process GUI cannot hold `cs_main`/`cs_wallet` or call
`decomposeTransaction` against the live wallet at all — those live in the
other process. Any work that keeps the synchronous, lock-coupled chain has
to be redone. Re-architecting the update path **once**, into a shape that is
correct in-process today and is also the consumer side of the eventual IPC
channel, is the only change that is not wasted motion.

### 3.2 One channel with a discriminated payload, not multiple queues

`WalletEvent` carries a `std::variant` payload (`TxAddedPayload`,
`TxRemovedPayload`, `ChainTipChangedPayload`). One queue, one discriminator.

*Alternative:* a separate queue per event kind.

*Rejected because:* the consumer must apply events in the order the core
produced them — a `TxAdded` followed by a `TxRemoved` for the same hash is
not the same as the reverse. A single queue preserves global order for
free. Multiple queues would need a cross-queue ordering barrier that just
reconstructs single-queue semantics.

### 3.3 `std::mutex` + `std::deque`, not a lock-free queue

*Alternative:* a lock-free MPSC ring buffer.

*Rejected because:* the contended path is tiny and the boring option is
verifiably correct. `push()` holds the mutex for one `deque::push_back` plus
a seqno increment — sub-microsecond. `drain()` is structured so the
producer-facing critical section is also tiny: the full-drain path swaps the
whole `deque` out under the lock in O(1) and builds the result vector after
releasing it. A lock-free structure would add real complexity to remove a
cost that is not on any hot path. If profiling ever shows the mutex is hot,
the structure can be swapped without changing the contract.

### 3.4 A monotonic `uint64_t` sequence number

Every event gets a process-global, monotonically increasing `seqno`,
assigned under the queue lock at push time.

*In-process* it is mostly a diagnostic and an ordering assertion. Its real
purpose is forward-looking: see §4.4.

### 3.5 A 500 ms timer drain, not a signal-driven wake

The consumer is a plain `QTimer` at `MODEL_EVENT_DRAIN_INTERVAL` (500 ms).

*Alternative:* wake the consumer immediately on each push (condition
variable, or a queued Qt signal).

*Rejected because:* a fixed-cadence drain is naturally batchy — a staked
block that lands nine wallet outputs is drained as one batch, not nine
wake-ups — and 500 ms latency on a transaction-list row is imperceptible. A
signal-per-push reproduces the per-notification overhead the redesign exists
to remove. A hybrid (signal that respects a minimum interval) is a possible
later optimisation, but the timer alone is sufficient and simplest.

### 3.6 Bounded per-tick batch with immediate re-arm

`drainEventQueue` drains at most `MODEL_EVENT_DRAIN_MAX_BATCH` (1024) events
per tick. If it hits the cap it re-arms itself via `QTimer::singleShot(0,
…)` rather than waiting for the next periodic tick.

*Rationale:* the `QList` insert is still O(N) (see §5), so an unbounded
drain of a huge backlog (reorg flood, IBD catch-up) could freeze the Qt main
thread in one apply pass. Bounding the batch caps the main-thread work per
event-loop turn; the immediate re-arm means a genuine backlog still clears
in a few turns rather than one batch per 500 ms. The GUI stays responsive
between batches without slowing backlog drain.

### 3.7 Producer-side decomposition

`TransactionRecord::decomposeTransaction` runs in the producer — on the core
thread that fired the notification, under the `cs_wallet` it already holds.
A `TxAddedPayload` carries fully-decomposed `TransactionRecord`s.

*Alternative:* push the bare hash; let the consumer decompose.

*Rejected because:* decomposition needs wallet access (`IsMine`, the
`CWalletTx`). If the consumer did it, the consumer would need `cs_wallet` —
which destroys the whole "consumer touches no core state" invariant, and is
*impossible* once the consumer is a separate process (§4). Doing it on the
producer is also net-zero wall-clock work relative to the legacy design,
which decomposed too — just on the GUI thread under a contended `LOCK2`.

There is a real consequence, documented honestly in the report
(`gui_event_queue_report.md` §4.6): the decompose now runs inline on the
core thread, so during pathological IBD orphan-block megabatches its cost
concentrates into that thread's `cs_main` hold. That is the correct trade —
it is the legacy GUI↔core lock ping-pong, not the decompose work itself,
that made the original bug pathological — but it is a property to be aware
of.

### 3.8 Producer-side visibility filter, consumer-side display filter

Two filters decide whether a transaction is shown. They are split by what
each side legitimately knows:

- The **producer** applies the wtx-level checks in
  `showTransaction(wtx, false, 0)` — orphan coinstake/coinbase, the legacy
  OP_RETURN rule. These need wallet and chain state, which the producer has.
- The **consumer** applies the user's `limitTxnDisplay` datetime cutoff — a
  cheap predicate on `record.time`. This is a GUI display setting; the core
  has no business knowing it.

This split is not arbitrary tidiness: it is the same boundary the IPC split
imposes. The producer side is "what the node knows about the wallet"; the
consumer side is "what the GUI's user has configured."

### 3.9 Event-driven refresh, replacing the poll timers

Two periodic refreshers — the 4-second `WalletModel::pollBalanceChanged` and
the 30-second `ResearcherModel` refresh — are replaced by refreshes driven
off `uiInterface.NotifyBlocksChanged` (a `ChainTipChangedPayload` event for
the former; a direct subscription for the latter). Balance and accrual are
deterministic functions of chain state; the natural cadence is "every chain
tip advance," not a wall clock. This also removes two recurring `cs_main`
contenders. (`ClientModel`'s 4-second timer is left alone — it only emits a
network-traffic-graph signal and takes no locks.)

## 4. Why this is preparatory work for #2937

Discussion #2937 proposes splitting the Gridcoin GUI and node into separate
processes, communicating through a Bitcoin Core-style `interfaces::`
abstraction over libmultiprocess / Cap'n Proto, with the wallet remaining
in the node process. Its Phase 1 introduces the `interfaces::` abstraction
while the build is still monolithic.

A separate-process GUI imposes one hard constraint: **the GUI process cannot
acquire `cs_main` or `cs_wallet`, cannot read `mapWallet`, cannot call
`decomposeTransaction` against the live wallet** — all of that lives in the
other process's address space. Everything the GUI needs must arrive over an
IPC channel as self-contained messages.

### 4.1 The legacy design violated that constraint everywhere

The legacy transaction-model path took `cs_main` and `cs_wallet` directly on
the GUI thread, looked up `mapWallet[hash]`, and ran `decomposeTransaction`
against the live wallet. Every one of those is a direct reach into core
state. Under #2937 none of them is even compilable in the GUI process.

### 4.2 The new design already obeys it

The consumer — `drainEventQueue`, `applyEventBatch`, `applyTxAdded`,
`applyTxRemoved` — acquires no core lock and reads no core state. It operates
purely on `WalletEvent`s and its own `cachedWallet`. It is already written
as if the wallet were in another process.

### 4.3 Each element maps to an IPC concept

| In-process today | Becomes, under #2937 |
|---|---|
| `WalletEventQueue` | the IPC notification channel — the `interfaces::Wallet` notification stream |
| producer `push()` | node-side serialization of a notification onto the wire |
| `WalletEvent` (seqno + self-contained payload) | one wire message |
| producer-side `decomposeTransaction` | the node does the wallet work; the GUI receives a ready-to-render record and never calls back into the wallet |
| `drainEventQueue` 500 ms timer | the GUI process's receive/consume loop |
| consumer applying events with no core lock | unchanged — it already needs nothing from the node process |

### 4.4 The seqno is the reconnect primitive

In-process the `uint64_t` seqno is mostly diagnostic. Across an IPC boundary
it becomes load-bearing: a GUI process that disconnects and reconnects to the
node (node restart, transport blip) must resynchronize. A monotonic seqno
lets the GUI tell the node "I last saw event N — send me everything after
N," or, on a gap, "resync from scratch." Designing it in now means the wire
protocol does not need a format change later.

### 4.5 What #2937 still has to do

This redesign deliberately stops at the in-process boundary. It does **not**
introduce the `interfaces::` abstraction, does not pull in libmultiprocess or
Cap'n Proto, and does not move the producer into a separate process. Those
are #2937's work.

What #2937 inherits, already done: the GUI-side transaction-model update
path is in the correct shape. The remaining change for *this* path is to
swap the transport — the in-process `WalletEventQueue` for the IPC channel —
and relocate the producer to the node process. The consumer does not need to
be touched. Without this redesign, #2937 would have to re-architect the
transaction model from scratch as part of the process split; with it, that
sub-problem is already solved and validated.

This is the intended pattern for approaching #2937 generally: convert each
GUI↔core coupling into a producer/consumer shape incrementally, each step a
shippable in-process improvement on its own, so that the eventual process
split is a transport change rather than a rewrite.

## 5. Deliberate non-goals

Scope boundaries, recorded so they are not mistaken for oversights:

- **The `QList<TransactionRecord>` backing container is unchanged** — still
  O(N) per insert. The redesign moves that shift cost off the `cs_main`
  critical path (it is now GUI-thread-internal, never blocking msghand) but
  does not eliminate it. An O(log N) container is a separate, orthogonal
  change.
- **`TransactionTablePriv::loadWallet`** (initial bulk load, filter-change
  reset) still takes `LOCK2(cs_main, cs_wallet)`. It is infrequent and not
  the bottleneck. Under #2937 it becomes a request/response IPC call rather
  than a notification-stream consumer — a different shape, out of scope here.
- **`TransactionTablePriv::index()`** lazy per-row status refresh still uses
  `TRY_LOCK`. `TRY_LOCK` already prevents the GUI from blocking.
- **`PollTableModel::refresh()`** does a bulk synchronous recompute that
  stalls the GUI at the IBD in_sync transition. It is a separate voting-
  subsystem bottleneck that wants the same event-driven treatment; it is
  noted as a follow-up, not addressed here.
- The **wallet stays in the node process** under #2937; nothing here assumes
  or requires splitting the wallet out. The producer is wallet-side / node-
  side; the consumer is pure GUI. That division is consistent with #2937's
  stated scoping.
