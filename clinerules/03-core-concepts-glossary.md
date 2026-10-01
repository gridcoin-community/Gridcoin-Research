# Gridcoin Core Concepts Glossary

## Quick Reference Guide

This glossary provides definitions for Gridcoin-specific terminology. Understanding these concepts is essential for working with the codebase.

---

## Research & Identity

### CPID (Cross-Project Identifier)
- **Definition**: MD5 hash that uniquely identifies a researcher across all BOINC projects
- **Format**: 32-character hexadecimal string (16 bytes)
- **Calculation**: `MD5(internal_cpid + email)`
- **Purpose**: Links BOINC work to blockchain rewards without revealing email
- **Location**: `src/gridcoin/cpid.h/cpp`
- **Example**: `a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6`

### Mining ID
- **Definition**: Variant type representing researcher identity state
- **Types**:
  - `Cpid`: Valid researcher with BOINC projects
  - `Noncruncher`: Investor mode (no research)
  - `Invalid`: No valid configuration
- **Location**: `src/gridcoin/cpid.h`

### Split CPID
- **Definition**: Using different email addresses across BOINC projects
- **Effect**: Creates multiple CPIDs, which is prohibited
- **Detection**: Automatic validation during researcher detection
- **Consequence**: Prevents beacon advertisement and reward claims

---

## Beacon System

### Beacon
- **Definition**: Cryptographic advertisement linking wallet address to CPID
- **Components**:
  - Public key (for verification)
  - Timestamp (for expiration tracking)
  - Hash (unique identifier)
  - Signature (proves ownership)
- **Lifetime**: ~6 months (renewable after 5 months)
- **Purpose**: Proves CPID ownership when claiming research rewards in stake blocks
- **Location**: `src/gridcoin/beacon.h/cpp`

### Beacon Registry
- **Definition**: Global registry tracking all active and historical beacons
- **Storage**: Persistent database + in-memory index
- **States**: Pending → Active → Renewable → Expired
- **Key Methods**:
  - `Try()`: Find beacon for CPID
  - `TryActive()`: Find active beacon
  - `ContainsActive()`: Check beacon existence
- **Contract Type**: `ContractType::BEACON`

### Pending Beacon
- **Definition**: Beacon awaiting activation in next superblock
- **Duration**: Pending until next superblock commit
- **Requirement**: Must appear in superblock's verified beacon list to activate

---

## Magnitude & Rewards

### Magnitude
- **Definition**: Measure of research contribution relative to network
- **Scale**: 0-32767 (uint16_t, 1000x actual magnitude for precision)
- **Formula**: `(User RAC / Total Network RAC) × Total Active Magnitude`
- **Total Pool**: Distributed proportionally to all researchers
- **Zero Magnitude**: Inactive or ineligible researchers
- **Location**: `src/gridcoin/magnitude.h`

### RAC (Recent Average Credit)
- **Definition**: BOINC's metric for computing power contribution
- **Source**: Calculated by BOINC projects
- **Collection**: Scrapers harvest from project statistics
- **Usage**: Input for magnitude calculations in superblocks

### Accrual
- **Definition**: Research rewards earned but not yet claimed
- **Calculation**: `Σ(magnitude × magnitude_unit × time_period)`
- **Limit**: ~16,384 GRC maximum (configurable)
- **Claiming**: Included in coinstake transaction of staked block
- **Location**: `src/gridcoin/tally.h/cpp`

### MRC (Manual Reward Claim)
- **Definition**: Contract allowing non-staking researchers to claim accrued rewards
- **Fee**: Percentage of claimed amount (protocol-defined)
- **Purpose**: Ensures researchers without staking coins can receive rewards
- **Requirements**: Valid beacon, positive accrual balance
- **Location**: `src/gridcoin/mrc.h/cpp`
- **Contract Type**: `ContractType::MRC`

---

## Consensus & Superblocks

### Superblock
- **Definition**: Blockchain-committed snapshot of research statistics
- **Contents**:
  - CPID magnitudes (compressed format)
  - Project statistics (RAC totals)
  - Verified beacon list
  - Convergence hint
- **Frequency**: ~24 hours (approximately daily)
- **Version**: Current = v2 (binary format)
- **Location**: `src/gridcoin/superblock.h/cpp`

### Quorum
- **Definition**: Legacy component (pre-Fern), now primarily a facade for scraper convergence functions. Consensus mechanism for validating superblocks
- **Requirements**:
  - Supermajority agreement among scrapers
  - Matching quorum hash
  - Valid manifest convergence
- **Validation**: Multiple levels (content, signatures, convergence)
- **Location**: `src/gridcoin/quorum.h/cpp`

### Quorum Hash
- **Definition**: Unique identifier for superblock content
- **Types**:
  - SHA256 (current): Full superblock hash
  - MD5 (legacy): Compatibility with older format
- **Purpose**: Verify agreement without transmitting full superblock
- **Comparison**: Used for voting and validation

### Convergence
- **Definition**: Process of finding agreement among scraper manifests
- **Algorithm**:
  1. Collect manifests from multiple scrapers
  2. Find common project statistics
  3. Build consensus superblock from agreement
  4. Validate supermajority threshold
- **Fallback**: Project-by-project convergence if full convergence fails

### Scraper
- **Definition**: Node that downloads and verifies BOINC project statistics
- **Function**:
  - Downloads project stat files
  - Parses and validates data
  - Creates signed manifest
  - Publishes for network convergence
- **Independence**: Multiple scrapers operate independently
- **Location**: `src/gridcoin/scraper/`

### Manifest
- **Definition**: Signed package of scraped BOINC statistics
- **Components**:
  - Project parts (individual project data)
  - Beacon verification data
  - Timestamp and version
  - Scraper signature
- **Distribution**: P2P network message

---

## Projects & Whitelisting

### Whitelist
- **Definition**: Registry of BOINC projects approved for Gridcoin rewards
- **Management**: Community-controlled via contract system
- **Storage**: Persistent database
- **Attributes per project**:
  - Name and URL
  - GDPR controls status
  - Stats URL pattern
  - Activation timestamps
- **Location**: `src/gridcoin/project.h/cpp`
- **Contract Type**: `ContractType::PROJECT`

### Greylist (Auto-greylist)
- **Definition**: Projects temporarily excluded from magnitude calculations
- **Triggers**:
  - Zero credit days (ZCD): Project shows no activity
  - Whitelist activity score (WAS): Low relative activity
- **Purpose**: Automatic quality control for project data
- **Duration**: Until project meets criteria again
- **Design**: See `doc/automated_greylisting_design_highlights.md`

### Project Entry
- **Definition**: Individual project record in whitelist registry
- **Status**: Active, Deleted, Pending
- **Filter Flags**: Active only, include deleted, pending only

---

## Staking & Blocks

### Proof-of-Stake (PoS)
- **Definition**: Consensus mechanism using coin ownership instead of mining
- **Process**: Nodes stake coins to earn right to create blocks
- **Difficulty**: Adjusts based on network stake weight
- **Reward**: Block subsidy + research subsidy (if applicable)
- **Energy**: Much lower than proof-of-work

### Kernel
- **Definition**: Hash calculation determining if UTXO can stake
- **Inputs**: Previous block hash, UTXO data, current time
- **Target**: Must be less than difficulty target
- **Attempts**: Checked every ~8 seconds (loop has 8-second sleep), with 16-second time mask granularity for stake distinguishability

### Coinstake
- **Definition**: Special transaction that proves stake and distributes rewards
- **Structure**:
  - **Input**: UTXO being staked
  - **Output 0**: Empty (proof marker for PoS validation)
  - **Output 1**: Primary reward output (stake subsidy + research subsidy to staker)
  - **Output 2+**: Variable additional outputs, can include:
    - **Stake Split Outputs**: Multiple outputs to staker's address (if splitting enabled)
    - **Mandatory Sidestakes**: Protocol-enforced reward distributions (max 4 in v13+)
    - **Local Sidestakes**: Voluntary reward distributions (configurable)
    - **MRC Payment Outputs**: Rewards for Manual Reward Claims (max 10 in v12+, 3 on testnet)
    - **Foundation Output**: Portion of MRC fees directed to foundation address
- **Output Limits**:
  - **v10-11**: Max 8 total outputs (excluding MRCs which weren't supported)
  - **v12**: Max 10 non-MRC + 10 MRC outputs (total ≤ 20, testnet ≤ 13)
  - **v13+**: Max 10 non-MRC (including ≤ 4 mandatory sidestakes) + 10 MRC outputs (total ≤ 20, testnet ≤ 13)
- **Order**: `[empty] → [reward splits] → [mandatory sidestakes] → [local sidestakes] → [foundation fee] → [MRC payments]`
- **Research Claim**: Includes CPID and accrual claim if researcher
- **Location**: Created in `CreateCoinStake()`, modified in `SplitCoinStakeOutput()` and `CreateMRCRewards()`

### Coin Age
- **Legacy**: Coin age not used after block version 9. Modern staking uses coin weight only.
- **Definition**: Time-weighted coin value (Amount × Days Held)
- **Minimum**: Coins must age before eligible for staking
- **Consumed**: Resets to zero when coins stake
- **Purpose**: Prevents rapid restaking, distributes block creation

---

## Contract System

### Contract
- **Definition**: Special transaction that modifies blockchain state
- **Detection**: Identified by specific message format in transaction
- **Burn Fee**: Coins sent to unspendable address (prevents spam)
- **Types**: Unknown, Beacon, Claim, Message, Poll, Project, Protocol, Scraper, Vote, MRC, SideStake, OUT_OF_BOUND (marker)
- **Location**: `src/gridcoin/contract/`

### Contract Action
- **Types**:
  - `ADD`: Create new entry
  - `REMOVE`: Mark entry as deleted
- **Validation**: Type-specific rules enforced by handlers
- **Reversibility**: Some registries support reversion on reorg

### Contract Handler
- **Definition**: Component that processes contracts of specific type
- **Interface**: `IContractHandler` (handler.h)
- **Methods**:
  - `Validate()`: Check contract validity
  - `BlockValidate()`: Context-aware validation
  - `Add()`: Process ADD action
  - `Delete()`: Process REMOVE action
  - `Revert()`: Undo on blockchain reorganization

### Registry
- **Definition**: Persistent state managed by contract handler
- **Examples**: BeaconRegistry, Whitelist, ProtocolRegistry, SideStakeRegistry
- **Database**: LevelDB backend for most registries
- **Note**: Not all contract types have backing registries:
  - **With Registries**: Beacon, Project, Protocol, Scraper, SideStake (need persistent state)
  - **Without Registries**: Claim (validated inline), Message (stateless), Poll/Vote (separate system), MRC (stateless validation), Unknown/OUT_OF_BOUND (markers)
- **Pattern**: Factory pattern for type-specific access

---

## Side Stakes

### Side Stake
- **Definition**: Automatic distribution of block rewards to specified addresses
- **Types**:
  - **Local**: Configured in local config file (voluntary)
  - **Mandatory**: Protocol-enforced via blockchain contracts
- **Allocation**: Percentage of stake reward (uses Fraction type)
- **Purpose**: Fund development, infrastructure, community projects
- **Location**: `src/gridcoin/sidestake.h/cpp`

### Allocation
- **Definition**: Fraction representing percentage of rewards
- **Storage**: Numerator/denominator pair (precise calculation)
- **Conversion**: To/from percentage and CAmount
- **Validation**: Total allocations must not exceed 100%

---

## Protocol & Configuration

### Protocol Entry
- **Definition**: Blockchain-stored configuration parameter
- **Examples**:
  - Minimum stake age
  - Superblock interval
  - Version numbers
- **Management**: Protocol contracts (requires higher burn fee)
- **History**: Full historical record maintained
- **Location**: `src/gridcoin/protocol.h/cpp`

### AppCache (Legacy)
- **Definition**: Older key-value storage system for protocol data
- **Status**: Effectively phased out. Remaining legacy calls pending complete removal.
- **Sections**: Different categories of cached data
- **Location**: `src/gridcoin/appcache.h/cpp`

---

## Researcher Context

### Researcher
- **Definition**: Local representation of user's BOINC participation
- **Detection**: Parses BOINC `client_state.xml` for projects
- **Modes**:
  - **Solo**: Individual researcher with CPID
  - **Pool**: GUI indicator for pool participation - causes wallet to act identically to non-cruncher mode except for certain GUI functions. No actual pool delegation occurs.
  - **Investor**: No BOINC participation (noncruncher)
- **Eligibility**: Valid projects, team membership, beacon status
- **Location**: `src/gridcoin/researcher.h/cpp`

### Research Account
- **Definition**: Per-CPID accounting record in tally system
- **Tracks**:
  - Total research subsidy earned
  - Total magnitude over time
  - First/last reward blocks
  - Average magnitude
- **Purpose**: Historical record and accrual calculation

---

## Network & Synchronization

### Snapshot (Deprecated)
- **Status**: Discontinued - snapshot servers disabled
- **Reason**: Security - full sync from genesis eliminates trust requirements
- **Sync Time**: Complete sync now takes < 5 hours on modern hardware with good connection
- **Note**: Snapshot functionality still exists in code but is no longer recommended or supported

### Checkpoint
- **Definition**: Hardcoded block hash at specific height
- **Purpose**: Prevent reorg attacks, validate sync progress
- **Frequency**: Periodic updates in new releases
- **Location**: `src/checkpoints.cpp`

### Reorg (Reorganization)
- **Definition**: Blockchain switches to different chain with more trust (not more work)
- **Cause**: Competing chains, network splits
- **Depth**: Number of blocks replaced
- **Impact**: Contracts may be reverted, transactions may become invalid

---

## Data Types & Utilities

### CAmount
- **Definition**: 64-bit integer representing coin amount
- **Precision**: 8 decimal places (satoshi precision)
- **Example**: 1.00000000 GRC = 100000000 units
- **Type**: `int64_t`

### uint256
- **Definition**: 256-bit unsigned integer (lightweight blob, no arithmetic)
- **Uses**: Block hashes, transaction IDs, Merkle roots
- **Format**: Usually displayed as 64-character hex string
- **Location**: `src/uint256.h`

### arith_uint256
- **Definition**: 256-bit unsigned integer with arithmetic operations support
- **Location**: `src/arith_uint256.h`
- **Note**: Split from uint256 (following Bitcoin Core) for performance - use lighter uint256 when arithmetic not needed

### Fraction
- **Definition**: Rational number represented as numerator/denominator
- **Consensus-Critical**: The Fraction class is essential for deterministic calculations. Historical use of floating-point caused forking incidents due to CPU architecture differences. All consensus arithmetic now uses integer-based Fraction calculations.
- **Usage**: Precise percentage calculations (sidestakes, allocations)
- **Operations**: Simplification, comparison, arithmetic
- **Location**:
  - **Base `Fraction` class**: `src/util.h` (core rational number implementation)
  - **Derived `Allocation` class**: `src/gridcoin/sidestake.h` (extends Fraction for percentage allocations in sidestake rewards)

### CTxDestination
- **Definition**: Variant type for transaction destinations
- **Types**: Public key hash, script hash, etc.
- **Usage**: Addresses, sidestake destinations
- **Encoding**: Base58 address representation

---

## Development Concepts

### DoS (Denial of Service) Score
- **Definition**: Penalty score for invalid messages/blocks
- **Purpose**: Protect network from malicious/broken nodes
- **Threshold**: High scores trigger peer ban
- **Usage**: Returned by validation functions

### Hard Fork
- **Definition**: Protocol change requiring all nodes to upgrade
- **Activation**: Specific block height
- **Incompatibility**: Old nodes reject new blocks
- **Example**: V5 transition, block version upgrades

### Lock Order
- **Definition**: Sequence in which mutexes must be acquired
- **Critical**: Prevents deadlocks in multi-threaded code
- **Debug**: the `-DENABLE_DEBUG_LOCKORDER=ON` CMake option detects violations
- **Standard**: `cs_main` before `cs_wallet`

---

## Testing & Debugging

### Testnet
- **Definition**: Separate blockchain for testing
- **Activation**: `-testnet` command line flag
- **Differences**: Different ports, magic bytes, genesis block
- **Purpose**: Safe environment for development

### RPC (Remote Procedure Call)
- **Definition**: API for programmatic interaction with wallet
- **Protocol**: JSON-RPC over HTTP
- **Port**: 15715 (mainnet), 25715 (testnet)
- **Location**: `src/rpc/`
- **Authentication**: Username/password in config file

### Debug Categories
- **Definition**: Selective logging for different subsystems
- **Configuration**: `-debug=<category>` flag
- **Examples**: `net`, `contract`, `scraper`, `miner`, `tally`
- **Output**: `debug.log` file

### Boost Unit Testing Suite
- **Framework**: Boost.Test
- **Location**: `src/test/`
- **Run**: `make check` or via CMake test targets

---

## Key Abbreviations

| Abbrev | Full Term | Context |
|--------|-----------|---------|
| **BOINC** | Berkeley Open Infrastructure for Network Computing | Volunteer computing platform |
| **CPID** | Cross-Project Identifier | Researcher identity hash |
| **RAC** | Recent Average Credit | BOINC computation metric |
| **PoS** | Proof-of-Stake | Consensus mechanism |
| **MRC** | Manual Reward Claim | Contract for claiming without staking |
| **GRC** | Gridcoin Research | Currency ticker symbol |
| **UTXO** | Unspent Transaction Output | Blockchain accounting unit |
| **RPC** | Remote Procedure Call | API interface |
| **P2P** | Peer-to-Peer | Network architecture |
| **GUI** | Graphical User Interface | Qt-based wallet interface |
| **CLI** | Command Line Interface | Terminal-based wallet |
| **DB** | Database | LevelDB storage |

---

## Related Documentation

- **Architecture**: See `02-architecture-overview.md` for system design
- **Coding Style**: See `01-coding.md` for conventions
- **Components**: See `04-component-guide.md` for file organization
- **Tasks**: See `05-common-tasks.md` for how-to guides

This glossary is essential context for understanding Gridcoin discussions, code comments, and RPC commands. Refer to it frequently when exploring the codebase.
