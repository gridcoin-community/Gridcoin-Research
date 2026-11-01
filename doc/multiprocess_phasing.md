# Multiprocess Program — Detailed Phasing Plan

Status: working plan of record for the phase-by-phase execution of the multiprocess
program (RFC discussion #2937). `doc/multiprocess_design.md` is the *design* of record —
architecture, settled decisions, cross-cutting rules, and the interface catalog. This
document carries the detail the design document's summary table does not: per-phase
scope, ordered work items, the specific couplings each phase removes, and acceptance
criteria. Where this document and the design document disagree on design, the design
document wins; where they disagree on sequencing or scope detail, this document wins.

The plan is grounded in a nine-agent per-file census of `src/qt` (~129 translation
units, 60 coupled at baseline) run at program kickoff. The census-derived tracking
checklist lives in issue #3153.

## 1. Progress ledger

| Step | PR | Merged | Delivered |
|---|---|---|---|
| PR-0 | #3154 | 2026-07-13 | Vestigial-include + dead-code sweep (22 files); removed the dead `updateWeight` slot and the emitter-less `ThreadSafeAskQuestion`; shrank the lint baseline before the ratchet existed |
| 1a | #3156 | 2026-07-14 | `src/interfaces/{README.md,handler.h,node.h,wallet.h,staking.h,init.h}` + in-process impls (`src/node/interfaces.cpp`, `src/wallet/interfaces.cpp`); `MakeSignalHandler`; `test/lint/lint-qt-includes.sh` ratchet (163-entry allowlist); interface unit tests; the two hard rules documented (no `cs_main` in notification callbacks; value types only) |
| 1b | #3157 | 2026-07-14 | ClientModel + BanTableModel onto `interfaces::Node` + new `interfaces::StakingStatus`; scraper by-reference global getter → value snapshot; alert queries folded into Node; `tryGetNumBlocksOfPeers` TRY_LOCK idiom; ratchet 163 → 150 |
| 1c-i | #3158 | 2026-07-15 | WalletModel query/command surface onto `interfaces::Wallet` (balances snapshot, encryption commands with the staking-only preference folded into `unlockWallet`, key ops, value-typed coin control, sendCoins moved node-side); **ThreadSafeAskFee eliminated** (stateless `FeeConfirmationRequired` + accepted-fee retry, design §4.5); trust-boundary re-validation node-side; `fWalletUnlockStakingOnly` made atomic; `WalletCoinControl` value type; ratchet 150 → 147 |

Remaining: 1c-ii, 1d (five sub-steps), 1e, 1f, then Phase 2 and Phase 3.

## 2. Conventions that apply to every remaining PR

- **Grow-per-migration**: interfaces grow exactly the surface the migrating consumer
  needs, in the migration PR — no speculative methods.
- **Ratchet discipline**: every migration PR runs `test/lint/lint-qt-includes.sh` and
  removes the allowlist entries it makes stale; the lint enumerates them. Entries are
  never added except where a migration surfaces *pre-existing* coupling previously
  hidden behind a transitive include.
- **Value types only** across the boundary; notification callbacks enqueue and return
  (design §4.1). New value structs get the marshalability treatment from the start.
- **Behavior parity by default**: migrations preserve observable behavior; deliberate
  deltas (bug fixes surfaced by the migration, trust-boundary hardening) are called out
  in the PR description as such. Anti-patterns listed for each phase below are the
  *sanctioned* deltas — each is a work item, not incidental churn.
- **Locks**: canonical order `cs_main → cs_wallet → subsystem/leaf`. TRY_LOCK
  bow-out patterns cross the boundary as `try*` interface methods (`tryGetBalances`,
  `tryGetNumBlocksOfPeers` are the templates).
- Each sub-phase is one reviewable PR unless noted; the monolithic build stays green
  the whole way.

## 3. Phase 1c-ii — `interfaces::WalletTxSource`

The windowed transaction-table stack (#2944, #3010–#3049) is adopted as-is as the
wallet-side transport seam; it was built for this boundary and its in-code comments
designate it as the #2937 IPC seam. 1c-ii formalizes it. This is the largest remaining
Phase-1 step and splits into ordered, individually buildable stages (candidate PR
boundaries marked ▸):

**▸ 1c-ii-a — DTO hygiene (mechanical, self-contained).**
`TransactionRecord`/`TransactionStatus` are scrubbed of Qt types: `qint64` → `int64_t`,
`QString`/`QList` members and static-method signatures → `std::string`/`std::vector`.
The GUI-side formatters absorb the conversions. A marshalability `static_assert`
(trivially-copyable members or explicitly listed value containers) pins the property.
Rationale: the node cannot compile the DTO while it drags Qt headers; every later stage
depends on this. The Qt-free cores (`txfilter`, `txorder`, `cursor`, `windowcache`)
already compile GUI-OFF and need no rework.

**▸ 1c-ii-b — interface + producer hoist + ownership inversion (the core PR).**
- `src/interfaces/wallet_tx_source.h`: the store's existing call shapes, near verbatim —
  up-channel `registerView(viewId, FilterSpec, sort)`, `setViewSort/Filter/Limit`,
  `getRows`/`getAllRows` → `RowsResult` (already an atomic value bundle),
  `rowForKey`, `reloadAndSnapshot`; down-channel = the existing `WalletEventPayload`
  variants (already pointer-free value types with seqno/viewId/epoch reconciliation
  keys). The event queue's `push`/`drain` pair wraps behind the interface unchanged.
- **Producer hoist**: the two producer free functions living in `walletmodel.cpp`
  (`NotifyTransactionChanged` — decompose + status-stamp under `cs_main`/`cs_wallet`;
  `NotifyBlocksChangedForWallet` — chain-tip push + inline volatile-set refresh) become
  internals of the node-side source. They are self-contained (they touch only
  wallet/chain state plus the store/queue), so the hoist is a move, not a rewrite.
- **Ownership inversion**: `WalletModel` currently owns `WalletEventQueue` +
  `WalletTxStore` by value (with a load-bearing declaration-order dependency); the
  source owns them, constructed node-side by a factory. `WalletModel`'s
  `getTxStore()/getEventQueue()` accessors become the interface handle.
- The store, queue, worker thread, `TransactionRecord` fill logic, and the Qt-free
  ordering/filter/cursor cores move out of the GUI-only build unit into the node-side
  library. GUI keeps: `WindowCache`, the three view models and their formatters
  (address-book labels and unit formatting are GUI state), the drain loop, and
  viewport/anchor logic.

**▸ 1c-ii-c — view lifecycle.**
- Add the missing `unregisterView`; views today leak-by-design because consumers die
  with WalletModel. Per-connection view namespaces (or connection-scoped registration)
  so a future second client cannot collide with view ids.
- **Attach/detach lifecycle**: the source is constructed lazily on first GUI attach and
  torn down (refcounted) on last detach — mapping 1:1 onto today's WalletModel
  ctor/dtor. "Headless pays nothing" is preserved as a *lifecycle property*, not a
  config flag: no store, no worker thread, no signal subscriptions exist without an
  attached client. This is the contract Phase 2's capability construct/destruct
  implements directly.

**▸ 1c-ii-d — row detail.**
`getRowDetail` currently returns node-rendered HTML (`TransactionDesc::toHTML`) as a
`QString`. Per the settled decision, it becomes a structured value DTO (the fields
`toHTML` renders: status/date/source/amounts/message/inputs, plus the Gridcoin
stake/MRC specifics) rendered — and translated — GUI-side, per design §4.1. The
node-side HTML path is deleted, closing the one standing localization violation on
this component.

**Known synchronous couplings, and their treatment:**
- `TransactionTablePriv::loadWallet → reloadAndSnapshot` blocks the Qt thread under
  `LOCK2` at startup. In-process this is tolerated (it predates the program); over IPC
  it is one big-value snapshot call. The real fix — progressive startup decompose,
  step 6 of the windowed-model plan — remains explicitly unbuilt and is *not* pulled
  into 1c-ii; it can land any time after, independently.
- `DetailedTxModel::fetchWindow`'s drain-then-fetch sequencing assumes same-thread
  queue access. The epoch/high-water gates already make the asynchronous
  generalization safe-by-retry; 1c-ii replaces the direct drain call with a
  source-side flush (or drops it), so the discipline survives the split.

**Acceptance:** node-side units compile in the GUI-OFF build (add them to the
non-GUI test target as the Qt-free cores already are); `walletmodel.cpp`'s
`main.h`/`wallet/wallet.h` ratchet entries removed (the producer leg was their last
justification); the tx-table Qt tests still pass; marshalability asserts in place;
no `tr()`-dependent method on the interface.

## 4. Phase 1d — Gridcoin domain interfaces (one PR per model family)

Ordering is by risk reduction first, then dependency convenience. Each item lists the
consumers, the surface, and the specific anti-patterns it removes.

**▸ 1d-i — `interfaces::MRC` (first: fixes a live race).**
MRCModel's refresh/submit paths are annotated `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` but
run from GUI-thread call sites holding nothing — the annotation is invisible across
the Qt slot boundary, and `pindexBest`/`mempool` reads race today. The interface gives
MRC **one atomic node-side snapshot call** (height, version gates, trial `CreateMRC`
result, output limit, fee, queue position) plus a submit command. Consumers: MRCModel
(and via it the request page). Acceptance: no lock annotations on GUI-side MRC code
because no GUI-side code touches core state.

**▸ 1d-ii — `interfaces::SideStakeManager`.**
SideStakeTableModel hands out raw `GRC::SideStake*` through
`QModelIndex::internalPointer` and mutates the registry directly
(`NonContractAdd/Delete`). The interface supplies value-type entry rows and
command-style add/delete/validate; mutations return the post-mutation
`registry_version` (design §4.4) and `RwSettingsUpdated` carries it. Consumers:
SideStakeTableModel, OptionsDialog.

**▸ 1d-iii — `interfaces::VotingManager`.**
VotingModel today drives poll-registry traversal internals (the
`registry_traversal_in_progress` flag and the reorg-retry loop) from the GUI —
inverted ownership. The interface owns the retry internally behind **one atomic
poll-table build call**, and adds poll/vote submission plus a real fee query
(replacing the hard-coded estimate and its "add core API" TODO). The vote-submission
path's modal unlock-under-`cs_main` is reordered so user interaction happens outside
core locks (design §4.5, same treatment as ThreadSafeAskFee). The whitelist snapshot
VotingModel shares with ResearcherModel lands here or in 1d-iv, whichever cuts first —
it is one method (`ResearcherContext` owns it per the catalog).

**▸ 1d-iv — `interfaces::ResearcherContext` + `interfaces::BeaconManager` + `interfaces::Scraper`.**
One PR, three headers — the consumers overlap almost completely (ResearcherModel,
DiagnosticsDialog). CPID/status/magnitude/accrual and BOINC context; beacon
status/expiry/advertise/renewal (the `GenerateBeaconKey`/`SendBeaconContractV3` paths
under `LOCK2` move behind commands); project whitelist snapshot; pool-mode detection;
scraper convergence *value* snapshots (the ClientModel half landed in 1b — this
completes the ResearcherModel uses, e.g. convergence-excluded projects) and
`Quorum::ExplainMagnitude`. The `cs_msMiningErrors` read moves behind
`StakingStatus`.

**▸ 1d-v — `interfaces::PSGTPool`.**
Pool snapshot rows, per-entry wallet relevance ("needs my signature" — today computed
with direct `pwalletMain->HaveKey`/`PSGTSignedBy` reaches), and sign/add/remove/
broadcast commands. Consumers: PSGTPoolTableModel, PSGTPoolPage, and MultisignDialog —
which currently reimplements the walletprocesspsgt/submitpsgt RPC logic in-dialog and
converts to pure command consumption here (listed under 1e in the design doc's table;
executing it with the PSGTPool PR keeps the dialog from being migrated twice).

`interfaces::ContractFactory` from the catalog is grown opportunistically inside these
PRs where a send path needs contract construction not already covered (the TxMessage
embed moved node-side in 1c-i; beacon/vote/MRC contracts arrive with their domain
PRs). If nothing remains by the end of 1d, the catalog entry is retired rather than
scaffolded empty.

## 5. Phase 1e — composition root, dialogs, settings

- **`bitcoin.cpp` init/shutdown via `interfaces::Init`**: replace the
  `bGridcoinCoreInitComplete` busy-wait with an Init-interface handoff; model
  construction order becomes the documented attach order. This is where the Phase-2
  seam gets its final in-process shape: monolithic `MakeGridcoinInit()` vs a future
  IPC `Init` proxy is the *only* difference between the builds (design §2).
- **`bitcoingui.cpp` reaches**: the inline `extern` declarations that bypass include
  analysis; the remaining direct global reads move to model queries.
- **Dialog cleanup**:
  - SignVerifyMessageDialog's `pwalletMain->GetKey` → a signing command on
    `interfaces::Wallet` (message signing never exports the key).
  - The coin-control/consolidate-wizard twins **deduplicate first** (they carry
    duplicated fee-estimation and tree-building logic), then the shared helper
    consumes the 1c-i value types; the per-output `getPubKey` round-trip is replaced
    by a signature-size (or key-id) field on `WalletOutput` — a carried 1c-i follow-up.
  - AboutDialog's synchronous network fetch moves off the GUI thread.
- **Settings flows** (the E4 census): rw-settings writes become a Node method
  (GUI edit → node persists → node broadcasts versioned `RwSettingsUpdated`), killing
  the re-entrant refresh loop risk at the split; proxy/UPnP edits become Node methods
  with immediate-effect semantics; QSettings stays GUI-local; the config file remains
  read-only from the GUI.
- **AddressTableModel** onto `interfaces::Wallet` (address-book read/write/keypool
  surface — deferred from 1c-i deliberately).
- **Carried 1c-i follow-ups land here** (pre-freeze shape fixes, cheap while nothing
  depends on the signatures): wallet lock-state consolidated into one snapshot struct
  mirroring `WalletBalances`; `unlockWallet` gains the time-bound parameter design
  §4.3 requires of the split build.
- **Allowlist burn-down**: by the end of 1e the ratchet allowlist should be at or near
  zero; anything left is explicitly justified in the lint file.

## 6. Phase 1f — tests and the gate

- Interface mocks (hand-rolled, like upstream's) + Qt test coverage exercising the
  models against mocks — the "GUI testable in isolation" motivation made real.
- Marshalability static asserts on every boundary struct.
- **Flip `lint-qt-includes.sh` from ratchet to hard-fail** (empty allowlist): no
  `src/qt` file includes `main.h`, `wallet/wallet.h`, registry headers, or reaches
  core globals. **This flip is the gate to Phase 2.**

## 7. Phase 2 — IPC layer, opt-in build

### 7.1 Packaging (settled 2026-07-14; supersedes the RFC-era umbrella-launcher sketch)

**Two binaries only — no umbrella launcher, no separate GUI/node executables.**

- `gridcoinresearchd -multiprocess`: the daemon also listens on the IPC socket.
- `gridcoinresearch -multiprocess`: the GUI runs as a client — no core init, no
  datadir lock; it performs the §4.3 handshake and obtains its `interfaces::*` from
  the IPC `Init` proxy instead of `MakeGridcoinInit()`. The Init seam built in 1e is
  exactly this switch.
- The default is multiprocess disabled: without the option both binaries are
  monolithic, exactly as today, indefinitely. Split mode is enabled by
  `-multiprocess` on the command line or, equivalently, `multiprocess=1` in the
  config file (config options mirror CLI arguments as usual) — and because the
  config file is shared, that one line flips a deployment coherently. Each
  binary ignores the other's-only arguments, and the handshake catches
  network/datadir mismatch.
- v1 behavior: the GUI errors clearly when no daemon is listening (auto-spawn is later
  sugar; the listen/connect transport supports it without upstream's fd-passing).
- Settings ownership in split mode: conf read by both; QSettings GUI-local;
  rw-settings JSON node-owned with GUI writes via the Node method (1e); the GUI logs
  to its own file.

Rationale: the complexity drivers behind upstream's multi-binary sprawl and `bitcoin`
wrapper (wallet-process split, multiwallet) do not exist here — the wallet stays
in-node (settled decision 2).

### 7.2 Sub-phases

| Step | Scope |
|---|---|
| 2a | Add libmultiprocess + capnproto to `depends`; `ENABLE_MULTIPROCESS` build plumbing; `.capnp` schemas for `Init`/`Handler`/`Node`/`StakingStatus` mirroring the headers 1:1; schema-diff lint vs previous release tag (design §4.2) |
| 2b | Transport + handshake: AF_UNIX everywhere incl. Windows; socket permissions and `bind()`-exclusivity recovery rules; cookie-PSK `Init::authenticate`; `node_identity.json` + QSettings expectation binding; build-identities dialog + dismissible soft-warn banner (design §4.2/§4.3, connect sequence steps 1–8) |
| 2c | `Wallet` + `WalletTxSource` over IPC: the event channel becomes the connection (seqno resync from high-water on reconnect); attach/detach = capability construct/destruct; `SecureString` passphrase bytes zeroed both sides; time-bounded unlock enforced |
| 2d | Domain interfaces over IPC (schemas for the 1d set); versioned-notification high-water reset tied to the handshake identity step (design §4.4) |
| 2e | Integration tests: handshake-failure suite (wrong cookie, mismatched schema major/minor, wrong UID, stale socket, foreign node squatting), kill/restart matrices, one native-Windows CI job for ACL/peer-identity assertions |

Blocking-signal completions that belong to Phase 2 (design §4.5): `ThreadSafeMessageBox`
MODAL becomes async-with-acknowledgement (the node never blocks on GUI dismissal);
`Translate` leaves the interface surface entirely — the node side of a split build
runs untranslated. ThreadSafeAskFee (eliminated, 1c-i) and ThreadSafeAskQuestion
(deleted, PR-0) are already gone.

## 8. Phase 3 — hardening

- **Reconnect logic**: GUI survives node restart (re-handshake, high-water reset,
  full-snapshot refetch — the §4.4 pattern makes this a code path, not a redesign);
  node survives GUI disconnect (refcounted source teardown from 1c-ii-c is the
  mechanism).
- IPC metrics in the About dialog.
- GUI-process capability drop / seccomp on Linux (full sandboxing stays out of scope,
  design §8).

## 9. Dependency summary

```
1c-ii-a ─► 1c-ii-b ─► 1c-ii-c ─► 1c-ii-d      (strictly ordered)
1d-i … 1d-v                                    (independent of 1c-ii and of each other;
                                                recommended order as listed)
1e  ◄─ requires 1c-ii + 1d complete (composition root wires everything)
1f  ◄─ requires 1e (allowlist near zero)       ══ GATE ══► Phase 2
2a ─► 2b ─► 2c ─► 2d ─► 2e                     Phase 2 internally ordered
Phase 3 ◄─ requires 2 shipped behind the flag
```

Progressive startup decompose (windowed-model step 6) is deliberately outside the
critical path: it improves startup latency in both builds and can land any time after
1c-ii-b.
