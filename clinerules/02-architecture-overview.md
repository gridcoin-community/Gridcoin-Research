# Gridcoin Architecture Overview

## System Architecture

Gridcoin is a Bitcoin-derived proof-of-stake cryptocurrency with integrated research reward mechanisms. The architecture can be understood in several interconnected layers:

```
┌─────────────────────────────────────────────────────────────────┐
│                     USER INTERFACE LAYER                         │
│  Qt GUI (src/qt/) | RPC API (src/rpc/) | CLI (gridcoinresearchd) │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                   APPLICATION LOGIC LAYER                        │
│  Wallet | Miner/Staker | Researcher Context | Backup/Upgrade     │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                  GRIDCOIN BUSINESS LOGIC LAYER                   │
│  Contract System | Research Accounting | Consensus (Quorum)      │
│  Beacon Registry | Project Whitelist | Superblock Management    │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                   BLOCKCHAIN CONSENSUS LAYER                     │
│  Block Validation | Transaction Processing | Proof-of-Stake      │
│  Chain Management | Checkpoint System                            │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                    NETWORK & STORAGE LAYER                       │
│  P2P Networking (src/net.*) | LevelDB | Wallet DB | Scraper     │
│  BOINC Integration | Address Management | Backup System          │
└─────────────────────────────────────────────────────────────────┘
```

## Core Architectural Components

### 1. Contract System (`src/gridcoin/contract/`)
The contract system is Gridcoin's mechanism for blockchain-based governance and configuration changes.

**Key Concepts:**
- **Contracts** are special transactions that modify blockchain state
- **Contract Types**: Unknown, Beacon, Claim, Message, Poll, Project, Protocol, Scraper, Vote, MRC, SideStake (+ OUT_OF_BOUND marker)
- **Contract Actions**: Add, Delete, Remove
- **Note**: Not all contract types have persistent registry databases:
  - **With Registry DB**: Beacon, Project, Protocol, Scraper, SideStake
  - **Without Registry DB**: Claim, Message, Poll, Vote, MRC, Unknown, OUT_OF_BOUND
- **Handlers** process and validate contracts (registry pattern)
- **Registries** maintain current state for contract types with persistent storage

**Data Flow:**
```
Transaction → Contract Detection → Validation → Handler Dispatch → Registry Update
                                        ↓
                              (Burn Fee Verification)
```

### 2. Research Reward System (`src/gridcoin/`)

**Components:**
- **Beacon Registry** (`beacon.h/cpp`): Maps CPIDs to public keys for reward claims
- **Researcher Context** (`researcher.h/cpp`): Tracks local BOINC projects and eligibility
- **Tally System** (`tally.h/cpp`): Calculates research reward accruals
- **Magnitude** (`magnitude.h`): Measures relative research contribution (0-32767 scale)

**Reward Flow:**
```
BOINC Stats → Scraper Collection → Superblock Consensus →
Magnitude Assignment → Accrual Calculation → Stake Block Claim
```

### 3. Superblock & Quorum System (`src/gridcoin/quorum.*, superblock.*`)

**Purpose:** Quorum voting was the consensus mechanism pre-Fern. In Fern+ (block version 11+), the Quorum component remains as a facade for scraper convergence and superblock validation logic, but no voting occurs. Scraper convergence now achieves consensus.

**Process:**
1. **Scraper Nodes** collect BOINC project statistics independently
2. **Convergence** algorithm finds agreement among scraper manifests
3. **Superblock** created containing project stats and CPID magnitudes
4. **Quorum** validation ensures supermajority agreement
5. **Committed** to blockchain in the next staked block after 24 hours have passed since the last superblock and convergence has been achieved on statistics. In general, this equals ~24 hours + 45 seconds (half of the 90-second block interval).

**Validation Hierarchy:**
```
Raw Stats → Manifest Creation → Convergence Analysis →
Superblock Generation → Quorum Validation → Blockchain Commitment
```

### 4. Proof-of-Stake Consensus (`src/miner.*`, `src/validation.cpp`, `src/gridcoin/staking/`)

**Key Differences from Bitcoin:**
- **No mining**: Uses coin weight only (not coin-age, which was removed after block version 9) instead of proof-of-work
- **Research Rewards**: Stake blocks claim accumulated research rewards
- **Dual Subsidy**: Block reward = stake subsidy + research subsidy
- **Required Elements**: Kernel meets difficulty target, valid coinstake transaction

**Staking Process:**
```
UTXO Selection → Kernel Hash Calculation → Difficulty Check →
Coinstake Creation → Research Claim (if applicable) → Block Assembly → Broadcast
```

### 5. Accrual Accounting System (`src/gridcoin/tally.*`)

**Purpose:** Track research rewards earned but not yet claimed.

**Modes:**
- **Snapshot Mode** (current): Fast accrual calculation using periodic snapshots
- **Legacy Mode** (pre-v5): Full blockchain scan for each calculation

**Key Operations:**
- `RecordRewardBlock()`: Mark when CPID claims rewards
- `GetAccrual()`: Calculate pending rewards for a CPID
- `ApplySuperblock()`: Update magnitude assignments
- `LegacyRecount()`: Rebuild the two-week network averages (legacy mode)

### 6. Scraper System (`src/gridcoin/scraper/`)

**Function:** Distributed statistics collection from BOINC projects.

**Components:**
- **Scraper**: Downloads and parses project statistics files
- **Convergence**: Finds agreement among multiple scrapers
- **Manifest**: Signed package of collected statistics
- **Project Parts**: Individual project data with hash verification

**Redundancy:** Multiple independent scrapers ensure no single point of failure.

## Data Persistence

### Databases
- **Blockchain** (`blk*.dat`, `rev*.dat`): Block and undo data
- **LevelDB** (`blocks/index/`): Block index, transaction index
- **Wallet** (`wallet.dat`): Keys, transactions, metadata
- **Registry DBs**: Beacon, project, protocol, sidestake state
- **Note**: Registries are also persisted in LevelDB

### Configuration
- **gridcoinresearch.conf**: Read-only user settings (cannot be modified while wallet running)
- **gridcoinsettings.json**: Read-write settings (modified by wallet during runtime)
- **config.xml** (BOINC): Client state for researcher detection
- **Note**: Like Bitcoin Core, configuration is split between a read-only .conf file and a read-write .json file for runtime changes. However, not all config settings support modification while the wallet is running.

## Thread Architecture

Key threads (see `01-coding.md` for complete list):
- **grc-appinit2**: Main initialization
- **grc-net**: P2P networking
- **grc-msghand**: Message processing
- **grc-stake-miner**: Block staking
- **grc-scraper**: Statistics collection (alternative to subscriber)
- **grc-scraper-subscriber**: Receive manifests from other scrapers

## Critical Synchronization

**Locks:**
- `cs_main`: Blockchain state (most critical)
- `cs_wallet`: Wallet operations
- `pwalletMain->cs_wallet`: Wallet instance lock
- Registry-specific locks for contract state

**Lock Order:** Generally `cs_main` → `cs_wallet` to prevent deadlocks.

## Upgrade & Compatibility

**Version Transitions:**
- **Hard Forks**: Require blockchain-wide upgrade at specific height
- **Protocol Bumps**: Change P2P message format or validation rules
- **Soft Changes**: Backward compatible improvements

**Block Versions:**
- Version 12: Current consensus rules (mainnet), Version 13 in testnet/development
- Version 11: Previous consensus rules
- Version 10: Legacy superblock format
- Earlier versions: Phased out

## External Integrations

### BOINC
- **Detection**: Parse `client_state.xml` for projects and CPIDs
- **Statistics**: Scraper downloads from project stat export URLs
- **No Direct Communication**: Gridcoin reads BOINC state passively

### Network Services
- **Snapshot (Deprecated)**: Snapshot functionality has been deprecated in favor of full blockchain sync from genesis (typically < 5 hours on modern hardware). This improves security by eliminating trust in snapshot providers.
- **Update Checker**: Version notification system
- **DNS Seeders**: Bootstrap peer discovery

## Security Considerations

1. **Beacon Security**: Private key proves CPID ownership for reward claims
2. **Superblock Validation**: Multi-scraper consensus prevents manipulation
3. **Contract Burns**: Prevent spam by requiring burned coins
4. **Mandatory Sidestakes**: Protocol-enforced fee distributions
   - **Note**: Mandatory sidestakes are implemented in v13+ (currently testnet/development only)
5. **Split CPID Detection**: Prevents gaming by using same email across projects

**Poll Weight Types:**
- Only BALANCE and BALANCE_AND_MAGNITUDE PollWeightTypes are allowed, as these are immune to Sybil attacks without biometric identification.

## Performance Characteristics

- **Block Time**: ~90 seconds target
- **Superblock Interval**: ~1 day (daily consensus on magnitudes)
- **Beacon Lifetime**: ~6 months before renewal required
- **Accrual Limit**: ~16,384 GRC maximum (current), will scale with protocol parameters in Natasha (5.5.0.0)
- **Stake Weight**: Calculated from coin age and UTXO amount

## Future Architecture Notes

**Modular Design Goals:**
- Cleaner separation between consensus and business logic
- More testable components with dependency injection
- Reduced global state and lock contention
- Better abstraction of blockchain storage
- Scraper convergence is fundamental to superblock consensus

This architecture has evolved from Bitcoin's original design while adding substantial complexity for research reward integration. Understanding the contract system, superblock consensus, accrual accounting, and scraper convergence is essential for working with Gridcoin-specific features.
