# Clang Thread Safety Analysis — issue #2869 rollout report

The issue #2869 multi-phase thread-safety rollout. Captures the build-system enablement, the canonical lock order it established, a per-resolution breakdown of every warning the analyzer surfaced, and the deferral decisions taken to preserve historical lock-scope design intent.

The phases are an internal organizing tool, not a release boundary:

- **Phase 1**: enable the analyzer; annotate cs_main globals + their immediate call chains; annotate `setpwalletRegistered`; document Phase 2/3/4 deferrals in-code with `TODO(#2869 Phase N — area)` comments.
- **Phase 2**: replace the Phase 1 `NO_THREAD_SAFETY_ANALYSIS` placeholders in: block_finder, tally, mrc + block_rewards + contract, wallet + rpcwallet, researcher/beacon, voting + miner + policy. One commit per sub-area; each commit ends warning-free.
- **Phase 3**: scraper + quorum (lambda-capture analyzer issues).
- **Phase 4**: network layer (`ProcessMessage`, `AlreadyHave`, `PushVersion`, `ArgsManager::GetBlocksDirPath`).
- **Follow-ups A/B/C/D** (post-Phase 4 coverage sweep): close remaining gaps surfaced by an end-state audit — `BeaconRegistry` cs_main contract (A), scraper internals (B), `CNode` per-node locks (C), and an alert/net `GUARDED_BY` sweep (D). See "Post-Phase 4 follow-ups" below.

After the four follow-ups, **two** `NO_THREAD_SAFETY_ANALYSIS` markers remain in production source:

1. `VoteResolver::GetMagnitudeFactor` at `src/gridcoin/voting/result.cpp:700` — held intentionally per the voting-redesign design constraint (see "Design constraint preserved" below).
2. `BeaconRegistry::GetBeaconChainletRoot`'s `ChainletErrorHandle` lambda body at `src/gridcoin/beacon.cpp:852` — analyzer cannot see hold-state through `[this]` lambda capture even though the enclosing `GetBeaconChainletRoot` is correctly `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`. Documented at the use site; will be revisited if/when the analyzer learns about lambda-capture flow.

Both will fall out naturally as the relevant subsystem work lands.

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

## Phase 2 — sub-area annotation commits

Phase 2 replaces every Phase 1 `NO_THREAD_SAFETY_ANALYSIS` placeholder in the listed sub-areas with the right `EXCLUSIVE_LOCKS_REQUIRED(...)` annotation, pushing the lock requirement out to callers (per the user's "annotate then push to caller" pattern — eliminates inversion risk and surfaces missing locks at the API boundary, not later in the call chain). Each sub-area is one commit; each commit ends with `cmake --build build_clang --clean-first` producing zero `-Wthread-safety` warnings and `test_gridcoin` reporting `*** No errors detected`.

| Commit | Sub-area | Functions promoted | Caller cascade |
|---|---|---|---|
| `42b1e9516` | `gridcoin/support/block_finder.cpp` | `BlockFinder::FindByHeight`, `FindByMinTime`, `FindByMinTimeFromGivenIndex`, `FindLatestSuperblock` | ~29 call sites across rpc/voting/scraper/superblock; almost all callers already held cs_main. `block_finder_tests.cpp` wrapped in `#pragma clang diagnostic` for `-Wno-thread-safety-analysis` (single-threaded test fixture mutates pindexBest directly). |
| `849a06700` | `gridcoin/tally.cpp` | `RepairZeroCpidIndex`, `ActivateSnapshotAccrual` (private+public), `GetStartHeight`, `RebuildAccrualSnapshots`, `Tally::GetNewbieSuperblockAccrualCorrection`, `Initialize` (private+public), `ApplySuperblock` (private+public), `BuildAccrualSnapshots`, `TallySuperblockAccrual`, `Tally::GetMagnitudeUnit` | All callers reached cs_main via existing init/validation/RPC paths; `tally.h` already documented "Always lock cs_main before calling its methods" — the analyzer now enforces it piecewise. |
| `3b33e8bbb` | `gridcoin/mrc.cpp` + `gridcoin/consensus/block_rewards.cpp` + `gridcoin/contract/contract.cpp` + `gridcoin/contract/message.cpp` | `MRC::ComputeMRCFee`, `MRCContractHandler::Validate/BlockValidate` overrides, `BlockRewardRules::CheckResearcherClaim/CheckMRCRewards/Check/CheckNoncruncherClaim`, `GRC::ReplayContracts`, `Contract::Body::ResetType`, `SelectMasterInputOutput`, `CreateContractTx`, `SendContractTx`, `GRC::SendContract` (both overloads). | `rpc/blockchain.cpp::addkey` RPC: scoped `LOCK(cs_main)` around the `SendContract` call. `mrc_tests.cpp::it_has_proper_fees_for_newbies`: `LOCK(cs_main)` added to fixture. |
| `eb56f3a1c` | `wallet/wallet.cpp` + `wallet/rpcwallet.cpp` + `main.cpp` cascade | `CWallet::AddToWallet`, `AddToWalletIfInvolvingMe`, `ResendWalletTransactions`, `GetKeyBirthTimes` (cs_main + cs_wallet), `CWalletTx::RevalidateTransaction`, `CWalletTx::GetGeneratedType` inline forwarder, `::GetGeneratedType` free function, `WalletTxToJSON`, `ListTransactions` (rpcwallet); `SyncWithWallets` + free `ResendWalletTransactions` wrappers in `main.h`/`main.cpp`. | 3 lock promotions, all canonical: `SendMessages` and `resendtx` RPC `LOCK(cs_setpwalletRegistered)` → `LOCK2(cs_main, cs_setpwalletRegistered)`; `accounting_tests.cpp` `LOCK(cs_wallet)` → `LOCK2(cs_main, cs_wallet)`. This commit **eliminates the latent Phase 1 inversion risk** where SendMessages and resendtx held cs_setpwalletRegistered without cs_main. |
| `20d800299` | `gridcoin/researcher.cpp` + Qt v3 beacon wizard | `GRC::GenerateBeaconKey`, `CheckBeaconTransactionViable`, `SendBeaconContract` (anon), `SendNewBeacon` (anon), `RenewBeacon` (anon), `GRC::SendBeaconContractV3` (cs_main + cs_wallet, since `RecentBeacons::Remember` requires the wallet lock). `Researcher::Accrual` / `AccrualNearLimit` LOCK moved up to cover `pindexBest`/`OutOfSyncByAge` reads (see "Known follow-up" below). `researcher.h` gains `#include "sync.h"`, `#include "wallet/wallet.h"`, `extern cs_main`/`extern pwalletMain`. | 2 new `LOCK2(cs_main, pwalletMain->cs_wallet)` in `researchermodel.cpp::generateBeaconKeyForV3` and `advertiseBeaconV3`. Mirrors the existing v2 `advertisebeacon` RPC pattern; both follow canonical lock order. |
| `47f9ae1d8` | `gridcoin/voting/builders.cpp` + `voting/result.cpp` + `miner.cpp` + `policy/policy.cpp` | `MagnitudeClaimBuilder::FindBeaconTxid`, `MagnitudeClaimBuilder::BuildClaim`, `VoteClaimBuilder::BuildClaim`, `VoteBuilder::BuildContractTx`, `PollBuilder::SetPayloadVersion`, `ResolveSuperblockForPoll`, `ResolveMoneySupplyForPoll`, `SplitCoinStakeOutput`, `IsStandardTx`. `voting/builders.h` gains `#include "sync.h"` + `extern cs_main`. **`VoteResolver::GetMagnitudeFactor` deliberately retained as `NO_THREAD_SAFETY_ANALYSIS`** — see "Design constraint" below. | `rpc/voting.cpp::addpoll` + `qt/voting/votingmodel.cpp::createPoll` each get a NEW tight `LOCK(cs_main)` scoped to the `SetPayloadVersion` call only (per the `feedback_preserve_lock_scopes` lesson — do not widen the existing surrounding LOCK blocks). `coinstake_construction_tests.cpp` Tests 4 + 5 get `LOCK(cs_main)` (Test 6 already had `LOCK2`; the script_p2sh sign/set tests already held cs_main from prior work). |

### Design constraint preserved — voting result calculation must not re-acquire cs_main

Historical work in the voting subsystem (see comments around `qt/voting/votingmodel.cpp:351-360`: *"We do this because we have eliminated the cs_main lock to free up the GUI"* / *"on a recursive lock on cs_main before for the ENTIRE run, and this is certainly better"*) deliberately moved poll result calculation out from under cs_main because the chain walk + vote counting can take seconds to tens of seconds on a node with deep history. The current shape:

```cpp
// gridcoin/voting/result.cpp::PollResult::BuildFor — preserved exactly by Phase 2
if (result.m_poll.IncludesMagnitudeWeight()) {
    SuperblockPtr superblock;
    uint64_t supply;
    {
        LOCK(cs_main);                                  // tight scope:
        superblock = ResolveSuperblockForPoll(poll);    //   snapshot
        supply     = ResolveMoneySupplyForPoll(poll);   //   snapshot
    }                                                   // <-- release
    counter.EnableMagnitudeWeight(std::move(superblock), supply);
}
...
counter.CountVotes(result, poll_ref.Votes());           // long, lock-free
```

Phase 2 annotates `ResolveSuperblockForPoll` / `ResolveMoneySupplyForPoll` as `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` because the tight inner LOCK already satisfies them, but **`VoteResolver::GetMagnitudeFactor` (reached from `ProcessLegacyVote` inside the lock-free `CountVotes` loop) stays under `NO_THREAD_SAFETY_ANALYSIS`** with an updated TODO. The pragma block at `result.cpp:959-966` around the `ProcessLegacyVote` call site is retained for the same reason.

Promoting `GetMagnitudeFactor` to `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` would force cs_main back across the entire `CountVotes` loop, undoing that historical work. The right fix is the standing voting-redesign work (committed votes with UTXO demarcation pinned at poll start, deterministic tallying without live chain reads) — at which point the legacy class is removed and the suppression goes with it.

### Known follow-up — researcher Accrual lock scope

In commit `20d800299`, `Researcher::Accrual()` and `Researcher::AccrualNearLimit()` had their internal `LOCK(cs_main)` moved up so the cs_main-guarded reads (`pindexBest`, `OutOfSyncByAge()`) happen under the lock. Hold time grows by a few microseconds in the common case (the `Tally::GetAccrual` walk dominates).

These functions are called from `ResearcherModel::formatAccrual` on GUI refresh ticks. During heavy chain activity (block connect, reorg) the GUI thread will queue on cs_main slightly more than before. Not a practical issue at present, but if profiling ever flags it, the cleaner pattern is to snapshot `pindexBest` + height/time under a tight cs_main scope, drop the lock, then call `Tally::GetAccrual` with the snapshotted values — needs a signature change on the Tally side, so out of scope for Phase 2.

### Lessons recorded for future phases

| Lesson | Where it applies |
|---|---|
| Do not widen existing tight LOCK scopes when fixing a thread-safety warning; add a NEW scoped LOCK above. Inner brace scopes are intentional (bound contention + protect cs_main → cs_wallet ordering). | Followed in both `addpoll` RPC and `votingmodel.cpp::createPoll` SetPayloadVersion fixes. |
| When a header annotation references a lock expression like `pwalletMain->cs_wallet`, the header must `#include "wallet/wallet.h"` so the analyzer can resolve the member-access type. Forward declaration of `CWallet` is not sufficient. | `researcher.h` had to pull in `wallet/wallet.h`; `voting/builders.h` only needed `sync.h` + `extern cs_main`. |
| EXCLUSIVE_LOCKS_REQUIRED is preferable to internal acquisition when the caller chain can hold the lock — it eliminates inversion risk and surfaces missing locks at the API boundary. | Pattern from Phase 1 `cs_setpwalletRegistered` fix; reapplied throughout Phase 2. |
| Pragma-suppressing a test case is acceptable when the test is single-threaded and the fixture mutates analyzer-tracked globals directly; adding `LOCK(cs_main)` in the test is preferable when the existing fixture already supports it. | `coinstake_construction_tests.cpp` Tests 4 + 5 got `LOCK(cs_main)` (fixture supports it); Phase 1 `block_finder_tests.cpp` got the pragma (fixture mutates pindexBest in setup). |

## Phase 3 — scraper + quorum sub-area commits

Phase 3 resolves the lambda-capture analyzer issues in `quorum.cpp` and the chain-state reads in the `testnewsb` scraper RPC.

| Commit | Sub-area | Functions / change |
|---|---|---|
| `848872500` | `gridcoin/quorum.cpp` + `quorum.h` + `miner.cpp` | `Quorum::SuperblockNeeded`, `Quorum::ValidateSuperblockClaim`, `AddSuperblockContractOrVote` (miner.cpp) → `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`. All four `SuperblockNeeded` callers already held cs_main. `ProjectCombiner::AddBeaconPartsData` lost its function-level NO_THREAD_SAFETY_ANALYSIS — it only takes scraper-subsystem locks. The two inner lambdas (`find_entry`, `add_optional_part`) were refactored from `[&manifest]` captures to take projects/vParts as explicit parameters so the analyzer can see that the values consumed are the ones the enclosing LOCK protects (lambda captures don't propagate hold-state). quorum.h gains `#include "sync.h"` + `extern cs_main`. No new LOCK statements; no behavior change. |
| `fdabee989` | `gridcoin/scraper/scraper.cpp` | `testnewsb` RPC drops NO_THREAD_SAFETY_ANALYSIS. The first `LOCK(cs_ConvergedScraperStatsCache)` around `BindShared(..., pindexBest)` promoted to `LOCK2(cs_main, cs_ConvergedScraperStatsCache)` — canonical cs_main → subsystem, matches the existing pattern in `ScraperGetSuperblockContract`. The second `pindexBest` read (past-convergence path) gets a NEW tight `LOCK(cs_main)` released BEFORE the `Quorum::ValidateSuperblock` calls that follow (past-convergence validation can be slow and does not itself need cs_main — mirrors the result-calculation scope-reduction documented for `PollResult::BuildFor`). |

### Lambda-capture lesson recorded

Clang's thread-safety analyzer cannot reason about lock state through lambda captures. A `[&obj]` capture means the analyzer sees the lambda body accessing `obj` but cannot prove which lock (if any) protects `obj` at the call site. Two fixes:

1. **Pass the relevant data into the lambda as a parameter** (preferred). The analyzer can then see that the value passed in is the one the enclosing LOCK protects. This is what the Phase 3 quorum commit did for both `find_project` and `add_optional_part`.
2. **Restructure to inline the work**. Acceptable when the lambda is small.

Anti-pattern: keeping the lambda capture and suppressing with `NO_THREAD_SAFETY_ANALYSIS` on the lambda body. The Phase 1 quorum.cpp shipped with this stopgap (which is exactly what Phase 3 cleaned up).

## Phase 4 — network layer

Phase 4 closes out the rollout. Resolves the remaining four `NO_THREAD_SAFETY_ANALYSIS` sites in the network layer.

| Commit | Sub-area | Change |
|---|---|---|
| `2c9c8f817` | `util/system.cpp` + `.h`, `net.cpp`, `main.cpp` | Combined Phase 4 commit covering: `ArgsManager::GetBlocksDirPath` — return-by-value (TODO's documented preferred fix; zero callers in tree, so safe API change). `CNode::PushVersion` — internal `WITH_LOCK(cs_main, return nBestHeight)` snapshot (called from CNode construction on socket-handler thread with no outer locks; internal snapshot keeps the caller-side contract unchanged). `AlreadyHave` — `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`; ProcessMessage inv-handling caller already held cs_main; SendMessages getdata-loop caller (~main.cpp:3346) gets a NEW tight `LOCK(cs_main)` scoped around just the AlreadyHave call, explicitly released BEFORE the `LOCK(CScraperManifest::cs_mapManifest)` below per the canonical cs_main → subsystem order documented at main.cpp:2533. `ProcessMessage` — NO_THREAD_SAFETY_ANALYSIS removed; three chain-state reads got tight cs_main coverage: VERSION/ARIES protocol-disconnect height check (pindexBest->nHeight via WITH_LOCK snapshot), VERSION/ARIES ask-for-blocks probe (scoped LOCK around nBestHeight + pindexBest reads + PushGetBlocks), ADDR/GRIDADDR threshold check (nBestHeight inline WITH_LOCK snapshot). |

### Snapshot pattern

The Phase 4 P2P entry-point cleanups use the `WITH_LOCK(cs_main, return X)` snapshot pattern instead of acquiring cs_main across the full message-handler body. This avoids holding cs_main across the (potentially slow) downstream P2P work in `PushMessage`, `PushGetBlocks`, address store, etc. It does mean the snapshotted value can be slightly stale by the time the handler acts on it, but for height-threshold heuristics (protocol-version disconnect, ask-for-blocks throttle, peer-storage threshold) staleness is harmless — the handler is making a coarse-grained policy decision, not enforcing a consensus rule.

## Post-Phase 4 follow-ups

After Phase 4 closed out the rollout, a second-pass coverage audit surfaced four narrow gaps. Each was addressed as a focused commit; all four are pure annotation work with one exception (`fScraperActive` → `std::atomic<bool>` in Follow-up B). All commits end clang-warning-free.

| Commit | Follow-up | Sub-area | Change |
|---|---|---|---|
| `630deb0e3` | A | `gridcoin/beacon.{h,cpp}` + researcher + accrual + diagnose + Qt + rpc/blockchain + tests | `BeaconRegistry`'s entire public API + `Researcher::Reload`/`Refresh` + `NewbieAccrualComputer` + `SnapshotAccrualComputer` + `block_rewards::CheckBeaconSignature` annotated `EXCLUSIVE_LOCKS_REQUIRED(cs_main)`. Production-caller `LOCK(cs_main)` additions at `ResearcherModel::reload`, `Researcher::ChangeMode`, `VerifyCPIDHasRAC::hasActiveBeacon`, `rainbymagnitude` RPC (with downstream `LOCK2(cs_main, cs_wallet)` demoted to `LOCK(cs_wallet)`), `resetcpids` RPC, `StoreBeaconList` (WITH_LOCK snapshot for log line). `Researcher::Initialize` flattens its existing `LOCK2` scope to keep `Reload()` under cs_main. Test helpers (`AddTestBeacon` and friends) get per-function `NO_THREAD_SAFETY_ANALYSIS`. Adds the documented `ChainletErrorHandle` lambda suppression (the second remaining marker — see "End state"). |
| `ab70f5d45` | B | `gridcoin/scraper/scraper.{h,cpp}` + `gridcoin/gridcoin.cpp` | `fScraperActive` → `std::atomic<bool>` (single startup write, scraper-thread reads — no lock needed; the only behavior-affecting change in the four follow-ups). `vuserpass` / `ndownloadsize` / `nuploadsize` annotated `GUARDED_BY(cs_Scraper)` to make the hoisted call-site `LOCK(cs_Scraper)` at `scraper.cpp:1640` clang-enforceable; the singleshot reentrant path (`ScraperGetSuperblockContract` → `ScraperSingleShot` → `Scraper(true)` from staking-miner/RPC threads) enters the same critical section, so the data is cs_Scraper-protected on every path. Cascade: `EXCLUSIVE_LOCKS_REQUIRED(cs_Scraper)` on `UserpassPopulated`, `class userpass` ctor/`import()`, and the four affected `DownloadProject*` / `ProcessProjectRacFileByCPID` helpers. `DownloadProjectPublicKeys` deliberately not annotated — it only touches `g_project_public_keys` under its own `cs_ProjectPublicKeys`, preserving the fine-grained-locking design. Prototype/definition consistency sweep across scraper.cpp for `ScraperHousekeeping`, `ScraperDirectoryAndConfigSanity`, `GetmScraperFileManifestHash`, `Insert/Delete/MarkScraperFileManifestEntry*`, `ScraperSendFileManifestContents`, `BinCScraperManifestsByScraper`, `ProcessProjectTeamFile`. `IsScraperMaximumManifestPublishingRateExceeded` in scraper.h gets `EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest)`. |
| `11994d6c0` | C | `src/net.{h,cpp}` + `src/main.{h,cpp}` | `CNode` members annotated `GUARDED_BY`: `ssSend`/`nSendSize`/`nSendOffset`/`vSendMsg` → `cs_vSend`; `vRecvMsg`/`nRecvVersion` → `cs_vRecvMsg`; `setInventoryKnown`/`vInventoryToSend` → `cs_inventory`; `mapMisbehavior` → `cs_mapMisbehavior`. Methods annotated `EXCLUSIVE_LOCKS_REQUIRED`: `GetTotalRecvSize`/`ReceiveMsgBytes`/`SetRecvVersion` → `cs_vRecvMsg`; `BeginMessage`/`AbortMessage`/`EndMessage`/`PushFields` → `cs_vSend`; free `SocketSendData` → `pnode->cs_vSend` (forward decl reordered below `CNode` so the lock expression resolves); `ProcessMessages` → `pfrom->cs_vRecvMsg`. Call-site additions: scoped `LOCK(pfrom->cs_vSend)` around `ssSend.SetVersion` in the VERSION handler, around VERACK-time send-version flip, around the GETHEADERS/GETBLOCKS push paths in SendMessages, etc. Replaces the "requires LOCK(cs_X)" comments that previously documented the contract informally. |
| `d4a52ceeb` | D | `src/alert.cpp` + `src/main.cpp` + `src/rpc/net.cpp` + `src/net.h` + `src/net.cpp` | Pure-annotation sweep of the remaining file-scope cs↔data pairs: `mapAlerts` → `GUARDED_BY(cs_mapAlerts)` (alert.cpp + 2 extern decls); `mapLocalHost`, `vAddedNodes`, `vOneShots` (file-static), `setservAddNodeAddresses` → `GUARDED_BY(cs_<paired>)`. All existing call sites already take the right lock; zero cascade needed. The `cs_*` declaration is moved above the data declaration where it wasn't already (clang requires the lock symbol to be in scope at the `GUARDED_BY` site). |

### Notes on patterns confirmed during the follow-ups

- **Scraper design philosophy** (Follow-up B). The scraper was written before C++11 atomics; cross-thread booleans / counters were either left racy or covered by an existing lock taken for some other reason. The right defaults now: (1) for a scalar with no existing call-site lock, prefer `std::atomic<T>`; (2) for data already inside a hoisted call-site lock, prefer honest `GUARDED_BY(lock)` over claiming "single-threaded" — the audit pass invalidated several apparent single-thread claims because the `ScraperSingleShot` path enters the same critical section as the main loop. Keep fine-grained locks scoped to specific data structures; do not fold ad-hoc state into a big `cs_Scraper` contract unless the data is in fact protected there.

- **Lambda-capture suppression is acceptable** (Follow-up A's ChainletErrorHandle). The enclosing function (`GetBeaconChainletRoot`) is correctly annotated, but the analyzer can't trace hold-state through `[this]` captures into `Reset()`. The lambda-body suppression with an explanatory comment pointing back to the enclosing annotation is the right shape — restructuring the lambda would not improve safety.

- **TRY_LOCK lock-order-avoidance pattern needs nothing extra** (audited during D). `sync.h:107` already declares `try_lock() EXCLUSIVE_TRYLOCK_FUNCTION(true)`, and the macro at `sync.h:256` propagates that annotation. The `if (lockX) { ... }` idiom in `net.cpp`'s `SendMessages` / `ProcessMessage` (`net.cpp:868,871,874,930,1083,1135,1877,1896,1899`) and the reversed-logic `cs_main` TRY_LOCK at `net.cpp:1888-1896` flow through TSA correctly. No suppressions required.

## End state

```
$ cmake --build build_clang --parallel 32 --target gridcoinresearchd gridcoinresearch test_gridcoin --clean-first
[100%] Built target gridcoinresearchd
[100%] Built target gridcoinresearch
[100%] Built target test_gridcoin

$ grep -c "warning:.*thread-safety" /tmp/clang_build.log
0

$ ./build_clang/src/test/test_gridcoin --log_level=warning
*** No errors detected

$ grep -rn 'NO_THREAD_SAFETY_ANALYSIS' src/ --include='*.cpp' --include='*.h' \
    | grep -v 'threadsafety.h\|leveldb/port/thread_annotations.h\|test/'
src/gridcoin/voting/result.cpp:700:    Weight GetMagnitudeFactor() NO_THREAD_SAFETY_ANALYSIS
src/gridcoin/beacon.cpp:852:    const auto ChainletErrorHandle = [this](...) NO_THREAD_SAFETY_ANALYSIS {
```

All four phases plus the four follow-ups ship warning-free across the daemon, the Qt wallet, and the test binary under clang. **Two** production-code `NO_THREAD_SAFETY_ANALYSIS` markers remain, both intentional:

1. `VoteResolver::GetMagnitudeFactor` — voting-redesign design constraint (above).
2. `BeaconRegistry::GetBeaconChainletRoot`'s `ChainletErrorHandle` lambda body — analyzer cannot reason about lock state through `[this]` capture; enclosing function is correctly annotated.

A small number of pragma-block suppressions remain in production source — voting (deferred), the `Serialize()` template fallback in `serialize.h` (annotating it would force a contract onto every `T::Serialize` member fleet-wide), and test code (test fixtures run single-threaded and mutate analyzer-tracked globals directly).

With this rollout complete, the `-Wno-error=thread-safety-analysis` and `-Wno-error=thread-safety-reference` downgrades that Phase 1 added to `CMakeLists.txt` can be dropped — a future change can promote stale annotations to hard build failures so newly-introduced races don't regress under the radar.
