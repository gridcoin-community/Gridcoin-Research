# Gridcoin Consensus Rules Reference

This document is the single authoritative reference for all consensus rules
that apply to block and transaction validation in the Gridcoin network. Rules
are organized by block version and are **cumulative** — each version inherits
all prior rules unless explicitly changed.

> **Scope.** A "consensus rule" is any rule whose violation causes a block or
> transaction to be rejected by a validating node. This includes block
> structure, coinstake validation, contract validation, reward calculation, the
> staking kernel, and script verification.
>
> **Source files cited.** Each section references the authoritative source.
> When reviewing this document, cross-check against those files to confirm
> that values have not been updated since the last revision of this document.

---

## Table of Contents

1. [Consensus Parameters Reference](#1-consensus-parameters-reference)
2. [General Constants](#2-general-constants)
3. [Pre-v8 Era (Genesis through Block Version 7)](#3-pre-v8-era)
4. [Block Version 8 — PoS Kernel Overhaul](#4-block-version-8)
5. [Block Version 9 — Superblock Quorum](#5-block-version-9)
6. [Block Version 10 — Constant Block Reward](#6-block-version-10)
7. [Block Version 11 — Contract System and Snapshot Accrual](#7-block-version-11)
8. [Block Version 12 — Manual Research Claims](#8-block-version-12)
9. [Block Version 13 — Mandatory Sidestakes and Configurable Parameters](#9-block-version-13)
10. [Block Version 14 — Script Timelocks and V3 Beacons](#10-block-version-14)
11. [Block Version 15 — On-Chain Pool Registration](#11-block-version-15)
12. [Cross-Cutting Consensus Rules](#12-cross-cutting-consensus-rules)
13. [Coinstake Output Structure Summary](#13-coinstake-output-structure-summary)
14. [Contract Payload Version Matrix](#14-contract-payload-version-matrix)
15. [Reward Formula Summary](#15-reward-formula-summary)

---

## 1. Consensus Parameters Reference

All height-gated parameters are defined in `Consensus::Params` (`src/consensus/params.h`)
and initialized for each network in `src/chainparams.cpp`. Helper predicates are
in `src/chainparams.h`.

### Height Gates

| Parameter | Mainnet | Testnet | Helper Function |
|---|---|---|---|
| `ProtocolV2Height` | 85,400 | 2,060 | `IsProtocolV2()` (> height) |
| `ResearchAgeHeight` | 364,501 | 36,501 | `IsResearchAgeEnabled()` (>= height) |
| `BlockV8Height` | 1,010,000 | 311,999 | `IsV8Enabled()` (> height) |
| `BlockV9Height` | 1,144,000 | 399,000 | `IsV9Enabled()` (>= height) |
| `BlockV9TallyHeight` | 1,144,120 | 399,120 | `IsV9Enabled_Tally()` (>= height) |
| `BlockV10Height` | 1,420,000 | 629,409 | `IsV10Enabled()` (>= height) |
| `BlockV11Height` | 2,053,000 | 1,301,500 | `IsV11Enabled()` (>= height) |
| `BlockV12Height` | 2,671,700 | 1,871,830 | `IsV12Enabled()` (>= height) |
| `BlockV13Height` | 3,989,800 | 2,870,000 | `IsV13Enabled()` (>= height) |
| `BlockV14Height` | 3,990,000 | 3,126,500 | `IsV14Enabled()` (>= height) |
| `BlockV15Height` | _not yet activated_ | _not yet activated_ | `IsV15Enabled()` (>= height) |
| `PollV3Height` | 2,671,700 | 1,944,820 | `IsPollV3Enabled()` (>= height) |
| `ProjectV2Height` | 2,671,700 | 1,944,820 | `IsProjectV2Enabled()` (>= height) |
| `ProjectV4Height` | _not yet activated_ | 2,870,000 | `IsProjectV4Enabled()` (>= height) |
| `SuperblockV3Height` | _not yet activated_ | 2,870,000 | `IsSuperblockV3Enabled()` (>= height) |
| `AutoGreylistAuditHeight` | _not yet activated_ | 3,111,000 | `IsAutoGreylistAuditEnabled()` (>= height) |

> **Note:** "not yet activated" means the height is set to
> `std::numeric_limits<int>::max()` in `CMainParams`. The feature exists in
> code and is active on testnet.

### Non-Height Parameters

Although these are not height gates themselves, each parameter becomes
consensus-critical at a specific block version activation height. The
"Active At" column indicates the height gate that activates the parameter.

| Parameter | Value | Active At | Notes |
|---|---|---|---|
| `DefaultConstantBlockReward` | 10 GRC | `BlockV10Height` | Overridable by `"blockreward1"` protocol entry; configurable range widens at `BlockV13Height` |
| `ConstantBlockRewardFloor` | 0 GRC | `BlockV13Height` | Lower clamp for configurable CBR (pre-v13: floor is 0 with narrower ceiling) |
| `ConstantBlockRewardCeiling` | 500 GRC | `BlockV13Height` | Upper clamp for configurable CBR (pre-v13: ceiling is 2× default = 20 GRC) |
| `InitialMRCFeeFractionPostZeroInterval` | 2/5 (40%) | `BlockV12Height` | Fee fraction for MRCs immediately after zero-payment interval |
| `MRCZeroPaymentInterval` | 14 days (mainnet), 10 min (testnet) | `BlockV12Height` | MRC 100% fee-forfeiture window after last reward |
| `MaxMandatorySideStakeTotalAlloc` | 1/4 (25%) | `BlockV13Height` | Maximum total allocation for mandatory sidestakes |
| `DefaultMagnitudeUnit` | 1/4 | `BlockV11Height` | GRC per magnitude per day; fixed at 1/4 until `BlockV13Height`, then configurable |
| `MaxMagnitudeUnit` | 5 | `BlockV13Height` | Upper clamp for configurable magnitude unit |
| `MinMagnitudeWeightFactor` | 1/10 | `BlockV13Height` | Lower clamp for voting magnitude weight factor |
| `DefaultMagnitudeWeightFactor` | 100/567 (~0.1764) | `BlockV11Height` | Voting weight conversion factor; configurable from `BlockV13Height` |
| `MaxMagnitudeWeightFactor` | 1 | `BlockV13Height` | Upper clamp for voting magnitude weight factor |
| `StandardContractReplayLookback` | 180 days | `BlockV11Height` | Contract validity window for types without registry DB (not applicable v13+ for protocol entries) |
| `PendingPoolRetention` | 28,800 blocks (~20 days) | `BlockV15Height` | PENDING pool registrations and POOL_APPROVE OPEN authorizations are query-time expired after this many blocks (issue #1783). Overridable for isolated-testnet via hidden `-pendingpoolretention`; consensus-affecting. See §11.3.1. |

> **Source:** `src/chainparams.cpp:61-91` (mainnet), `src/chainparams.cpp:184-213`
> (testnet)

### Other Inline Height Helpers

| Function | Height | Notes |
|---|---|---|
| `GetSuperblockAgeSpacing()` | 364,500 (mainnet) | Changes spacing from 43,200s (12h) to 86,400s (24h). Testnet is always 86,400s. |
| `GetOrigNewbieSnapshotFixHeight()` | 2,104,000 / 1,393,000 | First attempt at newbie accrual fix |
| `GetNewbieSnapshotFixHeight()` | 2,197,000 / 1,480,000 | Corrected newbie accrual fix |

> **Source:** `src/chainparams.h:187-201`

---

## 2. General Constants

Defined in `src/consensus/consensus.h` and `src/main.h`/`src/main.cpp`:

| Constant | Value | Notes |
|---|---|---|
| `LAST_POW_BLOCK` | 2,050 | Last block that may be proof-of-work |
| `MAX_BLOCK_SIZE` | 1,000,000 bytes | Maximum serialized block size |
| `MAX_BLOCK_SIZE_GEN` | 500,000 bytes | Maximum mined block size |
| `MAX_STANDARD_TX_SIZE` | 100,000 bytes | Maximum relayable transaction size |
| `MAX_BLOCK_SIGOPS` | 20,000 | Maximum signature operations per block |
| `MIN_TX_FEE` | 0.0001 GRC | Minimum transaction fee |
| `BLOCKS_PER_DAY` | 1,000 | Target blocks per day |
| `nCoinbaseMaturity` | 100 (mainnet), 10 (testnet) | Coinbase maturity depth |
| `nStakeMinAge` | 16 hours (mainnet), 1 hour (testnet) | Minimum UTXO age for staking eligibility |
| `nStakeMaxAge` | unlimited | No upper bound on stake age |
| `nGrandfather` | 1,034,700 (mainnet), 196,550 (testnet) | Block height below which relaxed validation applies |

> **Source:** `src/consensus/consensus.h`, `src/main.cpp:82-83,127-128,1651-1653`

---

## 3. Pre-v8 Era

### 3.1 Genesis and Proof-of-Work Phase (Blocks 0–2,050)

- Genesis block timestamp: 1413033777 (Oct 11, 2014)
- Genesis timestamp string: _"10/11/14 Andrea Rossi Industrial Heat vindicated with LENR validation"_
- Proof-of-work phase runs from block 0 through `LAST_POW_BLOCK` (2,050)
- After block 2,050, only proof-of-stake blocks are accepted

> **Source:** `src/chainparams.cpp:47-54`, `src/consensus/consensus.h:6`

### 3.2 Protocol v2 (Mainnet > 85,400)

- Block target spacing changes from 60 seconds to 90 seconds
- `GetTargetSpacing()` returns 90 for `IsProtocolV2()` blocks, 60 otherwise

> **Source:** `src/main.h:64`

### 3.3 Research Age (Mainnet >= 364,501)

- Per-CPID research reward accrual with magnitude-based calculation
- Superblock spacing changes from 12 hours (43,200s) to 24 hours (86,400s)
  at height 364,500 (mainnet)
- Research Age accrual computer: average magnitude over accrual period
  multiplied by magnitude unit and accrual days
- Maximum research subsidy schedule (daily cap per CPID):

| Period | Max Subsidy (GRC/day) |
|---|---|
| Inception – Nov 30, 2014 | 500 |
| Nov 30, 2014 – Jan 30, 2015 | 400 |
| Dec 30, 2014 – Jan 30, 2015 | 400 |
| Jan 30, 2015 – Feb 28, 2015 | 300 |
| Feb 28, 2015 – Mar 30, 2015 | 250 |
| Mar 30, 2015 – Apr 30, 2015 | 200 |
| May 1, 2015 – Jul 31, 2015 | 150 |
| Aug 1, 2015 – Oct 20, 2015 | 100 |
| Oct 20, 2015 – Nov 20, 2015 | 75 |
| Nov 20, 2015 – forever | 50 |

- Maximum single-block research reward: `MaxResearchSubsidy * 255 * COIN`
- Payment-per-day limit: lifetime average magnitude × magnitude unit × 5
- Minimum accrual block span: 10 blocks (returns 0 accrual if fewer)

> **Source:** `src/gridcoin/accrual/research_age.h:24-42,83-273`

### 3.4 Pre-v10 Coin-Year Interest Rate (Staking Reward)

The proof-of-stake reward for blocks before v10 uses a coin-year formula:

```
reward = nCoinAge * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8)
```

Interest rate schedule:

| Period | Annual Rate |
|---|---|
| Inception – Nov 30, 2014 | 9% |
| Nov 30, 2014 – Jan 30, 2015 | 8% |
| Jan 30, 2015 – Feb 28, 2015 | 7% |
| Feb 28, 2015 – Mar 30, 2015 | 6% |
| Mar 30, 2015 – Apr 30, 2015 | 5% |
| May 1, 2015 – Jul 31, 2015 | 4% |
| Aug 1, 2015 – Nov 20, 2015 | 3% |
| Nov 21, 2015 – forever | 1.5% |

> **Source:** `src/gridcoin/staking/reward.cpp:11-26,101-111`

### 3.5 Legacy PoS v3 Kernel (Block Version 7 and Below)

The v3 staking kernel computes the proof hash from:

```
hash(RSA_weight, input_block.nTime, input_tx.nTime, input_tx.hash, outpoint.n, coinstake.nTime, por_nonce)
```

- RSA weight = sum of RSA weight field and magnitude from legacy claim context
- This kernel is used only for block versions ≤ 7
- For version 7 blocks in the range before v8 activation (mainnet ≥ 999,000
  or > `nGrandfather`), the legacy proof hash is computed via
  `CalculateLegacyV3HashProof()` to carry stake modifiers into v8+

> **Source:** `src/gridcoin/staking/kernel.cpp:438-467`

### 3.6 Grandfather Rule

Blocks at or below `nGrandfather` (mainnet 1,034,700, testnet 196,550) use
relaxed validation. Certain checks are skipped:
- Double-spend detection (spent inputs)
- Coinbase timestamp validation
- Block signature verification (PoS)
- Serialized height enforcement in coinbase
- Excessive difficulty rejection

Version 8+ blocks have **no grandfather exceptions** — strict validation
always applies.

> **Source:** `src/validation.cpp:416-422,1542,1939,1984,2110-2218`, `src/main.cpp:127-128,1653`

### 3.7 Coinstake Output Limit (Pre-v10)

Maximum coinstake outputs: **3**

> **Source:** `src/validation.cpp:598-602`

---

## 4. Block Version 8 — PoS Kernel Overhaul

**Activation:** Mainnet > 1,010,000, Testnet > 311,999

### 4.1 V8 Staking Kernel

The v8 kernel replaces RSA weight with a simplified hash:

```
hash(StakeModifier, MaskedBlockTime(input_block.nTime), input_tx.hash, outpoint.n, MaskedStakeTime(coinstake.nTime))
```

Key changes from v3:
- RSA weight removed from hash inputs
- Block and stake timestamps are masked (16-second granularity via
  `STAKE_TIMESTAMP_MASK = 15`, i.e. bottom 4 bits zeroed, so the effective
  time step is 2^4 = 16 seconds)
- Stake modifier is the most recently generated modifier walking back from
  `pindexPrev`

> **Source:** `src/gridcoin/staking/kernel.cpp:509-525`, `src/gridcoin/staking/kernel.h:15,30-33`

### 4.2 V8 Stake Weight

Simplified weight from UTXO value (integer division):

```
weight = nValueIn / 1,250,000
```

Since this is integer division (in halfords), the minimum UTXO value that
produces a nonzero weight is 1,250,000 halfords = **0.0125 GRC** (1/80 GRC).
Any UTXO with a value below 1/80 GRC has zero weight and cannot stake.

The proof-of-stake check verifies:

```
hash(kernel_inputs) <= target * weight
```

where `target` is derived from `nBits` using a 320-bit multiplication.

> **Source:** `src/gridcoin/staking/kernel.cpp:545-555,581-657`

### 4.3 Strict Validation

No grandfather exceptions apply for v8+ blocks. All validation checks are
enforced unconditionally.

### 4.4 hashProof Tracking

The computed proof hash (`hashProofOfStake`) is stored in the block index
as `hashProof` for:
- Duplicate stake detection via `g_seen_stakes`
- Stake modifier computation for subsequent blocks

> **Source:** `src/validation.cpp:2168-2193`

---

## 5. Block Version 9 — Superblock Quorum

**Activation:** Mainnet >= 1,144,000, Testnet >= 399,000

### 5.1 Superblock v1 with MD5 Quorum Hash

- Superblocks use an MD5-based quorum hash for convergence validation
- Tally activation occurs 120 blocks after v9 (`BlockV9TallyHeight`)

### 5.2 Legacy Tally

- Two-week rolling network average for dynamic magnitude unit computation
- Research Age accrual computer continues from pre-v8 era

### 5.3 Reward Validation

- Still uses coin-year reward formula
- `CheckReward` allows a 1 GRC wiggle tolerance for pre-v10 blocks

---

## 6. Block Version 10 — Constant Block Reward

**Activation:** Mainnet >= 1,420,000, Testnet >= 629,409

### 6.1 Constant Block Reward (CBR)

Replaces the coin-year interest rate formula with a fixed block reward:

- Default: **10 GRC** (`DefaultConstantBlockReward`)
- Overridable via administrative protocol entry with key `"blockreward1"`
  (value in halfords/satoshis)
- Pre-v13 clamp: `[0, 2 * DefaultConstantBlockReward]` = [0, 20 GRC]
- Pre-v13 protocol entry lookback: `StandardContractReplayLookback` (180 days)

The transition is controlled in `GetProofOfStakeReward()`:
```cpp
if (pindexLast->nVersion >= 10)
    return GetConstantBlockReward(pindexLast);
else
    return nCoinAge * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8);
```

> **Source:** `src/gridcoin/staking/reward.cpp:33-111`

### 6.2 Coinstake Output Limit

Raised from 3 to **8** for block versions 10 and 11. This enables:
- Stake splitting (multiple outputs back to staker)
- Voluntary sidestaking (outputs to configured addresses)

> **Source:** `src/validation.cpp:605-606`

---

## 7. Block Version 11 — Contract System and Snapshot Accrual

**Activation:** Mainnet >= 2,053,000, Testnet >= 1,301,500

Release: 5.0.0.0 "Fern" (2020-09-03)

This is the largest single consensus change in Gridcoin's history, introducing
the binary contract system, beacon registry, snapshot accrual, and superblock
v2.

### 7.1 Binary Contract System (Contract v2)

- All contract data serialized in binary format (contract version 2), replacing
  legacy XML-like string parsing
- Contract types dispatched to `IContractHandler` implementations via a registry
  pattern
- Contracts embedded in transaction version 2 (`tx.nVersion >= 2`)
- Maximum one contract per transaction (spam mitigation)
- Contracts prohibited in coinbase and coinstake transactions (except claim
  contracts in coinbase)
- Administrative contracts require master key input verification
- Standard burn fee: **0.5 GRC** per contract (sent to `OP_RETURN` output)

Contract types: BEACON, CLAIM, POLL, PROJECT, PROTOCOL, SCRAPER, VOTE, MRC,
SIDESTAKE (last two added in later versions).

> **Source:** `src/gridcoin/contract/contract.h:46-57`, `src/validation.cpp:147-200`

### 7.2 Beacon Registry (Beacon Payload v2)

- Beacons serialized in binary format (payload version 2)
- Lifecycle: pending → superblock activation → active (6-month expiry)
- Beacon key verification replaces RSA-based identity proof
- Beacon public key stored in registry; signature verification against
  claim context

### 7.3 Snapshot Accrual

Replaces Research Age accrual with periodic snapshots:

- Accrual snapshot stored per-CPID, computed from the last superblock's
  magnitude times magnitude unit times elapsed time
- Magnitude unit fixed at **1/4** (0.25 GRC per magnitude per day)
- Delta accrual since the active superblock is added to the snapshot
- O(1) lookup for any CPID's pending accrual

> **Source:** `src/gridcoin/accrual/snapshot.h:40-80`

### 7.4 Superblock v2 (SHA256 Quorum Hash)

- Superblock data serialized in binary (superblock version 2)
- Quorum hash computed using SHA256 instead of legacy MD5
- Scraper convergence protocol replaces voting for superblock consensus
- Contains: CPID magnitudes, project statistics, verified beacon list

### 7.5 Claim v2 and v3

- **Claim v2:** Binary-serialized claim data in coinbase contract
- **Claim v3:** Adds `last_block_hash` and coinstake transaction hash to the
  claim digest for signature verification, preventing claim replay attacks
- Magnitude looked up from the beacon/superblock registry instead of
  self-reported claim value

Although both versions belong to the v11 lineage, the v3 bump occurred during
v11 development (commit `9b9108be0`, "Sign claim contracts with coinstake
transaction") before mainnet v11 activated at block 2,053,000. From v11
activation onward the miner has unconditionally emitted v3 claims for pre-v12
blocks; claim v2 was a pre-release format and never appeared on mainnet. No
consensus rule gates claim version by height — the verifier accepts whichever
version it finds and selects the matching hash branch in `GetClaimHash()`.

> **Source:** `src/gridcoin/claim.h:27-69`, `src/gridcoin/claim.cpp:44-75`,
> `src/miner.cpp:1299-1300`

---

## 8. Block Version 12 — Manual Research Claims

**Activation:** Mainnet >= 2,671,700, Testnet >= 1,871,830

Release: 5.4.0.0 "Kermit's Mom" (2022-08-01)

### 8.1 MRC System

Manual Research Claims allow non-staking researchers to claim accrued research
rewards by submitting an MRC transaction that a staker includes in their
coinstake:

- MRC contract type (`CONTRACT_TYPE::MRC`), payload version 1
- MRC outputs are appended to the coinstake transaction
- MRC fee structure:
  - **Zero-payment interval:** 14 days (mainnet) / 10 min (testnet) after last
    reward — 100% of MRC reward forfeited as fees
  - **Post-zero-interval initial fee:** 40% (`InitialMRCFeeFractionPostZeroInterval`)
  - Fee decays over time toward the minimum
  - Foundation sidestake on MRC fees

### 8.2 MRC Output Limit

- Mainnet: **10** MRC outputs per coinstake (9 MRC slots + 1 foundation
  sidestake if foundation allocation is nonzero)
- Testnet: **3** MRC outputs per coinstake (2 + 1 foundation)

> **Source:** `src/validation.cpp:645-666`

### 8.3 Coinstake Output Limit (v12+)

The base coinstake output limit is:

```
10 (base) + GetMRCOutputLimit(block_version, true)
```

For mainnet: 10 + 10 = **20** total coinstake outputs.
For testnet: 10 + 3 = **13** total coinstake outputs.

The base limit of 10 accommodates the staker's own outputs plus anticipated
mandatory (non-MRC) sidestakes.

> **Source:** `src/validation.cpp:607-611`

### 8.4 Claim v4

- Adds `m_mrc_tx_map` to the claim structure, mapping MRC transaction hashes
  to their positions in the coinstake
- Claim payload version: **4** (current)

### 8.5 Stake Timestamp Mask Enforcement

Starting with v12, the coinstake transaction timestamp is required to equal
its masked value:

```cpp
if (Block.nVersion >= 12) {
    if (tx.nTime != MaskStakeTime(tx.nTime))
        return DoS(100, ...);
}
```

This enforces 16-second timestamp granularity on coinstake transactions
(since `STAKE_TIMESTAMP_MASK = 15`, masking zeroes the bottom 4 bits).

> **Source:** `src/gridcoin/staking/kernel.cpp:618-622`

### 8.6 Poll v3

- New poll payload version 3 with additional poll types
- Activated at `PollV3Height` (same height as v12 on mainnet, earlier on testnet)

### 8.7 Project v2

- GDPR and external adapter flags in project contract payload
- Activated at `ProjectV2Height`

### 8.8 Master Key Rotation

New compressed master key installed at height 2,671,700 (mainnet) and
1,964,600 (testnet), replacing the original uncompressed key.

> **Source:** `src/chainparams.cpp:172-175,254-257`

---

## 9. Block Version 13 — Mandatory Sidestakes and Configurable Parameters

**Activation:** Mainnet >= 3,989,800, Testnet >= 2,870,000

### 9.1 Mandatory Sidestakes

- Up to **4** mandatory sidestake outputs per coinstake
  (`GetMandatorySideStakeOutputLimit`)
- Maximum total allocation: **1/4** (25%) of block reward
  (`MaxMandatorySideStakeTotalAlloc`)
- Dust and degenerate case elimination: outputs below dust threshold are
  suppressed; when staker address matches a mandatory sidestake address, the
  output count is adjusted
- Sidestake contract type (`CONTRACT_TYPE::SIDESTAKE`), payload version 1
- Administrative contracts (master key required) to add/remove mandatory
  sidestakes

> **Source:** `src/validation.cpp:616-619`, `src/gridcoin/sidestake.h:491-495`

### 9.2 Configurable CBR

Block reward is configurable via protocol entry `"blockreward1"`:
- Default: 10 GRC (`DefaultConstantBlockReward`)
- Floor: 0 GRC (`ConstantBlockRewardFloor`)
- Ceiling: 500 GRC (`ConstantBlockRewardCeiling`)
- No lookback window limitation for protocol entries (v13+)

> **Source:** `src/gridcoin/staking/reward.cpp:61-73`

### 9.3 Configurable Magnitude Unit

Via protocol entry `"magnitudeunit"`:
- Default: 1/4
- Maximum: 5 (`MaxMagnitudeUnit`)
- Value specified as a fraction string (e.g., `"1/4"`, `"5"`)

### 9.4 Configurable Magnitude Weight Factor

Via protocol entry `"magnitudeweightfactor"`:
- Default: 100/567 (~0.1764)
- Minimum: 1/10 (`MinMagnitudeWeightFactor`)
- Maximum: 1 (`MaxMagnitudeWeightFactor`)

### 9.5 Superblock v3

- Adds project status and project "all CPID" total credits to superblock
  data to support automatic greylisting
- Superblock payload version: **3** (current)
- Activated at `SuperblockV3Height`

### 9.6 AutoGreylist

- Automatic exclusion of unresponsive BOINC projects based on Zero Credit
  Days (ZCD) and Whitelist Activity Score (WAS)
- Benefit-of-the-doubt audit logic activated at `AutoGreylistAuditHeight`
  (testnet 3,111,000)

### 9.7 Project v4

- Adds public key field for project-level cryptographic operations
- Activated at `ProjectV4Height`

### 9.8 Contract Payload Version Bumps

| Contract Type | New Payload Version | Notes |
|---|---|---|
| Beacon | v3 | V3 beacons with ownership proof (activated at v14) |
| Protocol | v2 | Binary-serialized protocol entries |
| Scraper | v2 | Binary-serialized scraper entries |
| Sidestake | v1 | New contract type |
| Contract (envelope) | v3 | Native serialization for all remaining payload types |

---

## 10. Block Version 14 — Script Timelocks and V3 Beacons

**Activation:** Mainnet >= 3,990,000, Testnet >= 3,126,500

### 10.1 BIP65 — OP_CHECKLOCKTIMEVERIFY

Enables `OP_CHECKLOCKTIMEVERIFY` (CLTV) for absolute time-locked scripts.
The `SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY` flag is set in `GetBlockScriptFlags()`
for v14+ blocks.

### 10.2 BIP112 — OP_CHECKSEQUENCEVERIFY

Enables `OP_CHECKSEQUENCEVERIFY` (CSV) for relative time-locked scripts.
The `SCRIPT_VERIFY_CHECKSEQUENCEVERIFY` flag is set in `GetBlockScriptFlags()`
for v14+ blocks.

### 10.3 BIP68 — Sequence Lock Enforcement

Relative lock-time semantics for transaction inputs based on `nSequence`
fields:
- Only enforced for `tx.nVersion >= 2`
- Height-based and time-based (512-second units) relative locks
- `CalculateSequenceLocks()` and `EvaluateSequenceLocks()` enforce the rules
- Checked via `CheckSequenceLocks()` during transaction validation

> **Source:** `src/consensus/tx_verify.cpp:74-207`

### 10.4 Script Flags Activation

```cpp
unsigned int GetBlockScriptFlags(const CBlockIndex& block_index)
{
    unsigned int flags{SCRIPT_VERIFY_P2SH};

    if (IsV14Enabled(block_index.nHeight)) {
        flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
        flags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    }

    return flags;
}
```

> **Source:** `src/validation.cpp:1624-1635`

### 10.5 HTLC Support

Hash Time-Locked Contracts enabled via `createhtlc`, `claimhtlc`, and
`refundhtlc` RPC commands. These use the newly-activated CLTV and CSV opcodes
for trustless atomic operations.

### 10.6 V3 Beacons with BOINC Ownership Proof

- Beacon payload version 3 adds RSA-SHA512 signature verification of BOINC
  project account ownership
- `beaconauth` RPC generates a beacon key and returns the public key hex
- `advertisebeaconv3` RPC accepts the BOINC project's XML response containing
  the ownership proof signature
- **V2 beacons remain valid in v14+ blocks.** Both the existing verification
  code flow (`advertisebeacon`) and the new BOINC ownership proof flow
  (`advertisebeaconv3`) are supported. The v3 beacon is an additional option,
  not a replacement — researchers may use either method.

> **Source:** `src/gridcoin/beacon.h:334-338`

---

## 11. Block Version 15 — On-Chain Pool Registration

**Activation:** Mainnet _not yet activated_, Testnet _not yet activated_

Issue [#1783](https://github.com/gridcoin-community/Gridcoin-Research/issues/1783) replaces the hardcoded `MiningPools` list with an on-chain registry so pool operators can self-register without a binary release. The V15 hard-fork machinery is present in code from the time of this PR; the activation height itself is a follow-up consensus decision and defaults to `std::numeric_limits<int>::max()` in `CMainParams` / `CTestNetParams` until pinned.

### 11.1 New Contract Types

Two new contract types share a single `PoolRegistry` backed by the standard `RegistryDB` template (history-tracked, reorg-safe). Both are gated by `IsV15Enabled` in `PoolRegistry::ValidateAtHeight` (called from both `Validate` at mempool admission and `BlockValidate` at block validation); pre-V15 they are silently invalid in both paths.

| Contract Type | Authority | Payload | Effect |
|---|---|---|---|
| `POOL_REGISTER` | Operator-self-signed (no master key) | CPID, name, URL, operator pubkey, signature | Lands in `PENDING` (or `DELETED` on REMOVE for operator self-withdrawal). The takeover defense applies to **both ADD and REMOVE**: on re-registration of an existing CPID, the signature must verify against the prior operator key on file. Without this, anyone could broadcast REMOVE for any pool and burn the slot. |
| `POOL_APPROVE` | Master-key (via `Contract::RequiresMasterKey()`) | CPID + (for `OPEN`) authorized operator pubkey | ADD flips to `ACTIVE`. REMOVE flips to `DELETED`. **OPEN** pre-authorizes a specific operator pubkey to claim a builtin pool slot — status is preserved from the prior chained entry; `Pool::m_authorized_operator_key` and `m_authorization_height` are set on the new chained entry. May also pre-stage placeholders for unknown CPIDs (foundation-managed entries; status `ACTIVE` on ADD, `DELETED` on REMOVE/OPEN). |

The `ContractAction` enum gains `OPEN` (parsed/printed as `"O"`); ADD and REMOVE keep their existing wire ordinals (1 and 2). The conditional serialization of `m_authorized_operator_key` only on OPEN means ADD/REMOVE payloads keep the original `(version, cpid) = 20 bytes` wire shape; OPEN extends to `(version, cpid, pubkey) = 53 bytes`.

> **Source:** `src/gridcoin/pool.h`, `src/gridcoin/pool.cpp`, `src/gridcoin/contract/payload.h`, dispatch in `src/gridcoin/contract/contract.cpp`.

### 11.2 Grandfathered Builtins

To eliminate a flag-day regression of pool-mode detection, `PoolRegistry`'s constructor pre-populates the same 5 CPIDs that the legacy `MiningPools` list carried (`src/gridcoin/researcher.h:88-95`). Each builtin is seeded at `m_height = 0`, status `ACTIVE`, with an invalid operator key (the slot is unclaimed until the foundation explicitly opens it). Synthetic per-CPID sentinel hashes (`Hash("POOL_BUILTIN_SEED:" + cpid)`) anchor the `m_previous_hash` chain so `Revert` of a first-real-contract restores the builtin unchanged.

Pre-V15, `ActivePoolsAtHeight(height < BlockV15Height)` returns exactly the legacy list bit-for-bit (verified by the `builtin_pools_match_g_mining_pools_pre_v15` unit test in `pool_tests.cpp`).

> **Source:** `src/gridcoin/pool.cpp` `BuiltinPoolSeeds()` / `SeedBuiltinPools()`.

### 11.3 Builtin Sticky-Claim Protection (Path 1 / Path 2)

Builtin pool slots are **permanently sticky** — every `POOL_REGISTER` on a builtin CPID is gated regardless of whether the slot was ever claimed before. The gate is implemented as a status-driven Path 1 / Path 2 split in `PoolRegistry::VerifyRegisterAuth` (called by both `Validate` and `BlockValidate`):

- **Path 1 — operational claimed builtin** (`m_status != DELETED && m_operator_key.IsValid()`). The slot has a live operator. Signature must verify against the existing operator key (rotation / withdrawal / update / takeover defense).
- **Path 2 — everything else** (fresh unclaimed seed, post-operator-REMOVE, post-Foundation-REMOVE of a previously-claimed builtin). The slot requires a fresh `POOL_APPROVE OPEN` authorization:
  1. `existing->m_authorized_operator_key.IsValid()` — Foundation must have OPEN'd; else reject.
  2. `at_height ≤ existing->m_authorization_height + consensus.PendingPoolRetention` — authorization not expired.
  3. `existing->m_authorized_operator_key == payload->m_operator_key` — payload key matches the pre-authorization.

If all three hold, the payload's own key authenticates the first claim (the new entry's operator key is set from the payload), and the auth fields are cleared on the chained entry — single-use consumption.

Discriminating on `m_status` rather than on `m_operator_key.IsValid()` closes a regression hole: a previously-claimed builtin that the Foundation later REMOVEs retains `K_real` on the propagated DELETED entry. A key-validity-only check would treat that state as "fall through to standard takeover" but the `!= DELETED` filter would exclude the entry, opening an identity-hijack window. The status-driven split sends that case to Path 2 (sticky), correctly requiring fresh OPEN authorization for re-claim.

#### Legitimate-claim flows for a builtin (e.g. `grcpool.com`)

Both variants are valid; the 3-step flow is preferred for wallet UX (no DELETED window where BOINC mode detection drops the pool).

**4-step flow** (REMOVE first):

1. Pre-V15: builtin seeded `ACTIVE`, empty operator key.
2. Post-V15: Foundation `POOL_APPROVE REMOVE` → status `DELETED`.
3. Opportunistic `POOL_REGISTER ADD` → Path 2 rejects (no auth).
4. Foundation `POOL_APPROVE OPEN` with `K_real` → status stays `DELETED`, auth fields set.
5. Real operator `POOL_REGISTER ADD` signed by `K_real`, payload key `K_real` → Path 2 accepts; status `PENDING`; auth cleared.
6. Foundation `POOL_APPROVE ADD` → status `ACTIVE`.

**3-step flow** (skip REMOVE — preferred):

1. Pre-V15: builtin seeded `ACTIVE`, empty operator key.
2. Post-V15: Foundation `POOL_APPROVE OPEN` with `K_real` directly on the still-`ACTIVE` seed → status stays `ACTIVE`, auth fields set. Wallets continue to see the pool as active during the transition.
3. Real operator `POOL_REGISTER ADD` signed by `K_real` → Path 2 accepts; status `PENDING`. Wallets briefly see PENDING.
4. Foundation `POOL_APPROVE ADD` → status `ACTIVE`.

If step 5 (4-step) / step 3 (3-step) doesn't happen within `consensus.PendingPoolRetention` blocks of the OPEN, the authorization expires at query time and the sticky-reject re-engages. Foundation can re-issue OPEN to refresh.

#### Authorization-field propagation

`m_authorized_operator_key` and `m_authorization_height` propagate through chained entries unchanged in `ApplyApprove` ADD/REMOVE. Only `ApplyApprove` OPEN sets them; only a successful `ApplyRegister` ADD that consumed the authorization clears them. This decouples auth validity from `m_height`, so Foundation admin actions between OPEN and the operator's claim don't shift the retention window. `ApplyRegister` constructs a fresh `Pool` from the payload, so the new entry starts with empty auth fields — implicit single-use clearing.

#### Non-builtin OPEN

`POOL_APPROVE OPEN` is admissible on any CPID, but the Path 2 sticky gate fires only on builtins. Foundation OPEN on a non-builtin CPID chains an entry with the auth fields set but no consensus rule treats them as required — non-builtin claims go through the standard first-claim path. Left admissible (rather than rejected) to support future extensions (e.g. curated whitelist mode).

> **Source:** `src/gridcoin/pool.cpp` `VerifyRegisterAuth`, `ValidateAtHeight`; `src/gridcoin/contract/payload.h` `ContractAction::OPEN`.

### 11.3.1 PENDING + OPEN Retention

`Consensus::Params::PendingPoolRetention` (default `28800` blocks ≈ 20 days at ~60s spacing) bounds two things:

- **PENDING entries** become query-time expired once `at_height > entry.m_height + retention`. The chained entry remains in storage but consensus treats it as absent for the takeover-check; a fresh `POOL_REGISTER ADD` from any key can supersede an expired PENDING **on a non-builtin slot**. On a builtin in Path 1 (claimed-operational), expired PENDING is NOT transparent — claimed-builtin protection persists regardless of retention.
- **OPEN authorizations** become query-time expired once `at_height > entry.m_authorization_height + retention`. Path 2 rejects POOL_REGISTER with an expired auth.

Both checks are pure functions of stored fields — no state mutation at the expiration boundary. Reorg back across the boundary naturally surfaces the entry as live again. This explicitly avoids the beacon-style stateful expiration which mutates state at superblock boundaries and requires reorg-resurrection logic.

#### Hidden `-pendingpoolretention` override

For isolated-testnet / regtest runs that need to exercise expiration boundaries without waiting ~20 days of mainnet-paced blocks, the hidden `-pendingpoolretention=N` arg (registered in `init.cpp`) shortens the window. **CONSENSUS-AFFECTING:** nodes with differing values disagree on POOL_REGISTER admission across expiration boundaries and **will fork off the network** if used on mainnet / public testnet. Mirrors the existing `-blockv15height` pattern in scope and warning.

> **Source:** `src/gridcoin/pool.cpp` `IsPendingExpired` / `IsAuthorizationExpired`; `src/chainparams.cpp` `GetPendingPoolRetention`; `src/init.cpp` hidden_args registration.

### 11.3.2 Operator Handbook (practical guidance)

- **Submit ADD within ~7 days of receiving Foundation OPEN.** A `POOL_REGISTER ADD` held in mempool through unusual block-inclusion delay can land at a height past `auth_height + retention` → rejected by Path 2 → 100 GRC burn for a tx the chain rejects (not actually burned since rejected at mempool / BlockValidate, but operator wastes fee-estimation + signing work). At 28800-block retention the practical risk is low; the guidance is conservative.
- **Foundation re-OPENs if no claim within ~21 days.** Keeps a comfortable margin before retention expires.
- **Do not bundle OPEN + operator-ADD + Foundation-ratify in the same block.** Order-dependence within a block is fragile — wrong tx ordering produces invalid blocks. Stage across blocks: OPEN in block N, operator's ADD in N+1, ratify in N+2.
- **Foundation does not `POOL_APPROVE ADD` before the operator's `POOL_REGISTER ADD` lands.** Doing so creates a zombie ACTIVE-with-no-operator-key state (the protocol doesn't enforce a PENDING precondition on `POOL_APPROVE ADD`); wallets would route BOINC users to a slot with no aggregation service. Order matters; always ratify a real PENDING.

### 11.4 Active Vote Weight Migration

The two consumers of the legacy `g_mining_pools` list in the voting subsystem are migrated to the registry's height-anchored API:

- `src/gridcoin/voting/registry.cpp` `PollReference::GetActiveVoteWeight`
- `src/gridcoin/voting/result.cpp` `VoteCounter::ProcessVote` (via cached `m_pool_cpids_at_start`)

Both call `GetPoolRegistry().ActivePoolsAtHeight(poll_start_height)` where `poll_start_height` is obtained from `PollReference::GetStartingHeight()`. The set used in AVW is therefore anchored to the poll's start height — every node tallying the same poll computes identical AVW even after post-V15 POOL contracts mutate the registry. Pre-V15 the registry returns the grandfathered builtins, which match the legacy `g_mining_pools` list bit-for-bit (see §11.2).

> **Source:** `src/gridcoin/pool.cpp` `ActivePoolsAtHeight`; consumer sites in `voting/registry.cpp` and `voting/result.cpp`.

### 11.5 Replay-Clamp Special-Case

`RegistryBookmarks::GetLowestRegistryBlockHeight` in `src/gridcoin/contract/registry.h:174` is extended with a POOL block analogous to the existing SIDESTAKE block, guarded on `BlockV15Height != std::numeric_limits<int>::max()`. Without the guard, an unactivated V15 would clamp the replay floor to `::max()` and break the clamp's overall semantics. With V15 activated, the clamp prevents contract replay from descending below `BlockV15Height` when the POOL bookmark height is still 0.

---

## 12. Cross-Cutting Consensus Rules

### 12.1 Script Verification Flags

Script verification flags vary by block version:

| Block Version | Consensus Flags |
|---|---|
| All versions | `SCRIPT_VERIFY_P2SH` |
| v14+ | + `SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY` + `SCRIPT_VERIFY_CHECKSEQUENCEVERIFY` |

Policy-level flags (applied to mempool transactions, not consensus):
`MANDATORY_SCRIPT_VERIFY_FLAGS` = P2SH | DERSIG | NULLDUMMY

> **Source:** `src/validation.cpp:1624-1635`, `src/policy/policy.h:19-21`

### 12.2 Block Version Enforcement

Blocks must meet the minimum version required at their height. Blocks below
the mandatory version for their height are rejected:
- Version 8 mandatory after `BlockV8Height`
- Each subsequent version mandatory after its activation height

### 12.3 Difficulty Retargeting

- Target block spacing: **90 seconds** (post-protocol v2), **60 seconds**
  (pre-protocol v2)
- Target timespan: **16 minutes** (960 seconds)
- Retarget interval: `nInterval = TARGET_TIMESPAN / nTargetSpacing` = 960 / 90 = **10**
  (integer division)
- Algorithm: exponential moving average toward target spacing

```
bnNew = bnPrev * ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing)
                / ((nInterval + 1) * nTargetSpacing)
```

where `nInterval = TARGET_TIMESPAN / nTargetSpacing`

- If `nActualSpacing < 0`, it is reset to `nTargetSpacing`
- Result clamped to `PROOF_OF_STAKE_LIMIT` (~arith_uint256() >> 20)
- Special difficulty resets:
  - Heights 91,387–91,500 (mainnet): reset to limit (Dec 2014 difficulty fix)
  - Any time current difficulty > 10^16: reset to limit

> **Source:** `src/gridcoin/staking/difficulty.cpp:22-90`

### 12.4 Stake Modifier Computation

The stake modifier is a 64-bit value recomputed every `nModifierInterval`
(10 minutes). It is built by selecting 64 blocks from candidates in a time
window and assembling their entropy bits:

1. Find the last generated stake modifier and its time
2. If not enough time has elapsed since the last modifier, reuse it
3. Collect candidate blocks in a selection interval
4. Sort candidates by timestamp (with block hash as tiebreaker)
5. For each of 64 rounds, select a block using a hash of (proof hash +
   previous modifier), favoring PoS blocks (hash >> 32)
6. Each selected block contributes one entropy bit to the new modifier

Special case: mainnet height 1,009,994 has a hardcoded modifier reset to
`0xdf209a3032807577` to correct a bug from the grandfather rule era.

> **Source:** `src/gridcoin/staking/kernel.cpp:20-384`

### 12.5 Stake Timestamp Mask

The `STAKE_TIMESTAMP_MASK` is 15 (0xF), which means:
- Stake timestamps have 16-second granularity (bottom 4 bits zeroed)
- `MaskStakeTime(t) = t & ~15`
- Enforced explicitly in coinstake validation for v12+ blocks
- Used implicitly in the v8 kernel hash computation for all v8+ blocks
- The wallet's staking loop iterates once per (UTXO selection and
  traversal time) + 8 seconds of sleep. As long as that total is
  <= 16 seconds, every mask quantum is covered. In practice, most
  computer/wallet combinations complete a loop iteration in well under
  16 seconds, meaning the loop traverses twice within the same quantum —
  but the second pass is degenerate because the masked timestamp has not
  changed, so the kernel hash is identical. Only one successful stake
  can occur per 16-second quantum. The staking status tracker uses
  these quantized intervals to compute actual staking efficiency
  (cumulative since last wallet restart) by counting mask intervals
  elapsed vs. successful stakes.

> **Source:** `src/gridcoin/staking/kernel.h:15,30-33`, `src/gridcoin/staking/status.cpp`

### 12.6 Transaction Version Requirements

- Transaction version 1: Legacy transactions, no contracts
- Transaction version 2: Required for binary contracts (v11+); enables
  sequence lock enforcement (BIP68 at v14+)

### 12.7 Contract Burn Fees

Every non-administrative contract transaction must include an `OP_RETURN`
output with a burn amount >= the required burn fee:
- Standard burn: **0.5 GRC** per contract
- Administrative contracts: validated by master key ownership, not burn fee

> **Source:** `src/gridcoin/contract/contract.h:57`, `src/validation.cpp:163-197`

### 12.8 Superblock Spacing

Superblocks are produced approximately once per day:
- Pre-height 364,500 (mainnet): every 43,200 seconds (12 hours)
- Post-height 364,500 (mainnet): every 86,400 seconds (24 hours)
- Testnet: always 86,400 seconds

> **Source:** `src/chainparams.h:187-190`

### 12.9 Coinbase Maturity

Coinbase and coinstake outputs require a minimum depth before they can be
spent:
- Mainnet: **100** blocks (+10 for `GetBlocksToMaturity()`)
- Testnet: **10** blocks

> **Source:** `src/main.cpp:86,695-696,1652`

---

## 13. Coinstake Output Structure Summary

| Block Version | Base Limit | MRC Limit | Mandatory SS Limit | Total Max |
|---|---|---|---|---|
| ≤ 9 | 3 | 0 | 0 | **3** |
| 10–11 | 8 | 0 | 0 | **8** |
| 12 (mainnet) | 10 | 10 | 0 | **20** |
| 12 (testnet) | 10 | 3 | 0 | **13** |
| 13+ (mainnet) | 10 | 10 | 4 | **20** |
| 13+ (testnet) | 10 | 3 | 4 | **13** |

Notes:
- The "base limit" for v12+ includes room for voluntary sidestakes and the
  staker's own split outputs
- MRC limit includes the foundation sidestake output (1 slot) when the
  foundation allocation is nonzero
- Mandatory sidestake outputs come from the base limit allocation, not in
  addition to it
- `GetCoinstakeOutputLimit()` returns base + MRC (the total)
- `GetMandatorySideStakeOutputLimit()` returns 4 for v13+, 0 otherwise

> **Source:** `src/validation.cpp:598-666`

---

## 14. Contract Payload Version Matrix

This table shows the current (maximum) payload version for each contract type
and when each version was introduced:

| Contract Type | v1 | v2 | v3 | v4 |
|---|---|---|---|---|
| **Envelope** | Legacy string | v11+ | v13+ (Scraper/Protocol native) | — |
| **Beacon** | Legacy string | v11+ | v14+ (ownership proof) | — |
| **Claim** | Legacy string | v11+ † | v11+ (coinstake in sig) | v12+ (MRC map) |
| **Poll** | Legacy string | v11+ | v12+ (PollV3Height) | — |
| **Vote** | v11+ ‡ | — | — | — |
| **Project** | Legacy K-V | v12+ (ProjectV2Height) | v13+ (gated by contract v3) | v13+ (ProjectV4Height) |
| **Protocol** | Legacy K-V | v13+ (contract v3) | — | — |
| **Scraper** | Legacy K-V | v13+ (contract v3) | — | — |
| **Superblock** | Legacy string | v11+ (SHA256 hash) | v13+ (SuperblockV3Height) | — |
| **MRC** | v12+ | — | — | — |
| **Sidestake** | v13+ | — | — | — |

For most payloads, v1 is the pre-binary legacy form (string or appcache K-V)
that gets parsed into a v1 instance, and v2 is the first native-binary format.
MRC, Sidestake, and Vote are exceptions: their v1 *is* native binary and they
have no legacy v1 entry — MRC and Sidestake never had a pre-binary form at
all, and pre-v11 votes live in a separate `LegacyVote` class rather than
being parsed as Vote v1.

Notes:
- The envelope is the outer container that wraps every payload. Envelope v2
  introduced the binary `vContracts` field on transactions at v11, dropping
  the v1 signature/pubkey that lived alongside the legacy hashBoinc XML.
  Envelope v3 at v13 completes the transition by moving the last two
  appcache-backed payloads (Scraper, Protocol) to native binary.
- Protocol and Scraper payloads were legacy K-V (appcache) through v11/v12;
  their payload v2 coincides with envelope v3 at v13.
- † Claim v2 was the initial binary-serialized format introduced during v11
  development. It was superseded by v3 (coinstake in signature) before v11
  activated on mainnet — no v11+ block has ever contained a v2 claim. The
  miner unconditionally emits v3 for pre-v12 blocks (`src/miner.cpp:1299`),
  and there is no consensus rule keying claim version to height; v2 remains
  only as a serialization branch retained for completeness.
- ‡ Vote v1 is native binary, introduced at v11 alongside the binary contract
  system. Unlike the other "v1 is the legacy form" payloads, pre-v11 votes
  are represented by a separate `LegacyVote` class (`src/gridcoin/voting/vote.h:180`)
  and are not parsed as Vote v1.

Current `CURRENT_VERSION` constants:

| Type | `CURRENT_VERSION` | Source |
|---|---|---|
| Contract (envelope) | 3 | `src/gridcoin/contract/contract.h:51` |
| Beacon (BeaconPayload) | 3 | `src/gridcoin/beacon.h:338` |
| Claim | 4 | `src/gridcoin/claim.h:38` |
| Poll (PollPayload) | 3 | `src/gridcoin/voting/payloads.h:29` |
| Vote | 1 | `src/gridcoin/voting/vote.h:28` |
| Project (ProjectEntry) | 4 | `src/gridcoin/project.h:66` |
| Protocol (ProtocolEntryPayload) | 2 | `src/gridcoin/protocol.h:215` |
| Scraper (ScraperEntryPayload) | 2 | `src/gridcoin/scraper/scraper_registry.h:245` |
| Superblock | 3 | `src/gridcoin/superblock.h:272` |
| MRC | 1 | `src/gridcoin/mrc.h:49` |
| Sidestake (SideStakePayload) | 1 | `src/gridcoin/sidestake.h:495` |

---

## 15. Reward Formula Summary

### 15.1 Pre-v10: Coin-Year Interest

```
staking_reward = coin_age * interest_rate * 33 / (365 * 33 + 8)
```

Where `interest_rate` follows the schedule in [Section 3.4](#34-pre-v10-coin-year-interest-rate-staking-reward).

Research reward (Research Age, pre-v11):
```
research_reward = accrual_days * avg_magnitude * magnitude_unit * COIN
```

Capped at `MaxResearchSubsidy * 255 * COIN` per block.

### 15.2 V10+: Constant Block Reward

```
staking_reward = GetConstantBlockReward(pindexLast)
```

- Default: 10 GRC
- Configurable via `"blockreward1"` protocol entry
- Pre-v13 clamp: [0, 20 GRC]
- V13+ clamp: [0, 500 GRC]

> **Source:** `src/gridcoin/staking/reward.cpp:33-99`

### 15.3 Research Accrual: Snapshot (v11+)

```
accrual = snapshot_balance + delta_accrual
delta_accrual = magnitude * magnitude_unit * elapsed_days * COIN
```

Where:
- `snapshot_balance` is the accumulated accrual stored per-CPID at the last
  tally point
- `magnitude` comes from the active superblock
- `magnitude_unit` = 1/4 (fixed, pre-v13) or configurable (v13+)
- `elapsed_days` = time since the active superblock, in days

### 15.4 Maximum Research Subsidy Schedule

The daily maximum research subsidy per CPID decreased over time (see
[Section 3.3](#33-research-age-mainnet--364501)). From Nov 20, 2015 onward,
the cap is **50 GRC/day** per CPID. Maximum single-block payout:
50 × 255 = **12,750 GRC**.

> **Source:** `src/gridcoin/accrual/research_age.h:24-42`

### 15.5 MRC Fees (v12+)

When a researcher submits an MRC:
1. If within the zero-payment interval (14 days mainnet / 10 min testnet since
   last reward): **100%** of the MRC reward is forfeited as fees
2. Otherwise: fees start at **40%** and decay over time
3. MRC fees are split between the staker and the foundation sidestake

> **Source:** `src/consensus/params.h:70-77`

---

## Appendix: Key Source File Index

| File | Contents |
|---|---|
| `src/consensus/params.h` | `Consensus::Params` struct — all consensus parameters |
| `src/consensus/consensus.h` | Block size, PoW limits, maturity constants |
| `src/chainparams.h` | `Is*Enabled()` height predicates, spacing helpers |
| `src/chainparams.cpp` | Mainnet/testnet parameter initialization |
| `src/validation.cpp` | `CheckReward`, coinstake output limits, `GetBlockScriptFlags`, `ConnectBlock` |
| `src/main.h` / `src/main.cpp` | `nGrandfather`, `nStakeMinAge`, `GetTargetSpacing` |
| `src/gridcoin/staking/kernel.h` | `STAKE_TIMESTAMP_MASK`, `MaskStakeTime`, kernel function declarations |
| `src/gridcoin/staking/kernel.cpp` | `CalculateStakeHashV8`, `CalculateStakeWeightV8`, stake modifier computation |
| `src/gridcoin/staking/reward.cpp` | `GetConstantBlockReward`, `GetProofOfStakeReward`, coin-year schedule |
| `src/gridcoin/staking/difficulty.cpp` | `GetNextTargetRequired`, retargeting algorithm |
| `src/gridcoin/accrual/research_age.h` | `GetMaxResearchSubsidy`, `ResearchAgeComputer` |
| `src/gridcoin/accrual/snapshot.h` | `SnapshotCalculator`, magnitude unit computation |
| `src/gridcoin/claim.h` | `Claim` class, version history |
| `src/gridcoin/contract/contract.h` | `Contract` class, burn fees, contract types |
| `src/gridcoin/beacon.h` | `BeaconPayload`, beacon lifecycle |
| `src/gridcoin/superblock.h` | `Superblock`, `QuorumHash` (MD5/SHA256) |
| `src/gridcoin/voting/payloads.h` | `PollPayload` |
| `src/gridcoin/voting/vote.h` | `Vote` payload |
| `src/gridcoin/sidestake.h` | `SideStakePayload` |
| `src/gridcoin/mrc.h` | `MRC` payload |
| `src/gridcoin/project.h` | `ProjectEntry` payload |
| `src/gridcoin/protocol.h` | `ProtocolEntryPayload` |
| `src/gridcoin/scraper/scraper_registry.h` | `ScraperEntryPayload` |
| `src/consensus/tx_verify.cpp` | `CalculateSequenceLocks`, `CheckSequenceLocks` (BIP68) |
| `src/miner.cpp` | Block creation, MRC/sidestake output assembly |
