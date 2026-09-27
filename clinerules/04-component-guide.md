# Gridcoin Component Guide

## Quick Component Reference

This guide maps major subsystems to their file locations and explains component interactions. Use this to quickly find where specific functionality lives.

---

## Directory Structure Overview

```
src/
├── gridcoin/              # Gridcoin-specific business logic
│   ├── contract/          # Contract system implementation
│   ├── accrual/           # Accrual calculation engines
│   ├── scraper/           # Scraper system (statistics collection & convergence)
│   └── *.h/cpp            # Core Gridcoin components
├── rpc/                   # RPC command implementations
├── qt/                    # Qt GUI components
├── test/                  # Unit tests
├── consensus/             # Consensus parameter definitions
├── primitives/            # Basic blockchain types (block, transaction)
├── crypto/                # Cryptographic primitives
├── policy/                # Policy constants
├── support/               # Utility functions
└── *.h/cpp                # Bitcoin-inherited core (main, net, wallet, etc.)
```

---

## Core Components

### 1. Contract System (`src/gridcoin/contract/`)

**Purpose**: Blockchain-based governance and state modification system.

#### Files & Responsibilities

| File | Purpose |
|------|---------|
| `contract.h/cpp` | Core contract parsing, validation, dispatch |
| `handler.h` | Interface for contract handlers (IContractHandler) |
| `payload.h` | Base interface for contract payloads (IContractPayload) |
| `message.h/cpp` | Contract transaction creation and broadcasting |
| `registry.h` | Registry management utilities and access helpers |
| `registry_db.h` | Template for persistent registry storage (LevelDB) |

#### Key Classes
- **Contract**: Parsed contract with type, action, and payload
- **ContractContext**: Contract + block context for validation
- **IContractHandler**: Interface for processing contracts
- **IContractPayload**: Interface for contract-specific data
- **Dispatcher**: Routes contracts to appropriate handlers

#### Interaction Pattern
```
RPC/GUI → Contract Creation → Transaction Broadcast →
Block Validation → Dispatcher → Handler → Registry Update
```

---

### 2. Beacon System (`src/gridcoin/beacon.h/cpp`)

**Purpose**: Link CPIDs to wallet addresses for reward claiming.

#### Key Classes
- **Beacon**: Public key + metadata for CPID verification
- **BeaconPayload**: Contract payload for beacon advertisements
- **BeaconRegistry**: Global beacon state manager (IContractHandler)
- **PendingBeacon**: Beacon awaiting superblock activation

#### Storage
- **Database**: `beacon.dat` (LevelDB)
- **In-Memory**: Active beacon map (Cpid → Beacon_ptr)
- **Historical**: Hash-based lookup for past beacons

#### Lifecycle
```
Generate Key → Create Payload → Sign → Broadcast Contract →
Pending State → Superblock Verification → Active →
(5 months) → Renewable → (6 months) → Expired
```

#### Key Operations
- `BeaconRegistry::Try(cpid)`: Find any beacon
- `BeaconRegistry::TryActive(cpid)`: Find active beacon only
- `BeaconRegistry::ActivatePending()`: Promote pending to active
- `BeaconRegistry::Validate()`: Contract validation

---

### 3. Research Reward System

#### 3a. Researcher Context (`src/gridcoin/researcher.h/cpp`)

**Purpose**: Local BOINC detection and eligibility tracking.

**Key Classes**:
- **Researcher**: Singleton managing local researcher state
- **MiningProject**: Individual BOINC project configuration
- **MiningProjectMap**: Collection of detected projects
- **AdvertiseBeaconResult**: Result of beacon advertisement attempt

**Detection Process**:
```
Read BOINC client_state.xml → Parse Projects →
Validate Team Membership → Check Whitelist →
Compute CPID → Determine Eligibility
```

**Key Methods**:
- `Researcher::Get()`: Access singleton instance
- `Researcher::Reload()`: Refresh BOINC detection
- `Researcher::AdvertiseBeacon()`: Create/renew beacon
- `Researcher::Magnitude()`: Query current magnitude from superblock

#### 3b. Tally System (`src/gridcoin/tally.h/cpp`)

**Purpose**: Calculate research reward accruals and track payment history.

**Key Classes**:
- **ResearchAccount**: Per-CPID accounting record
- **ResearcherTally** (internal): Manages all accounts
- **NetworkTally** (internal): Network-wide magnitude calculations

**Storage**:
- **Snapshots**: Periodic baseline snapshots for fast calculation
- **In-Memory**: Active accounts and accrual state

**Key Operations**:
- `Tally::GetAccrual(cpid, pindex)`: Calculate pending rewards
- `Tally::RecordRewardBlock(pindex)`: Mark reward claim
- `Tally::ApplySuperblock(superblock)`: Update magnitudes
- `Tally::Initialize(pindex)`: Load/rebuild from blockchain

**Calculation Modes**:
- **Snapshot** (current): O(1) accrual lookups using periodic snapshots
- **Legacy**: O(n) full blockchain scan from last payment

#### 3c. Magnitude (`src/gridcoin/magnitude.h`)

**Purpose**: Type-safe representation of research contribution.

**Key Class**:
- **Magnitude**: Scaled uint32_t (0-32767 × 1000 for precision)

**Conversions**:
- `Scaled()`: Internal representation (0-32,767,000)
- `Compact()`: Blockchain format (0-32,767)
- `Floating()`: Human-readable (0.000-32.767)

---

### 4. Superblock & Consensus System

#### 4a. Superblock (`src/gridcoin/superblock.h/cpp`)

**Purpose**: Snapshot of network research statistics.

**Key Classes**:
- **Superblock**: Contains magnitudes, projects, verified beacons
- **SuperblockPtr**: Smart pointer with block index binding
- **QuorumHash**: Content identifier (SHA256 or legacy MD5)
- **Superblock::CpidIndex**: Compressed magnitude storage
- **Superblock::ProjectIndex**: Project statistics

**Compression**:
- Zero magnitudes stored as count (not individual entries)
- Run-length encoding for repeated values
- Efficient serialization format

**Key Methods**:
- `Superblock::FromStats()`: Build from scraper statistics
- `Superblock::GetHash()`: Compute quorum hash
- `Superblock::WellFormed()`: Validate structure
- `SuperblockPtr::ReadFromDisk()`: Load from block

#### 4b. Quorum (`src/gridcoin/quorum.h/cpp`)

**Purpose**: Consensus validation and superblock management.

**Key Classes**:
- **Quorum** (static methods): Global consensus interface
- **SuperblockIndex** (internal): Current/pending superblock tracking
- **SuperblockValidator** (internal): Multi-level validation engine
- **LegacyConsensus** (internal): Pre-v11 voting system

**Validation Levels**:
1. **Structure**: Well-formed, correct format
2. **Content**: Beacon IDs valid, project data reasonable
3. **Convergence**: Matches scraper agreement
4. **Quorum**: Supermajority hash match

**Key Methods**:
- `Quorum::ValidateSuperblock()`: Full validation
- `Quorum::CurrentSuperblock()`: Get active superblock
- `Quorum::GetMagnitude(cpid)`: Query CPID magnitude
- `Quorum::CommitSuperblock()`: Promote pending to current

**Convergence Algorithm**:
```
Scraper Manifests → Part Hash Comparison →
Project-Level Agreement → Combine Compatible Parts →
Generate Candidates → Validate Supermajority → Select Best
```

---

### 5. Project Whitelist System (`src/gridcoin/project.h/cpp`)

**Purpose**: Manage approved BOINC projects and eligibility.

**Key Classes**:
- **ProjectEntry**: Individual project record
- **Project**: Contract payload version of ProjectEntry
- **Whitelist**: Registry of approved projects (IContractHandler)
- **WhitelistSnapshot**: Thread-safe immutable view of whitelist
- **AutoGreylist**: Automatic project exclusion system

**Storage**:
- **Database**: `project.dat` (LevelDB)
- **In-Memory**: Active project map (name → ProjectEntry_ptr)

**Key Methods**:
- `Whitelist::Snapshot()`: Get thread-safe project list
- `Whitelist::Validate()`: Contract validation
- `WhitelistSnapshot::Contains(name)`: Check project eligibility

**Auto-greylist Triggers**:
- **ZCD** (Zero Credit Days): Project shows no activity for N days
- **WAS** (Whitelist Activity Score): Project below activity threshold

**Reference**: See `doc/automated_greylisting_design_highlights.md`

---

### 6. Protocol Configuration (`src/gridcoin/protocol.h/cpp`)

**Purpose**: Blockchain-stored protocol parameters.

**Key Classes**:
- **ProtocolEntry**: Configuration parameter record
- **ProtocolEntryPayload**: Contract payload for parameter changes
- **ProtocolRegistry**: Parameter registry (IContractHandler)

**Common Parameters**:
- Minimum stake age
- Superblock intervals
- Version requirements
- Feature activation flags

**Access Pattern**:
```cpp
auto entry = GetProtocolRegistry().TryActive("KEY");
if (entry) {
    std::string value = entry->m_value;
}
```

---

### 7. Side Stake System (`src/gridcoin/sidestake.h/cpp`)

**Purpose**: Automatic reward distribution to specified addresses.

**Key Classes**:
- **SideStake**: Unified interface for local/mandatory sidestakes
- **LocalSideStake**: Voluntary sidestake from config
- **MandatorySideStake**: Protocol-enforced sidestake
- **SideStakeRegistry**: Combined registry (IContractHandler)
- **Allocation**: Precise fractional percentage

**Storage**:
- **Local**: `gridcoinresearch.conf` file
- **Mandatory**: `sidestake.dat` (LevelDB)

**Key Methods**:
- `SideStakeRegistry::ActiveSideStakeEntries()`: Get all active sidestakes
- `SideStakeRegistry::GetMandatoryAllocationsTotal()`: Sum mandatory percentages
- `SideStakeRegistry::NonContractAdd()`: Add local sidestake

**Integration**:
- Applied during coinstake output creation in `src/miner.cpp`

---

### 8. MRC System (`src/gridcoin/mrc.h/cpp`)

**Purpose**: Manual research reward claims for non-staking researchers.

**Key Classes**:
- **MRC**: Manual reward claim contract payload
- **MRCRegistry**: Validation handler (stateless)

**Process**:
```
Check Accrual > 0 → Calculate Fee → Create MRC Contract →
Sign with Beacon Key → Broadcast → Validation →
Claim Processed (no registry storage needed)
```

**Fee Calculation**:
- Based on claimed amount and protocol-defined percentage
- Higher for older accruals (incentivizes regular claiming)

---

### 9. Scraper System (`src/gridcoin/scraper/`)

**Purpose**: Collect, validate, and distribute BOINC project statistics for superblock generation. Scraper nodes download project stats, package them into signed manifests, and distribute via P2P. Subscriber nodes receive manifests from multiple scrapers and perform convergence to reach consensus on network statistics.

#### Files & Responsibilities

| File | Purpose |
|------|---------|
| `scraper.h/cpp` | Main scraper logic, statistics downloads, convergence |
| `fwd.h` | Forward declarations, data structures, enums |
| `http.h/cpp` | HTTP client for project statistics downloads |
| `scraper_net.h/cpp` | Manifest P2P networking and distribution |
| `scraper_registry.h/cpp` | Authorization contract management (IContractHandler) |

#### Key Classes
- **CScraperManifest**: Signed statistics package with projects and beacon data
- **ConvergedManifest**: Consensus result from multiple scrapers (manifest or project-level)
- **ScraperStats**: Parsed project statistics (TC, RAC, magnitudes)
- **ScraperEntry**: Authorization entry with status (AUTHORIZED, EXPLORER, etc.)
- **ScraperRegistry**: Contract-based authorization management
- **CSplitBlob**: Abstract base for part-based data distribution

#### Authorization System
Two-level authorization model controls scraper operations:
- **Level 1**: `IsScraperAuthorized()` - Network-wide policy for downloading statistics
- **Level 2**: `IsScraperAuthorizedToBroadcastManifests()` - Per-node permission to publish manifests

Authorization managed via SCRAPER contracts with statuses:
- **NOT_AUTHORIZED**: Cannot publish manifests
- **AUTHORIZED**: Can publish manifests
- **EXPLORER**: Extended statistics retention permissions

#### Convergence Process
Determines consensus from multiple scraper manifests:
1. **Manifest-level** (preferred): Multiple scrapers publish identical content hashes
2. **Project-level** (fallback): Agreement on per-project basis when manifests differ
3. **Supermajority**: `NumScrapersForSupermajority(count)` determines threshold
4. **Output**: `ScraperGetSuperblockContract()` generates superblock from convergence

#### Key Operations
**Active Scraper Mode**:
- Download project statistics via HTTP
- Parse and validate statistics files
- Create signed manifests with beacon verification data
- Publish manifests to network via `CScraperManifest::addManifest()`

**Passive Subscriber Mode**:
- Receive manifests from authorized scrapers
- Validate manifest signatures and authorization
- Perform convergence analysis
- Generate superblock contracts from consensus

**Storage**:
- **Database**: Authorization stored in main LevelDB (shared with blockchain) using registry-specific key prefix via `KeyType()`
- **Data Directory**: `<data directory>/Scraper` (mainnet) or `/testnet/Scraper` (testnet)

**Files on Authorized/Active Scraper Nodes**:
- `Manifest.csv.gz` - Compressed text file containing the File Manifest (listing of current project statistics files)
- `BeaconList.csv.gz` - Compressed text file containing list of active beacons
- `VerifiedBeacons.dat` - Binary file containing serialized list of pending beacons verified in latest convergence (will be activated on next superblock)
- `<project>-<etag>.csv.gz` - Compressed text files containing project statistics snapshots filtered for active CPIDs (beacons). The etag represents the fingerprint of those stats and is recorded in the filename.
- `Stats.csv.gz` - Statistics map output for current scraper stats based on last file manifest entry
- `ConvergedStats.csv.gz` - Statistics map output for current convergence
- Additional files present in explorer mode (raw unprocessed project statistics)

**Files on Scraper Subscriber (Normal) Nodes**:
- `ConvergedStats.csv.gz` - Only file present; represents current convergence output calculated from received manifests/project data

**Thread**: `ThreadScraper()` (active) or `ThreadScraperSubscriber()` (passive)

**Global Access**: `GetScraperRegistry()` → ScraperRegistry&

---

### 10. Staking & Mining (`src/miner.h/cpp`)

**Purpose**: Block creation via proof-of-stake.

**Key Functions**:
- `StakeMiner()`: Main staking loop
- `CreateCoinStake()`: Build coinstake transaction
- `CreateRestOfTheBlock()`: Fill the block with mempool transactions
- `CreateGridcoinReward()`: Compute the stake and research reward, add it to the coinstake, and fill in the claim
- `CreateMRCRewards()`: Add the MRC reward outputs
- `SignStakeBlock()`: Sign the finished block
- `CheckProofOfStakeV8()` (`src/gridcoin/staking/kernel.cpp`): Kernel validation

**Staking Process**:
```
Select UTXOs → Calculate Kernel Hash → Check Difficulty →
Create Coinstake → Add Research Claim (if eligible) →
Apply Sidestakes → Build Block → Sign → Broadcast
```

**Research Integration**:
- Query `Tally::GetAccrual()` for pending rewards
- Validate beacon with `BeaconRegistry::TryActive()`
- Include CPID and claim in coinstake

**Thread**: `ThreadStakeMiner()` (see `01-coding.md`)

---

### 11. RPC Interface (`src/rpc/`)

**Purpose**: External API for wallet control and queries.

#### Key Files by Category

**Blockchain**:
- `blockchain.cpp`: Block/chain queries
- `mining.cpp`: Staking controls (getmininginfo, getstakinginfo)

**Wallet** (in `src/wallet/`, not `src/rpc/`):
- `wallet/rpcwallet.cpp`: Balance, transactions, addresses
- `wallet/rpcdump.cpp`: Key import/export

**Gridcoin-Specific**:
- `blockchain.cpp`: Also holds the researcher, beacon, superblock, protocol and scraper RPCs (`beaconstatus`, `magnitude`, `superblocks`, `listprotocolentries`, `listscrapers`, `addkey`)
- `voting.cpp`: Polls and votes

**Network**:
- `net.cpp`: Peer management
- `server.cpp`: RPC server core

**Registration Pattern** (one row per command in `vRPCCommands[]`, `src/rpc/server.cpp`):
```cpp
static const CRPCCommand vRPCCommands[] =
{
    { "name", &impl, cat_x, &impl_helpman, heritage_<bucket>, "<fp>" },
    // ...
};
```
The heritage bucket and fingerprint are mandatory; see the RPC Heritage Ledger section of `CLAUDE.md`.

---

### 12. GUI Layer (`src/qt/`)

**Purpose**: Qt-based graphical user interface.

#### Key Components

**Main Windows**:
- `bitcoingui.h/cpp`: Main window and menu structure
- `overviewpage.h/cpp`: Dashboard view
- `researchermodel.h/cpp`: BOINC status display

**Dialogs**:
- `researcher/`: Beacon management, project display
- `voting/`: Voting system UI

**Models**:
- `walletmodel.h/cpp`: Wallet state and operations
- `clientmodel.h/cpp`: Network and blockchain state
- `transactiontablemodel.h/cpp`: Transaction list

**Forms**:
- `forms/*.ui`: Qt Designer UI definitions

**Integration**:
- Signals/slots connect models to views
- RPC calls or direct C++ access for data

---

### 13. Blockchain Core (`src/`)

**Bitcoin-Inherited Components** (modified for Gridcoin):

| File | Purpose |
|------|---------|
| `validation.h/cpp` | Block and transaction validation, consensus |
| `node/chainman.h/cpp` | Chain management: `ProcessBlock`, `SetBestChain`, reorganization |
| `node/blockstorage.h/cpp` | Block files and block index loading |
| `net.h/cpp`, `net_processing.h/cpp` | P2P networking, peer management, message handling |
| `wallet/wallet.h/cpp` | Wallet operations, transaction creation |
| `init.cpp` | Initialization and shutdown |
| `txdb.h` | Transaction database interface |
| `chainparams.h/cpp` | Network parameters (mainnet, testnet) |

**Gridcoin Modifications**:
- Contract validation integrated into block validation
- Research reward calculation in block rewards
- Superblock commitment validation
- Enhanced transaction types (coinstake with claims)

---

### 14. Utilities & Support

#### Backup System (`src/gridcoin/backup.h/cpp`)
- Automatic wallet backups
- Configuration backups
- Scheduled backup jobs

#### Upgrade System (`src/gridcoin/upgrade.h/cpp`)
- Version checking
- Snapshot downloads
- Blockchain data reset

#### BOINC Integration (`src/gridcoin/boinc.cpp`)
- Detect BOINC data directory
- Parse client_state.xml

#### Appcache (`src/gridcoin/appcache.h/cpp`)
- Legacy key-value storage (phased out - complete removal pending)
- Compatibility with older protocol entries

---

## Component Interaction Diagram

### High-Level Data Flow

```
┌─────────────┐
│   RPC/GUI   │ User Interface Layer
└──────┬──────┘
       │
┌──────▼──────────────────────────────────┐
│  Application Layer                       │
│  Wallet | Miner | Researcher | Backup    │
└──────┬───────────────┬───────────────────┘
       │               │
       │          ┌────▼────────┐
       │          │  Contracts  │ Governance Layer
       │          │  System     │
       │          └────┬────────┘
       │               │
┌──────▼───────────────▼──────────────────┐
│  Gridcoin Business Logic                │
│  Beacon | Tally | Quorum | Whitelist    │
└──────┬──────────────────────────────────┘
       │
┌──────▼──────────────────────────────────┐
│  Blockchain Consensus                    │
│  Block Validation | Chain Management     │
└──────┬──────────────────────────────────┘
       │
┌──────▼──────────────────────────────────┐
│  Storage & Network                       │
│  LevelDB | P2P | Scraper | BOINC         │
└──────────────────────────────────────────┘
```

### Example: Research Reward Claim

```
1. Miner finds valid kernel
   └─→ src/miner.cpp: CreateCoinStake()

2. Check if researcher
   └─→ src/gridcoin/researcher.cpp: Researcher::Get()

3. Get current magnitude
   └─→ src/gridcoin/quorum.cpp: Quorum::GetMagnitude()

4. Calculate accrual
   └─→ src/gridcoin/tally.cpp: Tally::GetAccrual()

5. Verify beacon
   └─→ src/gridcoin/beacon.cpp: BeaconRegistry::TryActive()

6. Add claim to coinstake
   └─→ Include CPID and research subsidy in transaction

7. Build and sign block
   └─→ src/miner.cpp: CreateRestOfTheBlock(), CreateGridcoinReward(), SignStakeBlock()

8. Validate claim
   └─→ src/validation.cpp: ConnectBlock() / GridcoinConnectBlock()
   └─→ src/gridcoin/claim.cpp: Claim::VerifySignature()

9. Record payment
   └─→ src/gridcoin/tally.cpp: Tally::RecordRewardBlock()
```

### Example: Superblock Consensus

```
1. Scrapers collect stats independently
   └─→ src/gridcoin/scraper/scraper.cpp: ProcessProjectStatsFromStreamByCPID()

2. Create manifests
   └─→ Sign and publish via P2P

3. Collect manifests
   └─→ src/gridcoin/quorum.cpp: Convergence logic

4. Find agreement
   └─→ Project-by-project comparison
   └─→ Build candidate superblocks

5. Validate convergence
   └─→ src/gridcoin/quorum.cpp: SuperblockValidator

6. Commit to blockchain
   └─→ Special contract in block
   └─→ src/gridcoin/quorum.cpp: Quorum::CommitSuperblock()

7. Activate pending beacons
   └─→ src/gridcoin/beacon.cpp: BeaconRegistry::ActivatePending()

8. Update magnitudes
   └─→ src/gridcoin/tally.cpp: Tally::ApplySuperblock()
```

---

## Key Singletons & Global Access

**Registry Access Functions** (from respective .cpp files):
- `GetBeaconRegistry()` → BeaconRegistry&
- `GetWhitelist()` → Whitelist&
- `GetProtocolRegistry()` → ProtocolRegistry&
- `GetSideStakeRegistry()` → SideStakeRegistry&

**Researcher Context**:
- `Researcher::Get()` → ResearcherPtr (shared_ptr)

**Global Locks** (in order):
1. `cs_main` - Blockchain state
2. `cs_wallet` - Wallet operations
3. Registry-specific locks

---

## Testing (`src/test/`)

**Test Organization**:
- `gridcoin/` - Gridcoin-specific unit tests
- `*.cpp` - General unit tests
- Test fixtures for blockchain state

**Key Test Files**:
- `gridcoin/beacon_tests.cpp` - Beacon system
- `gridcoin/claim_tests.cpp` - Research claims
- `gridcoin/superblock_tests.cpp` - Superblock parsing/validation
- `gridcoin/mrc_tests.cpp` - Manual research claims and their accrual

**Running Tests**:
```bash
./build/src/test/test_gridcoin
```

---

## Build System

**CMake** (primary):
- `CMakeLists.txt` (root)
- `src/CMakeLists.txt` (source configuration)

---

## Quick Component Lookup

**Need to modify...**

| Task | Primary Files |
|------|---------------|
| Beacon logic | `src/gridcoin/beacon.h/cpp` |
| Accrual calculation | `src/gridcoin/tally.h/cpp` |
| Superblock validation | `src/gridcoin/quorum.cpp`, `superblock.cpp` |
| Staking rewards | `src/miner.cpp`, `src/gridcoin/staking/reward.cpp` |
| RPC command | `src/rpc/<category>.cpp` |
| GUI dialog | `src/qt/<feature>/` |
| Contract type | Add to `src/gridcoin/contract/` |
| Consensus rules | `src/validation.cpp`, `src/gridcoin/<component>` |
| Network protocol | `src/net.cpp`, `src/protocol.cpp` |
| Scraper statistics | `src/gridcoin/scraper/scraper.cpp` |

---

## Related Documentation

- **Architecture**: `02-architecture-overview.md` - System design
- **Glossary**: `03-core-concepts-glossary.md` - Terminology
- **Tasks**: `05-common-tasks.md` - How-to guides
- **Coding**: `01-coding.md` - Style and conventions

This component guide helps you navigate the codebase efficiently. When starting work, identify which components are involved, then dive into the specific files.
