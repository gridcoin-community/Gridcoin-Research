# GUI wallet event-queue redesign — design and testing report

This report documents the redesign of the Qt wallet GUI's transaction-model
update path from a lock-bound, timer-polled architecture to a producer→consumer
event queue, and the isolated-testnet validation that backs it.

It accompanies the change set on the `gui-wallet-event-queue` branch. For the
*why* behind the design decisions — and why the redesign is shaped as
preparatory work for the GUI/node process separation (Discussion #2937) — see
the companion document `gui_event_queue_design.md`.

## 1. The bug

### Symptom

On a wallet with a large transaction count (the test wallet carried ~89,000
transactions) that also receives dense per-block wallet-relevant outputs —
the situation produced by long-running staking nodes with mutual sidestakes —
the Qt GUI froze for seconds per block-connect and minutes per block-disconnect
during a reorg. A depth-12 reorg was measured at ~22 minutes wall-clock for the
disconnect phase alone.

The wallet was never deadlocked; it always eventually caught up. The
user-visible effect was a multi-minute "frozen" wallet during chain catch-up
or reorg.

### Root cause

`TransactionTablePriv::updateWallet` held `LOCK2(cs_main, wallet->cs_wallet)`
on the Qt main thread while doing O(N = cachedWallet.size()) work per
notification:

- `QList<TransactionRecord>::insert` shifts up to N elements per insert. The
  `QList` is a contiguous array; a mid-list insert is a tail memmove.
- `decomposeTransaction` ran inside the same lock scope.
- `beginInsertRows` / `endInsertRows` triggered a view-layout recompute.

With 4–9 wallet-relevant transactions per staked block, per-block GUI work was
350k–800k element moves while holding `cs_main`. The message-handling thread
(`grc-msghand`) running `ConnectBlock` / `DisconnectBlock` was blocked on
`cs_main` acquisition for that whole window — the GUI had become the
`cs_main` serialization bottleneck for consensus.

The 4-second `MODEL_UPDATE_DELAY` poll timers (`ClientModel::updateTimer`,
`WalletModel::pollBalanceChanged`) and the 30-second `ResearcherModel`
refresh timer compounded the contention by adding periodic `cs_main`
contenders.

### Why it is rare on mainnet

Mainnet wallets typically have a few hundred to low-thousands of transactions
and less than one wallet-relevant transaction per block. The O(N) cost is
invisible under those conditions. The pathology needs both a large N and a
high per-block notification density — a combination essentially unique to
long-running test wallets with extensive sidestaking.

## 2. The redesign

The fix replaces the legacy chain

```
CWallet::NotifyTransactionChanged  (boost::signals2, on a core thread)
  → QMetaObject::invokeMethod("updateTransaction", QueuedConnection)
  → WalletModel::updateTransaction        (Qt main thread)
  → TransactionTableModel::updateTransaction
  → TransactionTablePriv::updateWallet    (takes LOCK2(cs_main, cs_wallet))
```

with a producer→consumer event queue:

```
core threads (msghand, stakeminer, RPC, init)
  → NotifyTransactionChanged handler  — decomposes the tx under the
                                        cs_wallet the caller already holds,
                                        pushes a WalletEvent
  → WalletEventQueue                  — MPSC: std::mutex + std::deque,
                                        monotonic uint64_t seqno assigned
                                        under the queue lock at push time
  → WalletModel::drainEventQueue      — Qt main thread, 500 ms QTimer,
                                        drains the queue in batches
  → TransactionTableModel::applyEventBatch  — applies inserts/removes with
                                              NO cs_main / cs_wallet held
```

### Key properties

- **No GUI→core lock acquisition on the incremental update path.**
  `LOCK2(cs_main, cs_wallet)` is gone from the per-notification path.
  `decomposeTransaction` runs on the producer thread, under the `cs_wallet`
  that thread already holds — net-zero wall-clock work relative to the old
  design, but the lock window no longer lands on the GUI thread competing
  with msghand.
- **Producer-side visibility filter, consumer-side datetime filter.** The
  producer applies the wtx-level `showTransaction` checks (orphan
  coinstake/coinbase, legacy OP_RETURN). The settings-dependent datetime
  cutoff (`limitTxnDisplay`) is a cheap per-record predicate the consumer
  applies before model mutation.
- **Batching is automatic.** A 500 ms drain coalesces a burst of
  notifications (a multi-output staked block, or a reorg flood) into one
  drain pass.
- **Forward-compatible with multiprocess separation (Discussion #2937).**
  The producer/consumer contract is the same one an IPC channel needs; the
  queue becomes the consumer side of that channel when the GUI/node split
  lands.

### Event-driven refresh

Two timer-polled refresh paths were also converted to event-driven:

- The 4-second `WalletModel::pollBalanceChanged` poll is replaced by a
  `ChainTipChangedPayload` event, pushed by a subscriber to
  `uiInterface.NotifyBlocksChanged` (fired in `main.cpp::SetBestChain` after
  every chain-tip advance). `checkBalanceChanged()` keeps its `TRY_LOCK`
  guards and its 4-second stale-time gate on the expensive `Get*Balance()`
  calls.
- The 30-second `ResearcherModel::refresh_timer` is replaced by the same
  `NotifyBlocksChanged`-driven refresh, so per-block accrual / magnitude /
  beacon-status updates propagate within one event-loop turn instead of
  within 30 seconds.

The 4-second `ClientModel::pollTimer` is intentionally retained — it only
emits the network-traffic-graph signal, takes no locks, and is not the
bottleneck.

### Commit breakdown

| Commit | Summary |
|---|---|
| 1 | `gui: add wallet_event_queue foundation (event types + MPSC queue)` — pure additions, no behavioural change; `WalletEvent`, `WalletEventQueue`, 6 Qt-test cases. |
| 2 | `gui: wire NotifyTransactionChanged producer side to WalletEventQueue` — queue member on `WalletModel`, producer push parallel to the legacy chain. |
| 3 | `gui: drain WalletEventQueue from a 500ms timer; remove LOCK2 from update path` — the core behavioural change; deletes `updateWallet`, the legacy `updateTransaction` slots, and the `QMetaObject::invokeMethod` chain. |
| 4 | `gui: replace pollBalanceChanged timer with ChainTipChanged events` — removes the 4-second balance poll. |
| 5 | `gui: drive ResearcherModel::refresh from NotifyBlocksChanged` — removes the 30-second researcher poll. |
| 6 | `gui: unify CT_NEW and CT_UPDATED producer-side handling` — fixes the regression described in section 4.4. |
| 7 | `doc: add GUI event-queue redesign design and testing report` — this document. |
| 8 | `review: address Copilot review on PR #2944` — minimise the `drain()` critical section (O(1) deque swap on the full-drain path); bound the per-tick drain batch with immediate re-arm. |
| 9 | `review: address second Copilot review pass on PR #2944` — `checkBalanceChanged` rate-limit fix (section 4.6); correct the `NotifyTransactionChanged` cs_main lock-state comment and add `AssertLockHeld`; remove the now-dead `TxUpdatedPayload` variant. |

## 3. Test environment

All testing was done on an isolated 3-node testnet, the nodes separated by
Linux network namespaces (`ip netns`) with firewall rules, so the chain is a
private fork unaffected by — and not affecting — the public testnet.

| Node | Namespace | Datadir | Role |
|---|---|---|---|
| inst8 | node1 | `.GridcoinResearch` | Control — older binary, Qt GUI, no event queue |
| inst10 | node3 | `.GridcoinResearch3` | Test node — event-queue binary, Qt GUI |

The test wallet carried ~89,000 transactions. All three nodes ran mutual
mandatory sidestakes, so every staked block produced 4+ wallet-relevant
outputs landing in each peer's wallet — the dense-notification condition the
bug requires.

### Latency probe ("sentinel")

The instrument is a shell probe that issues `getinfo` once per second against
a node and records the wall-clock RPC latency. `getinfo` takes
`LOCK2(cs_main, cs_wallet)` and does trivial work, so any latency above
~50 ms is dominated by lock-wait time. Every sample is logged; samples over a
threshold are surfaced live.

A daemon-only node (no GUI) establishes the floor: with no GUI thread
contending for `cs_main`, `getinfo` latency reflects only the intrinsic cost
of the call plus whatever msghand is doing.

## 4. Test results

### 4.1 Steady-state head-to-head

Both nodes at the same chain tip, `in_sync=true`, identical workload:

```
                                       samples  mean    median  p95     max
inst10 NEW-GUI (event queue)               630  0.219s  0.216s  0.241s  0.601s
inst8  OLD-GUI (limitTxnDisplay filter)     59  0.402s  0.387s  0.524s  0.601s
inst8  OLD-GUI (no filter, earlier run)    714  0.729s  0.616s  1.082s  7.386s
inst10 daemon control (no GUI)            1000  0.218s  0.216s  0.230s  0.369s
```

The new GUI binary's latency profile is statistically indistinguishable from
the daemon control — median 0.216 s vs 0.216 s. The GUI's lock-contention
overhead is gone. The new GUI is also faster than the old GUI even when the
old GUI has the `limitTxnDisplay` partial mitigation applied (which alone
roughly halves the pathology).

### 4.2 Reorg stress

During the binary swap onto inst10, the new binary inherited a stale chain
state and had to disconnect 16 blocks back to the common ancestor, then
reconnect ~70 blocks of catch-up to the network tip (block 2771082 →
2771152).

On the original binary this is the multi-minute disaster path described in
section 1. On the new binary the entire sequence completed in **~17 seconds**,
with `getinfo` latency pinned at 0.20–0.30 s throughout and **zero samples
above 1.0 s**.

### 4.3 Full initial block download (IBD-1)

A full sync from genesis — chainstate wiped, wallet.dat retained — was run
to exercise the heaviest sustained notification load the architecture can
see.

- Synced genesis → block ~2.77M in ~1 h 4 m, sustaining ~750 blocks/sec.
- During IBD proper: mean `getinfo` latency 0.221 s, p95 0.257 s.
- Largest single drain batch: 2,151 events absorbed into one 500 ms drain
  pass — the queue handled it without falling behind.
- Across 3,064 samples, only 2 exceeded 1.0 s, both at the `in_sync=false →
  true` transition.

The worst single sample was 14.0 s, at the in_sync transition. Tracing the
debug log placed it on `PollTableModel::refresh()` doing a bulk synchronous
recompute of `GetActiveVoteWeight` for every historical poll — a **separate**
GUI bottleneck on the voting subsystem, not the wallet model. The wallet
event queue itself never stalled msghand during the entire IBD. The
`PollTableModel` refresh is recorded as a follow-up (section 6).

### 4.4 The CT_UPDATED regression — found by IBD-1

IBD-1 surfaced a regression: after the sync completed, the GUI's transaction
list showed only one transaction.

Cause: the legacy `updateWallet` had auto-promotion logic —

```cpp
if (status == CT_UPDATED) {
    if (showTransaction && !inModel) status = CT_NEW;     // promote → insert
    if (!showTransaction && inModel) status = CT_DELETED; // demote  → remove
}
```

— which commit 3 removed along with `updateWallet`. The redesign assumed the
producer fires `CT_NEW` for every tx the GUI needs to insert. That assumption
fails when the wallet's `mapWallet` is populated from wallet.dat *before* the
chain that validates those transactions exists:

1. At startup `loadWallet()` filters out every coinstake — `IsInMainChain()`
   is false on an empty chain — so `cachedWallet` starts nearly empty.
2. During IBD, `AddToWallet` re-validates each wallet tx but fires
   `CT_UPDATED` (not `CT_NEW`) because the tx is already in `mapWallet`
   (`fInsertedNew == false`).
3. The redesigned consumer treated `CT_UPDATED` as status-only — "no row
   mutation" — so `cachedWallet` never gained those rows.

Confirmed by diagnosis: restarting the GUI after IBD completed populated the
list correctly, because `loadWallet()` then ran against the fully-synced
chain.

The fix (commit 6) unifies `CT_NEW` and `CT_UPDATED` handling on the producer
side: both look up the wtx, apply `showTransaction`, and push either a
`TxAdded` (decomposed) or a `TxRemoved`. The consumer's binary-search-by-hash
insert path de-dupes a `TxAdded` when the tx is already in `cachedWallet`,
and the remove path no-ops when it is not. This also covers the steady-state
visibility transitions the legacy auto-promotion handled (an orphan coinstake
becoming valid, or vice versa).

Cost of the fix: each `CT_UPDATED` now pays one `decomposeTransaction` on the
producer thread where before it paid only a hash push. `decomposeTransaction`
is O(vout count) with recursive `IsMine()` lookups under `cs_wallet` — sub-
millisecond per notification in steady state. Over a full IBD the cumulative
cost is bounded by `mapWallet.size()` × per-tx-decompose, the same order as
IBD's own block-validation cost.

### 4.5 Post-fix no-regression check

The fixed binary (commit 6) was restarted on inst10 with the chain already
synced. The transaction list displayed correctly, and steady-state latency
was unchanged: mean 0.219 s, median 0.216 s, max 0.601 s over 630 samples —
identical to section 4.1. The fix introduced no regression at startup or in
steady state.

### 4.6 Full IBD on the complete commit set

Two more full IBDs were run after the commit-6 fix to validate it end-to-end
— that `cachedWallet` populates correctly *during* IBD with no post-IBD
restart — and to exercise the Copilot-review changes.

The first of those surfaced one more issue: a handful of brief mid-IBD
latency blips traced to `checkBalanceChanged()`. Its `MODEL_UPDATE_DELAY`
stale-time gate only advanced its timestamp when a balance *change* was
detected, so a long-stable balance left the gate permanently open and every
drain tick re-ran the full-wallet `Get*Balance()` scans. With the
review-commit's immediate drain re-arm that became continuous. Fixed by
stamping the gate whenever a recompute is performed (not only on a detected
change).

The final validation IBD was run on the complete commit set (all of the
above plus both review commits). Chainstate wiped, fresh sync from genesis
on the isolated testnet against the same ~89k-tx wallet:

- Full sync genesis → tip in **~83 minutes**.
- Steady-state after sync, sustained: mean `getinfo` latency ~0.30 s,
  median ~0.29 s, p95 ~0.33 s — daemon-parity, holding over many hours of
  continuous staking.
- **The transaction list populated correctly during IBD; no post-IBD
  restart was needed** — the commit-6 fix validated in the field.
- Latency excursions over 1 s all fell into explained categories, none of
  them a GUI-induced `cs_main` stall:
  - Brief (1–2 s) `cs_main` holds while msghand connects clutches of
    blocks in the chain region dense with this wallet's transactions.
  - One ~32 s sample during an IBD orphan-block megabatch: ~11,000 blocks
    connected in the surrounding ~65 s window. This is msghand
    *productively* bulk-connecting the orphan backlog — RPC is unavailable
    because the core is busy, not because the GUI is stalling it. The
    legacy GUI doing the same catch-up would have lock-thrashed for hours.
  - Two samples (~20 s, ~25 s) at the in_sync transition: the
    `PollTableModel` bulk `GetActiveVoteWeight` recompute (section 6) — a
    separate voting-subsystem bottleneck, not the wallet model.

The whole point of the redesign held across the entire run: no GUI→core
lock ping-pong appears anywhere. What remains are msghand legitimately
holding `cs_main` to do block-connect work, and the unrelated
`PollTableModel` refresh.

#### Producer-side decomposition runs on msghand — a deliberate trade

One characteristic worth recording explicitly. The producer-side
`decomposeTransaction` (commit 6) runs inline in the
`NotifyTransactionChanged` handler — i.e. on whichever core thread fired
the notification, under the locks it already holds. During chain
catch-up that thread is msghand, holding `cs_main`. So the decompose
cost is *concentrated into msghand's `cs_main` hold* rather than being
spread to the GUI thread.

This is the correct trade, not a regression:

- In the legacy design the decompose ran on the GUI thread, which had to
  *acquire* `cs_main` to do it — producing exactly the lock ping-pong
  between msghand and the GUI that this redesign exists to remove. Total
  wall-clock for a burst was dominated by that handoff overhead.
- In the new design msghand does the decompose inline, with no handoff.
  Net system work is similar; msghand's individual `cs_main` holds are
  somewhat longer, but a burst completes far faster overall because the
  ping-pong is gone.

The visible effect: during a pathological IBD orphan-megabatch the cost
concentrates into one long msghand hold (the ~32 s sample above) instead
of a long stuttering sequence of shorter ones. In steady state — one
block at a time, a few wallet-relevant txs — the per-notification
decompose is sub-millisecond and invisible. Eliminating even the
concentrated-hold case would mean deferring decomposition off the
notification thread entirely, which requires wallet access from a
non-core thread — that is multiprocess-separation territory (section 6),
out of scope here.

## 5. Scope boundaries

What the redesign deliberately does **not** change:

- **`TransactionTablePriv::loadWallet`** — the initial bulk load and the
  filter-change full reset still take `LOCK2(cs_main, cs_wallet)`. These are
  infrequent and not the bottleneck.
- **`TransactionTablePriv::index()`** — the lazy per-row status refresh on
  read still takes `TRY_LOCK(cs_main) + TRY_LOCK(cs_wallet)`. `TRY_LOCK`
  semantics already prevent the GUI from blocking.
- **`QList<TransactionRecord>` backing container** — still O(N) per insert.
  The redesign moves that shift cost off the `cs_main` critical path (it now
  runs GUI-thread-internal, never blocking msghand), but does not eliminate
  it. Replacing the container with an O(log N) structure is a separate,
  orthogonal change (section 6).

## 6. Follow-ups

- **Replace `QList<TransactionRecord>`** with an O(log N) backing container.
  Orthogonal to this redesign — the shift cost is no longer on the
  consensus-critical path, so it is now a bounded GUI-thread-internal cost.
- **`PollTableModel::refresh()`** does a bulk synchronous `GetActiveVoteWeight`
  recompute for every historical poll, observed as a GUI stall at the IBD
  in_sync transition — ~14 s in the first full IBD (section 4.3) and ~20–25 s
  in the final one (section 4.6). Same architectural treatment —
  event-driven / incremental refresh — applies.
- **`interfaces::Wallet` boundary** — when multiprocess separation lands, the
  `WalletEventQueue` becomes the consumer side of the IPC channel; the
  producer side moves to the node process.
