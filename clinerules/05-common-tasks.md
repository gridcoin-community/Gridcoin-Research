# Gridcoin Common Tasks Guide

## Practical How-To Reference

This guide provides step-by-step workflows for common development scenarios in the Gridcoin codebase. Follow Baby Steps™ methodology - tackle one focused change at a time.

---

## Table of Contents

1. [Adding a New RPC Command](#1-adding-a-new-rpc-command)
2. [Adding a New Contract Type](#2-adding-a-new-contract-type)
3. [Modifying Consensus Rules](#3-modifying-consensus-rules)
4. [Adding GUI Features](#4-adding-gui-features)
5. [Debugging Research Reward Issues](#5-debugging-research-reward-issues)
6. [Working with the Test Suite](#6-working-with-the-test-suite)
7. [Investigating Blockchain Issues](#7-investigating-blockchain-issues)
8. [Modifying Scraper Logic](#8-modifying-scraper-logic)
9. [Protocol Parameter Changes](#9-protocol-parameter-changes)
10. [Performance Optimization](#10-performance-optimization)
11. [Ensuring Documentation Passes Lint Checks](#11-ensuring-documentation-passes-lint-checks)
12. [Working with CI/CD](#12-working-with-cicd)

---

## 1. Adding a New RPC Command

### Scenario
You want to add a new RPC command for users to query or modify wallet/blockchain state.

### Convention notes (post-#2922)

Every RPC in this codebase follows the `RPCHelpMan` pattern as of issue #2922
(completed June 2026). Help text is a declarative file-scope `RPCHelpMan`
object, not a runtime-thrown string. The dispatcher (`CRPCTable::execute` in
`src/rpc/server.cpp`) renders help via `pcmd->helpman().ToString()` directly
and pre-checks `IsValidNumArgs(n)` before invoking the body, so bodies do not
contain `if (fHelp || …) throw runtime_error(…)` gates. The actor signature is
`UniValue <name>(const UniValue& params)` — no `bool fHelp` parameter.

A handful of variadic / dynamic-helpman commands (`addkey`, `changesettings`,
`scanforunspent`, `votebyid`, `sendalert`, `parselegacysb`, `addpoll`) have
narrow exceptions documented in `src/rpc/util.h` (`MarkVariadic()` / dynamic
helpman accessor). New commands should not need these unless they truly take
variable arity.

### Steps

#### 1.1 Choose the Appropriate File
Determine which category your command belongs to (these are the actual
`src/rpc/*.cpp` files; wallet RPCs live under `src/wallet/`):

- **Blockchain queries / projects / researcher / beacon / MRC**: `src/rpc/blockchain.cpp`
- **Wallet operations**: `src/wallet/rpcwallet.cpp`
- **Wallet key dump/import**: `src/wallet/rpcdump.cpp`
- **Network / peers / banlist**: `src/rpc/net.cpp`
- **Staking / mining info / accrual**: `src/rpc/mining.cpp`
- **Voting / polls**: `src/rpc/voting.cpp`
- **Scrapers / manifests / convergence**: `src/rpc/dataacq.cpp` (some in `src/gridcoin/scraper/scraper.cpp`)
- **Raw transactions / signing**: `src/rpc/rawtransaction.cpp`
- **HTLC**: `src/rpc/htlc.cpp`
- **PSGT (Partially Signed Gridcoin Transactions)**: `src/rpc/psgt.cpp`
- **Server / misc / control**: `src/rpc/server.cpp` or `src/rpc/misc.cpp`

#### 1.2 Declare the file-scope RPCHelpMan and accessor

```cpp
// Example: src/rpc/blockchain.cpp

static const RPCHelpMan mycommand_help{
    "mycommand",
    "Description of what the command does.",
    {
        {"param", RPCArg::Type::STR, RPCArg::Optional::NO,
            "Description of the required string parameter."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "result", "Description of the returned string field."},
        }},
    RPCExamples{
        HelpExampleCli("mycommand", "\"example_value\"")
      + HelpExampleRpc("mycommand", "\"example_value\"")},
};
const RPCHelpMan& mycommand_helpman() { return mycommand_help; }
```

`RPCArg::Optional` values: `NO` (required), `OMITTED` (optional, may be absent).
`RPCResult::Type` values include `STR`, `STR_HEX`, `NUM`, `BOOL`, `OBJ`, `OBJ_DYN`, `ARR`, `NONE`,
`ELISION`. See `src/rpc/util.h` for the full vocabulary.

#### 1.3 Implement the Command Body

```cpp
UniValue mycommand(const UniValue& params)
{
    // 1. Parse parameters — arity has already been checked by the
    //    dispatcher; params[N] is guaranteed in-bounds.
    const std::string param = params[0].get_str();

    // 2. Acquire locks if needed (see doc/developer-notes.md for ordering).
    LOCK(cs_main);
    // LOCK2(cs_main, pwalletMain->cs_wallet);  // for wallet state

    // 3. Perform operation
    const std::string result = PerformOperation(param);

    // 4. Build JSON response
    UniValue response(UniValue::VOBJ);
    response.pushKV("result", result);
    return response;
}
```

#### 1.4 Forward-declare both in `src/rpc/server.h`

Add an `extern` for the actor and an `extern` for the helpman accessor in
the alphabetical block matching the rpccategory the command belongs to:

```cpp
// Wallet (or whichever category)
extern UniValue mycommand(const UniValue& params);
// ...alphabetical helpman block...
extern const RPCHelpMan& mycommand_helpman();
```

#### 1.5 Register the Command

In `src/rpc/server.cpp`, add a row to the `vRPCCommands[]` table in the
correct category section (alphabetical within section). The four fields are
`{ name, actor, category, helpman }`:

```cpp
{ "mycommand",  &mycommand,  cat_wallet,  &mycommand_helpman  },
```

Valid `rpccategory` enum values (`src/rpc/server.h`): `cat_wallet`,
`cat_staking`, `cat_developer`, `cat_network`, `cat_voting`. `cat_null`
is used for internal entries and excludes the command from the
categorized `help` output.

#### 1.6 Add a help-render test

Every dispatched RPC should have help-render coverage. The standard pattern
in `src/test/rpchelpman_tests.cpp` is to add an entry to the appropriate
tier-level `check_help_renders({...})` block:

```cpp
// In whichever tier-level BOOST_AUTO_TEST_CASE the command belongs to:
{"mycommand",            &mycommand_helpman},
```

For a one-off command not fitting any tier, write a dedicated
`BOOST_AUTO_TEST_CASE(mycommand_help_renders)` that calls
`mycommand_helpman().ToString()` and asserts on the rendered string.

#### 1.7 Build and test the command

```bash
cmake --build build -j $(nproc)

# Run the help-render test
./build/src/test/test_gridcoin --run_test=rpchelpman_tests/mycommand_help_renders

# Or smoke test against a live testnet daemon
./build/src/gridcoinresearchd -testnet -daemon
./build/bin/gridcoinresearch -testnet help mycommand
./build/bin/gridcoinresearch -testnet mycommand "test_value"

# Check debug.log for errors
tail -f ~/.GridcoinResearch/testnet/debug.log
```

### Common Patterns

**Reading Blockchain State**:
```cpp
LOCK(cs_main);
CBlockIndex* pindex = chainActive.Tip();
int height = pindex->nHeight;
```

**Reading Researcher Context**:
```cpp
auto researcher = GRC::Researcher::Get();
if (researcher->Id().Which() == GRC::MiningId::Kind::CPID) {
    GRC::Cpid cpid = researcher->Id().TryCpid().value();
}
```

**Querying Registries**:
```cpp
auto beacon = GetBeaconRegistry().Try(cpid);
if (beacon) {
    // Use beacon
}
```

---

## 2. Adding a New Contract Type

### Scenario
You need to add a new type of blockchain contract for governance or state management.

### Steps

#### 2.1 Define the Contract Type
Edit `src/gridcoin/contract/contract.h`:
```cpp
enum class ContractType : uint32_t {
    UNKNOWN,
    BEACON,
    // ... existing types ...
    MYNEWTYPE,  // Add your type
};
```

Update the type mapping functions in `src/gridcoin/contract/contract.cpp`:
```cpp
ContractType ContractType::Parse(std::string input) {
    // ... existing mappings ...
    if (input == "mynewtype") return ContractType::MYNEWTYPE;
    return ContractType::UNKNOWN;
}

std::string ContractType::ToString(ContractType type) {
    // ... existing cases ...
    case ContractType::MYNEWTYPE: return "mynewtype";
    default: return "unknown";
}
```

#### 2.2 Create the Payload Class
Create `src/gridcoin/mynewtype.h`:
```cpp
namespace GRC {

// Entry structure (what gets stored)
class MyNewTypeEntry {
public:
    std::string m_key;
    std::string m_value;
    int64_t m_timestamp;
    uint256 m_hash;

    // Status tracking
    enum class Status { ACTIVE, DELETED, PENDING };
    Status m_status;

    // Required methods
    bool WellFormed() const;
    std::string Key() const { return m_key; }
    std::pair<std::string, std::string> KeyValueToString() const;

    // Serialization
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(m_key);
        READWRITE(m_value);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
    }
};

// Contract payload (what goes in transactions)
class MyNewTypePayload : public IContractPayload {
public:
    MyNewTypeEntry m_entry;

    // IContractPayload interface
    GRC::ContractType ContractType() const override {
        return GRC::ContractType::MYNEWTYPE;
    }

    bool WellFormed(const ContractAction action) const override {
        return m_entry.WellFormed();
    }

    std::string LegacyKeyString() const override {
        return m_entry.Key();
    }

    std::string LegacyValueString() const override {
        return m_entry.m_value;
    }

    CAmount RequiredBurnAmount() const override {
        return 0.5 * COIN;  // Set appropriate burn fee
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(m_entry);
    }
};

// Registry (handler and state manager)
class MyNewTypeRegistry : public IContractHandler {
private:
    // Storage maps
    std::map<std::string, std::shared_ptr<MyNewTypeEntry>> m_entries;

public:
    // IContractHandler interface
    void Reset() override;
    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override;
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;
    void Add(const ContractContext& ctx) override;
    void Delete(const ContractContext& ctx) override;
    void Revert(const ContractContext& ctx) override;

    // Custom query methods
    std::shared_ptr<MyNewTypeEntry> Try(const std::string& key) const;
};

MyNewTypeRegistry& GetMyNewTypeRegistry();

} // namespace GRC
```

#### 2.3 Implement the Registry
Create `src/gridcoin/mynewtype.cpp`:
```cpp
#include "gridcoin/mynewtype.h"
#include "main.h"  // For cs_main

using namespace GRC;

namespace {
MyNewTypeRegistry g_registry;
}

MyNewTypeRegistry& GRC::GetMyNewTypeRegistry() {
    return g_registry;
}

bool MyNewTypeEntry::WellFormed() const {
    return !m_key.empty() && !m_value.empty();
}

bool MyNewTypeRegistry::Validate(const Contract& contract, const CTransaction& tx, int& DoS) const {
    const auto payload = contract.SharePayload<MyNewTypePayload>();

    if (!payload->WellFormed(contract.m_action)) {
        DoS = 25;
        return false;
    }

    // Additional validation logic
    return true;
}

void MyNewTypeRegistry::Add(const ContractContext& ctx) {
    const auto payload = ctx.m_contract.SharePayload<MyNewTypePayload>();
    auto entry = std::make_shared<MyNewTypeEntry>(payload->m_entry);
    entry->m_status = MyNewTypeEntry::Status::ACTIVE;
    entry->m_timestamp = ctx.m_pindex->nTime;

    m_entries[entry->Key()] = entry;
}

// Implement other methods...
```

#### 2.3.1 Registry Database Setup (Optional)

If your registry needs persistent storage, leverage the `registry_db.h` template system for automatic LevelDB serialization:

**Key Concept**: The `registry_db.h` template provides a standardized way to persist registry state to LevelDB with automatic serialization/deserialization.

**Setup Pattern**:
```cpp
// In mynewtype.h
#include "gridcoin/contract/registry_db.h"

class MyNewTypeRegistry : public IContractHandler {
private:
    RegistryDB<MyNewTypeEntry> m_db;  // Template handles persistence

public:
    MyNewTypeRegistry()
        : m_db("mynewtype")  // Database filename prefix
    {
    }

    // Database operations
    void StoreToDB(const MyNewTypeEntry& entry) {
        m_db.Store(entry.Key(), entry);
    }

    std::shared_ptr<MyNewTypeEntry> LoadFromDB(const std::string& key) {
        return m_db.Load(key);
    }

    void EraseFromDB(const std::string& key) {
        m_db.Erase(key);
    }
};
```

**Benefits**:
- Automatic serialization using entry's `SerializationOp`
- Consistent error handling
- Transaction support for atomic updates
- Iterator support for full registry scans

**Example Registries Using This Pattern**:
- `BeaconRegistry` → `beacon.dat`
- `Whitelist` → `project.dat`
- `ProtocolRegistry` → `protocol.dat`

**Note**: For stateless handlers (like MRCRegistry) or memory-only registries, skip the database template and manage state directly.

#### 2.4 Register with Dispatcher
Edit `src/gridcoin/contract/contract.cpp`:
```cpp
IContractHandler& Dispatcher::GetHandler(const ContractType type) {
    switch (type) {
        // ... existing cases ...
        case ContractType::MYNEWTYPE:
            return GetMyNewTypeRegistry();
        default:
            return s_unknown_handler;
    }
}
```

#### 2.5 Add to Build System
Edit `src/CMakeLists.txt`:
```cmake
# CMakeLists.txt
set(GRIDCOIN_SOURCES
    # ... existing files ...
    gridcoin/mynewtype.cpp
)
```

#### 2.6 Create RPC Commands
Add commands in `src/rpc/contract.cpp` or create new file for managing your contract type.

#### 2.7 Add Tests
Create `src/test/gridcoin/mynewtype_tests.cpp`:
```cpp
#include <boost/test/unit_test.hpp>
#include "gridcoin/mynewtype.h"

BOOST_AUTO_TEST_SUITE(mynewtype_tests)

BOOST_AUTO_TEST_CASE(it_validates_well_formed_entries)
{
    GRC::MyNewTypeEntry entry;
    entry.m_key = "test";
    entry.m_value = "value";

    BOOST_CHECK(entry.WellFormed() == true);
}

BOOST_AUTO_TEST_SUITE_END()
```

---

## 3. Modifying Consensus Rules

### Scenario
You need to change block validation, reward calculation, or other consensus-critical logic.

### ⚠️ WARNING
Consensus changes require:
- **Hard fork coordination**: All nodes must upgrade
- **Activation height**: Rules activate at specific block
- **Extensive testing**: Errors can fork the network
- **Community approval**: Major changes need governance

### Steps

#### 3.1 Identify the Change Location
Common consensus locations:
- **Block validation**: `src/main.cpp` → `CheckBlock()`, `AcceptBlock()`
- **Transaction validation**: `src/main.cpp` → `CheckTransaction()`
- **Reward calculation**: `src/miner.cpp` → Reward functions
- **Difficulty adjustment**: `src/main.cpp` → `GetNextTargetRequired()`

#### 3.2 Add Version/Height Gating
Always gate consensus changes by block height or version:
```cpp
// Example: New rule activating at height 3000000
bool NewConsensusRule(const CBlockIndex* pindex) {
    const int ACTIVATION_HEIGHT = 3000000;
    return pindex->nHeight >= ACTIVATION_HEIGHT;
}

// In validation function
bool ValidateBlock(const CBlock& block, const CBlockIndex* pindex) {
    // Existing validation
    if (!ExistingRule(block)) {
        return false;
    }

    // New rule (only after activation)
    if (NewConsensusRule(pindex)) {
        if (!NewRule(block)) {
            return error("Block fails new consensus rule");
        }
    }

    return true;
}
```

#### 3.3 Update Block Version If Needed
If adding substantial changes:
```cpp
// src/primitives/block.h
static const int32_t CURRENT_VERSION = 12;  // Increment

// src/main.cpp - Check version
if (block.nVersion < MINIMUM_VERSION) {
    return state.DoS(100, error("version too old"));
}
```

#### 3.4 Test on Testnet First
```bash
# Build with testnet
cmake --build . --target gridcoinresearchd

# Run testnet node
./gridcoinresearchd -testnet -daemon

# Monitor for issues
tail -f ~/.GridcoinResearch/testnet/debug.log
```

#### 3.5 Document the Fork
Update:
- `CHANGELOG.md`: Document the change
- `doc/release-process.md`: Add to release notes
- Community announcements

### Common Consensus Modifications

**Changing Block Reward**:
```cpp
// src/miner.cpp or wherever reward is calculated
CAmount GetBlockSubsidy(int nHeight) {
    if (nHeight < FORK_HEIGHT) {
        return OLD_REWARD;
    }
    return NEW_REWARD;
}
```

**Adding New Validation**:
```cpp
// src/main.cpp in block validation
if (pindex->nHeight >= NEW_RULE_HEIGHT) {
    if (!ValidateNewRule(block, pindex)) {
        return state.DoS(100, error("new rule violation"));
    }
}
```

---

## 4. Adding GUI Features

### Scenario
You want to add a new dialog, widget, or feature to the Qt GUI.

### Steps

#### 4.1 Create the Dialog Class
Create `src/qt/mynewdialog.h`:
```cpp
#ifndef MYNEWDIALOG_H
#define MYNEWDIALOG_H

#include <QDialog>

namespace Ui {
class MyNewDialog;
}

class WalletModel;

class MyNewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MyNewDialog(QWidget *parent = nullptr);
    ~MyNewDialog();

    void setModel(WalletModel *model);

private Q_SLOTS:
    void onButtonClicked();
    void updateDisplay();

private:
    Ui::MyNewDialog *ui;
    WalletModel *m_model;
};

#endif
```

#### 4.2 Create the UI File
Create `src/qt/forms/mynewdialog.ui` using Qt Designer or manually:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>MyNewDialog</class>
 <widget class="QDialog" name="MyNewDialog">
  <property name="geometry">
   <rect><x>0</x><y>0</y><width>400</width><height>300</height></rect>
  </property>
  <property name="windowTitle">
   <string>My New Feature</string>
  </property>
  <layout class="QVBoxLayout">
   <item>
    <widget class="QLabel" name="label">
     <property name="text">
      <string>Feature Description</string>
     </property>
    </widget>
   </item>
   <item>
    <widget class="QPushButton" name="actionButton">
     <property name="text">
      <string>Perform Action</string>
     </property>
    </widget>
   </item>
  </layout>
 </widget>
</ui>
```

#### 4.3 Implement the Dialog
Create `src/qt/mynewdialog.cpp`:
```cpp
#include "mynewdialog.h"
#include "ui_mynewdialog.h"
#include "walletmodel.h"

MyNewDialog::MyNewDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MyNewDialog),
    m_model(nullptr)
{
    ui->setupUi(this);

    // Connect signals
    connect(ui->actionButton, &QPushButton::clicked,
            this, &MyNewDialog::onButtonClicked);
}

MyNewDialog::~MyNewDialog()
{
    delete ui;
}

void MyNewDialog::setModel(WalletModel *model)
{
    m_model = model;
    updateDisplay();
}

void MyNewDialog::onButtonClicked()
{
    if (!m_model) return;

    // Perform action using model
    // m_model->someOperation();

    updateDisplay();
}

void MyNewDialog::updateDisplay()
{
    // Update UI elements
}
```

#### 4.4 Add to Main Window
Edit `src/qt/bitcoingui.cpp`:
```cpp
// Include header
#include "mynewdialog.h"

// In constructor, add menu action
QAction *myNewAction = new QAction(tr("My New Feature"), this);
myNewAction->setStatusTip(tr("Open my new feature dialog"));
connect(myNewAction, &QAction::triggered, this, &BitcoinGUI::showMyNewDialog);
someMenu->addAction(myNewAction);

// Add slot method
void BitcoinGUI::showMyNewDialog()
{
    if (!walletFrame) return;
    MyNewDialog dialog(this);
    dialog.setModel(walletFrame->currentWalletModel());
    dialog.exec();
}
```

#### 4.5 Register with Build System
Edit `src/qt/CMakeLists.txt` or `src/Makefile.qt.include`:
```cmake
set(QT_FORMS_UI
    # ... existing forms ...
    forms/mynewdialog.ui
)

set(QT_MOC_CPP
    # ... existing mocs ...
    moc_mynewdialog.cpp
)
```

#### 4.6 Test the GUI
```bash
cmake --build . --target gridcoinresearch-qt
./gridcoinresearch-qt -testnet
```

---

## 5. Debugging Research Reward Issues

### Scenario
A user reports incorrect research rewards, zero magnitude, or beacon problems.

### Diagnostic Steps

#### 5.1 Check Researcher Status
```bash
# RPC command
./gridcoinresearch-cli getstakinginfo

# Look for:
# - "researcher_status": Should be "active"
# - "cpid": Should be valid hex string
# - "magnitude": Should be > 0 if doing research
```

#### 5.2 Verify Beacon Status
```bash
./gridcoinresearch-cli beaconstatus

# Check:
# - Beacon age (should be < 6 months)
# - Status (should be "active")
# - Public key matches wallet
```

#### 5.3 Check BOINC Detection
```bash
./gridcoinresearch-cli listprojects

# Verify:
# - Projects are detected
# - Team is correct (Gridcoin)
# - CPID matches
```

#### 5.4 Examine Current Superblock
```bash
# Get current superblock
./gridcoinresearch-cli superblocks

# Check specific CPID magnitude
./gridcoinresearch-cli magnitude <cpid>
```

#### 5.5 Check Accrual
```bash
./gridcoinresearch-cli getaccrual <cpid>

# Returns pending research rewards
```

#### 5.6 Debug Logging
Enable verbose logging:
```bash
# Add to gridcoinresearch.conf
debug=contract
debug=scraper
debug=miner
debug=tally

# Restart and monitor
tail -f ~/.GridcoinResearch/debug.log | grep -i "beacon\|magnitude\|accrual"
```

### Common Issues

**Zero Magnitude**:
1. Check if CPID is in current superblock
2. Verify RAC is being reported by projects
3. Check if project is greylisted
4. Ensure beacon is active

**Beacon Not Activating**:
1. Wait for next superblock (up to 24 hours)
2. Verify beacon transaction confirmed
3. Check beacon appears in pending list

**Accrual Not Increasing**:
1. Verify magnitude > 0
2. Check superblock is being updated
3. Ensure beacon hasn't expired

---

## 6. Working with the Test Suite

### Scenario
You need to add tests or debug failing tests.

### Running Tests

```bash
# Build tests
cmake --build . --target test_gridcoinresearch

# Run all tests
./src/test/test_gridcoinresearch

# Run specific test suite
./src/test/test_gridcoinresearch --run_test=beacon_tests

# Run with verbose output
./src/test/test_gridcoinresearch --log_level=all
```

### Writing Unit Tests

Create `src/test/gridcoin/myfeature_tests.cpp`:
```cpp
#include <boost/test/unit_test.hpp>
#include "gridcoin/myfeature.h"

BOOST_AUTO_TEST_SUITE(myfeature_tests)

BOOST_AUTO_TEST_CASE(it_does_something_correctly)
{
    // Arrange
    GRC::MyFeature feature;

    // Act
    bool result = feature.DoSomething();

    // Assert
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_handles_errors_gracefully)
{
    GRC::MyFeature feature;

    // Test error condition
    BOOST_CHECK_THROW(feature.DoInvalidOperation(), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
```

### Test Fixtures
For tests requiring blockchain state:
```cpp
#include "test/test_gridcoin.h"  // Provides TestChain100Setup

BOOST_FIXTURE_TEST_SUITE(myfeature_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(it_works_with_blockchain)
{
    // You have a 100-block chain available
    BOOST_CHECK(chainActive.Height() == 100);

    // Test code
}

BOOST_AUTO_TEST_SUITE_END()
```

---

## 7. Investigating Blockchain Issues

### Scenario
Node is stuck, forked, or showing unexpected behavior.

### Diagnostic Commands

```bash
# Check sync status
./gridcoinresearch-cli getblockchaininfo

# Get current best block
./gridcoinresearch-cli getbestblockhash

# Check specific block
./gridcoinresearch-cli getblock <hash>

# View mempool
./gridcoinresearch-cli getrawmempool

# Check peers
./gridcoinresearch-cli getpeerinfo

# Examine connections
./gridcoinresearch-cli getnetworkinfo
```

### Common Fixes

**Node Stuck Syncing**:
```bash
# Restart with fresh peers
./gridcoinresearch-cli stop
rm ~/.GridcoinResearch/peers.dat
./gridcoinresearchd -daemon
```

**Potential Fork**:
```bash
# Compare block hash with explorer
./gridcoinresearch-cli getblockhash <height>

# If forked, may need to resync
./gridcoinresearch-cli stop
# Backup wallet first!
rm -rf ~/.GridcoinResearch/blocks ~/.GridcoinResearch/chainstate
./gridcoinresearchd -daemon
```

**Debug Validation Failures**:
```bash
# Enable verbose validation logging
./gridcoinresearchd -debug=validation -printtoconsole
```

---

## 8. Modifying Scraper Logic

### Scenario
You need to change how statistics are collected or processed.

### Key Files
- `src/scraper/scraper.cpp`: Main scraper logic
- `src/scraper/http.cpp`: HTTP downloading
- `src/gridcoin/quorum.cpp`: Convergence algorithm

### Example: Adding Project Validation

Edit `src/scraper/scraper.cpp`:
```cpp
bool ValidateProjectStats(const ProjectStats& stats) {
    // Add custom validation
    if (stats.total_rac < 0) {
        LogPrintf("ERROR: Negative RAC detected");
        return false;
    }

    // Existing validation
    return true;
}
```

### Testing Scraper Changes

```bash
# Run as active scraper
./gridcoinresearchd -scraper -debug=scraper

# Monitor scraper activity
tail -f ~/.GridcoinResearch/debug.log | grep -i scraper

# Check manifest generation
./gridcoinresearch-cli getscrapermanifest
```

---

## 9. Protocol Parameter Changes

### Scenario
Need to modify protocol parameters (stake age, superblock interval, etc.)

### Via Protocol Contract (Runtime)

1. Create protocol entry contract
2. Broadcast to network
3. Parameters update when activated

### Via Code (Hard Fork)

Edit consensus parameters:
```cpp
// src/gridcoin/quorum.cpp
int64_t GetSuperblockInterval() {
    if (nBestHeight >= FORK_HEIGHT) {
        return NEW_INTERVAL;
    }
    return OLD_INTERVAL;
}
```

---

## 10. Performance Optimization

### Profiling

```bash
# Build with profiling
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .

# Run with profiler
valgrind --tool=callgrind ./gridcoinresearchd

# Analyze
kcachegrind callgrind.out.*
```

### Common Optimizations

**Reduce Lock Contention**:
```cpp
// Move expensive work outside locks
std::vector<Data> data;
{
    LOCK(cs_registry);
    data = registry.GetData();  // Quick copy
}
// Process data without lock
ProcessData(data);
```

**Cache Expensive Queries**:
```cpp
mutable std::optional<CachedResult> m_cache;

Result GetResult() const {
    if (!m_cache) {
        m_cache = ExpensiveCalculation();
    }
    return *m_cache;
}
```

**Use Const References**:
```cpp
// Avoid copies
void ProcessData(const LargeObject& obj) {  // Reference
    // Use obj
}
```

---

## 11. Ensuring Documentation Passes Lint Checks

### Scenario
You've created or modified markdown documentation and need to ensure it passes the project's lint checker before submitting to GitHub.

### Understanding Lint Requirements
The Gridcoin project uses `test/lint/lint-whitespace.sh` which checks:
- **Trailing whitespace** at end of lines
- **Tab characters** instead of spaces (in code files)
- Various other whitespace issues

Lint failures will **block PR merges** in GitHub CI/CD, so it's essential to verify before committing.

### Quick Fix for Markdown Files

Remove trailing whitespace from all markdown files in a directory:
```bash
cd path/to/directory
for file in *.md; do sed -i 's/[[:space:]]*$//' "$file"; done
```

Verify the fix worked:
```bash
# Should return nothing if all whitespace is removed
grep -n '\s\+$' *.md
```

### Running Full Lint Suite

From repository root:
```bash
# Run all lint checks
test/lint/lint-all.sh

# Run only whitespace check
test/lint/lint-whitespace.sh
```

### Common Issues

**VS Code "Trim Trailing Whitespace" Not Working**:
- May only apply to current file, not all files
- Use the sed command above for comprehensive cleanup

**Mixed Line Endings** (Windows):
```bash
# Convert CRLF to LF
dos2unix *.md
# Or
sed -i 's/\r$//' *.md
```

**Tab Characters in Markdown**:
```bash
# Find tabs
grep -n $'\t' *.md

# Replace tabs with 4 spaces
sed -i 's/\t/    /g' *.md
```

### Pre-Commit Checklist
- [ ] No trailing whitespace
- [ ] No tab characters in markdown
- [ ] Code examples use consistent indentation
- [ ] Lint checker passes (`test/lint/lint-all.sh`)

**Remember**: Always verify lint compliance before submitting documentation PRs to avoid CI failures.

---

## General Development Tips

### Baby Steps™ Checklist
- [ ] Make one focused change
- [ ] Compile and test immediately
- [ ] Document what and why
- [ ] Commit with clear message
- [ ] Validate before moving to next step

### Code Review Checklist
- [ ] Follows coding style (`01-coding.md`)
- [ ] Proper lock ordering
- [ ] No consensus changes without hard fork planning
- [ ] Added tests for new features
- [ ] Updated documentation
- [ ] Considered backward compatibility

### Debugging Tools
- **Debug logging**: `-debug=<category>` flag
- **Print to console**: `-printtoconsole` flag
- **GDB**: `gdb ./gridcoinresearchd`
- **Valgrind**: Memory leak detection
- **Address sanitizer**: `-fsanitize=address` compile flag

---

## 12. Working with CI/CD

### Scenario
You want to verify your changes will pass GitHub Actions CI/CD before pushing, or understand what automated checks run on your PRs.

### Overview

The project uses **4 GitHub Actions workflows** in `.github/workflows/` to ensure code quality:

| Workflow | File | Purpose |
|----------|------|---------|
| **Production** | `cmake_production.yml` | Builds release artifacts (Linux Static, Windows, macOS) and handles deployment |
| **Compatibility** | `cmake_compatibility.yml` | Tests cross-compilation for ARM64/ARMhf architectures |
| **Quality** | `cmake_quality.yml` | Runs sanitizers (ASan/UBSan) and linters (code format, shell scripts, Python) |
| **Distro Validation** | `cmake_distros.yml` | Validates build scripts on 6+ Linux distribution families |

**Trigger**: All workflows run automatically on:
- Push to any branch
- Pull Request creation/updates
- Tagged releases (`v*`, `5.*`)

### Running CI Locally

The **killer feature** of Gridcoin's CI system is that you can run the **exact same** pipelines locally before pushing. This uses `act` (Docker-based GitHub Actions runner) to simulate the CI environment.

#### Prerequisites

```bash
# Install act
sudo zypper install act  # openSUSE
brew install act         # macOS
# Or see: https://github.com/nektos/act

# Verify Docker is running
docker ps
```

#### Basic Usage

Run the full Production pipeline:
```bash
cd Gridcoin-Research/
./contrib/devtools/run-local-ci.sh workflow=.github/workflows/cmake_production.yml
```

Run a specific job (e.g., Windows build only):
```bash
./contrib/devtools/run-local-ci.sh \
  workflow=.github/workflows/cmake_production.yml \
  job=depends-builds \
  matrix=host:x86_64-w64-mingw32
```

Run Quality checks (sanitizers + linters):
```bash
./contrib/devtools/run-local-ci.sh workflow=.github/workflows/cmake_quality.yml
```

Validate a specific Linux distro:
```bash
./contrib/devtools/run-local-ci.sh \
  workflow=.github/workflows/cmake_distros.yml \
  job=validate-distro \
  matrix=image:"archlinux:latest"
```

#### Key Features

**Isolation**: Builds run in `/tmp/act-gridcoin-XXX` - your local source tree is never touched.

**Auto-Scaling**: The script calculates `nproc / jobs` to prevent system overload (e.g., 6 parallel distro builds on 32-core machine = ~5 threads each).

**Caching**: Maps `~/.ccache` to containers for fast subsequent runs.

**Dirty Tree Support**: Uses `rsync` to copy your working directory, including uncommitted changes.

### Common CI/CD Tasks

#### Before Pushing Code

```bash
# 1. Run linters (fastest check)
./contrib/devtools/run-local-ci.sh \
  workflow=.github/workflows/cmake_quality.yml \
  job=lint

# 2. If lint passes, test your platform
./contrib/devtools/run-local-ci.sh \
  workflow=.github/workflows/cmake_production.yml \
  job=depends-builds \
  matrix=host:x86_64-pc-linux-gnu  # Or your target platform
```

#### Debugging CI Failures

**View GitHub Actions logs**:
1. Go to PR → "Checks" tab
2. Click failing workflow
3. Expand failed step to see error

**Reproduce locally**:
```bash
# Copy the exact matrix parameters from the failing job
./contrib/devtools/run-local-ci.sh \
  workflow=<failing_workflow.yml> \
  job=<job_name> \
  matrix=<exact_matrix_params>
```

**Common Issues**:
- **Lint failures**: Run `test/lint/lint-all.sh` locally first
- **Test failures**: Run `./src/test/test_gridcoinresearch --log_level=all` for verbose output
- **Sanitizer errors**: Run with ASan locally: `./contrib/devtools/run-local-ci.sh workflow=.github/workflows/cmake_quality.yml job=sanitizers`

#### Understanding Workflow Structure

Each workflow YAML file defines:
- **Triggers**: When it runs (push, PR, tags)
- **Jobs**: Parallel tasks (e.g., linux-build, windows-build, macos-build)
- **Matrix**: Parameter variations (e.g., different architectures, distros)
- **Steps**: Individual commands within each job

**Example job hierarchy**:
```
cmake_production.yml
├── depends-builds (job)
│   ├── matrix: x86_64-pc-linux-gnu (Linux Static)
│   ├── matrix: x86_64-w64-mingw32 (Windows)
│   └── matrix: x86_64-apple-darwin (macOS - runs on separate job)
└── deploy (job - only on tags)
```

### Release Deployment

When a tag is pushed (e.g., `v5.4.9.99`):

1. All Production builds complete
2. `deploy` job activates
3. Downloads all artifacts (Linux `.tar.gz`, Windows `.exe`, macOS `.dmg`)
4. Generates `SHA256SUMS.txt`
5. Creates **Draft Release** on GitHub with binaries attached

**Manual steps** (maintainer only):
- Review draft release
- Add release notes
- Publish release

### Best Practices

**Pre-Push Checklist**:
- [ ] Run linters locally (`test/lint/lint-all.sh`)
- [ ] Run unit tests (`./src/test/test_gridcoinresearch`)
- [ ] Test CI job for your platform if making build system changes
- [ ] Check that code follows `01-coding.md` style guide

**For Major Changes**:
- [ ] Run full Production pipeline locally
- [ ] Run Compatibility checks if modifying core logic
- [ ] Run Quality checks if touching consensus/memory-critical code

**Documentation Changes**:
- [ ] Always run lint checker before pushing markdown
- [ ] Use `sed` to strip trailing whitespace (see section 11)
- [ ] Remember: Lint failures **block PR merges**

### Advanced: Technical Details

For complete documentation on:
- Hermetic build system (`depends`)
- Cross-compilation toolchain setup
- Windows testing via Wine
- Distro isolation strategies
- Caching and optimization

**See**: `Gridcoin-Research/doc/ci_cd.md`

---

## Related Documentation

- **Architecture**: `02-architecture-overview.md`
- **Glossary**: `03-core-concepts-glossary.md`
- **Components**: `04-component-guide.md`
- **Coding Style**: `01-coding.md`
- **CI/CD Technical Details**: `Gridcoin-Research/doc/ci_cd.md`

This guide provides practical workflows for common tasks. Always take Baby Steps™ - make one focused change at a time, test thoroughly, and document your work. **The process is the product.**
