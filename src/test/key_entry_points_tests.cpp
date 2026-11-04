// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "fs.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/mnemonics.h"
#include "gridcoin/researcher.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/scraper/scraper.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/voting/builders.h"
#include "gridcoin/voting/fwd.h"
#include "gridcoin/voting/poll.h"
#include "init.h"
#include "interfaces/mrc.h"
#include "interfaces/node.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "key.h"
#include "key_io.h"
#include "node/ui_interface.h"
#include "primitives/transaction.h"
#include "psgt.h"
#include "rpc/client.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "streams.h"
#include "support/allocators/secure.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "test/data/key_entry_points.json.h"
#include "test/mock_wallet_db.h"
#include "test/psgt_test_helpers.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "util/fraction.h"
#include "util/strencodings.h"
#include "util/system.h"
#include "util/time.h"
#include "validation.h"
#include "version.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include <boost/signals2/connection.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/tree/traverse.hpp>
#include <boost/test/tree/visitor.hpp>

#include <algorithm>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// The scraper's directory for its log, set by the scraper threads.
extern fs::path pathDataDir;

// Every entry point that reaches the wallet's private keys -- GetKey(),
// DecryptSecretWithMasterKey(), GetSeedPhraseBlob() or AddKeyPubKey() -- is a
// row of src/test/data/key_entry_points.json. Each row is driven on an
// encrypted wallet unlocked for staking only: without a capability it must be
// refused, and under a capability of its kind it must not be. The lint
// test/lint/lint-staking-key-access.py keeps the table's list of the files that
// call the four functions current, so a new path to the keys needs a row here.
namespace {

using grc_test::MockWalletDbScope;
using grc_test::SwapMainWallet;

//! The RPCs that read, sign with or write a private key or the seed phrase, as
//! an inventory of every command found them. Each is a row of the table or is
//! listed there with the reason it is not, so no row can be dropped unnoticed.
const std::vector<std::string> INVENTORY_RPCS{
    "burn", "claimhtlc", "createhtlc", "dumpprivkey", "dumpseedphrase", "dumpwallet", "encryptwallet",
    "getaccountaddress", "getnewaddress", "getnewpubkey", "importprivkey", "importwallet", "keypoolrefill",
    "consolidateunspent", "makekeypair", "makeseedphrase", "rainbymagnitude", "refundhtlc",
    "restoreseedphrase", "sendfrom", "sendmany", "sendtoaddress", "sethdseed", "splitunspent",
    "signmessage", "signrawtransaction", "signrawtransactionwithkey", "signrawtransactionwithwallet",
    "sweepuncoveredcoins", "upgradewallet", "walletlock", "walletpassphrase", "walletpassphrasechange",
    "walletprocesspsgt", "walletcreatefundedpsgt", "signpsgtinpool",
    "advertisebeacon", "advertisebeaconv3", "beaconauth", "createmrcrequest", "revokebeacon",
    "registerpool", "withdrawpool", "approvepool", "authorizepool", "removepool", "addkey",
    "sendalert", "sendalert2", "sendscraperfilemanifest",
    "addpoll", "vote", "votebyid",
};

//! The same for the IPC methods, as interface.method.
const std::vector<std::string> INVENTORY_IPC{
    "Wallet.encryptWallet", "Wallet.lockWallet", "Wallet.elevateWallet", "Wallet.unlockWallet",
    "Wallet.changeWalletPassphrase", "Wallet.getKeyFromPool", "Wallet.sendCoins", "Wallet.signMessage",
    "Wallet.getNewReceiveAddress", "Wallet.getNewReceiveAddressWithLabel",
    "MRC.snapshot", "MRC.submit", "MRC.submitConfirmed",
    "VotingManager.submitPoll", "VotingManager.submitVote",
    "ResearcherContext.advertiseBeacon", "ResearcherContext.generateBeaconKeyForV3",
    "ResearcherContext.advertiseBeaconV3",
    "PSGTPoolContext.signPoolEntry", "PSGTPoolContext.signPSGT",
    "Node.executeRpcConsoleCommand",
};

//! The environments a row can ask for.
const std::set<std::string> KNOWN_NEEDS{
    "in_sync", "not_ibd", "v15", "accounts", "researcher", "scraper", "import_file",
    "premine", "no_seed_phrase", "empty_pool",
};

//! The narrowest functions a row can be checked at.
const std::set<std::string> KNOWN_PROBES{
    "IsScraperAuthorizedToBroadcastManifests", "VoteBuilder::BuildContractTx",
};

const std::string CPID_HEX{"f5d8234352e5a5ae3915debba7258294"};
const std::string HASH_HEX(64, '1');
const std::string DUMP_FILE{"key_entry_points_dump.txt"};
const std::string IMPORT_FILE{"key_entry_points_import.txt"};

//! A well-formed PSGT pool image that is not in the pool.
const std::string ABSENT_POOL_IMAGE(40, 'a');

const std::string MRC_STAKING_ONLY_REFUSAL =
    "The wallet is unlocked for staking only. Unlock it fully to submit a manual research claim.";

//! A function rather than a constant: the translation it goes through is not
//! set up yet while namespace-scope objects are initialised.
std::string VotingStakingOnlyRefusal()
{
    return _("Error: Wallet unlocked for staking only, unable to create transaction.");
}

UniValue Table()
{
    UniValue table;
    BOOST_REQUIRE_MESSAGE(table.read(std::string(json_tests::key_entry_points,
                                                 json_tests::key_entry_points + sizeof(json_tests::key_entry_points))),
                          "key_entry_points.json does not parse");
    BOOST_REQUIRE(table.isObject());
    return table;
}

std::vector<std::string> Strings(const UniValue& value)
{
    std::vector<std::string> out;
    if (value.isNull()) return out;
    BOOST_REQUIRE(value.isArray());
    for (size_t i = 0; i < value.size(); ++i) {
        BOOST_REQUIRE(value[i].isStr());
        out.push_back(value[i].get_str());
    }
    return out;
}

std::set<std::string> Needs(const UniValue& row)
{
    const std::vector<std::string> needs = Strings(find_value(row, "needs"));
    return {needs.begin(), needs.end()};
}

std::string Field(const UniValue& row, const std::string& key)
{
    const UniValue& value = find_value(row, key);
    return value.isStr() ? value.get_str() : std::string();
}

std::string IpcName(const UniValue& row)
{
    return Field(row, "interface") + "." + Field(row, "method");
}

std::optional<CapabilityKind> ParseKind(const std::string& name)
{
    static const std::map<std::string, CapabilityKind> kinds{
        {"Send", CapabilityKind::Send},
        {"SignMessage", CapabilityKind::SignMessage},
        {"SignPSGT", CapabilityKind::SignPSGT},
        {"Contract", CapabilityKind::Contract},
        {"Backup", CapabilityKind::Backup},
        {"FileTransfer", CapabilityKind::FileTransfer},
        {"Settings", CapabilityKind::Settings},
        {"AutoUnlockSetup", CapabilityKind::AutoUnlockSetup},
        {"RpcRequest", CapabilityKind::RpcRequest},
    };

    const auto it = kinds.find(name);
    if (it == kinds.end()) return std::nullopt;
    return it->second;
}

//! Every test case in the binary, as suite/case, nested suites joined by '/'.
class TestNames : public boost::unit_test::test_tree_visitor
{
public:
    std::set<std::string> names;

    using boost::unit_test::test_tree_visitor::visit;

    void visit(const boost::unit_test::test_case& test) override
    {
        names.insert(Path() + test.p_name.get());
    }

    bool test_suite_start(const boost::unit_test::test_suite& suite) override
    {
        // The master suite is the root and has no name of its own in a path.
        if (suite.p_parent_id != boost::unit_test::INV_TEST_UNIT_ID) m_path.push_back(suite.p_name.get());
        return true;
    }

    void test_suite_finish(const boost::unit_test::test_suite& suite) override
    {
        if (suite.p_parent_id != boost::unit_test::INV_TEST_UNIT_ID) m_path.pop_back();
    }

private:
    std::vector<std::string> m_path;

    std::string Path() const
    {
        std::string path;
        for (const std::string& part : m_path) path += part + "/";
        return path;
    }
};

std::set<std::string> AllTestNames()
{
    TestNames visitor;
    boost::unit_test::traverse_test_tree(boost::unit_test::framework::master_test_suite(), visitor,
                                         /*ignore_status=*/true);
    return visitor.names;
}

//! What the wallet holds besides its own key, as the rows' needs say.
struct WalletOptions
{
    bool seed_phrase{true};
    bool empty_pool{false};
    bool premine{false};

    static WalletOptions For(const std::set<std::string>& needs)
    {
        WalletOptions options;
        options.seed_phrase = needs.count("no_seed_phrase") == 0;
        options.empty_pool = needs.count("empty_pool") != 0;
        options.premine = needs.count("premine") != 0;
        return options;
    }

    bool operator<(const WalletOptions& other) const
    {
        return std::tie(seed_phrase, empty_pool, premine) < std::tie(other.seed_phrase, other.empty_pool, other.premine);
    }
};

//! An encrypted, file-backed HD wallet with one loose key of its own, a key
//! pool and, unless asked otherwise, a seed phrase whose master is the HD
//! master, as makeseedphrase leaves a wallet. Swapped into pwalletMain and left
//! locked.
//!
//! File-backed on the mocked database, so that the key pool, the HD chain and
//! the records an operation writes behave as they do on a node. The scope
//! isolates its records from every other case's.
struct KeyReachWallet
{
    static inline const SecureString PASSPHRASE{"key-entry-points-test"};
    static inline const std::string WALLET_FILE{"wallet_key_entry_points.dat"};

    grc_test::ForcedArgGuard m_pool_size{"keypool", "2"};
    MockWalletDbScope m_db{WALLET_FILE};
    CWallet wallet{WALLET_FILE};
    SwapMainWallet m_swap{&wallet};

    CKey own;
    CKeyID own_id;

    explicit KeyReachWallet(const WalletOptions& options)
    {
        BOOST_REQUIRE(m_db.isolated());

        own.MakeNewKey(true);
        own_id = own.GetPubKey().GetID();

        {
            LOCK(wallet.cs_wallet);
            BOOST_REQUIRE(wallet.AddKey(own));

            if (options.seed_phrase) {
                CKey master;
                const SecureString phrase = GRC::Mnemonics::GenerateSeedPhrase(SecureString(), master);

                CSeedPhraseData data;
                data.vchBlob.resize(GRC::Mnemonics::ENCIPHERED_LENGTH);
                BOOST_REQUIRE(GRC::Mnemonics::DecodeSeedPhrase(phrase, MakeWritableByteSpan(data.vchBlob)));

                CKey parsed;
                BOOST_REQUIRE(GRC::Mnemonics::ParseSeedPhrase(phrase, SecureString(), parsed, &data.nBirthday));
                data.masterKeyID = master.GetPubKey().GetID();

                BOOST_REQUIRE(wallet.SetHDMasterKey(wallet.DeriveNewMasterHDKey(master)));
                BOOST_REQUIRE(wallet.SetSeedPhraseData(data));
            } else {
                BOOST_REQUIRE(wallet.SetHDMasterKey(wallet.GenerateNewHDMasterKey()));
            }
        }

        // Encrypted as an in-memory wallet: on a file-backed one EncryptWallet
        // rewrites wallet.dat, which the mocked database refuses. The key pool
        // it makes is written all the same, through the wallet's file name.
        wallet.fFileBacked = false;
        const bool encrypted = wallet.EncryptWallet(PASSPHRASE);
        wallet.fFileBacked = true;
        BOOST_REQUIRE(encrypted);

        // The premine key as a regtest node plants it, with the genesis
        // coinbase that pays it.
        if (options.premine) {
            UnlockFor(UnlockScope::Full);
            PlantRegtestPremineKey(&wallet);
            BOOST_REQUIRE(wallet.HaveKey(grc_test::PremineKey().GetPubKey().GetID()));
        }

        if (options.empty_pool) DrainPool();

        wallet.Lock();
        BOOST_REQUIRE(wallet.IsLocked());
        BOOST_REQUIRE(wallet.IsHDEnabled());
        BOOST_REQUIRE_EQUAL(WITH_LOCK(wallet.cs_wallet, return wallet.HasSeedPhrase()), options.seed_phrase);
    }

    ~KeyReachWallet()
    {
        wallet.Lock();
    }

    KeyReachWallet(const KeyReachWallet&) = delete;
    KeyReachWallet& operator=(const KeyReachWallet&) = delete;

    void UnlockFor(UnlockScope scope)
    {
        wallet.Lock();
        BOOST_REQUIRE(wallet.Unlock(PASSPHRASE, scope));
        BOOST_REQUIRE(wallet.GetUnlockScope() == scope);
    }

    //! Empties the pool, as handing out every key in it would.
    void DrainPool()
    {
        LOCK(wallet.cs_wallet);
        for (const int64_t index : wallet.setKeyPool) {
            wallet.KeepKey(index);
        }
        wallet.setKeyPool.clear();
    }

    size_t KeyCount() const
    {
        std::set<CKeyID> keys;
        wallet.GetKeys(keys);
        return keys.size();
    }
};

//! Puts the sync time at now, so the node reads as in sync, and back as it was.
class InSync
{
public:
    InSync() { g_previous_block_time.store(GetAdjustedTime()); }
    ~InSync() { g_previous_block_time.store(m_saved); }

    InSync(const InSync&) = delete;
    InSync& operator=(const InSync&) = delete;

private:
    const int64_t m_saved{g_previous_block_time.load()};
};

//! The researcher has a CPID for its life, and is put back after.
class ResearcherWithCpid
{
public:
    ResearcherWithCpid()
    {
        gArgs.LockSettings([&](util::Settings& settings) { m_saved = settings; });

        gArgs.ForceSetArg("forcecpid", CPID_HEX);
        gArgs.ForceSetArg("noncruncher", "0");

        LOCK(cs_main);
        GRC::Researcher::Reload(GRC::MiningProjectMap());
    }

    ~ResearcherWithCpid()
    {
        gArgs.ForceSetArg("noncruncher", "1");

        {
            LOCK(cs_main);
            GRC::Researcher::Reload(GRC::MiningProjectMap());
        }

        gArgs.LockSettings([&](util::Settings& settings) { settings = m_saved; });
    }

    ResearcherWithCpid(const ResearcherWithCpid&) = delete;
    ResearcherWithCpid& operator=(const ResearcherWithCpid&) = delete;

private:
    util::Settings m_saved;
};

//! The wallet's own key is an authorised scraper, and -scraperkey names it.
//! The scraper's log goes to the data directory meanwhile, not the working one.
class ScraperKey
{
public:
    explicit ScraperKey(const CKeyID& id) : m_saved_log_dir(pathDataDir)
    {
        if (pathDataDir.empty()) pathDataDir = GetDataDir();

        CMutableTransaction mtx;
        mtx.vContracts.push_back(GRC::MakeContract<GRC::ScraperEntryPayload>(
            uint32_t {3}, GRC::ContractAction::ADD, uint32_t {2}, id, GRC::ScraperEntryStatus::AUTHORIZED));
        m_tx = CTransaction(mtx);

        {
            LOCK(cs_main);
            GRC::GetScraperRegistry().Add({m_tx.vContracts[0], m_tx, &m_index});
        }

        m_key.emplace("scraperkey", EncodeDestination(CTxDestination(id)));
    }

    ~ScraperKey()
    {
        pathDataDir = m_saved_log_dir;
    }

    ScraperKey(const ScraperKey&) = delete;
    ScraperKey& operator=(const ScraperKey&) = delete;

private:
    const fs::path m_saved_log_dir;
    grc_test::RegistryReset m_scrapers{{GRC::ContractType::SCRAPER}};
    CTransaction m_tx;
    CBlockIndex m_index{};
    std::optional<grc_test::ForcedArgGuard> m_key;
};

//! Out of initial block download. The check reads as downloading for 15 s
//! after the tip changes, so the clock is moved past that once; the chain
//! fixture's clock only moves forward.
void LeaveInitialBlockDownload()
{
    if (IsInitialBlockDownload()) SetMockTime(GetTime() + 16);
    BOOST_REQUIRE_MESSAGE(!IsInitialBlockDownload(), "the node is still in initial block download");
}

//! The environment a row's needs ask for, for the row's life. The wallet's own
//! needs (premine, no_seed_phrase, empty_pool) are its WalletOptions.
class Environment
{
public:
    Environment(const std::set<std::string>& needs, const KeyReachWallet& fx)
    {
        if (needs.count("in_sync")) m_in_sync.emplace();
        if (needs.count("not_ibd")) LeaveInitialBlockDownload();
        if (needs.count("v15")) m_v15.emplace(0);
        if (needs.count("accounts")) {
            m_accounts.emplace("enableaccounts", "1");
            m_no_staking.emplace("staking", "0");
        }
        if (needs.count("researcher")) m_researcher.emplace();
        if (needs.count("scraper")) m_scraper.emplace(fx.own_id);
        if (needs.count("import_file")) {
            fsbridge::ofstream file(GetDataDir() / IMPORT_FILE);
            file << "# A wallet dump with no keys in it.\n";
        }
    }

    ~Environment()
    {
        // What a row's run may have written to the data directory.
        fs::remove(GetDataDir() / IMPORT_FILE);
        fs::remove(GetDataDir() / DUMP_FILE);
    }

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;

private:
    std::optional<InSync> m_in_sync;
    std::optional<grc_test::V15HeightGuard> m_v15;
    std::optional<grc_test::ForcedArgGuard> m_accounts;
    std::optional<grc_test::ForcedArgGuard> m_no_staking;
    std::optional<ResearcherWithCpid> m_researcher;
    std::optional<ScraperKey> m_scraper;
};

//! The values the table's arguments name.
class Corpus
{
public:
    explicit Corpus(const KeyReachWallet& fx)
    {
        CKey fresh;
        fresh.MakeNewKey(true);

        const CTransaction previous = psgt_test::MakePrevTx(fx.own, 10 * COIN);
        const PartiallySignedTransaction spend = psgt_test::MakeSpendPSGT(previous, 9 * COIN);
        const std::vector<unsigned char> psgt = SerializePSGT(spend);

        CDataStream raw(SER_NETWORK, PROTOCOL_VERSION);
        raw << CTransaction(spend.tx);

        // Longest first, so that $own does not match inside $own_pubkey.
        m_values = {
            {"$own_pubkey", HexStr(fx.own.GetPubKey())},
            {"$premine", EncodeDestination(CTxDestination(grc_test::PremineKey().GetPubKey().GetID()))},
            {"$rawtx", HexStr(raw)},
            {"$psgt", EncodeBase64(psgt.data(), psgt.size())},
            {"$hash", HASH_HEX},
            {"$cpid", CPID_HEX},
            {"$own", EncodeDestination(CTxDestination(fx.own_id))},
            {"$wif", EncodeSecret(fresh)},
        };
    }

    std::vector<std::string> Expand(const std::vector<std::string>& args) const
    {
        std::vector<std::string> out;
        for (std::string arg : args) {
            for (const auto& [name, value] : m_values) {
                for (size_t at = arg.find(name); at != std::string::npos; at = arg.find(name, at + value.size())) {
                    arg.replace(at, name.size(), value);
                }
            }
            out.push_back(std::move(arg));
        }
        return out;
    }

private:
    std::vector<std::pair<std::string, std::string>> m_values;
};

//! What an RPC call came to.
struct Outcome
{
    bool returned{false};
    UniValue result;
    std::optional<int> code;
    std::string message;

    //! It failed outside the command: the arguments did not fit it. The table
    //! turns every exception a command throws into an RPC error, so only the
    //! conversion and the arity check throw anything else.
    bool malformed{false};

    std::string Describe() const
    {
        if (returned) return "returned " + result.write();
        if (code) return strprintf("RPC error %d: %s", *code, message);
        return "malformed call: " + message;
    }
};

Outcome Call(const std::string& name, const std::vector<std::string>& args)
{
    Outcome out;

    try {
        out.result = tableRPC.execute(name, RPCConvertValues(name, args));
        out.returned = true;
    } catch (const UniValue& error) {
        out.code = find_value(error, "code").get_int();
        out.message = find_value(error, "message").getValStr();
    } catch (const std::exception& e) {
        out.malformed = true;
        out.message = e.what();
    }

    return out;
}

//! How a row is refused.
enum class Refusal {
    UnlockNeeded,  //!< RPC_WALLET_UNLOCK_NEEDED
    KeypoolRanOut, //!< RPC_WALLET_KEYPOOL_RAN_OUT, for a pool draw that would make a key
    NotAuthorized, //!< the scraper finds no key it may publish with: nothing published, false
};

std::optional<Refusal> ParseRefusal(const std::string& name)
{
    if (name.empty() || name == "unlock_needed") return Refusal::UnlockNeeded;
    if (name == "not_authorized") return Refusal::NotAuthorized;
    return std::nullopt;
}

//! A probe: true when the narrowest function the row reaches refuses. It runs
//! on the calling thread, under whatever capability scope that thread holds.
bool ProbeRefuses(const std::string& probe, KeyReachWallet& fx)
{
    if (probe == "IsScraperAuthorizedToBroadcastManifests") {
        // As sendscraperfilemanifest asks: no staking access.
        CTxDestination address;
        CKey key;
        return !IsScraperAuthorizedToBroadcastManifests(address, key, nullptr);
    }

    if (probe == "VoteBuilder::BuildContractTx") {
        const GRC::Poll poll(GRC::PollType::SURVEY, GRC::PollWeightType::BALANCE,
                             GRC::PollResponseType::YES_NO_ABSTAIN, 7, "Key entry points", "https://gridcoin.us",
                             "Question?", GRC::Poll::ChoiceList(), GetAdjustedTime(), Fraction());

        // Under the IPC slot's capability, as submitVote builds its vote.
        CapabilityScope scope(fx.wallet, fx.wallet.IpcCapability(), CapabilityKind::Contract);

        try {
            LOCK(cs_main);
            GRC::VoteBuilder::ForPoll(poll, uint256::ONE).AddResponse(uint8_t {0}).BuildContractTx(&fx.wallet, 2);
        } catch (const std::exception& e) {
            return e.what() == VotingStakingOnlyRefusal();
        }

        return false;
    }

    BOOST_ERROR("unknown probe " << probe);
    return false;
}

//! The manifests a scraper row published, for its messages; nothing for the
//! other rows.
std::string Published(Refusal refusal, int published)
{
    return refusal == Refusal::NotAuthorized ? strprintf(", manifests published %d", published) : std::string();
}

//! Whether \p outcome is the refusal \p refusal names. \p published counts the
//! manifests the scraper published meanwhile.
bool IsRefusal(Refusal refusal, const Outcome& outcome, int published)
{
    switch (refusal) {
    case Refusal::UnlockNeeded:
        return outcome.code == RPC_WALLET_UNLOCK_NEEDED;
    case Refusal::KeypoolRanOut:
        return outcome.code == RPC_WALLET_KEYPOOL_RAN_OUT;
    case Refusal::NotAuthorized:
        return outcome.returned && outcome.result.isFalse() && published == 0;
    }

    return false;
}

//! Drive the rows of one RPC section, each refused without a capability and
//! not refused under an RpcRequest capability. Rows that want the same wallet
//! share one, which the rows run on in table order: a refused row changes no
//! key material, as each checks, and a row that ran under the capability
//! leaves the wallet unlocked for staking only, as the next one checks.
void DriveRpcRows(const UniValue& rows, std::optional<Refusal> section_refusal)
{
    BOOST_REQUIRE(rows.isArray());

    const std::set<std::string> test_names = AllTestNames();

    // Rows by the wallet they need, in table order.
    std::map<WalletOptions, std::vector<const UniValue*>> by_wallet;
    std::vector<const UniValue*> covered;

    for (size_t i = 0; i < rows.size(); ++i) {
        const UniValue& row = rows[i];

        if (!Field(row, "covered_by").empty()) {
            covered.push_back(&row);
        } else {
            by_wallet[WalletOptions::For(Needs(row))].push_back(&row);
        }
    }

    for (const UniValue* row : covered) {
        const std::string covered_by = Field(*row, "covered_by");
        BOOST_CHECK_MESSAGE(test_names.count(covered_by), Field(*row, "name") << ": covered_by names "
                            << covered_by << ", which is no test case in this binary");
        BOOST_TEST_MESSAGE(Field(*row, "name") << ": not driven here; covered by " << covered_by);
    }

    for (const auto& [options, wallet_rows] : by_wallet) {
        KeyReachWallet fx(options);
        const Corpus corpus(fx);

        fx.UnlockFor(UnlockScope::StakingOnly);

        // One capability for every row on this wallet; each row opens its own
        // scope, on this thread, as a request carrying the passphrase does.
        const std::optional<CapabilityId> id = fx.wallet.BeginElevation(KeyReachWallet::PASSPHRASE,
                                                                        CapabilityKind::RpcRequest);
        BOOST_REQUIRE(id.has_value());

        for (const UniValue* row : wallet_rows) {
            const std::string name = Field(*row, "name");
            const std::set<std::string> needs = Needs(*row);
            const std::string probe = Field(*row, "probe");
            const std::vector<std::string> args = corpus.Expand(Strings(find_value(*row, "args")));

            const std::optional<Refusal> refusal = section_refusal ? section_refusal
                                                                   : ParseRefusal(Field(*row, "refusal"));
            BOOST_REQUIRE_MESSAGE(refusal.has_value(), name << ": unknown refusal " << Field(*row, "refusal"));

            Environment environment(needs, fx);

            if (needs.count("empty_pool")) fx.DrainPool();

            BOOST_REQUIRE_MESSAGE(fx.wallet.IsUnlockedForStakingOnly() && !fx.wallet.CanUseKeys(),
                                  name << ": the wallet is not unlocked for staking only");

            int published = 0;
            const boost::signals2::scoped_connection manifests(uiInterface.NotifyScraperEvent_connect(
                [&published](const scrapereventtypes& type, ChangeType, const std::string&) {
                    if (type == scrapereventtypes::Manifest) ++published;
                }));

            // Refused without a capability, before any key material changed.
            const size_t keys_before = fx.KeyCount();
            const Outcome refused = Call(name, args);

            const int published_while_refused = published;

            BOOST_CHECK_MESSAGE(IsRefusal(*refusal, refused, published),
                                name << ": not refused while unlocked for staking only: " << refused.Describe()
                                << Published(*refusal, published));
            BOOST_CHECK_MESSAGE(fx.KeyCount() == keys_before, name << ": a refused call changed the wallet's keys");

            if (!probe.empty()) {
                BOOST_CHECK_MESSAGE(ProbeRefuses(probe, fx), name << ": " << probe << " not refused");
            }

            // Not refused under the capability.
            published = 0;
            {
                CapabilityScope scope(fx.wallet, id, CapabilityKind::RpcRequest);
                BOOST_REQUIRE(scope.Active());

                const Outcome allowed = Call(name, args);

                BOOST_CHECK_MESSAGE(!allowed.malformed, name << ": the corpus does not fit the command: "
                                    << allowed.Describe());
                BOOST_CHECK_MESSAGE(allowed.code != RPC_WALLET_UNLOCK_NEEDED && !IsRefusal(*refusal, allowed, published),
                                    name << ": refused under an RpcRequest capability: " << allowed.Describe());

                if (*refusal == Refusal::NotAuthorized) {
                    BOOST_CHECK_MESSAGE(published > 0, name << ": nothing published under an RpcRequest capability");
                }

                if (!probe.empty()) {
                    BOOST_CHECK_MESSAGE(!ProbeRefuses(probe, fx), name << ": " << probe
                                        << " refused under an RpcRequest capability");
                }

                BOOST_TEST_MESSAGE(name << ": refused (" << refused.Describe()
                                   << Published(*refusal, published_while_refused) << "); under the capability: "
                                   << allowed.Describe() << Published(*refusal, published));
            }
        }

        fx.wallet.EndElevation(*id);
        BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    }
}

//! The result of an IPC driver: whether it was refused, and what it said.
struct IpcOutcome
{
    bool refused{false};
    std::string detail;
};

using IpcDriver = std::function<IpcOutcome(KeyReachWallet&)>;

//! One driver per IPC row, by interface.method. Each calls the method as an
//! IPC client would, through the in-process interface on the fixture's wallet,
//! and says whether it was refused.
std::map<std::string, IpcDriver> IpcDrivers()
{
    std::map<std::string, IpcDriver> drivers;

    drivers["Wallet.signMessage"] = [](KeyReachWallet& fx) {
        std::string signature;
        const interfaces::MessageSignStatus status =
            interfaces::MakeWallet(&fx.wallet)->signMessage(EncodeDestination(CTxDestination(fx.own_id)), "x", signature);
        return IpcOutcome{status == interfaces::MessageSignStatus::KeyNotAvailable,
                          strprintf("status %d", static_cast<int>(status))};
    };

    drivers["Wallet.sendCoins"] = [](KeyReachWallet& fx) {
        interfaces::WalletSendRecipient recipient;
        recipient.address = EncodeDestination(CTxDestination(fx.own_id));
        recipient.amount = COIN;

        const interfaces::SendCoinsResult result = interfaces::MakeWallet(&fx.wallet)->sendCoins({recipient}, std::nullopt, 0);
        return IpcOutcome{result.status == interfaces::SendCoinsStatus::WalletUnlockedForStakingOnly,
                          strprintf("status %d", static_cast<int>(result.status))};
    };

    drivers["PSGTPoolContext.signPSGT"] = [](KeyReachWallet& fx) {
        const CTransaction previous = psgt_test::MakePrevTx(fx.own, 10 * COIN);
        const interfaces::PSGTSignResult result = interfaces::MakePSGTPoolContext(&fx.wallet)->signPSGT(
            SerializePSGT(psgt_test::MakeSpendPSGT(previous, 9 * COIN)));
        return IpcOutcome{!result.ok, strprintf("ok %d, signed %d: %s", result.ok, result.signed_now, result.error)};
    };

    drivers["PSGTPoolContext.signPoolEntry"] = [](KeyReachWallet& fx) {
        std::string error;
        std::string txid;
        const interfaces::PSGTSignStatus status =
            interfaces::MakePSGTPoolContext(&fx.wallet)->signPoolEntry(ABSENT_POOL_IMAGE, error, txid);
        return IpcOutcome{status == interfaces::PSGTSignStatus::FAILED,
                          strprintf("status %d: %s", static_cast<int>(status), error)};
    };

    drivers["MRC.submit"] = [](KeyReachWallet& fx) {
        const interfaces::MRCSubmitResult result = interfaces::MakeMRC(&fx.wallet)->submit(0, false);
        return IpcOutcome{result.error == MRC_STAKING_ONLY_REFUSAL, result.success ? "submitted" : result.error};
    };

    drivers["MRC.submitConfirmed"] = [](KeyReachWallet& fx) {
        const interfaces::MRCSubmitResult result = interfaces::MakeMRC(&fx.wallet)->submitConfirmed(0, false, 0, 0);
        return IpcOutcome{result.error == MRC_STAKING_ONLY_REFUSAL, result.success ? "submitted" : result.error};
    };

    drivers["VotingManager.submitPoll"] = [](KeyReachWallet&) {
        interfaces::PollSubmission poll;
        poll.type = static_cast<int>(GRC::PollType::SURVEY);
        poll.title = "Key entry points";
        poll.duration_days = 7;
        poll.question = "Question?";
        poll.url = "https://gridcoin.us";
        poll.weight_type = static_cast<int>(GRC::PollWeightType::BALANCE);
        poll.response_type = static_cast<int>(GRC::PollResponseType::YES_NO_ABSTAIN);

        const interfaces::VotingSubmitResult result = interfaces::MakeVotingManager()->submitPoll(poll);
        return IpcOutcome{result.status == interfaces::VotingSubmitStatus::FAILED
                              && result.error == VotingStakingOnlyRefusal(),
                          strprintf("status %d: %s", static_cast<int>(result.status), result.error)};
    };

    drivers["VotingManager.submitVote"] = [](KeyReachWallet& fx) {
        // The method itself finds no such poll; the probe builds the vote.
        const interfaces::VotingSubmitResult result =
            interfaces::MakeVotingManager()->submitVote(HASH_HEX, {uint8_t {0}});
        BOOST_CHECK(result.status == interfaces::VotingSubmitStatus::POLL_NOT_FOUND);

        return IpcOutcome{ProbeRefuses("VoteBuilder::BuildContractTx", fx), "probe VoteBuilder::BuildContractTx"};
    };

    drivers["ResearcherContext.advertiseBeacon"] = [](KeyReachWallet& fx) {
        const interfaces::BeaconAdvertiseResult result = interfaces::MakeResearcherContext(&fx.wallet)->advertiseBeacon();
        return IpcOutcome{result.status == interfaces::BeaconStatus::ERROR_WALLET_LOCKED,
                          strprintf("status %d", static_cast<int>(result.status))};
    };

    drivers["ResearcherContext.generateBeaconKeyForV3"] = [](KeyReachWallet& fx) {
        const std::string public_key = interfaces::MakeResearcherContext(&fx.wallet)->generateBeaconKeyForV3();
        return IpcOutcome{public_key.empty(), public_key.empty() ? "no key" : "key " + public_key};
    };

    drivers["ResearcherContext.advertiseBeaconV3"] = [](KeyReachWallet& fx) {
        const std::string proof = "<master_url>https://gridcoin.us/</master_url>"
                                  "<msg>1 " + HexStr(fx.own.GetPubKey()) + "</msg>"
                                  "<signature>AAAA</signature>";
        const interfaces::BeaconAdvertiseResult result =
            interfaces::MakeResearcherContext(&fx.wallet)->advertiseBeaconV3(proof);
        return IpcOutcome{result.status == interfaces::BeaconStatus::ERROR_WALLET_LOCKED,
                          strprintf("status %d", static_cast<int>(result.status))};
    };

    drivers["Node.executeRpcConsoleCommand"] = [](KeyReachWallet& fx) {
        const interfaces::RpcConsoleResult result = interfaces::MakeNode()->executeRpcConsoleCommand(
            "signmessage", {EncodeDestination(CTxDestination(fx.own_id)), "x"});
        return IpcOutcome{result.wallet_unlock_needed, strprintf("ok %d: %s", result.ok, result.output)};
    };

    drivers["Wallet.getNewReceiveAddress"] = [](KeyReachWallet& fx) {
        std::string address;
        const bool drawn = interfaces::MakeWallet(&fx.wallet)->getNewReceiveAddress(address);
        return IpcOutcome{!drawn, drawn ? "drew " + address : "no address"};
    };

    drivers["Wallet.getNewReceiveAddressWithLabel"] = [](KeyReachWallet& fx) {
        std::string address;
        const bool drawn = interfaces::MakeWallet(&fx.wallet)->getNewReceiveAddressWithLabel("key entry points", address);
        return IpcOutcome{!drawn, drawn ? "drew " + address : "no address"};
    };

    drivers["Wallet.getKeyFromPool"] = [](KeyReachWallet& fx) {
        CPubKey key;
        const bool drawn = interfaces::MakeWallet(&fx.wallet)->getKeyFromPool("", key);
        return IpcOutcome{!drawn, drawn ? "drew a key" : "no key"};
    };

    return drivers;
}

//! Drive the rows of one IPC section: refused without a capability and under a
//! capability of another kind, and not refused under one of the row's kind,
//! each begun in the IPC slot as an IPC client begins one. A row without a kind
//! is only checked for its refusal. Rows that want the same wallet share one,
//! as the RPC rows do.
void DriveIpcRows(const UniValue& rows)
{
    BOOST_REQUIRE(rows.isArray());

    const std::map<std::string, IpcDriver> drivers = IpcDrivers();

    std::map<WalletOptions, std::vector<const UniValue*>> by_wallet;
    for (size_t i = 0; i < rows.size(); ++i) {
        by_wallet[WalletOptions::For(Needs(rows[i]))].push_back(&rows[i]);
    }

    for (const auto& [options, wallet_rows] : by_wallet) {
        KeyReachWallet fx(options);
        fx.UnlockFor(UnlockScope::StakingOnly);

        const std::unique_ptr<interfaces::Wallet> client = interfaces::MakeWallet(&fx.wallet);

        for (const UniValue* row : wallet_rows) {
            const std::string name = IpcName(*row);
            const std::set<std::string> needs = Needs(*row);

            const auto driver = drivers.find(name);
            BOOST_REQUIRE_MESSAGE(driver != drivers.end(), name << ": no driver");

            Environment environment(needs, fx);

            if (needs.count("empty_pool")) fx.DrainPool();

            BOOST_REQUIRE_MESSAGE(fx.wallet.IsUnlockedForStakingOnly() && !fx.wallet.CanUseKeys(),
                                  name << ": the wallet is not unlocked for staking only");

            const size_t keys_before = fx.KeyCount();
            const IpcOutcome refused = driver->second(fx);

            BOOST_CHECK_MESSAGE(refused.refused, name << ": not refused while unlocked for staking only: "
                                << refused.detail);
            BOOST_CHECK_MESSAGE(fx.KeyCount() == keys_before, name << ": a refused call changed the wallet's keys");

            const UniValue kind_value = find_value(*row, "kind");
            if (kind_value.isNull()) {
                BOOST_TEST_MESSAGE(name << ": refused (" << refused.detail << "); it has no capability kind");
                continue;
            }

            const std::optional<CapabilityKind> kind = ParseKind(kind_value.get_str());
            BOOST_REQUIRE_MESSAGE(kind.has_value(), name << ": unknown kind " << kind_value.get_str());

            // A capability of another kind lifts nothing. No row runs under
            // Backup, so that is the other kind.
            BOOST_REQUIRE(*kind != CapabilityKind::Backup);
            BOOST_REQUIRE(client->beginElevation(KeyReachWallet::PASSPHRASE, CapabilityKind::Backup));
            const IpcOutcome other_kind = driver->second(fx);
            client->endElevation();

            BOOST_CHECK_MESSAGE(other_kind.refused, name << ": not refused under a Backup capability: "
                                << other_kind.detail);

            BOOST_REQUIRE(client->beginElevation(KeyReachWallet::PASSPHRASE, *kind));
            const IpcOutcome allowed = driver->second(fx);
            client->endElevation();

            BOOST_CHECK_MESSAGE(!allowed.refused, name << ": refused under a " << kind_value.get_str()
                                << " capability: " << allowed.detail);

            BOOST_TEST_MESSAGE(name << ": refused (" << refused.detail << "); under the capability: "
                               << allowed.detail);
        }

        BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    }
}

} // namespace

BOOST_AUTO_TEST_SUITE(key_entry_points_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

BOOST_AUTO_TEST_CASE(the_table_names_every_key_reaching_entry_point)
{
    const UniValue table = Table();
    const std::set<std::string> test_names = AllTestNames();

    // Every RPC the table names is a command, named once.
    std::set<std::string> rpcs;
    for (const std::string section : {"rpc", "rpc_pool_draw", "rpc_not_rows"}) {
        const UniValue& rows = find_value(table, section);
        BOOST_REQUIRE_MESSAGE(rows.isArray(), "no \"" << section << "\" array");

        for (size_t i = 0; i < rows.size(); ++i) {
            const std::string name = Field(rows[i], "name");

            BOOST_CHECK_MESSAGE(tableRPC[name] != nullptr, section << ": " << name << " is no command in tableRPC");
            BOOST_CHECK_MESSAGE(rpcs.insert(name).second, name << " is in the table twice");

            if (section == "rpc_not_rows") {
                BOOST_CHECK_MESSAGE(!Field(rows[i], "reason").empty(), name << ": no reason");
                continue;
            }

            BOOST_CHECK_MESSAGE(find_value(rows[i], "args").isArray(), name << ": no args");
            BOOST_CHECK_MESSAGE(!Field(rows[i], "reaches").empty(), name << ": no reaches");

            for (const std::string& need : Needs(rows[i])) {
                BOOST_CHECK_MESSAGE(KNOWN_NEEDS.count(need), name << ": unknown need " << need);
            }

            const std::string probe = Field(rows[i], "probe");
            BOOST_CHECK_MESSAGE(probe.empty() || KNOWN_PROBES.count(probe), name << ": unknown probe " << probe);

            BOOST_CHECK_MESSAGE(section != "rpc" || ParseRefusal(Field(rows[i], "refusal")).has_value(),
                                name << ": unknown refusal " << Field(rows[i], "refusal"));
        }
    }

    // Every RPC the inventory found handling key material is in the table.
    for (const std::string& name : INVENTORY_RPCS) {
        BOOST_CHECK_MESSAGE(rpcs.count(name), name << " reaches key material and is not in the table");
    }

    // Every IPC row has a driver, and every driver a row.
    const std::map<std::string, IpcDriver> drivers = IpcDrivers();
    std::set<std::string> ipcs;
    std::set<std::string> driven;

    for (const std::string section : {"ipc", "ipc_pool_draw", "ipc_not_rows"}) {
        const UniValue& rows = find_value(table, section);
        BOOST_REQUIRE_MESSAGE(rows.isArray(), "no \"" << section << "\" array");

        for (size_t i = 0; i < rows.size(); ++i) {
            const std::string name = IpcName(rows[i]);
            BOOST_CHECK_MESSAGE(ipcs.insert(name).second, name << " is in the table twice");

            if (section == "ipc_not_rows") {
                BOOST_CHECK_MESSAGE(!Field(rows[i], "reason").empty(), name << ": no reason");
                BOOST_CHECK_MESSAGE(!drivers.count(name), name << " is driven but listed as no row");
                continue;
            }

            driven.insert(name);
            BOOST_CHECK_MESSAGE(drivers.count(name), name << " has no driver in key_entry_points_tests");

            const UniValue kind = find_value(rows[i], "kind");
            BOOST_CHECK_MESSAGE(kind.isNull() || (kind.isStr() && ParseKind(kind.get_str())),
                                name << ": unknown kind");
            BOOST_CHECK_MESSAGE(section == "ipc_pool_draw" || !kind.isNull(), name << ": no kind");

            for (const std::string& need : Needs(rows[i])) {
                BOOST_CHECK_MESSAGE(KNOWN_NEEDS.count(need), name << ": unknown need " << need);
            }
        }
    }

    for (const auto& [name, driver] : drivers) {
        BOOST_CHECK_MESSAGE(driven.count(name), name << " has a driver but no row in the table");
    }

    for (const std::string& name : INVENTORY_IPC) {
        BOOST_CHECK_MESSAGE(ipcs.count(name), name << " reaches key material and is not in the table");
    }

    // A row the fixture cannot drive names the test that covers it.
    for (const std::string section : {"rpc", "rpc_pool_draw", "ipc", "ipc_pool_draw", "ipc_not_rows"}) {
        const UniValue& rows = find_value(table, section);

        for (size_t i = 0; i < rows.size(); ++i) {
            const std::string covered_by = Field(rows[i], "covered_by");
            BOOST_CHECK_MESSAGE(covered_by.empty() || test_names.count(covered_by),
                                "covered_by names " << covered_by << ", which is no test case in this binary");
        }
    }

    BOOST_CHECK(find_value(table, "sources").isObject());
}

BOOST_AUTO_TEST_CASE(the_key_functions_refuse_while_staking_only)
{
    KeyReachWallet fx(WalletOptions{});
    fx.UnlockFor(UnlockScope::StakingOnly);

    // GetSeedPhraseBlob() reads the blob through DecryptSecretWithMasterKey().
    const auto reach = [&fx] {
        CKey fresh;
        fresh.MakeNewKey(true);

        CKey key;
        CKeyingMaterial blob;

        LOCK(fx.wallet.cs_wallet);
        return std::vector<bool>{fx.wallet.GetKey(fx.own_id, key),
                                 fx.wallet.GetSeedPhraseBlob(blob),
                                 fx.wallet.AddKeyPubKey(fresh, fresh.GetPubKey())};
    };

    BOOST_CHECK(reach() == std::vector<bool>({false, false, false}));

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(KeyReachWallet::PASSPHRASE,
                                                                    CapabilityKind::RpcRequest);
    BOOST_REQUIRE(id.has_value());

    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::RpcRequest);
        BOOST_REQUIRE(scope.Active());

        BOOST_CHECK(reach() == std::vector<bool>({true, true, true}));
    }

    fx.wallet.EndElevation(*id);

    BOOST_CHECK(reach() == std::vector<bool>({false, false, false}));
}

BOOST_AUTO_TEST_CASE(every_key_reaching_rpc_refuses_while_staking_only)
{
    DriveRpcRows(find_value(Table(), "rpc"), std::nullopt);
}

BOOST_AUTO_TEST_CASE(rpc_pool_draws_make_no_key_while_staking_only)
{
    DriveRpcRows(find_value(Table(), "rpc_pool_draw"), Refusal::KeypoolRanOut);
}

BOOST_AUTO_TEST_CASE(every_key_reaching_ipc_method_refuses_while_staking_only)
{
    DriveIpcRows(find_value(Table(), "ipc"));
}

BOOST_AUTO_TEST_CASE(ipc_pool_draws_make_no_key_while_staking_only)
{
    DriveIpcRows(find_value(Table(), "ipc_pool_draw"));
}

BOOST_AUTO_TEST_SUITE_END()
