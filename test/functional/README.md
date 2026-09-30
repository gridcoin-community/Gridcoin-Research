# Gridcoin functional tests

End-to-end tests that start real `gridcoinresearchd` nodes (in `-regtest` mode),
drive them over JSON-RPC and P2P, and assert on observable behavior. The
framework is ported from Bitcoin Core v0.21.2 (commit `831599913d`) and adapted
for Gridcoin's proof-of-stake + BOINC model. Tracking issue: #2932.

## Running the suite

### Via CMake / CTest (what CI runs)

```bash
cmake -B build -DENABLE_TESTS=ON -DENABLE_DAEMON=ON   # ENABLE_GUI optional
cmake --build build --target gridcoinresearchd -j$(nproc)
ctest --test-dir build -R functional_tests --output-on-failure
```

`-DENABLE_TESTS=ON` makes CMake generate `build/test/config.ini` (from
`test/config.ini.in`) and register two entry points:

- **`functional_tests`** — a CTest case (`ctest -R functional_tests`).
- **`check-functional`** — a convenience target (`cmake --build build --target
  check-functional`) that runs the suite with normal (non-CI) verbosity.

Both set `GRIDCOIND`/`GRIDCOINCLI` to `build/bin/gridcoinresearchd` (see "Binary
location" below) and pass `--configfile=build/test/config.ini`.

### Directly (local development / debugging)

```bash
# Point GRIDCOIND at the built daemon and pass the generated config.
GRIDCOIND=build/bin/gridcoinresearchd \
  python3 test/functional/test_runner.py --configfile=build/test/config.ini

# A single test, with full logging:
GRIDCOIND=build/bin/gridcoinresearchd \
  python3 test/functional/test_runner.py --configfile=build/test/config.ini wallet_basic.py

# Keep the datadir and tail logs on failure:
... test_runner.py --combinedlogslen=4000 ...
python3 test/functional/combine_logs.py /path/to/<test_tmpdir>
```

Useful flags: `--jobs=N`, `--tmpdirprefix=DIR`, `--nocleanup`, `--tracerpc`,
`--loglevel=DEBUG`, `--failfast`. Per-test flags: `--nocleanup`, `--pdbonfailure`.

## Environment / gotchas (Gridcoin-specific)

- **Binary location.** `gridcoinresearchd` is emitted to `build/bin/` (its
  `RUNTIME_OUTPUT_DIRECTORY`), but the ported framework defaults to looking in
  `build/src/`. The CMake `functional_tests` case sets the `GRIDCOIND`/
  `GRIDCOINCLI` overrides for you; when running `test_runner.py` by hand, export
  `GRIDCOIND=build/bin/gridcoinresearchd`. (Fixing the default path lives in the
  Phase 1 framework PR.)
- **No `gridcoin-cli`.** Gridcoin ships only the daemon; there is no separate
  `gridcoin-cli` binary. Given a non-switch argument the daemon acts as its own
  RPC client: `interface_cli.py` and `interface_cli_settings.py` drive that
  client mode through `node.cli`, `feature_stop_node_twice.py` sends one stop
  through it, and every other test uses JSON-RPC by default (`--usecli` routes
  the ones that support it through the client too).
- **No `createwallet` / multiwallet.** Gridcoin loads one default BDB wallet at
  startup. Tests override `setup_network()` to bypass the base class's regtest
  `createwallet` path; see `feature_regtest_staking.py` for the pattern.
- **Premine, not a mined cache.** The regtest genesis coinbase pays a
  deterministic premine (10 × 100,000 GRC) to `privkey=1`, planted into the
  wallet by the daemon under `IsMockableChain`. This replaces Bitcoin's
  200-block coinbase cache. Discover it with `listunspent(0)`.
- **`getbalance` reports 0 for the raw premine.** The premine coinbase is
  immature for *balance accounting* even though it is spendable/stakeable. Use
  `listunspent(0)` to source coins for raw transactions; stake a few blocks
  first if you need a positive `getbalance` (coinstake maturity is gated to 0
  under `IsMockableChain`).
- **Staking, not PoW.** There is no proof-of-work path. `generatetoaddress
  <n> <addr>` mints `n` proof-of-stake blocks via `TryMineRegtestBlock`. Use the
  **positional** form — `TestNode.generate()`'s named-arg + `maxtries` style is
  rejected by Gridcoin's RPC parser. Pass `-staking=0` to disable the background
  `ThreadStakeMiner` for deterministic height control.
- **Amounts are floats.** `AmountFromValue` rejects string-encoded amounts, so
  pass RPC amounts as Python floats (a `Decimal` serializes to a JSON string).
- **`getblock` has no raw-hex mode** — it always returns JSON. To get block
  bytes, fetch them over P2P (`getdata`); see `p2p_block_tx_relay.py`.
- **Serial execution.** `create_cache.py` is not ported (Bitcoin's cache builder
  is proof-of-work; the premine makes it unnecessary), so `test_runner.py` runs
  with `--jobs=1`. Fine for the current suite size.

## Test inventory

`test_runner.py` (`BASE_SCRIPTS` / `EXTENDED_SCRIPTS`) is the authoritative list;
this table mirrors it.

### Default suite (`BASE_SCRIPTS`)

| Test | Exercises |
|---|---|
| `feature_hello.py` | framework smoke test (start node, getblockchaininfo) |
| `feature_stop_node_without_rpc.py` | framework: `TestNode.stop_node()` falls back to SIGTERM for a node whose RPC never connected, and the daemon exits cleanly (skipped on Windows) |
| `feature_stop_node_twice.py` | framework: a repeated `TestNode.stop_node()` of a node that exited non-zero reports that exit, not a `ValueError` from the closed stderr (a killed connected node, a never-connected node stopped twice); a CLI stop that cannot connect still falls back to SIGTERM (that case skipped on Windows) |
| `feature_regtest_staking.py` | premine discovery, `generatetoaddress`, `stakelimit` get/set |
| `feature_shutdown.py` | `stop` does not hang on an in-service RPC connection whose worker is parked in a blocking read (#3123) |
| `feature_rpc_bind_v4_conflict.py` | an IPv4 RPC listener that cannot bind (127.0.0.1 already held) is logged, not swallowed, and the node still serves over `[::1]` (skipped on Windows or without a usable IPv6 loopback) |
| `p2p_version_handshake.py` | version/verack + ping/pong wire handshake |
| `p2p_version_timedrift.py` | the VERSION handshake's 480 s clock-drift gate: both edges, one second past each, and a crafted `nTime` that overflowed the old `abs()` check |
| `p2p_version_floor.py` | the VERSION handshake's protocol floor: `PROTOCOL_VERSION` and `PROTOCOL_VERSION - 1` complete the handshake, `PROTOCOL_VERSION - 2` and an old version are dropped, and each drop is checked in the node's log to come from the floor check |
| `p2p_time_votes.py` | an outbound peer's time vote is cast on connect (`AddTimeData`) and withdrawn when the connection is deleted (`RemoveTimeData`) |
| `p2p_block_tx_relay.py` | tx + block relay over P2P (wire serialization) |
| `p2p_psgt_relay.py` | PSGT pool relay: `MSG_PSGT` inv/getdata/`psgt` end to end, the connect-time push, no `MSG_PSGT` inventory to a pre-PSGT peer, repeated and garbage messages not relayed |
| `p2p_psgt_orphan.py` | a relayed PSGT whose funding tx the node lacks is held as an orphan without relay, then pooled and relayed once the funding arrives |
| `p2p_ping.py` | ping/pong keepalive over the P2P wire protocol |
| `rpc_help.py` | RPCHelpMan help-format + arity coverage (auto-discovery; #2922) |
| `rpc_signmessage.py` | `signmessage`/`verifymessage`/`validateaddress` |
| `rpc_rawtransaction.py` | `createrawtransaction`/decode/`decodescript`/sign |
| `rpc_psgt.py` | PSGT create/decode/convert/combine/process/finalize |
| `rpc_psgtpool.py` | PSGT pool RPC lifecycle across two nodes: `submitpsgt`, `listpsgtpool`, `signpsgtinpool` completing m-of-n, supersede, local-only `removepsgtfrompool`, `-psgtnotify` |
| `rpc_htlc.py` | `createhtlc` + `decodescript` of the redeem script |
| `rpc_txoutproof.py` | `gettxoutproof`/`verifytxoutproof` round trip with and without a blockhash; unknown, unconfirmed and corrupted inputs rejected |
| `rpc_blockchain.py` | `getblock*`/`getblockchaininfo`/`getdifficulty` |
| `rpc_audit_snapshot_accrual.py` | contract/smoke and liveness test of `auditsnapshotaccrual`/`auditsnapshotaccruals` and their snapshot-phase early returns; numeric parity needs a synced mainnet/testnet datadir and is not checked here |
| `rpc_netinfo.py` | `getnetworkinfo`/`getnettotals`/`getconnectioncount`/`getpeerinfo` |
| `rpc_getaddednodeinfo.py` | `getaddednodeinfo` `dns=true` returns an array of per-node objects; `dns=false` still returns the `addednode` mapping |
| `rpc_multisig.py` | `addmultisigaddress` -> `validateaddress` |
| `wallet_basic.py` | premine via `listunspent`, staked balance, a raw spend of a staked coinstake output accepted to the mempool, address validation (no `sendtoaddress`; confirmation not asserted) |
| `wallet_backup.py` | `dumpprivkey` returns a non-empty key string, and its invalid-address error (`backupwallet` and `importprivkey` not exercised) |
| `wallet_keypool.py` | `keypoolrefill`/`getnewaddress`/`dumpprivkey` |
| `wallet_listtransactions.py` | `listtransactions`/`gettransaction`/`listsinceblock` |
| `wallet_splitunspent.py` | `splitunspent` count/size/optimal modes, the per-piece fee floor, the piece cap, the `-minstakesplitvalue` floor, error paths, the `consolidateunspent` round trip |
| `interface_cli.py` | the daemon's CLI client mode via `node.cli`: string->JSON argument conversion (`RPCConvertValues`) for int/bool/array/object args and the dual-mode `logging <category>` form |
| `interface_cli_settings.py` | the daemon's RPC client reads `gridcoinsettings.json` but never writes it (same inode, same mtime); a client-only setting stored with `changesettings` (`rpcconnect`) is honoured by the client and the command line still outranks it; erasing it restores the default |
| `mempool_accept.py` | `sendrawtransaction` accept + double-spend rejection |
| `rpc_net.py` | two-node `getconnectioncount`/`getpeerinfo` over an `addnode` link + block propagation; `disconnectnode` argument validation, disconnect by address and by nodeid, no block crosses while split, reconnect |
| `p2p_disconnect_nodes_churn.py` | the framework helper `disconnect_nodes` returns when a peer arrives mid-wait (tests the helper, not daemon P2P behaviour) |
| `rpc_net_connman.py` | `addnode` add/remove and its already-added / not-added error codes, `getnodeaddresses`, `setban`/`listbanned`/`clearbanned` |
| `rpc_changesettings.py` | `changesettings` with an empty value erases the setting rather than forcing an empty string, handing it back to the command line, config file or default; a side-stake allocation changed a second time is reloaded at its new value; GUI options (`lang`, `suppressnetworkgraph`) are refused as not node settings while an erase is still accepted and a node setting the GUI reads (`showorphans`) is still stored |
| `feature_sidestake.py` | local sidestaking config + coinstake reward split |
| `feature_reorg_resurrect.py` | a mined wallet tx whose block is rolled back returns to the mempool at zero confirmations and is mined again by the next block |
| `wallet_resend_stranded.py` | a wallet tx whose resurrection after a rollback is refused (an `nLockTime` no longer reached) is stranded at -1 confirmations; a forced resend does not pool it while it is non-final, pools it at zero confirmations once it is final, and the next block mines it (#3382) |
| `feature_reorg.py` | two nodes split with `disconnectnode` build competing branches; on reconnect the shorter node reorganizes onto the longer |
| `feature_reorg_conflicted.py` | a wallet tx conflicted by a block stops being conflicted when that block is disconnected; also the refused, descendant and abandoned cases, and re-arming for announcement (`unbroadcast`) |
| `feature_blockindex_verification.py` | `LoadBlockIndex`'s startup verification pass against a block corrupted without disturbing its header: the `CheckBlock` merkle arm, the `-checklevel=5` unspent-prevout arm, and the Phase 2 coherence-recovery boundary; the tip never moves |
| `feature_no_mainnet_seeds_on_regtest.py` | regtest takes neither the DNS seed hostnames nor `pnSeed` |
| `feature_no_update_check_on_regtest.py` | regtest does not arm the GitHub release check |
| `feature_no_network_diagnose_on_regtest.py` | `walletdiagnose`'s clock (NTP) and TCP port checks report NA on regtest instead of reaching outside hosts, with no peers so that both would otherwise go to the network |

### Extended suite (opt-in via `--extended`; not run on push/PR — `cmake_functional_extended.yml` runs it weekly and on manual dispatch)

| Test | Exercises | Why extended |
|---|---|---|
| `feature_stakelimit.py` | background staker honors + resumes the `stakelimit` height ceiling | wall-clock bound (~16s/block via `STAKE_TIMESTAMP_MASK`) |
| `mining_fee_policy.py` | block inclusion ordered by fee rate, not absolute fee; the `-mintxfee` floor (default, raised, unparseable at startup) | residual stake-supply flake, about 1.3% (2 failures in 150 runs): each case parks about half its UTXOs in the mempool before staking, and `CreateCoinStake` can find no stake |
| `mining_fee_escalator.py` | the block-fill fee escalator under a raised `-blockmaxsize`: a transaction below the escalated floor is skipped and selection continues | builds and relays 83 transactions before staking, so it is slower than the default suite tolerates |

### Researcher flows (Phase 4B)

No RSA trust anchor is needed: a beacon advertised under `-forcecpid` is
activated by a `generatesuperblock` call that names its public key (see
`doc/regtest.md`). Landed so far, all in the default suite:

| Test | Exercises |
|---|---|
| `feature_generatesuperblock.py` | explicit superblock attach (the former `feature_superblock_inject.py`) |
| `feature_beacon_activation.py` | beacon advertisement -> pending -> activated by a superblock (the former `feature_beacon_inject.py`) |
| `feature_research_reward.py` | the activated CPID's own coinstake pays its accrued research subsidy; a pending beacon pays none |
| `feature_mrc.py` | two nodes: the researcher's MRC request is paid by the investor node's coinstake to the beacon address |
| `feature_contract_replay.py` | beacon contract state across `reorganize`: activation reverted, advertisement reverted and resurrected, replayed and re-activated |
| `feature_beacon_pending_proofs.py` | pending v3 beacons get their ownership proofs back after a restart and after a superblock disconnect |

## Cherry-pick log (post-v0.21.2 utilities)

None. `combine_logs.py` was ported verbatim from v0.21.2 (commit `831599913d`)
with only the `TMPDIR_PREFIX` and daemon-name wording changed. Record any future
post-v0.21.2 utility pulled in here, with its source commit.
