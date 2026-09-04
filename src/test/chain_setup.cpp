// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "test/chain_setup.h"

#include "chainparams.h"
#include "chainparamsbase.h"
#include "consensus/consensus.h"
#include "dbwrapper.h"
#include "fs.h"
#include "gridcoin/staking/kernel.h"
#include "init.h"
#include "gridcoin/staking/status.h"
#include "miner.h"
#include "net.h"
#include "node/blockstorage.h"
#include "primitives/block.h"
#include "script/sign.h"
#include "test/psgt_test_helpers.h"
#include "txmempool.h"
#include "util/system.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <set>

namespace grc_test {

namespace {

//! Every fixture transaction carries this timestamp. Fixed rather than
//! wall-clock so runs are reproducible, and comfortably after the regtest
//! genesis timestamp (1296688602) because ConnectInputs rejects a transaction
//! older than the input it spends.
constexpr int64_t FIXTURE_TX_TIME = 1600000000;

//! Restores the process-wide chain parameters. The in-tree idiom
//! (block_rewards_tests.cpp) calls SelectParams alone; the path cache must go
//! with it, because the datadir is network-specific.
struct ParamsRestorer
{
    std::string m_chain;

    ParamsRestorer() : m_chain(Params().NetworkIDString()) {}

    ~ParamsRestorer()
    {
        SelectParams(m_chain);
        gArgs.ClearPathCache();
    }
};

//! Restores the consensus globals LoadBlockIndex() overwrites for regtest.
//! It writes them before its own failure return, so even a construction that
//! goes on to throw has already mutated them.
struct ConsensusGlobalsRestorer
{
    unsigned int m_stake_min_age;
    int m_coinbase_maturity;
    int m_grandfather;
    int m_max_outbound;

    ConsensusGlobalsRestorer()
        : m_stake_min_age(nStakeMinAge)
        , m_coinbase_maturity(nCoinbaseMaturity)
        , m_grandfather(nGrandfather)
        , m_max_outbound(MAX_OUTBOUND_CONNECTIONS)
    {
    }

    ~ConsensusGlobalsRestorer()
    {
        nStakeMinAge = m_stake_min_age;
        nCoinbaseMaturity = m_coinbase_maturity;
        nGrandfather = m_grandfather;
        MAX_OUTBOUND_CONNECTIONS = m_max_outbound;
    }
};

//! Saves the chain tip globals and NULLS them, which is a precondition rather
//! than tidiness: CTxDB::LoadBlockIndex returns "hashBestChain not found"
//! whenever pindexGenesisBlock is non-null and the database holds no
//! hashBestChain record, and LoadBlockIndex() then bails before it would create
//! genesis. Suites that run earlier in this binary routinely leave that global
//! non-null, and several leave it dangling (see project_tests.cpp's
//! TestStateGuard, which records that a dangling pindexGenesisBlock is benign on
//! Linux but a wild-pointer fault under the Windows cross-compile's Wine run).
struct ChainTipRestorer
{
    CBlockIndex* m_pindex_best;
    CBlockIndex* m_pindex_genesis;
    uint256 m_hash_best;
    int m_best_height;
    //! Set by UpdateSyncTime on every connect and read by OutOfSyncByAge, which
    //! gates the scraper's unauthenticated manifest path among others. A suite
    //! that mines leaves it at a fresh block's time, and every later suite then
    //! believes the node is in sync.
    int64_t m_previous_block_time;
    //! Whatever an earlier suite left in mapBlockIndex, held aside for the
    //! fixture's lifetime. LoadBlockIndex() only creates genesis into an empty
    //! map, and which suites leave entries behind depends on link order: the
    //! Windows cross-compile leg runs a suite before this one that does. Moving
    //! the map keeps every node where it is, which matters because each
    //! CBlockIndex::phashBlock aliases its map key.
    decltype(mapBlockIndex) m_preexisting_index;

    ChainTipRestorer()
    {
        LOCK(cs_main);

        m_pindex_best = pindexBest;
        m_pindex_genesis = pindexGenesisBlock;
        m_hash_best = hashBestChain;
        m_best_height = nBestHeight;
        m_previous_block_time = g_previous_block_time.load();
        m_preexisting_index = std::move(mapBlockIndex);
        mapBlockIndex.clear();

        pindexBest = nullptr;
        pindexGenesisBlock = nullptr;
        hashBestChain = uint256();
        nBestHeight = -1;
    }

    ~ChainTipRestorer()
    {
        LOCK(cs_main);

        // The block index entries this fixture created are pool-allocated and
        // never freed (BlockIndexPool), and phashBlock aliases the map key, so
        // the map is erased and the objects are left behind. The map was
        // emptied at construction, so everything in it now is ours; then the
        // earlier suites' entries go back exactly as they were.
        mapBlockIndex.clear();
        mapBlockIndex = std::move(m_preexisting_index);

        pindexBest = m_pindex_best;
        pindexGenesisBlock = m_pindex_genesis;
        hashBestChain = m_hash_best;
        nBestHeight = m_best_height;
        g_previous_block_time.store(m_previous_block_time);
    }
};

//! Removes the CWalletTx entries the fixture added to the process-global
//! wallet. The planted key is left behind -- leaking keys into pwalletMain is
//! already precedented (addressbook_tests, qt_wallettxstore_chain_tests) -- but
//! wallet transactions are not: accounting_tests pins absolute nOrderPosNext
//! values that CWalletDB::ReorderTransactions recomputes over the whole
//! mapWallet.
struct WalletTxRestorer
{
    std::set<uint256> m_preexisting;

    WalletTxRestorer()
    {
        if (!pwalletMain) return;

        LOCK(pwalletMain->cs_wallet);

        for (const auto& entry : pwalletMain->mapWallet) {
            m_preexisting.insert(entry.first);
        }
    }

    ~WalletTxRestorer()
    {
        if (!pwalletMain) return;

        std::vector<uint256> added;

        {
            LOCK(pwalletMain->cs_wallet);

            for (const auto& entry : pwalletMain->mapWallet) {
                if (!m_preexisting.count(entry.first)) added.push_back(entry.first);
            }
        }

        for (const uint256& hash : added) {
            pwalletMain->EraseFromWallet(hash);
        }
    }
};

//! The fixture's state. Members are declared so that every restorer is fully
//! constructed before any mutation happens: a throw later in the constructor
//! then unwinds them, which a destructor could not do for a partially
//! constructed object.
struct ChainState
{
    ParamsRestorer m_params;
    ConsensusGlobalsRestorer m_consensus;
    WalletTxRestorer m_wallet;
    ChainTipRestorer m_tip;

    CKey m_key;
    CBasicKeyStore m_keystore;
    CTransaction m_premine_coinbase;

    ChainState();
    ~ChainState();
};

std::unique_ptr<ChainState> g_state;

ChainState& State()
{
    BOOST_REQUIRE_MESSAGE(g_state, "no RegtestChainSetup is alive");
    return *g_state;
}

ChainState::ChainState()
{
    // Regtest, and the datadir must follow it: GetDataDir() appends a
    // network-specific component, and the global TestingSetup points -datadir
    // at a temp path it never creates, so GetDataDir() resolves empty until it
    // does exist. The block flat files are not mocked -- only LevelDB is -- so
    // blk0001.dat and CheckDiskSpace both need a real directory.
    SelectParams(CBaseChainParams::REGTEST);
    gArgs.ClearPathCache();

    const std::string datadir = gArgs.GetArg("-datadir", "");
    BOOST_REQUIRE_MESSAGE(!datadir.empty(), "TestingSetup did not set -datadir");

    // Create the network subdirectory too, not just the base. GetDataDir() appends
    // BaseParams().DataDir() and hands the result to CreateOwnerOnlyDirectory, which
    // restricts only a directory it created itself and leaves a pre-existing one
    // alone. Creating it here therefore keeps the fixture out of that path -- which
    // matters because it is not always available: under Wine, where the Windows
    // cross-compile CI leg runs this suite, applying an owner-only DACL fails, the
    // helper withdraws the directory it just made, and GetDataDir() resolves empty.
    fs::create_directories(fs::path(datadir) / BaseParams().DataDir());
    gArgs.ClearPathCache();
    BOOST_REQUIRE_MESSAGE(!GetDataDir().empty(), "datadir still resolves empty");

    {
        LOCK(cs_main);
        // ChainTipRestorer moved any earlier suite's entries aside; if this
        // fires, a restorer was reordered after the mutating work.
        BOOST_REQUIRE_MESSAGE(mapBlockIndex.empty(),
            "mapBlockIndex is not empty after ChainTipRestorer set it aside");
    }

    // Takes cs_main itself, so it must not be held here.
    BOOST_REQUIRE_MESSAGE(LoadBlockIndex(/*fAllowNew=*/true), "LoadBlockIndex failed");

    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexGenesisBlock != nullptr);
        BOOST_REQUIRE(pindexBest != nullptr);
        BOOST_REQUIRE_EQUAL(nBestHeight, 0);
    }

    // The premine key, and the coins it owns. The genesis block is
    // deterministic, so its coinbase can be recomputed rather than read back.
    m_key = GetRegtestPremineKey();
    BOOST_REQUIRE(m_key.IsValid());
    m_keystore.AddKey(m_key);

    const CBlock genesis = CreateGenesisBlock();
    BOOST_REQUIRE(!genesis.vtx.empty());
    m_premine_coinbase = genesis.vtx[0];
    BOOST_REQUIRE_MESSAGE(!m_premine_coinbase.vout.empty(),
                          "regtest genesis coinbase has no premine outputs");

    // LoadBlockIndex writes this one index entry itself, because genesis
    // bypasses ConnectBlock. Without it nothing can spend the premine, and the
    // selection loop would never see a resolvable input.
    {
        LOCK(cs_main);
        CTxDB txdb("r");
        CTxIndex txindex;
        BOOST_REQUIRE_MESSAGE(txdb.ReadTxIndex(m_premine_coinbase.GetHash(), txindex),
                              "regtest genesis coinbase is not in the transaction index");
    }

    // Give the wallet the premine key and let it see the genesis outputs, so
    // CreateAndProcessBlock() has something to stake.
    //
    // The plant helper rescans only when it adds the key. A second fixture in
    // the same process finds the key already there, because keys are never
    // erased, while WalletTxRestorer removed the coinbase entry the first
    // fixture's rescan added. So rescan here unconditionally; the helper's own
    // rescan, when it runs, is repeated harmlessly.
    if (pwalletMain) {
        PlantRegtestPremineKey(pwalletMain);

        LOCK(cs_main);
        pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, /*fUpdate=*/true);
    }
}

ChainState::~ChainState()
{
    // AppendBlockFile caches a FILE* and the current block file number; both
    // are process-global and must not outlive the datadir this fixture used.
    {
        LOCK(cs_main);
        CloseBlockFile();
    }

    mempool.clear();

    // CreateAndProcessBlock() drives the real miner, which records its kernel
    // search in the process-global miner status; StakingActive() then answers
    // true for the rest of the run, and interfaces_tests pins it false.
    g_miner_status.ClearLastSearch();
    g_miner_status.ClearLastStake();

    // The restorers unwind in reverse declaration order from here.
}

} // anonymous namespace

RegtestChainSetup::RegtestChainSetup()
{
    BOOST_REQUIRE_MESSAGE(!g_state,
        "a RegtestChainSetup is already alive: it is a suite-level fixture "
        "(*boost::unit_test::fixture<RegtestChainSetup>()), not a per-case one");

    g_state = std::make_unique<ChainState>();
}

RegtestChainSetup::~RegtestChainSetup()
{
    g_state.reset();
}

const CKey& PremineKey() { return State().m_key; }

const CBasicKeyStore& PremineKeystore() { return State().m_keystore; }

const CTransaction& PremineCoinbase() { return State().m_premine_coinbase; }

CScript PremineScript() { return psgt_test::P2PKH(PremineKey().GetPubKey().GetID()); }

int64_t FixtureTxTime() { return FIXTURE_TX_TIME; }

std::vector<COutPoint> SpendablePremineOutputs()
{
    const CTransaction& coinbase = PremineCoinbase();
    const uint256 hash = coinbase.GetHash();

    std::vector<COutPoint> out;

    LOCK(cs_main);

    CTxDB txdb("r");
    CTxIndex txindex;

    if (!txdb.ReadTxIndex(hash, txindex)) return out;

    for (unsigned int i = 0; i < coinbase.vout.size(); ++i) {
        if (i >= txindex.vSpent.size()) break;
        if (!txindex.vSpent[i].IsNull()) continue;
        if (coinbase.vout[i].IsEmpty()) continue;

        out.emplace_back(hash, i);
    }

    return out;
}

CTransaction CreateSpendToScript(const CTransaction& txFrom, uint32_t n, CAmount fee,
                                 const CScript& script_pub_key, int n_outputs, int64_t tx_time)
{
    BOOST_REQUIRE(n < txFrom.vout.size());
    BOOST_REQUIRE(n_outputs >= 1);

    const CAmount value_in = txFrom.vout[n].nValue;
    const CAmount value_out = value_in - fee;
    BOOST_REQUIRE_MESSAGE(value_out > 0, "fee exceeds the value of the input");

    CMutableTransaction mtx;
    mtx.nTime = static_cast<unsigned int>(tx_time != 0 ? tx_time : FIXTURE_TX_TIME);
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(txFrom.GetHash(), n);
    mtx.vout.resize(n_outputs);

    const CAmount each = value_out / n_outputs;
    BOOST_REQUIRE_MESSAGE(each > 0, "too many outputs for the value being spent");

    for (int i = 0; i < n_outputs; ++i) {
        mtx.vout[i].nValue = each;
        mtx.vout[i].scriptPubKey = script_pub_key;
    }

    // Any rounding remainder goes to the fee rather than to an output, so the
    // fee a test asks for is the fee the miner will compute.
    mtx.vout[0].nValue += value_out - each * n_outputs;

    BOOST_REQUIRE_MESSAGE(SignSignature(PremineKeystore(), txFrom, mtx, 0),
                          "failed to sign the fixture spend");

    return CTransaction(mtx);
}

CTransaction CreateSpend(const CTransaction& txFrom, uint32_t n, CAmount fee, int n_outputs,
                         int64_t tx_time)
{
    return CreateSpendToScript(txFrom, n, fee, PremineScript(), n_outputs, tx_time);
}

CTransaction CreateCoinstakeShaped(const CTransaction& txFrom, uint32_t n, CAmount fee)
{
    BOOST_REQUIRE(n < txFrom.vout.size());

    const CAmount value_in = txFrom.vout[n].nValue;
    BOOST_REQUIRE_MESSAGE(value_in > fee, "fee exceeds the value of the input");

    CMutableTransaction mtx;
    mtx.nTime = static_cast<unsigned int>(FIXTURE_TX_TIME);
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(txFrom.GetHash(), n);
    mtx.vout.resize(2);
    mtx.vout[0].SetEmpty();
    mtx.vout[1].nValue = value_in - fee;
    mtx.vout[1].scriptPubKey = PremineScript();

    BOOST_REQUIRE_MESSAGE(SignSignature(PremineKeystore(), txFrom, mtx, 0),
                          "failed to sign the fixture coinstake");

    const CTransaction tx(mtx);
    BOOST_REQUIRE_MESSAGE(tx.IsCoinStake(), "fixture coinstake is not shaped like one");

    return tx;
}

void AddToMempool(const CTransaction& tx, CAmount fee)
{
    LOCK(mempool.cs);

    mempool.addUnchecked(
        tx.GetHash(),
        CTxMemPoolEntry(tx, fee, tx.nTime, nBestHeight,
                        ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)));
}

void MakeBlockAndCoinstake(CBlock& block, CMutableTransaction& coinbase,
                           CMutableTransaction& coinstake)
{
    block.SetNull();

    // Regtest activates every fork at height 0 except V15, so this is the block
    // version the chain actually runs, and it is >= 12, which is what makes
    // CreateRestOfTheBlock compute the coinstake size reserve rather than
    // starting from a flat 1000 bytes.
    block.nVersion = ComputeBlockVersion(nBestHeight + 1);
    block.nTime = static_cast<unsigned int>(FIXTURE_TX_TIME + 1000);
    block.vtx.resize(2);

    coinbase = CMutableTransaction();
    coinbase.nTime = block.nTime;

    // The prologue reads vout[1] for its size reserve, and CreateCoinStake
    // leaves exactly these two outputs before splitting and MRC binding.
    coinstake = CMutableTransaction();
    coinstake.nTime = block.nTime;
    coinstake.vin.resize(1);
    coinstake.vout.resize(2);
    coinstake.vout[0].SetEmpty();
    coinstake.vout[1].nValue = 0;
    coinstake.vout[1].scriptPubKey = PremineScript();
}

bool CreateAndProcessBlock(CBlock& block_out, std::string& err)
{
    if (!pwalletMain) {
        err = "no wallet";
        return false;
    }

    // The kernel is evaluated at one 16-second-masked timestamp with no time
    // search, so a retry only helps if it steps to a fresh slot. This mirrors
    // the loop in generatetoaddress.
    bool ok = false;

    for (int attempt = 0; attempt < 5 && !ok; ++attempt) {
        err.clear();
        ok = TryMineRegtestBlock(pwalletMain, block_out, err, attempt);
    }

    if (!ok) return false;

    // RegisterValidationInterface() is a silent no-op in this binary --
    // registration is gated on a MainSignalsInstance that only
    // RegisterBackgroundSignalScheduler creates, and TestingSetup builds no
    // scheduler -- so CWallet::BlockConnected never fires. Rescan instead,
    // which reaches AddToWalletIfInvolvingMe without the signal layer.
    {
        LOCK(cs_main);

        if (pindexGenesisBlock) {
            pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, /*fUpdate=*/true);
        }
    }

    return true;
}

} // namespace grc_test
