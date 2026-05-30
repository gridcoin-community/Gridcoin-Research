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
- **No `gridcoin-cli`.** Gridcoin ships only the daemon, which answers RPC
  directly; the suite drives nodes over JSON-RPC, so the CLI path is unused.
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

| Test | Exercises |
|---|---|
| `feature_hello.py` | framework smoke test (start node, getblockchaininfo) |
| `rpc_help.py` | RPCHelpMan help-format + arity coverage (auto-discovery; #2922) |
| `feature_regtest_staking.py` | premine discovery, `generatetoaddress`, `stakelimit` |
| `p2p_version_handshake.py` | version/verack + ping/pong wire handshake |
| `p2p_block_tx_relay.py` | tx + block relay over P2P (wire serialization) |
| `wallet_basic.py` | raw-tx + `sendtoaddress` spend, balance, confirmations |
| `wallet_backup.py` | `backupwallet` + `dumpprivkey`/`importprivkey` round-trip |
| `mempool_accept.py` | `sendrawtransaction` accept + double-spend rejection |
| `rpc_net.py` | two-node `getpeerinfo`/`addnode` + block propagation |
| `feature_sidestake.py` | local sidestaking config + coinstake reward split |
| `feature_reorg.py` | two-node divergent chains + reorg on connect |

### Deferred to Phase 4B (need the synthetic beacon + RSA trust anchor, 2B.3/2B.4)

`feature_beacon_inject.py`, `feature_superblock_inject.py` (also needs
`generatesuperblock` un-stubbed), `feature_mrc.py`, and the beacon flavor of
`feature_contract_replay.py`. These require an active regtest beacon/CPID, which
investor-mode staking does not (see the staking path: `TrySignClaim` skips the
beacon signature for non-crunchers).

## Cherry-pick log (post-v0.21.2 utilities)

None. `combine_logs.py` was ported verbatim from v0.21.2 (commit `831599913d`)
with only the `TMPDIR_PREFIX` and daemon-name wording changed. Record any future
post-v0.21.2 utility pulled in here, with its source commit.
