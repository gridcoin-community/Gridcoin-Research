# Multiprocess Interface Specification — building an alternative front end

This document is the **interface contract** for talking to a running
`gridcoinresearchd -multiprocess` node over its IPC seam, from the point of view
of someone writing an *alternative front end* — a different GUI, a TUI, or an
automation client — in place of the bundled Qt wallet.

It describes the interface **as it exists on this branch** (post the
identity-handshake work: `NodeIdentity` is `{identity_token, network}`, IPC
schema major is `2`). It is a companion to, not a replacement for:

- `doc/multiprocess_design.md` — the design of record (rules, decisions, phasing).
- `doc/multiprocess.md` — how to build, run, and stop the split binaries.

Where those docs describe *why* and *how to run*, this one describes *what a
client may call and must handle*. Every claim below traces to a symbol in
`src/interfaces/*.h`, `src/ipc/*`, or `src/ipc/capnp/*.capnp`; when something is
not yet exposed or is a known limitation, it says so explicitly (see
§10, Gaps).

---

## 1. Overview and model

The multiprocess build (RFC #2937) splits the historically monolithic wallet
into two independently-started binaries:

- **`gridcoinresearchd -multiprocess`** — the *node*. It runs the whole core
  (chain, P2P, staking, scraper, and **the wallet**), and after core init it
  listens on an AF_UNIX socket and *serves* a tree of `interfaces::` objects
  over Cap'n Proto RPC (libmultiprocess). See `src/gridcoinresearchd.cpp`.
- **`gridcoinresearch -multiprocess`** — the bundled *GUI client*. It starts no
  core; it *connects* to the node's socket and drives the served interfaces. See
  `src/qt/bitcoin.cpp`.

An alternative front end plays exactly the role of `gridcoinresearch`: it is a
**connect-only client**. Key consequences:

- **The node is never spawned by the client.** Gridcoin's `interfaces::Ipc`
  deliberately omits `spawnProcess` / `startSpawnedProcess`
  (`src/interfaces/ipc.h`): the daemon is started separately (by the user, a
  service manager, etc.), and the client only ever `connectAddress()`es. You
  start the daemon yourself and connect to it.
- **The wallet stays in the node, in every build configuration.** No key
  material crosses the boundary. Signing, key generation, and passphrase
  handling all happen node-side (see the wallet interface, §4.4, and the
  security notes, §7). The client sends *requests* and receives *value
  snapshots*.
- **Only value types cross the boundary.** Every method argument and return is a
  pointer-free, copyable, Qt-free value type. This is enforced at compile time
  by `INTERFACES_ASSERT_MARSHALABLE` (`src/interfaces/marshal.h`) on every DTO
  and, ultimately, by the Cap'n Proto schema generation.

### Responsibility split

| The node owns | The front end owns |
|---|---|
| Chain state, P2P, staking, scraper, contracts | Presentation, layout, localization/translation |
| **The wallet** and all key material; signing, encryption, unlock | Deciding *when* to unlock and prompting the user |
| All fee arithmetic, coin selection, tx construction/commit | Rendering fee/quantity summaries the node computed |
| Filtering, sorting, and windowing the transaction table | Displaying the served row slice; driving cursors |
| Validation of every address/amount/allocation on submit | Optional pre-validation for UX (never load-bearing) |
| Identity token minting; cookie authentication | Cookie reading; identity *binding* (remembering it) |

The last row matters: the node **authenticates** the client (cookie), but the
node does **not** decide whether the client should trust *this* wallet. Identity
*binding* — "is this the same wallet I connected to last time?" — is the
client's own responsibility (§2, §7).

---

## 2. Establishing a connection

The reference implementation is `ipc::ConnectToNode()`
(`src/ipc/connect.{h,cpp}`), which returns a `GuiConnection`
(the `Ipc`, the remote `Init`, and the `HandshakeResult`). A client written in
C++ against libmultiprocess can call it directly; a client in another language
reproduces its steps (§9).

### 2.1 Socket path

The node listens on:

```
<datadir>/<network>/node.sock
```

`ParseAddress()` (`src/ipc/process.cpp`) resolves the address string `"unix"`
(or `"auto"`) to `<data_dir>/node.sock`, where `<data_dir>` is already the
network-specific directory (`GetDataDir()`; testnet/regtest nest under the base
datadir). A custom path may be given as `"unix:<path>"`. The socket is created
`0600`, its parent directory forced to `0700`, and the node **fails closed** if
it cannot restrict them (`ProcessImpl::bind`).

The node caps the listener at **one simultaneous connection**
(`mp::ListenConnections<messages::Init>(..., /*max_connections=*/1)` in
`src/ipc/capnp/protocol.cpp`). An alternative front end must therefore be the
*only* client attached; a second connection will not be accepted while one is
live (see §10).

### 2.2 The cookie

Authentication is a shared-secret cookie:

- The node writes a fresh 256-bit random cookie to `<datadir>/<network>/ipc.cookie`
  (hex, `0600`, atomic temp-file + rename) **once per startup**
  (`ipc::WriteCookie`, `src/ipc/handshake.cpp`).
- The client reads it with `ipc::ReadCookie` (`src/ipc/handshake.cpp`), which
  trims trailing whitespace. **An absent cookie means the node has never run on
  this datadir** (or never with `-multiprocess`): there is no credential and
  nothing to dial — do not attempt to connect.
- The cookie is **not** removed at shutdown, so a stale cookie can outlive a
  stopped node. That is harmless: the subsequent `connect()` simply fails with
  no listener.

### 2.3 The handshake, step by step

The client-side half is `ipc::ClientHandshake()` (`src/ipc/handshake.cpp`). The
full ordered sequence a client must perform:

1. **Read the stored identity expectation** (client-side; see §2.4). The bundled
   GUI keys this by a hash of the canonical datadir in `QSettings`.
2. **Read `ipc.cookie`.** Absent ⇒ node not running; stop.
3. **`connect(node.sock)`** — obtain the remote `Init` proxy
   (`Ipc::connectAddress("unix", on_disconnect)`). Returns null if the address
   is empty/disabled or (for `"auto"`) if nothing is listening; throws on an
   unexpected socket error.
4. **`Init::authenticate(cookie)`** — the **first** IPC call. The node compares
   it constant-time (`ConstantTimeEqual`) against the cookie it wrote. A `false`
   result is a hard fail: **build and identity are never served to an
   unauthenticated peer**, and every other method throws until `authenticate()`
   has returned `true` (`ServeInit::RequireAuth`, `src/ipc/serve_init.cpp`).
5. **`Init::getBuildInfo()`** — the node's `BuildInfo` (`git_commit`,
   `built_at`, `schema_major`, `schema_minor`, `protocol_version`). Compare per
   §3.
6. **`Init::getIdentity()`** — the node's `NodeIdentity` (`identity_token`,
   `network`). Steps 5 and 6 are fetched in **one guarded round-trip**.
7. **Compatibility checks** (all inside one `try`; see §2.5).
8. **`Init::makeNode()` / `makeWallet()` / …** — proceed to the interface tree.

Only after a successful handshake may the client call the `makeX` factories.
The node additionally polls `Init::isCoreReady()` readiness: `authenticate()`
succeeds as soon as the listener is up, and `makeWallet()` / `makeMRC()` / etc.
return null until wallet startup completes — a client should tolerate a null
factory result and retry, exactly as the bundled GUI loops on `isCoreReady()`
(`src/qt/bitcoin.cpp`).

### 2.4 Identity binding (client-side)

`getIdentity()` returns `identity_token = HEX(SHA256(domain-tag ‖ LE32(len) ‖
wallet_uuid))` (`ipc::ComputeIdentityToken`), where `wallet_uuid` is a random
16-byte tag minted once into `wallet.dat` (`CWallet::GetWalletUuid`,
`src/wallet/wallet.h`). The token fingerprints *which wallet* the node serves; it
is deliberately independent of chain state and datadir path (a resync does not
change it; replacing the wallet does). The raw UUID never crosses the wire — only
the hash.

The node does not judge the token; the *client* decides whether to trust it. The
pure comparison helper is `ipc::CheckIdentityBinding(reported, stored)`, which
returns a `BindOutcome`:

| `BindOutcome` | Meaning | Client action (reference: `ResolveNodeIdentity`) |
|---|---|---|
| `FirstSeen` | Nothing stored, token present | Persist it, proceed |
| `Match` | Stored == reported | Proceed |
| `Mismatch` | Stored != reported | Prompt "trust / quit", or rebind under a `-autotrustidentity`-style flag |
| `UnavailableFresh` | Reported empty, nothing stored | Proceed unbound (log once) |
| `UnavailableStored` | Reported empty **but** a token was stored | Treat as a **mismatch** (downgrade signal), never a silent skip |

An **empty token = "unavailable"** (mockable/regtest chain, or the UUID could
not be minted). The `UnavailableStored` case is the important adversarial edge:
if the client once bound a token and the node now reports empty, treat it as a
change — do not silently drop the guard.

### 2.5 Failure modes a client must handle

`ClientHandshake` classifies these; a client MUST handle each:

| Condition | Detected by | Severity |
|---|---|---|
| Wrong / stale cookie | `authenticate()` returns false | **Hard fail** — "node may have restarted; reconnect" |
| `schema_major` mismatch | `remote.schema_major != local.schema_major` | **Hard fail** — incompatible wire format |
| `protocol_version` mismatch | `remote.protocol_version != local.protocol_version` | **Hard fail** |
| Client `schema_minor` **>** node's | `local.schema_minor > remote.schema_minor` | **Hard fail** — "update the node" |
| `network` mismatch | `ident.network != local_network` | **Hard fail** — wrong chain |
| Node `schema_minor` **>** client's | (soft) | `SoftWarn::GuiOlderMinor` — forward-compatible, log only |
| `git_commit` mismatch | (soft) | `SoftWarn::GitCommitMismatch` — mixed-build banner |
| Identity token change | client-side `CheckIdentityBinding` | prompt / rebind / quit |
| **Daemon vanished mid-handshake** | any IPC call throws | **Hard fail** — `ClientHandshake` catches `std::exception`, clears soft findings, returns `ok=false` with "lost connection …" |

That last row is the critical robustness point: **any IPC call can throw** if the
daemon disconnects mid-flight. `ClientHandshake` wraps steps 4–7 in one
`try/catch` so a throw becomes a clean `ok=false` rather than an escaping
exception. A client must apply the same discipline to *every* later call, not
just the handshake (§6).

> **Doc/code divergence (see §10):** design §4.3 lists an additional step 4,
> a `SO_PEERCRED` / `LOCAL_PEERCRED` peer-identity check, before
> `authenticate()`. That check is **not implemented** on this branch —
> neither `ConnectToNode` nor the node performs it. The cookie is the sole
> authenticator today.

---

## 3. Versioning contract

Three constants are embedded in **both** binaries and compared during the
handshake (`ipc/handshake.h`):

```cpp
constexpr uint32_t IPC_SCHEMA_MAJOR   = 2;
constexpr uint32_t IPC_SCHEMA_MINOR   = 0;
constexpr uint32_t IPC_PROTOCOL_VERSION = 1;
```

Rules a client must honor (from `ClientHandshake` and design §4.2):

- **`schema_major`** — the Cap'n Proto wire contract. Must match exactly; any
  difference is a hard fail. **Major 2** reflects the identity model being
  re-based onto the wallet UUID (the former `node_id` / `datadir` /
  `genesis_hash` fields were retired — a breaking change). A breaking schema
  change bumps this and requires a one-version transition release.
- **`schema_minor`** — additive schema changes. Asymmetric:
  *client minor > node minor* is a **hard fail** ("update the node"); *client
  minor < node minor* is a **soft, forward-compatible** finding (the node has
  features the client does not use).
- **`protocol_version`** — the transport/handshake shape. Must match exactly.
- **`git_commit`** / **`built_at`** — not compatibility gates; a `git_commit`
  mismatch is a soft mixed-build warning, `built_at` is informational.

**Identity-token domain tag:** `ipc::IDENTITY_TOKEN_DOMAIN =
"gridcoin-node-identity-v1"` (`ipc/handshake.h`). It is hashed in with its
trailing NUL excluded and the UUID length-prefixed (LE32) so the tag/UUID
boundary is unambiguous. Bump the `-vN` suffix only if the token *algorithm*
changes, independent of the capnp schema version.

---

## 4. The interface surface

`Init` is the root capability; every other interface is reached through one of
its `makeX` factories. All interfaces below are pure-virtual C++ classes in
`src/interfaces/`, mirrored 1:1 by a Cap'n Proto schema in `src/ipc/capnp/`
(pure-abstract classes must be fully schema'd, or the generated `ProxyClient`
stays abstract). DTOs are documented at their definition sites — this section is
a map, not a field-by-field restatement.

### 4.0 Init — `src/interfaces/init.h`, `ipc/capnp/init.capnp`

The per-process bootstrap. Beyond the handshake methods (`authenticate`,
`getBuildInfo`, `getIdentity`, §2) and `isCoreReady()`, it hands out:

| Factory | Returns | Notes |
|---|---|---|
| `makeNode()` | `Node` | always available |
| `makeStakingStatus()` | `StakingStatus` | always available |
| `makeWallet()` | `Wallet` | null before wallet startup |
| `makeWalletTxSource()` | `shared_ptr<WalletTxSource>` | null before wallet startup |
| `makeMRC()` | `MRC` | null before wallet startup |
| `makeVotingManager()` | `VotingManager` | |
| `makeResearcherContext()` | `ResearcherContext` | null before wallet startup |
| `makePSGTPoolContext()` | `PSGTPoolContext` | null before wallet startup |
| `makeSideStakeManager()` | `SideStakeManager` | **monolith only — not in `init.capnp`; see §10** |

The `init.capnp` `Init` interface additionally declares `construct @0
(threadMap …)` — the libmultiprocess lifecycle entry point (a `ThreadMap`
exchange), not a Gridcoin method (§9).

### 4.1 Node — `src/interfaces/node.h`, `node.capnp`

Chain/network state plus node-side notification registration. Query methods
include `getNodeCount`, `getNumBlocks`, `getBestBlockHash` (returns `uint256`),
`getLastBlockTime`, `getDifficulty`, `isInitialBlockDownload`,
`isOutOfSyncByAge`, `getWarnings`, `getClientVersion`, `isTestNet`, and the
byte counters. Note the `try*` variants (`tryGetNumBlocksOfPeers`) that return
`std::nullopt` instead of blocking on `cs_main` — provided for callers that must
never wait on a core lock.

Command/action methods: `startShutdown`, `banNode`, `unban`, `disconnectNode`,
`executeRpcConsoleCommand(method, args) -> RpcConsoleResult` (the whole UniValue
round trip happens node-side; only formatted text crosses), `listRpcCommands`,
the typed settings getters (`getSettingBool/Int/Str`, `isSettingSet`) and
`changeSettings(...) -> SettingChangeResult`, plus `checkForLatestUpdate` and
`runDiagnostics`.

DTOs: `BannedNode`, `PeerInfo`, `RpcConsoleResult`, `SettingChangeResult`,
`ScraperConvergenceSnapshot`, `LatestVersionInfo`, `DiagnosticResult` (+ the
`DiagnosticTest` / `DiagnosticStatus` enums whose integer values mirror the core
enums).

Notification bridges (each returns a `Handler`, §5): `handleRwSettingsUpdated`,
`handleInitShutdown` (the **core→client shutdown** signal — a client must react
by quitting its own process), `handleNotifyBlocksChanged`,
`handleNotifyNumConnectionsChanged`, `handleBannedListChanged`,
`handleNotifyAlertChanged`, `handleMinerStatusChanged`, `handlePSGTPoolChanged`,
`handleNotifyScraperEvent`.

`executeRpcConsoleCommand` is worth calling out for an automation client: it is
the full RPC dispatch surface behind one method, so a headless client can drive
essentially any RPC through `Node` without a separate HTTP-RPC port.

### 4.2 StakingStatus — `src/interfaces/staking.h`, `staking.capnp`

Queries only (status *changes* arrive via `Node::handleMinerStatusChanged`):
`isStaking`, `getErrors`, `getCoinWeight`, `getNetworkWeight` (blocking) plus
non-blocking `tryGetNetworkWeight` / `tryGetEstimatedTimeToStake`.

### 4.3 Wallet — `src/interfaces/wallet.h`, `wallet.capnp`

The node's single wallet. Balances (`getBalance`, `getStake`,
`getUnconfirmedBalance`, `getImmatureBalance`, and the atomic
`tryGetBalances(WalletBalances&)` that bows out without blocking); lock/encryption
state (`getLockState -> WalletLockState`, `isUnlockedForStakingOnly`,
`getUnlockStakingOnlyFlag`); mutation of lock state (`encryptWallet`,
`lockWallet`, `unlockWallet`, `changeWalletPassphrase` — all taking a
`SecureString` passphrase, §7).

Coin control / send: `getOutputs`, `listCoins`, `computeCoinControlSummary(...)
-> CoinControlSummary` (all fee math runs node-side), `getMaxConsolidationInputs`,
and `sendCoins(recipients, coin_control, accepted_fee) -> SendCoinsResult`. Note
the fee-confirmation protocol: `sendCoins` returns
`SendCoinsStatus::FeeConfirmationRequired` with the fee to confirm and commits
**nothing**; the client re-invokes with `accepted_fee >= fee`. This replaces the
old modal "ask fee" dialog that blocked inside core locks (design §4.5) — a
client MUST implement the re-invoke, not assume a single call commits.

Addresses / messages: `getAddresses`, `getAddressLabel`, `isMine`,
`setAddressBook`, `delAddressBook`, `getNewReceiveAddress`,
`getNewReceiveAddressWithLabel`, `getUnbookedReceiveAddresses`, `getPubKey` /
`getKeyFromPool` (**public keys only**), and `signMessage` / `verifyMessage`
(private key never leaves the node).

DTOs: `WalletBalances`, `WalletLockState`, `WalletOutput`, `WalletAddress`,
`WalletCoinControl` (the client-owned coin-selection container, keyed by
`COutPoint`), `CoinControlSummary`, `WalletSendRecipient`, `SendCoinsResult`
(+ `SendCoinsStatus`, `MessageSignStatus`, `MessageVerifyStatus` enums).

Bridges: `handleStatusChanged`, `handleAddressBookChanged`,
`handleTransactionChanged`.

### 4.4 WalletTxSource — `src/interfaces/wallet_tx_source.h`, `wallet_tx_source.capnp`

The windowed transaction-table channel — see §5.2 for the full model.

### 4.5 MRC — `src/interfaces/mrc.h`, `mrc.capnp`

Manual Research Claims. `snapshot(fee_boost, wallet_locked, researcher_eligible)
-> MRCSnapshot` (one atomic node-side read of eligibility + fee-queue state);
`submit(fee_boost, wallet_locked) -> MRCSubmitResult`; `isOutOfSync()`;
`handleMRCChanged`.

### 4.6 VotingManager — `src/interfaces/voting.h`, `voting.capnp`

Polls and votes. `buildPollTable(filter_flags) -> vector<PollTableItem>`
(tallied node-side against a pinned tip — call it off any UI thread),
`getPollTypes`, `pollV3Enabled`, `estimatePollFee`, `currentPollTitle`,
`latestActivePollTime`, `submitPoll(PollSubmission) -> VotingSubmitResult`,
`submitVote(poll_txid, choice_offsets) -> VotingSubmitResult`;
`handleNewPollReceived`, `handleNewVoteReceived`. The wallet must already be
unlocked by the caller for the submit commands. DTOs carry precomputed display
values (types/weights already string-ified and scaled to whole GRC), so no core
voting types cross.

### 4.7 ResearcherContext — `src/interfaces/researcher.h`, `researcher.capnp`

Beacon / magnitude / accrual. `snapshot() -> ResearcherSnapshot` (blocking) and
`trySnapshot()` (non-blocking); `outOfSync`; the fused `projects(extended) ->
vector<ResearcherProjectRow>` (whitelist + local projects + scraper magnitude in
one node-side pass); `whitelistProjects`, `v3CapableProjects`, `hasV3CapableProjects`,
`maxProjectNameLength` / `maxProjectUrlLength`; commands `switchMode`,
`advertiseBeacon`, `generateBeaconKeyForV3`, `advertiseBeaconV3`, `reload`; and
the `handleResearcherChanged` / `handleBeaconChanged` / `handleAccrualChanged` /
`handleBlocksChanged` bridges. `BeaconStatus` / `ResearcherMode` enums classify
state node-side.

### 4.8 PSGTPoolContext — `src/interfaces/psgt.h`, `psgt.capnp`

Partially-Signed Gridcoin Transactions: the pool table + multisig workbench. PSGTs
cross as `PSGTBytes` (the core `SerializePSGT()` byte format), never a live core
type. Pool: `entries`, `signPoolEntry`, `removePoolEntry`, `poolStatus`,
`poolEntryPsgt`. Workbench: `describePSGT`, `signPSGT`, `combinePSGTs`,
`submitPSGTToPool`, `finalizeToRawTxHex`, `decodePSGT`, `walletHasSignature`,
`walletMustSignRevision`. Rich result DTOs (`PSGTDescription`, `PSGTSignResult`,
`PSGTSubmitResult`, …) plus the several status enums mirror the core results.
Pool *change* notifications arrive via `Node::handlePSGTPoolChanged`.

### 4.9 SideStakeManager — `src/interfaces/sidestake.h` (no capnp schema)

The unified mandatory + local sidestake table with add/edit/delete commands and a
versioned-refetch revision (`SideStakeSnapshot::local_revision`, design §4.4).
`entries`, `localRevision`, `addLocal`, `setAllocation`, `setDescription`,
`deleteLocal`, `handleRwSettingsUpdated`, `handleMandatorySideStakeChanged`.

**Not served over the IPC wire on this branch.** There is no `sidestake.capnp`, and
`init.capnp`'s `Init` has no `makeSideStakeManager` method. The bundled MP GUI's
sidestake table is nonetheless populated because `OptionsModel` (and its
`SideStakeTableModel`) is still constructed from a **local, in-GUI-process**
`SideStakeManager` — `bitcoin.cpp` mints it via `gui_init = MakeGridcoinInit();
gui_init->makeSideStakeManager()` — a deliberately un-migrated Phase-2 piece (the
`bitcoin.cpp` comment: *"Phase 2 will hand this out from the single process Init
instead of a locally-minted one"*). That local manager reads the **GUI process's own**
`SideStakeRegistry`, which reflects the settings-based *local* sidestakes in the shared
datadir but **not** contract-derived *mandatory* sidestakes or any registry state that
requires core block sync. So an alternative front end cannot obtain the node's full
sidestake state through `interfaces::` today; use `Node::executeRpcConsoleCommand` (the
sidestake RPCs) for the authoritative node-side view. **A follow-up migrates the
core-state-owned parts of `OptionsModel` (sidestake included) onto the remote node Init
over IPC** — see §10, gap 1.

---

## 5. Notifications and the transaction model

### 5.1 The Handler subscription pattern

Every `handleXxx(...)` method registers a callback and returns a
`std::unique_ptr<interfaces::Handler>` (`src/interfaces/handler.h`,
`handler.capnp`). The `Handler` is an RAII subscription: `disconnect()` cancels
it, and destroying it also disconnects. A client keeps the `Handler` alive for as
long as it wants the callback, and drops it to unsubscribe.

Over IPC the callback is itself a capability: `node.capnp` wraps each
`std::function` signature as a `ProxyCallback<…>` interface (e.g.
`VoidCallback`, `NotifyBlocksChangedCallback`), so the node calls *back into the
client* over the same connection. Two rules bind a client's callback code
(`src/interfaces/README.md`, `node.h`):

- **Callbacks fire on core threads, often while core locks are held.** A callback
  must **enqueue and return** — it must not take core locks or re-enter interface
  methods that do. In practice: post the event to your own thread/loop and return
  immediately.
- **Payload-free "refetch trigger" signals.** Many notifications
  (`ResearcherChanged`, `BeaconChanged`, `MRCChanged`, `RwSettingsUpdated`,
  `BannedListChanged`, …) carry no data; the contract is that you re-call the
  corresponding `snapshot()` / `entries()` / query on your own thread. Do not try
  to reconstruct state from the notification itself.

The node also guards *its* side: `interfaces::GuardNotify` (`handler.h`) swallows
the "IPC client method called after disconnect" exception that libmultiprocess
raises when it tries to deliver a notification to a client that has gone away, so
a departed client cannot unwind a core thread. A client should expect that its
in-flight notifications are simply dropped after it disconnects.

The `Node::handleInitShutdown` callback is special: it is the **core→client
shutdown** signal (RPC `stop`, SIGTERM, low-disk abort, …). A client must treat
it as "the node is going down" and quit its own process — otherwise its next
proxy call will throw against a dead connection.

### 5.2 The windowed transaction channel

`WalletTxSource` (`wallet_tx_source.h`, and its DTOs in `wallet_tx_channel.h` /
`wallet_tx_record.h` / `wallet_tx_filter.h`) lets a front end show a transaction
list **without ever pulling the whole wallet across the boundary**. The node owns
the ordered record store, a worker thread, and the producer subscriptions; the
client drives per-view *cursors* and pulls a bounded event stream. This is the
one interface returned by `shared_ptr` (a producer callback in flight keeps the
source alive across teardown — see §6).

Lifecycle and usage:

1. **Attach.** `Init::makeWalletTxSource()` constructs the node-side store and
   worker — "headless pays nothing": a client that never calls this incurs no
   cost. Releasing the last reference detaches (unsubscribes, joins the worker).
2. **`prime(limit_enabled, limit_time)`** — scan the wallet, (re)build the ordered
   record table and every registered cursor, and push each a `Reset`. Returns
   nothing; the list is never shipped whole. `limit_*` is the datetime-display
   cutoff; call again when it changes. (This replaced the old full-wallet
   snapshot; do not expect a "give me everything" call.)
3. **Register views.** `registerView(view_id, FilterSpec, sort_column, sort_order)`
   creates a server-side cursor (filter + sort). `view_id` is one of
   `GRC::VIEW_FULL` (0), `VIEW_OVERVIEW` (1), `VIEW_DETAILED` (2). Adjust with
   `setViewSort` / `setViewFilter` / `setViewLimit`; drop with `unregisterView`
   (idempotent, but must be called **while the source is still alive** — teardown
   order matters, §6).
4. **Fetch rows.** `getRows(view_id, first, count) -> GRC::RowsResult` returns the
   `[first, first+count)` slice **plus** the view's `total_accepted`, `epoch`, and
   `high_water`, all sampled under one store-lock hold. `getAllRows` returns every
   accepted row (for export). `rowForKey` / `getRowDetail` resolve a single
   transaction; `getRowDetail -> GRC::WalletTxDetail` is the fully-structured,
   translation-free detail for a double-click dialog.
5. **Drain events.** `drainEvents(max_batch) -> vector<GRC::WalletEvent>` pulls
   pending events in `seqno` order (a client polls this periodically). Payloads:
   `RowsInsertedPayload`, `RowsRemovedPayload`, `RowsResetPayload`,
   `RowCountChangedPayload`, `RowsChangedPayload`, `ChainTipChangedPayload` (a
   `std::variant`, marshalled as a discriminated `WalletEventPayloadWire` struct).
6. **Address-book feedback.** `noteAddressBookChanged(address, label)` tells the
   source a label changed so it re-snapshots the affected rows' sort/filter key.

**Race-free reconciliation.** Each event and result carries `viewId` + `epoch` +
`seqno`/`high_water`. A cursor rebuild bumps `epoch`; a content fetch is only
adopted when its `epoch` and `high_water` match the structural (event-stream)
channel. The commentary in `wallet_tx_channel.h` (`RowsResult`) is the normative
description — a client that windows the list must implement this two-channel
reconciliation or it will drop/double-count rows during concurrent updates.

`FilterSpec` (`wallet_tx_filter.h`) is a plain value type: date range, a
`type_mask` bitfield of `(1u << TransactionRecord::Type)`, an `address_substr`
matched case-insensitively against address *or* label, `min_amount`,
`limit_rows`, and the `show_inactive` / `show_orphans` gates. `TransactionRecord`
/ `TransactionStatus` (`wallet_tx_record.h`) are the row DTOs; the *translated*
type/status rendering is the client's job (only locale-free data crosses).

---

## 6. Lifecycle and threading

- **Proxy lifetime is bounded by the `Ipc`.** The remote `Init` and every
  interface reached through it are proxies valid **only while the `Ipc` (and its
  background event loop) lives** (`GuiConnection` keeps `ipc`, `init`, and the
  `HandshakeResult` together for exactly this reason — `connect.h`). Keep the
  connection object alive for as long as you use *any* interface obtained from
  it. Destroy children before the connection.
- **Factory objects own node-side machinery.** A `WalletTxSource` owns a store +
  worker thread + subscriptions; other `makeX` results own their node-side
  wrappers. Releasing the client-side handle tears the node-side object down. The
  bundled GUI constructs these *before* its readiness wait and destroys them
  *before* the node shuts the wallet down (`src/qt/bitcoin.cpp`); an alt client
  must respect the same ordering — e.g. `unregisterView` before releasing the
  source, and release wallet-derived interfaces before the connection.
- **`WalletTxSource` is `shared_ptr` on purpose.** Its producer subscriptions
  hold a `weak_ptr` and lock it per callback, so a core producer thread executing
  a callback at the moment the owner releases keeps the source alive until that
  callback returns. This closes a cross-thread teardown race; the single client
  owner still governs lifetime.
- **Disconnect can surface at any call.** If the daemon exits (crash, `SIGKILL`,
  or a `stop` that races the shutdown handshake), the next proxy call throws.
  There are two client-side hooks:
  - The **`on_disconnect` callback** passed to `connectAddress` /
    `ConnectToNode`, invoked *on the IPC event-loop thread* when the connection
    drops unexpectedly. The bundled GUI routes it to a graceful quit
    (`QuitOnDaemonConnectionLost`, `src/qt/bitcoin.cpp`).
  - A **top-level exception guard** around your event loop: the GUI wraps its Qt
    event dispatch (`GridcoinApplication::notify`) to treat the "called after
    disconnect" exception as "node went away" rather than letting it escape (Qt
    forbids exceptions crossing the event loop). A non-Qt client must likewise
    never let an IPC throw unwind its main loop.
- **Reentrancy / locks.** Notification callbacks must not take core locks or
  re-enter locking interface methods (§5.1). On the request side, prefer the
  `try*` variants for anything on a latency-sensitive thread — the blocking
  variants take `cs_main` and can stall.

---

## 7. Security model for clients

- **The cookie is the authenticator.** It is 256 bits of strong randomness,
  written `0600`, fresh per node startup, and compared constant-time
  (`ConstantTimeEqual`). Possession of the cookie *is* authorization: anything
  that can read `<datadir>/<network>/ipc.cookie` can drive the wallet. Protect
  it exactly as you would `wallet.dat`.
- **Transport is local AF_UNIX only.** The socket is `0600` inside a `0700`
  directory, and the node fails closed if it cannot enforce that (POSIX
  `chmod`; on Windows the socket and cookie inherit the owner-only datadir ACL).
  There is no network transport and no TLS — the trust boundary is the local
  filesystem's access control.
- **Identity binding is *your* responsibility.** The node authenticates you but
  does not stop you from attaching to a *different wallet* than you expect. If
  your front end shows balances, you should persist the `identity_token` per
  datadir and re-check it on every connect (§2.4), or you risk silently
  displaying the wrong wallet after a wallet swap/restore. `CheckIdentityBinding`
  gives you the decision; the persistence and prompt are yours.
- **Passphrases cross the wire.** `encryptWallet` / `unlockWallet` /
  `changeWalletPassphrase` take a `SecureString`, which travels over the socket
  to the node (the wallet lives there). It is zeroed on both sides after the call
  and unlock is time-bounded, but a client author should be aware the secret
  transits the AF_UNIX socket. Keys and signing never cross — only the
  passphrase, and only for these calls.
- **What the node does *not* protect against.** It does not verify *which*
  process holds the cookie (the design's optional `SO_PEERCRED` peer-credential
  check is not implemented on this branch, §10). It caps concurrency at one
  connection but its auth flag is process-global and *sticky* — once any peer
  authenticates, the flag stays set for the life of the served `Init`
  (`ServeInit`); per-connection auth is a later hardening. And identity binding,
  as above, is not enforced node-side at all.

---

## 8. A minimal client walkthrough

The smallest sequence to connect, authenticate, and read the balance, using the
real symbols (C++/libmultiprocess client):

```cpp
// 0. You need an Init to hand MakeIpc even as a connect-only client: it is the
//    Init this process *would* serve if it listened (the GUI never does). The
//    in-process monolith Init is cheap. It must outlive the connection.
std::unique_ptr<interfaces::Init> local_init = interfaces::MakeGridcoinInit();

// 1-3, 4-7. ConnectToNode does the whole handshake: read cookie, connect,
//    authenticate(cookie), getBuildInfo()/getIdentity(), compatibility checks.
std::string err;
std::optional<ipc::GuiConnection> conn =
    ipc::ConnectToNode(GetDataDir(), *local_init, err,
        /*on_disconnect=*/[]{ /* node went away: quit your loop */ });
if (!conn) { /* err explains the hard fail */ return; }

// 2.4. Identity binding is yours: compare conn->handshake.remote_ident against
//    what you stored for this datadir.
switch (ipc::CheckIdentityBinding(conn->handshake.remote_ident.identity_token,
                                  stored_token)) { /* Match / FirstSeen / ... */ }

// 8. Wait for the wallet, then read a value snapshot.
while (!conn->init->isCoreReady()) { /* sleep briefly */ }
std::unique_ptr<interfaces::Wallet> wallet = conn->init->makeWallet();
if (!wallet) { /* wallet not up yet; retry */ }

interfaces::WalletBalances b;
if (wallet->tryGetBalances(b)) {
    // b.balance / b.stake / b.unconfirmed_balance / b.immature_balance
}
// Keep `conn` alive while you use `wallet` (proxies die with the Ipc).
```

`src/qt/bitcoin.cpp` (search `ConnectToNode`, `ResolveNodeIdentity`,
`isCoreReady`, `makeWallet`) is the complete, production reference for this flow.

---

## 9. Non-C++ / non-libmultiprocess clients

Nothing about the wire is C++-specific — it is Cap'n Proto RPC over an AF_UNIX
stream — but the framing is **libmultiprocess's**, not vanilla Cap'n Proto, so a
foreign client is a real undertaking:

- **The schemas** are in `src/ipc/capnp/*.capnp`. Each interface is declared with
  `$Proxy.wrap("interfaces::X")`, and struct fields carry `$Proxy.name("…")`
  annotations that map camelCase wire names to the C++ snake_case members (e.g.
  `gitCommit @0 :Text $Proxy.name("git_commit")` in `init.capnp`). A foreign
  client can ignore the C++ names and speak the wire field ordinals directly, but
  the `.capnp` files are the source of truth for the message shapes.
- **`mpgen`** (built from the vendored `src/ipc/libmultiprocess` subtree, wired
  through `target_capnp_sources` in `src/ipc/CMakeLists.txt`) is what turns each
  `.capnp` into the `ProxyClient`/`ProxyServer` glue for the C++ build. A foreign
  client does not use `mpgen`; it must reproduce libmultiprocess's **runtime
  conventions** by hand:
  - **The `construct @0 (threadMap …)` / `Proxy.Context` handshake.** Every method
    takes a `context :Proxy.Context`, and `Init.construct` exchanges a
    `ThreadMap`. This is libmultiprocess's threading model (request/response
    thread affinity), layered *on top of* Cap'n Proto — a foreign client must
    implement it, not just the application methods.
  - **Capability passing.** `makeX` returns a Cap'n Proto *interface capability*,
    and `handleXxx` passes a client-hosted `ProxyCallback<…>` capability back to
    the node. A foreign client must host these callback capabilities to receive
    notifications.
  - **`std::variant` framing.** Discriminated unions like
    `WalletEventPayloadWire` (a `which` tag plus one field per alternative, in the
    C++ variant's order — see `wallet_tx_source.capnp` and
    `ipc/capnp/type-variant.h`) are a libmultiprocess convention, not a Cap'n
    Proto union.
- **Practical recommendation.** Because of the ThreadMap/Context layer, the
  realistic path for a foreign-language front end today is to **link
  libmultiprocess** (it is C++), or to talk to the node through
  `Node::executeRpcConsoleCommand` after a minimal C++ shim — rather than to
  reimplement the libmultiprocess framing from scratch. Treat a pure-foreign
  Cap'n Proto client as a research effort, not a supported path, on this branch.

---

## 10. Gaps and doc/code divergences an alt-front-end author will hit

Reported honestly so nobody designs against a contract that is not there yet:

1. **`SideStakeManager` is not on the IPC wire, and the MP GUI's sidestake table
   runs on a local fallback.** `makeSideStakeManager` is a full member of the C++
   `Init` interface and is wrapped by `ServeInit`, but there is **no
   `sidestake.capnp`** and **no `makeSideStakeManager` in `init.capnp`**, so it is
   not served over IPC. The bundled MP GUI still shows a sidestake table because
   `OptionsModel` is constructed from a **local, in-GUI-process** `SideStakeManager`
   (`bitcoin.cpp`: `gui_init = MakeGridcoinInit()`), reading the GUI process's own
   registry — settings-based local sidestakes only, **not** contract-derived
   mandatory sidestakes or core-synced state. This is a real gap, not just a missing
   schema: the whole of `OptionsModel` that reads/writes *core-owned* state (the
   sidestake registry, and any node rw-settings) currently runs against GUI-local
   globals rather than the node. **A follow-up migrates the core-state-owned parts
   of `OptionsModel` onto the remote node Init over IPC** (which requires adding
   `sidestake.capnp` + `makeSideStakeManager @13` to `init.capnp`, and restructuring
   GUI startup so `OptionsModel` is wired from the remote Init). Until then, use
   `Node::executeRpcConsoleCommand` (the sidestake RPCs) for the authoritative
   node-side view.
2. **The peer-identity check is documented but unimplemented.** Design §4.3
   step 4 (`SO_PEERCRED` / `LOCAL_PEERCRED` / `SIO_AF_UNIX_GETPEERPID`) is called
   "best-effort defense-in-depth," but neither `ConnectToNode`/`ClientHandshake`
   nor the node performs it. The cookie is the only authenticator. Do not assume
   the node will reject a wrong-PID peer.
3. **Auth is process-global and sticky, and capped at one connection.**
   `ServeInit::m_authenticated` is set once and never cleared, shared across the
   (single allowed) connection; the listener is `max_connections=1`. Per-connection
   auth and multi-client support are explicitly deferred (`serve_init.cpp`,
   `protocol.cpp`). An alt front end must be the sole client and cannot rely on
   any multi-client isolation.
4. **Versioned-refetch is only partly realized.** Design §4.4 describes every
   void signal growing a `registry_version` parameter. In practice only
   `SideStakeManager` carries a revision (`local_revision`) — and it is not even
   IPC-exposed (gap 1). All other notifications are payload-free refetch triggers
   with no coalescing version on the wire; a client must refetch on every one and
   cannot dedupe by version except for sidestake.
5. **The mixed-build warning surfaces as a dismissible banner.**
   `SoftWarn::GitCommitMismatch` is both logged (`ResolveNodeIdentity`) and shown
   as a dismissible in-window banner (`BitcoinGUI::showBuildMismatchWarning`) in
   the reference GUI, suppressible per instance with `-nobuildwarn`. A client gets
   the soft finding in `HandshakeResult::soft` and decides how to present it.
6. **Identity-token algorithm precision.** `init.h` and design §4.2 describe the
   token as `SHA256(domain-tag ‖ wallet_uuid)`; the actual implementation
   (`ComputeIdentityToken`) is `HEX(SHA256(domain-tag_without_NUL ‖ LE32(len) ‖
   uuid))`. A client only ever *compares* the string the node reports, so the
   shorthand is harmless in practice — but the length-prefixed form is the real
   algorithm if you ever need to reproduce it.
7. **Interfaces grow per migration.** The surface is intentionally only as wide
   as the bundled GUI's migrated consumers need (see each header's phase notes and
   `src/interfaces/README.md`). Methods are added when a consumer needs them, not
   speculatively — so absence of a capability is expected, and
   `Node::executeRpcConsoleCommand` is the general-purpose escape hatch.
