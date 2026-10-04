# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Gridcoin is a Bitcoin-derived proof-of-stake cryptocurrency that rewards users for contributing computational power to scientific research through BOINC. The codebase is C++17 with a Qt GUI, using CMake as the primary build system.

## Build Commands

### Development Build (Linux Native)

```bash
# Configure (from repo root)
cmake -B build -DENABLE_GUI=ON -DENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build
cmake --build build -j $(nproc)

# Executables: build/bin/gridcoinresearchd, build/bin/gridcoinresearch
```

On Windows, develop inside WSL with the clone in the WSL filesystem, not `/mnt/c`
(see [doc/build-windows-wsl.md](doc/build-windows-wsl.md#1-get-the-source-code)); then
run the Linux-native commands above unchanged. For Qt GUI iteration there is a tracked
`gridcoin-gui-wsl` skill under `.claude/skills/` (GUI-only; it builds with
`-DENABLE_TESTS=OFF`).

### Automated Build Script

```bash
./build_targets.sh TARGET=native BUILD_TYPE=RelWithDebInfo
# Other targets: depends (Linux static), win64 (Windows cross-compile)
```

### Tests

The `build_targets.sh` script runs the test suite automatically after building.

```bash
# Run all tests
ctest --test-dir build
# Or directly:
./build/src/test/test_gridcoin

# Run a specific test suite
./build/src/test/test_gridcoin --run_test=beacon_tests

# Run with verbose output
./build/src/test/test_gridcoin --log_level=all
```

### Linting

```bash
test/lint/lint-all.sh                    # All lint checks
test/lint/lint-whitespace.sh             # Trailing whitespace, tabs
test/lint/lint-include-guards.sh         # Header guard consistency
test/lint/lint-circular-dependencies.sh  # Circular #include detection
```

### Local CI (requires Docker + act)

```bash
./contrib/devtools/run-local-ci.sh workflow=.github/workflows/cmake_quality.yml job=lint
```

## Code Style

The project follows **modern Bitcoin Core coding standards** documented in `doc/developer-notes.md`. Key points:

- **Allman/ANSI brace style**, 4-space indentation, no tabs
- No space after function names; one space after `if`, `for`, `while`
- ClangFormat config at `src/.clang-format`
- **Older code** uses Hungarian notation (`nCount`, `strName`, `fEnabled`, `vItems`, `mapEntries`, `pPointer`). When working in an area with older style, use judgment about whether to match the local conventions or use the modern standard -- avoid gratuitous style churn in otherwise focused patches.

## Architecture

### Layer Model

```
User Interface:     Qt GUI (src/qt/)  |  RPC (src/rpc/)  |  CLI (gridcoinresearchd)
                                         |
Gridcoin Logic:     Contract System  |  Beacon  |  Tally/Accrual  |  Quorum/Superblock
                    (src/gridcoin/)
                                         |
Blockchain Core:    Block Validation  |  Staking  |  P2P Network  |  Wallet
                    (validation.cpp, node/, miner.cpp, net*.cpp, wallet/)
                                         |
Data/Network:       LevelDB  |  BDB (wallet)  |  Scraper (src/gridcoin/scraper/)
```

### Gridcoin-Specific Subsystems (src/gridcoin/)

These are what differentiate Gridcoin from Bitcoin:

- **Contract system** (`contract/`): Blockchain-stored governance actions (beacon, project whitelist, protocol params, votes). Contracts are special transactions dispatched to `IContractHandler` implementations via a registry pattern.
- **Beacon** (`beacon.h/cpp`): Links BOINC CPIDs to wallet keys for reward eligibility. Lifecycle: pending -> superblock activation -> active (6-month expiry).
- **Tally** (`tally.h/cpp`): Tracks per-CPID research reward accruals using periodic snapshots for O(1) lookups.
- **Superblock/Quorum** (`superblock.h/cpp`, `quorum.h/cpp`): Daily consensus snapshots of network research statistics. Scraper convergence (not voting) achieves consensus in current protocol.
- **Scraper** (`scraper/`): Distributed BOINC statistics collection. Active scrapers download project stats and publish signed manifests; subscriber nodes receive manifests and run convergence to build superblocks.
- **AutoGreylist** (`autogreylist.h/cpp`, `autogreylist_v2.h/cpp`, `project.h/cpp`): Automatically excludes unresponsive BOINC projects based on Zero Credit Days (ZCD) and Whitelist Activity Score (WAS).
- **MRC** (`mrc.h/cpp`): Manual Research Claims for non-staking researchers.
- **Side Stakes** (`sidestake.h/cpp`): Automatic reward distribution to configured addresses.

### Key Bitcoin-Inherited Files (modified for Gridcoin)

| File | Role |
|------|------|
| `src/validation.cpp` | Block/transaction validation (`CheckBlock`, `ConnectBlock`, `AcceptBlock`, `AcceptToMemoryPool`), including contract and MRC validation |
| `src/node/chainman.cpp` | Chain management: `ProcessBlock`, `SetBestChain`, reorganization |
| `src/node/blockstorage.cpp` | Block files and block index loading (`LoadBlockIndex`) |
| `src/miner.cpp` | Proof-of-stake block creation, research reward claiming, sidestake application |
| `src/net.cpp`, `src/net_processing.cpp` | P2P networking and message handling |
| `src/wallet/wallet.cpp` | Wallet operations |
| `src/init.cpp` | Startup/shutdown orchestration |

`src/main.cpp` and `src/main.h` no longer exist. The #3125 refactor series moved their contents out, mostly into the files above, and deleted `main.cpp` (`29cb43458`); its successor #3269 retired the `main.h` umbrella (`1b1cc5633`).

### Registry Access Pattern

Gridcoin subsystems use singleton registries accessed via global functions:
```cpp
GetBeaconRegistry(), GetWhitelist(), GetProtocolRegistry(),
GetSideStakeRegistry(), GetScraperRegistry()
```

### Thread Architecture

Key threads: `ThreadStakeMiner` (block generation), `ThreadScraper`/`ThreadScraperSubscriber` (statistics collection, mutually exclusive), `ThreadSocketHandler`/`ThreadMessageHandler` (P2P), and the RPC server thread pool (`src/rpc/server.cpp`). Full list in `doc/developer-notes.md`.

### Lock Ordering

`cs_main` (blockchain state) must be acquired before `cs_wallet` (wallet operations), followed by any subsystem locks. `LOCK2(a, b)` acquires in argument order (not by address). Compile with `-DENABLE_DEBUG_LOCKORDER=ON` to detect violations. See `doc/developer-notes.md` for the canonical ordering and details.

### Consensus Changes

All consensus rule changes must be gated by block height or version. Mainnet block-version activation heights are set in `src/chainparams.cpp` (`CMainParams`) — check there for the current, authoritative values rather than relying on this file. As of this writing: `BlockV13Height = 3989800`, `BlockV14Height = 3990000` (both set, scheduled as future mainnet activations at the time of this line's last update). Consensus changes require hard fork coordination.

**Feature activation heights are never independent on mainnet.** The per-feature heights (`AutoGreylistDeepCopyHeight`, `AutoGreylistRedesignHeight`, `MessageContractDisableHeight` and the like, with their hidden `-...height` overrides) exist to make testing easy on a private testnet fork, or perhaps in an extraordinary emergency on testnet. On mainnet every one of them is set to the same height as the block version gate of the mandatory release that carries it. When judging whether a code path matters on mainnet, assume all of a release's feature gates switch on together at that height; a path that needs some gates on and others off is reachable only on a test network with split overrides.

### RPC Heritage Ledger (adding or changing an RPC)

Every `vRPCCommands[]` row in `src/rpc/server.cpp` carries a **mandatory heritage classification** — the `CRPCCommand` constructor requires it, so a new RPC will not compile without one:

```
{ "name", &impl, cat_x, &help_helpman, heritage_<bucket>, "<fp>" }
```

- **Buckets:** `heritage_pure_upstream` (rote port, backport-safe), `heritage_mixed` (upstream analogue with Gridcoin-specific divergence — the "looks portable, isn't" case), `heritage_removed_upstream` (frozen fork, deleted upstream), `heritage_pure_gridcoin` (no upstream analogue). Rule of thumb: a rote GRC substitution stays pure-upstream; any *retained divergence decision* (extra/removed result fields, different args, account/contract entanglement) is mixed.
- **Fingerprint `<fp>`:** required for the three fingerprinted buckets; `""` for pure-gridcoin. **Never hand-compute it** — run `test/lint/lint-rpc-heritage.py`, which prints the expected value on a mismatch. Use `"manual"` when the output isn't literal-key-trackable (dynamically-keyed object, or a positional array of scalars).
- **Doc:** add a matching row to `doc/rpc-heritage.md` (correct bucket table + fp) and bump the per-bucket tally — the lint checks the doc row-by-row *and* that the tally equals the table.
- **Drift:** if the lint flags a fingerprint mismatch on an existing RPC you touched, re-confirm the bucket is still correct, then update both the row's `heritage_fp` and the doc row. Full runbook in `doc/developer-notes.md`.
- **Regtest-only RPCs** gate on `Params().IsMockableChain()` and throw `JSONRPCError(RPC_METHOD_NOT_FOUND, ...)` (maps to HTTP 404, not `std::runtime_error`), matching `generate`/`generatesuperblock`/`stakelimit`.

`test/lint/lint-rpc-heritage.py` (wired into `lint-all.sh`) is the single fingerprint authority — it computes and validates the baselines, so run it after any RPC change.

## Test Conventions

- Framework: Boost Unit Test
- Test files: `src/test/<source>_tests.cpp` or `src/test/gridcoin/<source>_tests.cpp`
- Suite naming: `<source_filename>_tests`
- Global fixture: `TestingSetup` (a `BOOST_GLOBAL_FIXTURE` in `src/test/test_gridcoin.cpp`) runs for every test — it sets up an in-memory tx database, a mock wallet (`pwalletMain`), ECC, and quiescent net managers. It does **not** build a block chain. (`TestChain100Setup` appears once in `util_tests.cpp`, inside a commented-out block; it is not defined in-tree — do not rely on it.)
- Real chain: `grc_test::RegtestChainSetup` (`src/test/chain_setup.h`) builds the regtest genesis with spendable premine outputs through the production `LoadBlockIndex()`. Attach it **per suite** with `BOOST_AUTO_TEST_SUITE(x, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())`, never per case — see the header comment for why, and `chainman_reorg_tests.cpp` / `miner_block_assembly_tests.cpp` for users.
- Mock block context: tests that only need `CBlockIndex` entries use `GRC::MockBlockIndex::InsertBlockIndex(...)` (see `gridcoin/beacon_tests.cpp`, `gridcoin/mrc_tests.cpp`, `wallet_tests.cpp`).
- State isolation: all suites share one process, so a suite that leaves a global changed fails the run via the leak detector (`src/test/state_leak_detector.h`). Restore globals with `StateGuard` and its opt-ins in `src/test/state_guard.h`.

## PR Title Prefixes

Use component prefixes: `accrual`, `build`, `consensus`, `contract`, `doc`, `gui`/`qt`, `ipc`, `mempool`, `mining`, `net`/`p2p`, `refactor`, `researcher`, `rpc`, `scraper`, `staking`, `superblock`, `test`/`qa`/`ci`, `voting`, `wallet`, `whitelist`

## Key Documentation

- `doc/build.md` - Build guide (CMake)
- `doc/cmake-options.md` - CMake configuration reference
- `doc/developer-notes.md` - Code style, threading, and lock ordering
- `doc/automated_greylisting_design_highlights.md` - AutoGreylist design
- `clinerules/` - Detailed architecture docs (component guide, common tasks, glossary)
