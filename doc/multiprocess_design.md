# Multiprocess Architecture Design — GUI/Node Separation

Status: design of record for the multiprocess program (RFC discussion #2937, Phase 0
converged 2026-07). Phase 1 (the `interfaces::` abstraction layer) is in progress; this
document codifies the decisions the RFC thread settled and the revised implementation
plan. Where this document and the RFC discussion disagree, this document wins.

Companion documents: `doc/transaction_table_windowed_model.md` and
`doc/windowed-transaction-table-architecture.md` describe the windowed transaction-table
stack that serves as the wallet-side transport seam for this design.

## 1. Motivation

The wallet today is a monolithic binary: GUI, node, wallet, scraper, miner, and contract
handlers share one address space. The costs:

- **No privilege boundary.** A Qt-side bug (image decoder, URL handler, clipboard parser)
  sits in the same process as the wallet's private keys and the consensus state machine.
- **Coupling makes refactoring slow.** Qt models reach directly into `pindexBest`,
  `pwalletMain`, `nBestHeight`, `g_connman`, and the Gridcoin registry singletons. Every
  consensus refactor has to consider whether some widget reads the same global.
- **The GUI cannot be tested in isolation.** No abstraction means no mocks.
- **No upgrade path to richer UIs** (remote-attach, dashboard, QML) without one.

Bitcoin Core solved this with an `interfaces::` abstraction layer plus an opt-in IPC
build. Their abstraction layer has been in production for years; notably, upstream
master's *shipping* GUI still runs in-process (only the Mining/rpc/echo IPC surfaces are
in-tree). Our Stage 1 therefore targets exactly where upstream production code actually
is today, and Stage 1 delivers value even if Stage 2 never ships.

## 2. Architecture

### Stage 1 — `interfaces::` abstraction (monolithic build, no IPC)

Pure-virtual interfaces in `src/interfaces/`, with concrete in-process implementations
that wrap the existing globals and registries:

```
 Qt (src/qt/)          →  src/interfaces/*.h (pure virtual)  →  concrete impls
 ClientModel               node.h, wallet.h, handler.h,          src/node/interfaces.cpp
 WalletModel               init.h + Gridcoin domain              src/wallet/interfaces.cpp
 domain models             interfaces (§5)                       src/gridcoin/interfaces.cpp
```

The monolithic build keeps working the entire time; each migrated model is an
independent PR. A `NodeContext`-style context struct is **explicitly not a
prerequisite** — Bitcoin Core shipped its interfaces two years before NodeContext
existed. Impl methods wrap the globals directly (`LOCK(cs_main); return nBestHeight;`)
and can migrate to a context struct incrementally later.

### Stage 2 — IPC + process split (opt-in build)

libmultiprocess + Cap'n Proto proxies generated from `.capnp` schemas that mirror the
`interfaces::` headers 1:1. Same headers, different backend: `ENABLE_MULTIPROCESS=OFF`
gives direct calls; `=ON` gives generated proxies. Binaries: `gridcoinresearch-gui`,
`gridcoinresearch-node`, and an umbrella `gridcoinresearch` launcher dispatching on
`argv[0]` / `-m`. Transport: unix domain socket at `<datadir>/<network>/node.sock`
(AF_UNIX on Windows as well — see §4.3).

## 3. Settled decisions

These were converged in RFC #2937 and are not open for relitigation in Phase 1/2 PRs.

1. **IPC stack: libmultiprocess + Cap'n Proto.** Interface pointers cross the wire,
   bidirectional callbacks work, and the monolithic/multiprocess builds share headers.
2. **Stage 2 scope is the GUI/node split only. The wallet stays in-node indefinitely.**
   We do not run multi-wallet, and the staking path (`miner.cpp` fetches the beacon key
   from the wallet during claim signing) makes a wallet process split a real staking
   risk with no compelling driver. Revisit only if a concrete driver appears.
3. **Front-end/back-end matching policy** (§4.2).
4. **Authentication: cookie-PSK handshake layered over filesystem permissions** (§4.3).
   The cookie handshake is the load-bearing, platform-portable authenticator; peer
   UID/PID checks are best-effort defense-in-depth.
5. **Registry-mutation ordering: monotonic versions with a consumer high-water mark**
   (§4.4).
6. **Sequencing**: Phase 1 began after the v13/v14 mandatory (5.5.0.0 "Natasha"), the
   post-Natasha hotfix line, and the PSGT workstream all shipped.

## 4. Cross-cutting design rules

### 4.1 Interface hygiene (the two hard rules)

1. **Notification callbacks must never take `cs_main`** (nor call interface methods that
   do). Gridcoin's validation and UI signals are emitted synchronously, frequently while
   the emitter holds `cs_main` (and in some cases `cs_mapAlerts`,
   `cs_ConvergedScraperStatsCache`, or a registry lock). A callback that re-enters core
   under those locks deadlocks the split build and silently serializes the monolithic
   one. Callbacks enqueue and return.
2. **Only value types cross the boundary.** No `CBlockIndex*`, `CWalletTx&`,
   `GRC::SideStake*`, or references to global caches in any interface signature.
   Existing anti-patterns to be eliminated during migration include the by-reference
   `ConvergedScraperStatsCache` getter with a caller-side lock comment-contract, and the
   raw `GRC::SideStake*` handed out through `QModelIndex::internalPointer`.

Localization never crosses the boundary in either direction: no interface method may
depend on `tr()`/`QString` rendering, and the node side of a split build runs
untranslated (the `Translate` UI signal is a monolith-only convenience; in the split
build node-emitted strings are English and only GUI-owned strings are translated).

### 4.2 Matching policy (Stage 2)

The node writes `<datadir>/<network>/node_identity.json` (random per-datadir UUID,
network, canonical datadir, genesis hash). The GUI persists the expected
`node_id` + `network` (QSettings) on first successful connect and, on every later
launch, hard-fails when a *different* `node_id` answers at the same socket path —
that is what catches a moved datadir, a `-reindex` identity rotation, or a foreign
node squatting the socket. The comparison is between the stored expectation and the
identity the node reports over IPC, never merely between the json file and the node
that wrote it.

Both binaries embed `git_commit / built_at / schema_major / schema_minor /
protocol_version` at compile time. The commit fingerprint builds on the existing
build metadata (`BUILD_GIT_COMMIT` / `BUILD_GIT_TAG` from
`build-aux/cmake/VersionFromGit.cmake`, which already appends a `-dirty` suffix to
the commit hash). Today those variables are simply left empty when git metadata is
unavailable; Stage 2 additionally requires a configure-time refusal for
split-capable builds when the commit is not resolvable at all, so no such build
ships an empty fingerprint. Immediately after authentication (see the handshake
sequence in §4.3), the build-info exchange compares:

| Field | Mismatch policy |
|---|---|
| `node_id` / `network` | Hard fail with explanatory dialog |
| `schema_major` | Hard fail (incompatible wire format) |
| `schema_minor` | GUI > node: refuse. GUI < node: soft warn |
| `protocol_version` | Hard fail outside the compatible range |
| `git_commit` | Soft-warn banner, dismissible, keyed by the commit-hash pair |
| `built_at` | Informational (About dialog) |

Dev builds with dirty trees get an explicit `dirty-<parent_commit>-<diff_hash>` identity;
the banner simply triggers more often. Schema evolution: additive changes bump minor;
breaking changes (field reorder/renumber/removal) require a major bump and a
one-version transition release where the node speaks both — CI enforces via a
schema-diff lint against the previous release tag.

### 4.3 Authentication and transport (Stage 2)

- Socket directory `0700`, socket `0600`; refuse to start on looser permissions.
- Cookie-PSK: node writes a fresh 256-bit cookie (`ipc.cookie`, `0600`, atomic rename)
  each startup; first IPC call is `Init::authenticate(cookie)`, constant-time compare,
  disconnect on mismatch. The cookie is pure portable code and is the load-bearing
  authenticator on every platform.
- Peer-identity checks (`SO_PEERCRED` / `LOCAL_PEERCRED` / `SIO_AF_UNIX_GETPEERPID`) are
  best-effort defense-in-depth — platform asymmetry must not become a security
  regression.
- **One transport everywhere: AF_UNIX**, including Windows (supported since Win10 1803;
  filesystem-path based, same endpoint code). The remaining platform delta hides behind
  a two-method port (listen/connect + peer identity) with one contract-test suite. The
  only new CI cost is a single native-Windows job for ACL/peer-identity assertions
  (Wine does not enforce NTFS ACLs).
- Anti-replay / anti-hijack: the node creates the socket path with `O_EXCL`; if the
  path already exists (stale from a crash), the node verifies no process is listening
  before removing and recreating it — it never unlinks a live socket, and it never
  silently displaces another listener.
- Wallet passphrase crosses as bytes zeroed on both sides after the call; unlock is
  always time-bounded.

The connect-time sequence, in order (steps 1–7 add ~5–10 ms to startup):

1. GUI reads `node_identity.json` → expected `node_id`, `network`
2. GUI reads `ipc.cookie` (absent cookie ⇒ node not running, do not dial)
3. `connect(node.sock)`
4. Peer-identity check (`SO_PEERCRED` / `LOCAL_PEERCRED` /
   `SIO_AF_UNIX_GETPEERPID`; best-effort defense-in-depth per the
   authentication layers listed earlier in this section) → hard fail on mismatch
5. `Init::authenticate(cookie)` → hard fail on mismatch
6. `Init::getBuildInfo()` → schema/protocol/commit comparison per §4.2
7. `Init::getIdentity()` → compare against the stored expectation per §4.2
8. `Init::makeNode()` / `makeWallet()` / … → proceed

Authentication (step 5) precedes every other exchange — build and identity
information is never served to an unauthenticated peer.

### 4.4 Registry/state change notifications: versioned refetch

Several UI signals are payload-free refetch triggers (`RwSettingsUpdated`,
`BeaconChanged`, `MRCChanged`, `ResearcherChanged`, `AccrualChangedFromStakeOrMRC`,
`BannedListChanged`). Over IPC (and already today, across the queued-connection hop to
the Qt thread) notification order is not synchronized with call returns. The pattern:

1. Each mutable registry/service keeps a monotonic `registry_version`, **assigned at the
   commit point under the registry's own lock**, and the notification carries it (the
   void signals grow a version parameter at the interface boundary).
2. The consumer keeps a **high-water mark across all sources** (call returns and
   accepted notifications) and drops any notification with version ≤ high-water. This
   also coalesces bursts for free. Comparing against "my last call" only is not enough —
   registries also mutate from RPC, config reload, and chain-driven contracts.
3. **Read-your-writes via the returned version**: mutating calls return the
   post-mutation version (ideally `(snapshot, version)` atomically) and the caller
   refetches immediately rather than waiting for a notification it may legitimately drop.
4. **The high-water mark resets on reconnect** (versions are per-node-process counters);
   tie the reset to the identity-binding handshake step.

Dropping stale notifications is safe because every accepted notification triggers a full
snapshot refetch — the pattern requires refetch-style consumers, which is what the GUI
models already are. (Note: `SideStakePayload::m_version` is a serialization format
version; the new counter is named `registry_version` to avoid collision.)

### 4.5 Blocking request/response signals

Synchronous core→GUI round-trips do not survive a process boundary gracefully and are
handled as follows during Phase 1:

- **`ThreadSafeAskFee`** — currently fired *while the sender holds*
  `LOCK2(cs_main, cs_wallet)`, blocking a wallet thread on a modal. Eliminated: the fee
  decision moves ahead of transaction commitment (precompute and confirm before locks),
  not ported as a blocking IPC call.
- **`ThreadSafeAskQuestion`** — zero emit sites in the tree; deleted rather than ported.
- **`ThreadSafeMessageBox` (MODAL)** — becomes async-with-acknowledgement; the node
  never blocks on GUI dismissal in the split build.
- **`Translate`** — monolith-only (§4.1); removed from the interface surface.
- Modal unlock prompts invoked while holding `cs_main` (e.g. the vote-submission path)
  are reordered so user interaction happens outside core locks.

## 5. Interface catalog

Bitcoin Core parallels: `Node`, `Wallet`, `Handler`, `Init`. Core's `Chain` interface
exists to serve an out-of-process *wallet*; since our wallet stays in-node, chain-state
queries fold into `Node` and `Chain` is not ported. `WalletLoader`/multiwallet and the
fee-estimation surface are likewise not ported.

| Interface | Serves | Notes |
|---|---|---|
| `interfaces::Init` | process bootstrap | hands out the other interfaces; replaces the GUI's init busy-wait |
| `interfaces::Node` | ClientModel, RPCConsole, BanTableModel, PeerTableModel, OptionsModel | chain tip/IBD state, peers/ban list, net byte counters, warnings/alerts, `executeRpc`, settings writes (rw-settings + immediate-effect knobs like proxy/UPnP) |
| `interfaces::Wallet` | WalletModel, AddressTableModel, dialogs | balances, send, encrypt/unlock, keys, coin control, address book |
| `interfaces::WalletTxSource` | TransactionTableModel, OverviewTxModel, DetailedTxModel | the windowed tx-table contract (§6) |
| `interfaces::Handler` | all models | RAII notification registration (the retained-`scoped_connection` pattern already in the models, formalized) |
| `interfaces::StakingStatus` | ClientModel, DiagnosticsDialog | miner status, weight, ETTS |
| `interfaces::ResearcherContext` | ResearcherModel, DiagnosticsDialog | CPID/status/magnitude/accrual, BOINC context, project whitelist snapshot (also used by voting), pool-mode detection |
| `interfaces::BeaconManager` | ResearcherModel | beacon status/expiry, advertise, renewal |
| `interfaces::Scraper` | ClientModel, ResearcherModel | convergence/scraper events and **value-snapshot** statistics (replaces the by-reference global getter) |
| `interfaces::VotingManager` | VotingModel | atomic poll-table build (owns the reorg-retry internally — the model's current direct manipulation of registry traversal flags moves behind this call), poll/vote submission, real fee query |
| `interfaces::SideStakeManager` | SideStakeTableModel, OptionsDialog | value-type entries (no raw pointers), command-style add/delete, versioned `RwSettingsUpdated` |
| `interfaces::MRC` | MRCModel | **one atomic node-side snapshot** (height, version gates, trial `CreateMRC`, output limit, queue position) plus submit. Fixes an existing race: the model's refresh path reads `pindexBest`/`mempool` from the GUI thread with lock annotations that do not survive the Qt slot boundary |
| `interfaces::PSGTPool` | PSGTPoolTableModel, PSGTPoolPage, MultisignDialog | pool snapshot, per-entry wallet relevance ("needs my signature"), sign/add/remove/broadcast commands (the dialog currently reimplements the corresponding RPC logic in-dialog) |
| `interfaces::ContractFactory` | send paths | beacon/vote/MRC/message contract construction where not already covered by the domain interfaces |

Deferred within Phase 1 (interface-shaped but not model-blocking): a `Diagnostics`
async-job surface (the diagnose library is a hybrid of core reads and its own network
probes) and the updater/snapshot-download progress surface.

## 6. The wallet transaction channel

The windowed transaction-table stack is adopted as-is as the wallet-side transport seam;
it was explicitly built for this boundary. What exists today (see the companion docs):
producer handlers decompose and status-stamp transactions *on core threads under core
locks*, push into a seqno/viewId/epoch MPSC event queue, a store worker maintains
ordered records and per-view cursors without core locks, and the GUI drains on its own
timer — the render path takes no core locks at all. The Qt-free cores (`txfilter`,
`txorder`, `cursor`, `windowcache`) already compile without Qt.

Phase 1c work items to formalize it as `interfaces::WalletTxSource`:

- **Producer hoist**: the producer halves currently living in `walletmodel.cpp`
  (`NotifyTransactionChanged`, `NotifyBlocksChangedForWallet`) become internals of the
  node-side source.
- **DTO hygiene**: `TransactionRecord`/`TransactionStatus` are scrubbed of Qt types
  (`qint64`, `QString`/`QList` signatures) so the node can compile them; a marshalability
  static assert pins this.
- **Ownership inversion**: `WalletModel` currently owns the store and queue by value;
  the source owns them behind the interface, constructed node-side.
- **View lifecycle**: add the missing `unregisterView`; views are per-connection.
- **Attach/detach lifecycle**: the source is constructed lazily when a GUI attaches and
  torn down (refcounted) on last detach — the IPC capability construct/destruct maps 1:1
  onto today's WalletModel ctor/dtor. **No node-side machinery exists when no GUI is
  attached** ("headless pays nothing"), and this is a lifecycle property, not a config
  flag.
- **Row detail**: `getRowDetail` returns a structured value DTO; HTML rendering (and all
  translation) moves GUI-side, per §4.1. The current node-side `toHTML` call is the one
  standing violation and is replaced during the migration.

## 7. Implementation plan

### Phase 1 — `interfaces::` abstraction, monolithic build

| Step | Scope |
|---|---|
| PR-0 | Vestigial-include and dead-code sweep in `src/qt` (shrinks the lint baseline; includes removing the dead `updateWeight` slot and the emitter-less `ThreadSafeAskQuestion`) |
| 1a | Scaffold `src/interfaces/{node,wallet,handler,init}.h` + in-process impls; `MakeSignalHandler` over boost::signals2; add `lint-qt-includes.sh` as a **ratchet** (no new core includes in `src/qt`; existing offenders tracked in an allowlist that only shrinks) |
| 1b | ClientModel → `interfaces::Node` (+ pilot port: BanTableModel, whose upstream analogue is the same file) |
| 1c-i | `interfaces::Wallet` (balances/send/encrypt/keys/coins/address book — independent of the tx table) |
| 1c-ii | `interfaces::WalletTxSource` (§6) |
| 1d | Gridcoin domain interfaces (§5), one PR per model family; MRC first (fixes the live race), then SideStake (raw-pointer removal), Voting, Researcher/Beacon/Scraper, PSGTPool |
| 1e | Composition root: `bitcoin.cpp` init/shutdown via `interfaces::Init` (replacing the init-complete busy-wait), `bitcoingui.cpp` reaches, direct-core dialogs (MultisignDialog → interface commands; the coin-control/consolidate-wizard twins are deduplicated before cutting the boundary through both) |
| 1f | Interface mocks + Qt test coverage; flip the include lint from ratchet to hard-fail |

The lint hard-fail (no `src/qt` file includes `main.h`, `wallet/wallet.h`, registry
headers, or reaches core globals) is the **gate to Phase 2**.

### Phase 2 — IPC layer, opt-in build

Add libmultiprocess + capnproto to depends; `.capnp` schemas mirroring the headers;
handshake per §4.2/§4.3 including the QSettings-persisted node-identity check;
build-identities dialog + soft-warn banner; umbrella launcher; integration tests
including the handshake-failure suite (wrong cookie, mismatched schema, wrong UID,
stale socket per the §4.3 recovery rules).

### Phase 3 — Hardening

Reconnect logic (GUI survives node restart; node survives GUI disconnect), IPC metrics
in the About dialog, GUI process capability drop / seccomp on Linux.

## 8. Out of scope

- **Wallet-process split** (see decision 2 — the staking signing path makes this a
  real risk with no driver).
- **Remote-attach GUI / TCP transport** — mechanical once the abstraction exists, but
  bundles TLS/auth/remote-passphrase questions that are deliberately excluded.
- **QML frontend** — orthogonal; the interface layer is toolkit-neutral.
- **Full sandboxing** (namespaces/AppContainer/app sandbox) beyond the Phase 3
  capability drop.

## 9. References

- RFC discussion: https://github.com/gridcoin-community/Gridcoin-Research/discussions/2937
- Bitcoin Core `src/interfaces/README.md` and `src/ipc/` (upstream design this adapts)
- `doc/transaction_table_windowed_model.md`, `doc/windowed-transaction-table-architecture.md`
  (the wallet transaction channel)
- `doc/gui_event_queue_design.md` (the event-queue predecessor work)
