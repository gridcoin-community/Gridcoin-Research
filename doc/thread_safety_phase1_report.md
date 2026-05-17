# Clang Thread Safety Analysis — Phase 1 report (issue #2869)

Phase 1 of the issue #2869 multi-phase thread-safety rollout. Captures the build-system enablement, the canonical lock order it established, and a per-resolution breakdown of every warning the analyzer surfaced.

The phases are an internal organizing tool, not a release boundary:

- **Phase 1 (this PR)**: enable the analyzer; annotate cs_main globals + their immediate call chains; annotate `setpwalletRegistered`; document Phase 2/3/4 deferrals in-code with `TODO(#2869 Phase N — area)` comments.
- **Phase 2**: researcher/beacon, wallet, tally, mrc, block_rewards, contract, voting, miner, policy, block_finder.
- **Phase 3**: scraper, quorum (lambda-capture analyzer issues).
- **Phase 4**: network layer (`ProcessMessage`, `AlreadyHave`, `PushVersion`, `ArgsManager::GetBlocksDirPath`).

Each phase ends with the same invariant: `cmake --build build_clang --target gridcoinresearchd gridcoinresearch test_gridcoin` produces zero `-Wthread-safety-*` warnings and the boost test suite passes.

## Build-system enablement

The Clang Thread Safety Analyzer ships with upstream Clang and Apple Clang but is not in `-Wall`, so on Linux/Windows clang builds the analyzer was effectively dormant — every existing `GUARDED_BY` / `EXCLUSIVE_LOCKS_REQUIRED` annotation in the tree was aspirational documentation. The pre-existing block in `CMakeLists.txt` only downgraded `-Werror=thread-safety-*` inside `if(APPLE)`, implying the analyzer was on by default in Apple Clang and the build had pre-existing warnings there too.

**Change**: replace the `if(APPLE)`-only block with a portable `check_cxx_compiler_flag(-Wthread-safety HAVE_CLANG_THREAD_SAFETY)` detection. When the flag is supported, enable `-Wthread-safety` and downgrade both `-Werror=thread-safety-analysis` and `-Werror=thread-safety-reference` on every platform. Kept warning-only across Phases 1–4; the `-Wno-error=` flags can be dropped after Phase 4 to promote stale annotations to hard build failures.

## Pre-existing fix: `sync.h` negative-capability annotation

Before any Phase 1 globals were annotated, the analyzer emitted 96 warnings from a single source:

```cpp
template <typename MutexType>
void AssertLockNotHeldInternal(const char* pszName, const char* pszFile, int nLine, MutexType* cs)
    EXCLUSIVE_LOCKS_REQUIRED(!cs);
```

`cs` is a `MutexType*`. The `!cs` inside the attribute is evaluated before the analyzer sees it; on a pointer, `operator!` yields `bool`, not the negative-capability `const MutexType&` overload defined on `AnnotatedMixin`. The fix is to dereference inside the attribute:

```cpp
    EXCLUSIVE_LOCKS_REQUIRED(!(*cs));
```

Applied to both the `DEBUG_LOCKORDER` and no-debug declarations. **Annotation-only — callers were already correct.** Eliminated all 96 `-Wthread-safety-attributes` warnings before Phase 1 began counting cs_main warnings.

## Canonical lock order established in Phase 1

```
cs_main -> cs_setpwalletRegistered -> cs_wallet -> subsystem locks
```

The wallet-registry lock (`cs_setpwalletRegistered`) was not previously part of the canonical order documented in `CLAUDE.md`. Phase 1 inserted it between `cs_main` and `cs_wallet` because the wallet-registry wrappers (`SyncWithWallets` and friends) iterate `setpwalletRegistered` and then dispatch into per-wallet methods that take `cs_wallet`. Acquisition at the call site (rather than inside the wrapper) avoids the inversion an inside-lock would have created with `sendrawtransaction`'s `LOCK2(cs_main, cs_wallet)` → `SyncWithWallets` path against the `SendMessages` → `ResendWalletTransactions` path (which holds no cs_main).

## Globals annotated in Phase 1

| Variable | File:line | Lock |
|---|---|---|
| `mapBlockIndex` | `src/main.h:76` | `cs_main` |
| `pindexGenesisBlock` | `src/main.h:77` | `cs_main` |
| `nBestHeight` | `src/main.h:82` | `cs_main` |
| `nBestChainTrust` | `src/main.h:83` | `cs_main` |
| `hashBestChain` | `src/main.h:84` | `cs_main` |
| `pindexBest` | `src/main.h:85` | `cs_main` |
| `setpwalletRegistered` | `src/main.h:89` | `cs_setpwalletRegistered` |

`net.h`'s `vNodes` / `cs_vNodes` / `mapRelay` / `cs_mapRelay` and the `wallet.cs_wallet` mutex were already declared and partially annotated upstream; Phase 1 left those declarations as-is and resolved only the warnings that surfaced from the new cs_main `GUARDED_BY` additions.

## Resolution log: annotation-only

The function already runs with the relevant lock held by every observed caller; adding `EXCLUSIVE_LOCKS_REQUIRED` on the declaration just makes the existing contract enforceable. No behavior change.

### Chain-state / block index

| File:line | Function | Lock | Notes |
|---|---|---|---|
| `src/main.h:617` | `CBlockIndex::IsInMainChain` | `cs_main` | Reads `pindexBest`. |
| `src/main.h:1080` | `CBlockLocator::GetDistanceBack` | `cs_main` | Iterates `mapBlockIndex`. |
| `src/main.h:1101` | `CBlockLocator::GetBlockIndex` | `cs_main` | Iterates `mapBlockIndex` and reads `pindexGenesisBlock`. |
| `src/main.h:1117` | `CBlockLocator::GetBlockHash` | `cs_main` | Iterates `mapBlockIndex`. |
| `src/main.h:1133` | `CBlockLocator::GetHeight` | `cs_main` | Transitive: calls `GetBlockIndex`. |
| `src/main.h:142` | `AbandonChainTo` | `cs_main` | Already had `AssertLockHeld(cs_main)`; analyzer now enforces it. |
| `src/main.h:177` | `GetClaimByIndex` | `cs_main` | Reads `pindexBest`. |
| `src/main.cpp:797` | `InvalidChainFound` (file-static) | `cs_main` | Reads / writes `pindexBest`, `nBestInvalidBlockTrust`. |
| `src/main.cpp:1430` | `GridcoinServices` | `cs_main` | Walks chain. |
| `src/main.h:124` (comment) | free `LoadBlockIndex` | takes `cs_main` internally | Documented contract in comment; callers MUST NOT hold cs_main. |
| `src/dbwrapper.h:483, src/dbwrapper.cpp:472` | `CTxDB::LoadBlockIndex` / `LoadBlockIndexGuts` / file-static `InsertBlockIndex` | `cs_main` | Populate mapBlockIndex; called from the free `LoadBlockIndex` wrapper. |
| `src/validation.h:73, 102, 108, 122, 124, 73` | `DisconnectInputs`, `GetDepthInMainChain`, `AddToBlockIndex`, `ValidateMRC` (both), `ConnectInputs`, `DisconnectBlock`, `ConnectBlock` | `cs_main` | Annotated on both declaration + definition. The previous redundant `LOCK(cs_main)` inside `AddToBlockIndex` was removed (was triggering "already-held" warnings on the recursive mutex). |
| `src/validation.cpp:672` | `ReturnCurrentMoneySupply` (anonymous-namespace static) | `cs_main` | Walks pprev chain. |
| `src/consensus/tx_verify.cpp:143` | `GetHeightForTxIndex` (file-static) | `cs_main` | Reads mapBlockIndex; sole caller `CheckSequenceLocks` already required cs_main. |
| `src/rpc/blockchain.h:45` | `GRC::MockBlockIndex::InsertBlockIndex` | `cs_main` | Used by RPC and tests. |
| `src/rpc/blockchain.cpp:189` | `blockToJSON` | `cs_main` | Iterates block txs; RPC callers already take cs_main. |
| `src/rpc/blockchain.cpp:30, 31, 37, 40` | `MagnitudeReport`, `SuperblockReport`, `GetJSONVersionReport`, `TxToJSON` | `cs_main` | Annotation on both forward declarations + definitions. |
| `src/wallet/rpcwallet.cpp:36` | `TxToJSON` forward decl | `cs_main` | Matches `rpc/blockchain.cpp` definition. |
| `src/qt/transactiondesc.cpp:22` | `GetTxStakeBoincHashInfo` forward decl | `cs_main` | Sole caller `TransactionDesc::toHTML` takes `LOCK2(cs_main, wallet->cs_wallet)`. |
| `src/rpc/rawtransaction.cpp:44` | `GetTxStakeBoincHashInfo` definition | `cs_main` | Same. |
| `src/rpc/rawtransaction.cpp:327` | `TxToJSON` definition | `cs_main` | Production callers (`getrawtransaction`, `decoderawtransaction`, `sendrawtransaction`, `gettransaction` RPC) all take cs_main. |
| `src/rpc/psgt.cpp:26, 52` | `AddHDKeypathIfAvailable`, `FillHDKeypaths` (file-static) | `wallet.cs_wallet` | Sole call sites take `LOCK2(cs_main, pwalletMain->cs_wallet)`. |
| `src/gridcoin/gridcoin.cpp:133, 652, 701` | `VerifyCheckpoints`, `GRC::RebuildBeaconRegistry`, `GRC::Initialize` | `cs_main` | Init-time callers now take cs_main explicitly (see `init.cpp` real-acquisition list). `gridcoin.h` updated with matching annotations. |
| `src/main.cpp` setpwalletRegistered wrappers (`SyncWithWallets`, `UpdatedTransaction`, `ResendWalletTransactions`, `EraseFromWallets`, `PrintWallets`, `Inventory`, `SetBestChain(CBlockLocator&)`, `GetTransaction(uint256,CWalletTx&)`) | `cs_setpwalletRegistered` | Hoisted to callers — see canonical lock order section above. |

### Phase 2/3/4 placeholders (`NO_THREAD_SAFETY_ANALYSIS` + TODO)

Phase 1 captures these so the daemon target builds clean today; Phase 2+ will replace each placeholder with the right lock annotation after the caller-chain audit each subsystem needs. The grep one-liner for the punch list is:

```
grep -rn 'TODO(#2869 Phase' src/
```

Grouped by phase target:

- **Phase 2 — researcher/beacon** (`gridcoin/researcher.cpp`): `GenerateBeaconKey`, `CheckBeaconTransactionViable`, `SendBeaconContract`, `SendBeaconContractV3`, `Researcher::Accrual`, `Researcher::AccrualNearLimit`.
- **Phase 2 — wallet** (`wallet/wallet.cpp`, `wallet/rpcwallet.cpp`): `CWallet::AddToWallet`, `CWallet::ResendWalletTransactions`, `CWalletTx::RevalidateTransaction`, `CWallet::GetKeyBirthTimes`, `GetGeneratedType`, `WalletTxToJSON`.
- **Phase 2 — tally** (`gridcoin/tally.cpp`): `RepairZeroCpidIndex`, `ActivateSnapshotAccrual`, `GetStartHeight`, `RebuildAccrualSnapshots`, `Tally::GetNewbieSuperblockAccrualCorrection`.
- **Phase 2 — mrc** (`gridcoin/mrc.cpp`): `MRC::ComputeMRCFee`, `MRCContractHandler::Validate`.
- **Phase 2 — block_rewards** (`gridcoin/consensus/block_rewards.cpp`): `BlockRewardRules::CheckResearcherClaim`, `BlockRewardRules::CheckMRCRewards`.
- **Phase 2 — contract** (`gridcoin/contract/contract.cpp`, `contract/message.cpp`): `GRC::ReplayContracts`, `Contract::Body::ResetType`, `SelectMasterInputOutput`.
- **Phase 2 — voting** (`gridcoin/voting/builders.cpp`, `voting/result.cpp`): `PollBuilder::SetPayloadVersion`, `FindBeaconTxid`, `GetMagnitudeFactor`, `ResolveSuperblockForPoll`, `ResolveMoneySupplyForPoll`.
- **Phase 2 — block_finder** (`gridcoin/support/block_finder.cpp`): `FindByHeight`, `FindByMinTime`, `FindByMinTimeFromGivenIndex`, `FindLatestSuperblock`. ~29 call sites across the codebase; cascade will dominate Phase 2 work.
- **Phase 2 — miner** (`miner.cpp`): `SplitCoinStakeOutput`. Test paths in `coinstake_construction_tests.cpp` don't hold cs_main, blocking the required annotation.
- **Phase 2 — policy** (`policy/policy.cpp`): `IsStandardTx`. Test paths in `script_p2sh_tests.cpp` don't hold cs_main.
- **Phase 3 — scraper/quorum** (`gridcoin/quorum.cpp`, `scraper/scraper.cpp`): `AddBeaconPartsData` (+ two lambdas with their own NO_THREAD_SAFETY_ANALYSIS markers — the analyzer cannot see hold state through lambda captures), `Quorum::SuperblockNeeded`, `testnewsb` RPC.
- **Phase 4 — net** (`main.cpp`, `net.cpp`, `util/system.cpp`): `AlreadyHave`, `ProcessMessage`, `CNode::PushVersion`, `ArgsManager::GetBlocksDirPath` (returns reference to cs_args-guarded cache).

## Resolution log: real LOCK acquisitions added

These are behavior changes (the previous code accessed guarded state without holding the lock) and warrant the most review.

### Init-time wrappers — `src/init.cpp`

Init runs single-threaded, so contention is theoretical. The locks document the contract the analyzer enforces on the called functions.

| Call site | What it protects | Notes |
|---|---|---|
| `LOCK(cs_main)` around `txdb.LoadBlockIndex()` in `-loadblockindextest` path | `CTxDB::LoadBlockIndex` is now `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` | Scoped block; the free `LoadBlockIndex()` below takes cs_main internally and does not need this wrapper. |
| `LOCK(cs_main)` at top of `-printblock` branch | `mapBlockIndex` iteration | Always-compiled debug path; runs only when arg supplied. |
| `LOCK(cs_main)` after `RegisterWallet(pwalletMain)` (covers wallet rescan + `GRC::Initialize` through end of `AppInit2`) | `pindexBest` / `pindexGenesisBlock` / `mapBlockIndex` reads + the now-`EXCLUSIVE_LOCKS_REQUIRED(cs_main)` `GRC::Initialize` and friends | Scope intentionally wide because the rest of `AppInit2` is uncontended init. |
| `LOCK(pwalletMain->cs_wallet)` moved to before the verbose-debug-print block in `AppInit2` | cs_wallet reads for `setKeyPool.size()` / `mapWallet.size()` / `mapAddressBook.size()` | cs_main is still held from the wallet-rescan block above; canonical order preserved. The two prior `LogPrintf` lines (mapBlockIndex.size, nBestHeight) move under the cs_wallet scope but were already under cs_main and there is no other lock the order could conflict with. |

### `gridcoin/gridcoin.cpp` `CheckBlockIndex`

Moved the existing `LOCK(cs_main)` up by 7 lines so it covers the `if (!pindexGenesisBlock)` early-return check (which now requires cs_main). No callers; no path change other than the lock being held briefly during a fast-return.

### `validation.cpp` AddToBlockIndex

Removed the inner `LOCK(cs_main)` (was redundant on a recursive mutex and triggered "acquiring mutex 'cs_main' that is already held" + "not held on every path through here" warnings on every exit). The function is now `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` from its declaration in `validation.h`, and both callers (`main.cpp:1957` inside `LoadBlockIndex` which takes cs_main, and `validation.cpp:1495` inside `AcceptBlock` which is itself `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`) hold the lock at the call site.

### `main.cpp` `AskForOutstandingBlocks`

`LOCK(cs_vNodes)` → `LOCK2(cs_main, cs_vNodes)`. The function reads `nBestHeight` inside the iteration, which now requires cs_main. The cs_main → cs_vNodes order matches the existing pattern in `validation.cpp:1508` (AcceptBlock's inventory-relay loop). Callers: `ForceReorganizeToHash` already holds cs_main; the two RPC entry points (`sendblock`, `askforoutstandingblocks`) previously made unprotected reads — `LOCK2` corrects both.

### RPC entry points — `rpc/blockchain.cpp`, `rpc/mining.cpp`

These RPCs previously read chain-state globals before acquiring `cs_main` (the read was racy with chain-tip changes). Phase 1 acquires cs_main at the right point. Where the read was a bounds check whose failure threw a JSON error, the original error-message ordering is preserved (block-validity error surfaces before batch-size error in `getblocksbatch`; `LogPrint("Getblockhash %d", nHeight)` still runs only on valid input).

| File:line | RPC | Change |
|---|---|---|
| `src/rpc/blockchain.cpp:773` | `showblock` | Take cs_main before bounds check on `nBestHeight`. |
| `src/rpc/blockchain.cpp:888` | `getblockhash` | Take cs_main; preserve "only LogPrint valid input" by ordering bounds check before the LogPrint inside the lock. |
| `src/rpc/blockchain.cpp:911` | `getblock` | Take cs_main before `mapBlockIndex.count(hash)`. |
| `src/rpc/blockchain.cpp:935` | `getblockbynumber` | Take cs_main before bounds check on `nBestHeight`. |
| `src/rpc/blockchain.cpp:959` | `getblockbymintime` | Take cs_main before reading `pindexGenesisBlock` / `pindexBest`. |
| `src/rpc/blockchain.cpp:987` | `getblocksbatch` | Single `LOCK(cs_main)` after parameter parsing so both the block-validity check (cs_main-protected) and the batch-size check (pure local) run in original order. |
| `src/rpc/blockchain.cpp:2016` | `beaconaudit` | Take cs_main before `pindexBest->nHeight`. |
| `src/rpc/mining.cpp:45` | `getstakinginfo` | Snapshot `nBestHeight` into a local inside the existing `LOCK2(cs_main, pwalletMain->cs_wallet)` block; report from the local. |
| `src/rpc/mining.cpp:246` | `auditsnapshotaccrual` | Take `LOCK(cs_main)` before the `pindexBest` null-check (which previously ran unlocked). |
| `src/rpc/mining.cpp:651` | `listresearcheraccounts` | Take `LOCK(cs_main)` before the `GRC::Tally::Accounts()` iteration that passes `pindexBest`. |
| `src/rpc/rawtransaction.cpp:`consolidatemsunspent`, `scanforunspent` | scoped `LOCK(cs_main)` for the `nBestHeight` bounds check; the original `LOCK2(cs_main, pwalletMain->cs_wallet)` scope for the wallet work below is left intact (`feedback_preserve_lock_scopes` lesson). |

### `cs_setpwalletRegistered` push-up

The 8 surviving wallet-registry wrappers in `main.cpp` were annotated `EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered)` and the lock was acquired at every call site, in the canonical cs_main → cs_setpwalletRegistered → cs_wallet order:

| Call site | Pre-existing locks at call site | LOCK added |
|---|---|---|
| `validation.cpp:593` (DisconnectBlock → SyncWithWallets, per-tx loop) | cs_main | `LOCK(cs_setpwalletRegistered)` outside the loop |
| `validation.cpp:1103` (ConnectBlock → SyncWithWallets, per-tx loop) | cs_main | same |
| `validation.cpp:1171` (AddToBlockIndex → UpdatedTransaction) | cs_main | scoped `LOCK(cs_setpwalletRegistered)` around the call |
| `main.cpp:597` (AcceptToMemoryPool → EraseFromWallets) | cs_main | scoped LOCK |
| `main.cpp:1379` (SetBestChain chain-state writer → `::SetBestChain(locator)` wallet-notify) | cs_main | scoped LOCK |
| `main.cpp:2028` (PrintBlockTree → PrintWallets) | cs_main (PrintBlockTree is `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`) | scoped LOCK |
| `main.cpp:2567, 2702` (ProcessMessage → Inventory, two sites) | none formal (ProcessMessage is `NO_THREAD_SAFETY_ANALYSIS` — Phase 4) | scoped LOCK at each site |
| `main.cpp:3219` (SendMessages → ResendWalletTransactions) | none | scoped LOCK |
| `main.cpp:3293` (SendMessages → static `GetTransaction(inv.hash, wtx)`) | none | LOCK inside the `if (!fTrickleWait)` scope |
| `rpc/rawtransaction.cpp:2113` (sendrawtransaction) | was `LOCK2(cs_main, pwalletMain->cs_wallet)` | rewritten as three sequenced LOCKs in canonical order: `LOCK(cs_main); LOCK(cs_setpwalletRegistered); LOCK(pwalletMain->cs_wallet);` |
| `rpc/htlc.cpp:141` (claimhtlc) | same | same rewrite |
| `rpc/htlc.cpp:275` (refundhtlc) | same | same rewrite |
| `wallet/rpcwallet.cpp:2706` (resendtx RPC) | none | scoped LOCK |

The dead static wrapper `bool IsFromMe(CTransaction&)` was deleted (zero callers — all observed `IsFromMe` uses in qt/wallet/test code are `CWallet`/`CWalletTx` methods). `SetBestChain(const CBlockLocator&)` looked dead by the same grep but has a global-namespace-qualified caller at `main.cpp:1379` (`::SetBestChain(locator)`) and is retained.

### Qt entry points

| File:line | Function | Change |
|---|---|---|
| `src/qt/researcher/researchermodel.cpp:804` | `ResearcherModel::isV14Enabled` (one-line function) | `LOCK(cs_main)` at top for the `IsV14Enabled(nBestHeight)` read. |
| `src/qt/voting/pollwizarddetailspage.cpp:312` | `PollWizardDetailsPage::initializePage` | Scoped `LOCK(cs_main)` around the `IsPollV3Enabled(nBestHeight)` read. |

GUI thread holds no backend locks at entry to these slots/handlers; cs_main is the natural first acquisition.

### Tests

Four test cases manipulate `pindexBest` / `pindexGenesisBlock` / `nBestHeight` directly (`new CBlockIndex` then assignment, then `delete`). Tests are single-threaded and the wrappers are documented locally:

- `src/test/gridcoin/superblock_tests.cpp` — `it_initializes_from_a_provided_scraper_convergence_v3` and `it_initializes_from_a_fallback_by_project_scraper_convergence_v3` each wrapped in `#pragma clang diagnostic push/pop` with `-Wno-thread-safety-analysis`.
- `src/test/gridcoin/mrc_tests.cpp:it_properly_records_blocks` — pragma-bracketed iteration of `pindexGenesisBlock`.

## Deadlock audit summary

Every `LOCK*` added in Phase 1 was traced through its full call graph for ordering inversions. Two issues were found during review and fixed:

1. **`cs_setpwalletRegistered` inside-lock pattern** — initially Phase 1 took the lock INSIDE the 8 wrapper functions. Reviewed and identified a real inversion: `SendMessages → ResendWalletTransactions` (no cs_main, no cs_wallet → would take cs_setpwalletRegistered then cs_wallet) inverted with `sendrawtransaction → LOCK2(cs_main, cs_wallet) → SyncWithWallets` (cs_wallet then cs_setpwalletRegistered). The cs_main mutex did not serialize the two threads because SendMessages doesn't hold cs_main. Fixed by hoisting the lock to callers and rewriting the RPC sites' `LOCK2` to sequenced LOCKs in canonical order.
2. **`getblockhash` LogPrint conditionality** — first-pass move of the bounds check after `LOCK(cs_main)` left an unconditional `LogPrint("Getblockhash %d", nHeight)` between them, causing the previously-conditional log to fire for invalid input. Fixed by placing bounds check + throw before the LogPrint inside the lock.

`LOCK2(cs_main, cs_vNodes)` in `AskForOutstandingBlocks` was verified consistent with `validation.cpp:1508` (the only other site in the daemon that combines those two locks), and no path anywhere takes `cs_vNodes → cs_main`.

## End state

```
$ cmake --build build_clang --parallel 32 --target gridcoinresearchd gridcoinresearch test_gridcoin --clean-first
[100%] Built target gridcoinresearchd
[100%] Built target gridcoinresearch
[100%] Built target test_gridcoin

$ grep -c "warning:" /tmp/clang_build.log
0

$ ./build_clang/src/test/test_gridcoin --log_level=warning
*** No errors detected
```

Phase 1 ships warning-free across the daemon, the Qt wallet, and the test binary under clang. The `-Wthread-safety-*` warnings the analyzer would emit are now Phase 2 (researcher/wallet/tally/...), Phase 3 (scraper/quorum lambdas), and Phase 4 (network) work — all suppressed with `NO_THREAD_SAFETY_ANALYSIS` and a TODO that names the lock and the caller-chain audit each phase needs.
