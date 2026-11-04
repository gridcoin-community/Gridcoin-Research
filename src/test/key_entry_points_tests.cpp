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
#include <iterator>
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
// refused, and under a capability of its kind it must not be, and must reach
// the key code unless the row says why the unit fixture cannot take it there.
// An RPC row is refused through the dispatcher, whose gate refuses an
// access_elevate row before its method runs, and its method is also called
// without the dispatcher, where it must refuse on its own.
//
// The lint test/lint/lint-staking-key-access.py keeps the table's count of the
// direct calls of the four functions in each file current, so a new direct call
// forces an update of the table, the prompt to add the entry point's row. Calls
// through the wrappers -- SignSignature() and ProduceSignature(),
// CreateTransaction(), AddKey(), GenerateNewKey() and TopUpKeyPool(),
// SignPSGTInput() -- are not counted; the inventory below is what catches a new
// entry point that reaches the keys only through one of them.
namespace {

using grc_test::MockWalletDbScope;
using grc_test::SwapMainWallet;

//! The RPCs found handling a private key or the seed phrase when every command
//! was inventoried. Each is a row of the table, or is listed there with the
//! reason it reaches no key, so none can be dropped from the table unnoticed.
const std::vector<std::string> INVENTORY_RPCS{
    "backupwallet", "burn", "claimhtlc", "createhtlc", "dumpprivkey", "dumpseedphrase", "dumpwallet", "encryptwallet",
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
    "Wallet.backupWallet", "Wallet.beginBackup", "Wallet.encryptWallet", "Wallet.lockWallet", "Wallet.elevateWallet",
    "Wallet.unlockWallet", "Wallet.changeWalletPassphrase", "Wallet.getKeyFromPool", "Wallet.sendCoins",
    "Wallet.signMessage", "Wallet.getNewReceiveAddress", "Wallet.getNewReceiveAddressWithLabel",
    "MRC.snapshot", "MRC.submit", "MRC.submitConfirmed",
    "VotingManager.submitPoll", "VotingManager.submitVote",
    "ResearcherContext.advertiseBeacon", "ResearcherContext.generateBeaconKeyForV3",
    "ResearcherContext.advertiseBeaconV3",
    "PSGTPoolContext.signPoolEntry", "PSGTPoolContext.signPSGT",
    "Node.executeRpcConsoleCommand", "Node.executeRpcConsoleSecretCommand",
};

//! The environments a row can ask for.
const std::set<std::string> KNOWN_NEEDS{
    "in_sync", "not_ibd", "v15", "accounts", "researcher", "scraper", "import_file",
    "premine", "no_seed_phrase", "pre_hd", "empty_pool", "own_wallet",
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
    bool hd{true};
    bool seed_phrase{true};
    bool empty_pool{false};
    bool premine{false};

    static WalletOptions For(const std::set<std::string>& needs)
    {
        WalletOptions options;
        options.hd = needs.count("pre_hd") == 0;
        options.seed_phrase = options.hd && needs.count("no_seed_phrase") == 0;
        options.empty_pool = needs.count("empty_pool") != 0;
        options.premine = needs.count("premine") != 0;
        return options;
    }

    bool operator<(const WalletOptions& other) const
    {
        return std::tie(hd, seed_phrase, empty_pool, premine)
               < std::tie(other.hd, other.seed_phrase, other.empty_pool, other.premine);
    }
};

//! The rows of one section, by the wallet they run on, in table order: rows
//! with the same WalletOptions share one, and a row that needs own_wallet gets
//! one to itself.
std::vector<std::pair<WalletOptions, std::vector<const UniValue*>>> ByWallet(const std::vector<const UniValue*>& rows)
{
    std::vector<std::pair<WalletOptions, std::vector<const UniValue*>>> groups;
    std::map<WalletOptions, size_t> shared;

    for (const UniValue* row : rows) {
        const std::set<std::string> needs = Needs(*row);
        const WalletOptions options = WalletOptions::For(needs);

        if (needs.count("own_wallet")) {
            groups.push_back({options, {row}});
            continue;
        }

        const auto [it, inserted] = shared.emplace(options, groups.size());
        if (inserted) groups.push_back({options, {}});
        groups[it->second].second.push_back(row);
    }

    return groups;
}

//! An encrypted, file-backed HD wallet with one loose key of its own, a key
//! pool and, unless asked otherwise, a seed phrase whose master is the HD
//! master, as makeseedphrase leaves a wallet. Asked for a wallet without an HD
//! seed, it has neither the seed nor the phrase, as a wallet made before HD.
//! Swapped into pwalletMain and left locked.
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

            // A wallet made before HD has neither an HD seed nor a seed phrase;
            // a seed phrase implies an HD seed (WalletOptions::For).
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
            } else if (options.hd) {
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
        BOOST_REQUIRE_EQUAL(wallet.IsHDEnabled(), options.hd);
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
    //! \p import_line is the dump file's one key line, for import_file.
    Environment(const std::set<std::string>& needs, const KeyReachWallet& fx, const std::string& import_line)
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
            file << "# A wallet dump with one key in it.\n" << import_line << "\n";
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

//! The values the table's arguments name. A fresh private key and seed phrase
//! each time, so a row that imports one adds a key whatever ran before it. The
//! phrase is made only for a row that asks for one: making it is slow.
class Corpus
{
public:
    Corpus(const KeyReachWallet& fx, bool with_phrase)
    {
        CKey fresh;
        fresh.MakeNewKey(true);

        // A test phrase, made as makeseedphrase makes one.
        SecureString phrase;
        if (with_phrase) {
            CKey phrase_master;
            phrase = GRC::Mnemonics::GenerateSeedPhrase(SecureString(), phrase_master);
        }

        const CTransaction previous = psgt_test::MakePrevTx(fx.own, 10 * COIN);
        const PartiallySignedTransaction spend = psgt_test::MakeSpendPSGT(previous, 9 * COIN);
        const std::vector<unsigned char> psgt = SerializePSGT(spend);

        CDataStream raw(SER_NETWORK, PROTOCOL_VERSION);
        raw << CTransaction(spend.tx);

        // The output the spend spends, as signrawtransaction takes it.
        UniValue prevout(UniValue::VOBJ);
        prevout.pushKV("txid", previous.GetHash().GetHex());
        prevout.pushKV("vout", 0);
        prevout.pushKV("scriptPubKey", HexStr(previous.vout[0].scriptPubKey));
        UniValue prevtxs(UniValue::VARR);
        prevtxs.push_back(prevout);

        // Longest first, so that $own does not match inside $own_pubkey.
        m_values = {
            {"$own_pubkey", HexStr(fx.own.GetPubKey())},
            {"$prevtxs", prevtxs.write()},
            {"$premine", EncodeDestination(CTxDestination(grc_test::PremineKey().GetPubKey().GetID()))},
            {"$phrase", std::string(phrase.begin(), phrase.end())},
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

//! Calls \p name's method as CRPCTable::execute() does, but without the
//! dispatcher's own checks, the arity check and the passphrase gate of an
//! access_elevate row, so that what the method itself does is seen.
Outcome CallMethod(const std::string& name, const std::vector<std::string>& args)
{
    Outcome out;

    const CRPCCommand* command = tableRPC[name];
    if (command == nullptr) {
        out.malformed = true;
        out.message = "no such command";
        return out;
    }

    try {
        out.result = command->actor(RPCConvertValues(name, args));
        out.returned = true;
    } catch (const UniValue& error) {
        out.code = find_value(error, "code").get_int();
        out.message = find_value(error, "message").getValStr();
    } catch (const std::exception& e) {
        // As the dispatcher reports it.
        out.code = RPC_MISC_ERROR;
        out.message = e.what();
    }

    return out;
}

//! How a row is refused.
enum class Refusal {
    UnlockNeeded,  //!< RPC_WALLET_UNLOCK_NEEDED
    KeypoolRanOut, //!< RPC_WALLET_KEYPOOL_RAN_OUT, for a pool draw that would make a key
};

std::optional<Refusal> ParseRefusal(const std::string& name)
{
    if (name.empty() || name == "unlock_needed") return Refusal::UnlockNeeded;
    return std::nullopt;
}

//! What a driver or probe came to: whether it was refused, whether it did what
//! it was asked, what it said, and where it stopped -- the status by name, or the
//! error text -- for the stop marks to compare with.
struct DriverOutcome
{
    bool refused{false};
    bool succeeded{false};
    std::string detail;
    std::string stop;
};

//! The name \p names gives \p status, or "status <n>" for one it does not name.
template <typename Status>
std::string StatusName(Status status, const std::vector<std::pair<Status, std::string>>& names)
{
    for (const auto& [value, name] : names) {
        if (value == status) return name;
    }

    return strprintf("status %d", static_cast<int>(status));
}

std::string BeaconStatusName(interfaces::BeaconStatus status)
{
    using interfaces::BeaconStatus;

    return StatusName(status, {{BeaconStatus::ACTIVE, "ACTIVE"},
                               {BeaconStatus::ERROR_INSUFFICIENT_FUNDS, "ERROR_INSUFFICIENT_FUNDS"},
                               {BeaconStatus::ERROR_MISSING_KEY, "ERROR_MISSING_KEY"},
                               {BeaconStatus::ERROR_TX_FAILED, "ERROR_TX_FAILED"},
                               {BeaconStatus::ERROR_INVALID_PROOF_XML, "ERROR_INVALID_PROOF_XML"},
                               {BeaconStatus::ERROR_WALLET_LOCKED, "ERROR_WALLET_LOCKED"},
                               {BeaconStatus::NO_CPID, "NO_CPID"},
                               {BeaconStatus::PENDING, "PENDING"}});
}

//! A probe: the narrowest function the row reaches, run on the calling thread
//! under whatever capability scope that thread holds.
DriverOutcome Probe(const std::string& probe, KeyReachWallet& fx)
{
    if (probe == "IsScraperAuthorizedToBroadcastManifests") {
        // As sendscraperfilemanifest asks: no staking access.
        CTxDestination address;
        CKey key;
        const bool authorized = IsScraperAuthorizedToBroadcastManifests(address, key, nullptr);
        const std::string said = authorized ? "authorised" : "not authorised";
        return {!authorized, authorized, said, said};
    }

    if (probe == "VoteBuilder::BuildContractTx") {
        const GRC::Poll poll(GRC::PollType::SURVEY, GRC::PollWeightType::BALANCE,
                             GRC::PollResponseType::YES_NO_ABSTAIN, 7, "Key entry points", "https://gridcoin.us",
                             "Question?", GRC::Poll::ChoiceList(), GetAdjustedTime(), Fraction());

        // Under the IPC slot's capability, as submitVote builds its vote.
        CapabilityScope scope(fx.wallet, fx.wallet.IpcCapability({}), CapabilityKind::Contract);

        try {
            LOCK(cs_main);
            GRC::VoteBuilder::ForPoll(poll, uint256::ONE).AddResponse(uint8_t {0}).BuildContractTx(&fx.wallet, 2);
        } catch (const std::exception& e) {
            return {e.what() == VotingStakingOnlyRefusal(), false, e.what(), e.what()};
        }

        return {false, true, "built", "built"};
    }

    BOOST_ERROR("unknown probe " << probe);
    return {false, false, "unknown probe", "unknown probe"};
}

//! The manifests a scraper row published, for its messages; nothing for the
//! other rows.
std::string Published(bool scraper, int published)
{
    return scraper ? strprintf(", manifests published %d", published) : std::string();
}

//! Whether \p outcome is the refusal \p refusal names.
bool IsRefusal(Refusal refusal, const Outcome& outcome)
{
    switch (refusal) {
    case Refusal::UnlockNeeded:
        return outcome.code == RPC_WALLET_UNLOCK_NEEDED;
    case Refusal::KeypoolRanOut:
        return outcome.code == RPC_WALLET_KEYPOOL_RAN_OUT;
    }

    return false;
}

//! Whether an RPC row's run under the capability shows it reached the key code:
//! it returned (with a 'complete' field, if its result has one, true), or it
//! failed after the key with the error the row names.
bool ReachedTheKey(const UniValue& row, const Outcome& outcome)
{
    const UniValue error_after_key = find_value(row, "error_after_key");
    if (error_after_key.isNum()) return outcome.code == error_after_key.get_int();

    if (!outcome.returned) return false;
    if (!outcome.result.isObject()) return true;

    const UniValue complete = find_value(outcome.result, "complete");
    return complete.isNull() || complete.isTrue();
}

//! Whether an RPC row the table marks as stopping before the key stopped where
//! its expected_stop says: with that RPC error code, or "returned".
bool StopsAsMarked(const UniValue& row, const Outcome& outcome)
{
    const UniValue expected = find_value(row, "expected_stop");

    if (expected.isNum()) return outcome.code == expected.get_int();
    return expected.isStr() && expected.get_str() == "returned" && outcome.returned;
}

//! Drive the rows of one RPC section. Each is refused without a capability,
//! through the dispatcher and by its method called without the dispatcher,
//! with no change to the wallet's keys, and not refused under an RpcRequest
//! capability, where it must also reach the key code, unless the row says in
//! stops_before_key why the unit fixture cannot take it there; a row that
//! reaches AddKeyPubKey must then have added a key. Rows that want the same
//! wallet share one, which the rows run on in table order: a refused row
//! changes no key material, as each checks, and a row that ran under the
//! capability leaves the wallet unlocked for staking only, as the next one
//! checks.
void DriveRpcRows(const UniValue& rows, std::optional<Refusal> section_refusal)
{
    BOOST_REQUIRE(rows.isArray());

    const std::set<std::string> test_names = AllTestNames();

    std::vector<const UniValue*> driven;

    for (size_t i = 0; i < rows.size(); ++i) {
        const UniValue& row = rows[i];
        const std::string covered_by = Field(row, "covered_by");

        if (covered_by.empty()) {
            driven.push_back(&row);
            continue;
        }

        BOOST_CHECK_MESSAGE(test_names.count(covered_by), Field(row, "name") << ": covered_by names "
                            << covered_by << ", which is no test case in this binary");
        BOOST_TEST_MESSAGE(Field(row, "name") << ": not driven here; covered by " << covered_by);
    }

    for (const auto& [options, wallet_rows] : ByWallet(driven)) {
        KeyReachWallet fx(options);

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
            const std::string stops_before_key = Field(*row, "stops_before_key");
            const bool adds_a_key = Field(*row, "reaches") == "AddKeyPubKey";
            const bool scraper = needs.count("scraper") > 0;

            const std::vector<std::string> raw_args = Strings(find_value(*row, "args"));
            const bool with_phrase = std::any_of(raw_args.begin(), raw_args.end(), [](const std::string& arg) {
                return arg.find("$phrase") != std::string::npos;
            });

            const Corpus corpus(fx, with_phrase);
            const std::vector<std::string> args = corpus.Expand(raw_args);

            const std::optional<Refusal> refusal = section_refusal ? section_refusal
                                                                   : ParseRefusal(Field(*row, "refusal"));
            BOOST_REQUIRE_MESSAGE(refusal.has_value(), name << ": unknown refusal " << Field(*row, "refusal"));

            Environment environment(needs, fx, corpus.Expand({"$wif 2020-01-01T00:00:00Z label="}).front());

            if (needs.count("empty_pool")) fx.DrainPool();

            BOOST_REQUIRE_MESSAGE(fx.wallet.IsUnlockedForStakingOnly() && !fx.wallet.CanUseKeys(),
                                  name << ": the wallet is not unlocked for staking only");

            int published = 0;
            const boost::signals2::scoped_connection manifests(uiInterface.NotifyScraperEvent_connect(
                [&published](const scrapereventtypes& type, ChangeType, const std::string&) {
                    if (type == scrapereventtypes::Manifest) ++published;
                }));

            // Refused without a capability, before any key material changed.
            const size_t keys_before_refusal = fx.KeyCount();
            const Outcome refused = Call(name, args);
            const int published_while_refused = published;

            BOOST_CHECK_MESSAGE(IsRefusal(*refusal, refused),
                                name << ": not refused while unlocked for staking only: " << refused.Describe()
                                << Published(scraper, published));

            // The method refuses on its own too: the dispatcher's gate is not
            // the only thing between a staking-only wallet and the keys.
            const Outcome refused_by_method = CallMethod(name, args);
            const int published_by_refused_method = published - published_while_refused;

            BOOST_CHECK_MESSAGE(IsRefusal(*refusal, refused_by_method),
                                name << ": the method, called without the dispatcher, is not refused while unlocked "
                                "for staking only: " << refused_by_method.Describe()
                                << Published(scraper, published_by_refused_method));
            BOOST_CHECK_MESSAGE(published == 0, name << ": a manifest was published while refused");
            BOOST_CHECK_MESSAGE(fx.KeyCount() == keys_before_refusal,
                                name << ": a refused call changed the wallet's keys");

            if (!probe.empty()) {
                const DriverOutcome probed = Probe(probe, fx);
                BOOST_CHECK_MESSAGE(probed.refused, name << ": " << probe << " not refused: " << probed.detail);
            }

            // Not refused under the capability, and on to the key code.
            published = 0;
            {
                CapabilityScope scope(fx.wallet, id, CapabilityKind::RpcRequest);
                BOOST_REQUIRE(scope.Active());

                const size_t keys_before = fx.KeyCount();
                const Outcome allowed = Call(name, args);
                const size_t keys_after = fx.KeyCount();

                BOOST_CHECK_MESSAGE(!allowed.malformed, name << ": the corpus does not fit the command: "
                                    << allowed.Describe());
                BOOST_CHECK_MESSAGE(allowed.code != RPC_WALLET_UNLOCK_NEEDED && !IsRefusal(*refusal, allowed),
                                    name << ": refused under an RpcRequest capability: " << allowed.Describe());

                if (scraper) {
                    BOOST_CHECK_MESSAGE(published > 0, name << ": nothing published under an RpcRequest capability");
                }

                if (!stops_before_key.empty()) {
                    BOOST_CHECK_MESSAGE(StopsAsMarked(*row, allowed), name << ": no longer stops as its mark says ("
                                        << find_value(*row, "expected_stop").write() << "): " << allowed.Describe()
                                        << "; update stops_before_key and expected_stop");
                } else {
                    BOOST_CHECK_MESSAGE(ReachedTheKey(*row, allowed), name << ": did not reach the key code under "
                                        "an RpcRequest capability, and the table gives no reason: "
                                        << allowed.Describe());

                    if (adds_a_key) {
                        BOOST_CHECK_MESSAGE(keys_after > keys_before, name << ": added no key under an RpcRequest "
                                            "capability (" << keys_before << " keys before, " << keys_after
                                            << " after)");
                    }
                }

                if (!probe.empty()) {
                    const DriverOutcome probed = Probe(probe, fx);
                    BOOST_CHECK_MESSAGE(!probed.refused, name << ": " << probe
                                        << " refused under an RpcRequest capability: " << probed.detail);
                    BOOST_CHECK_MESSAGE(!stops_before_key.empty() || probed.succeeded,
                                        name << ": " << probe << " did not reach the key code: " << probed.detail);
                }

                BOOST_TEST_MESSAGE(name << ": refused (" << refused.Describe()
                                   << Published(scraper, published_while_refused) << "; the method alone: "
                                   << refused_by_method.Describe() << "); under the capability: "
                                   << allowed.Describe() << Published(scraper, published)
                                   << (stops_before_key.empty() ? std::string() : "; stops before the key: " + stops_before_key)
                                   << (adds_a_key ? strprintf("; keys %u -> %u", keys_before, keys_after) : std::string()));
            }
        }

        fx.wallet.EndElevation(*id);
        BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    }
}

using IpcDriver = std::function<DriverOutcome(KeyReachWallet&)>;

//! The staking-only refusal signPSGT() and signPoolEntry() report.
const std::string PSGT_SIGN_REFUSAL =
    "The wallet is locked or unlocked for staking only. Enter the wallet passphrase to sign.";

//! One driver per IPC row, by interface.method. Each calls the method as an
//! IPC client would, through the in-process interface on the fixture's wallet,
//! and says whether it was refused with the method's staking-only refusal and
//! whether it did what it was asked.
std::map<std::string, IpcDriver> IpcDrivers()
{
    std::map<std::string, IpcDriver> drivers;

    drivers["Wallet.signMessage"] = [](KeyReachWallet& fx) {
        std::string signature;
        const interfaces::MessageSignStatus status =
            interfaces::MakeWallet(&fx.wallet)->signMessage(EncodeDestination(CTxDestination(fx.own_id)), "x", signature);
        const std::string name = StatusName(status, {{interfaces::MessageSignStatus::OK, "OK"},
                                                     {interfaces::MessageSignStatus::KeyNotAvailable, "KeyNotAvailable"},
                                                     {interfaces::MessageSignStatus::SigningFailed, "SigningFailed"}});
        return DriverOutcome{status == interfaces::MessageSignStatus::KeyNotAvailable,
                             status == interfaces::MessageSignStatus::OK, name, name};
    };

    drivers["Wallet.sendCoins"] = [](KeyReachWallet& fx) {
        interfaces::WalletSendRecipient recipient;
        recipient.address = EncodeDestination(CTxDestination(fx.own_id));
        recipient.amount = COIN;

        const interfaces::SendCoinsResult result = interfaces::MakeWallet(&fx.wallet)->sendCoins({recipient}, std::nullopt, 0);
        using interfaces::SendCoinsStatus;
        const std::string name = StatusName(result.status, {{SendCoinsStatus::OK, "OK"},
                                                            {SendCoinsStatus::AmountExceedsBalance, "AmountExceedsBalance"},
                                                            {SendCoinsStatus::WalletUnlockedForStakingOnly,
                                                             "WalletUnlockedForStakingOnly"}});
        return DriverOutcome{result.status == SendCoinsStatus::WalletUnlockedForStakingOnly,
                             result.status == SendCoinsStatus::OK, name, name};
    };

    drivers["PSGTPoolContext.signPSGT"] = [](KeyReachWallet& fx) {
        const CTransaction previous = psgt_test::MakePrevTx(fx.own, 10 * COIN);
        const interfaces::PSGTSignResult result = interfaces::MakePSGTPoolContext(&fx.wallet)->signPSGT(
            SerializePSGT(psgt_test::MakeSpendPSGT(previous, 9 * COIN)));
        return DriverOutcome{!result.ok && result.error == PSGT_SIGN_REFUSAL,
                             result.ok && result.signed_now == 1,
                             strprintf("ok %d, signed %d: %s", result.ok, result.signed_now, result.error),
                             result.ok ? strprintf("signed %d", result.signed_now) : result.error};
    };

    drivers["PSGTPoolContext.signPoolEntry"] = [](KeyReachWallet& fx) {
        std::string error;
        std::string txid;
        const interfaces::PSGTSignStatus status =
            interfaces::MakePSGTPoolContext(&fx.wallet)->signPoolEntry(ABSENT_POOL_IMAGE, error, txid);
        using interfaces::PSGTSignStatus;
        const std::string name = StatusName(status, {{PSGTSignStatus::SIGNED_AND_RELAYED, "SIGNED_AND_RELAYED"},
                                                     {PSGTSignStatus::COMPLETED_AND_BROADCAST, "COMPLETED_AND_BROADCAST"},
                                                     {PSGTSignStatus::NO_NEW_SIGNATURES, "NO_NEW_SIGNATURES"},
                                                     {PSGTSignStatus::ALREADY_KNOWN, "ALREADY_KNOWN"},
                                                     {PSGTSignStatus::NOT_FOUND, "NOT_FOUND"},
                                                     {PSGTSignStatus::FAILED, "FAILED"}});
        return DriverOutcome{status == PSGTSignStatus::FAILED && error == PSGT_SIGN_REFUSAL,
                             status == PSGTSignStatus::SIGNED_AND_RELAYED
                                 || status == PSGTSignStatus::COMPLETED_AND_BROADCAST,
                             name + ": " + error, name};
    };

    drivers["MRC.submit"] = [](KeyReachWallet& fx) {
        const interfaces::MRCSubmitResult result = interfaces::MakeMRC(&fx.wallet)->submit(0, false);
        const std::string said = result.success ? "submitted" : result.error;
        return DriverOutcome{result.error == MRC_STAKING_ONLY_REFUSAL, result.success, said, said};
    };

    drivers["MRC.submitConfirmed"] = [](KeyReachWallet& fx) {
        const interfaces::MRCSubmitResult result = interfaces::MakeMRC(&fx.wallet)->submitConfirmed(0, false, 0, 0);
        const std::string said = result.success ? "submitted" : result.error;
        return DriverOutcome{result.error == MRC_STAKING_ONLY_REFUSAL, result.success, said, said};
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
        using interfaces::VotingSubmitStatus;
        const std::string name = StatusName(result.status, {{VotingSubmitStatus::OK, "OK"},
                                                            {VotingSubmitStatus::POLL_NOT_FOUND, "POLL_NOT_FOUND"},
                                                            {VotingSubmitStatus::POLL_LOAD_FAILED, "POLL_LOAD_FAILED"},
                                                            {VotingSubmitStatus::FAILED, "FAILED"}});
        const std::string said = result.error.empty() ? name : name + ": " + result.error;
        return DriverOutcome{result.status == VotingSubmitStatus::FAILED && result.error == VotingStakingOnlyRefusal(),
                             result.status == VotingSubmitStatus::OK, said, said};
    };

    drivers["VotingManager.submitVote"] = [](KeyReachWallet& fx) {
        // The method itself finds no such poll; the probe builds the vote.
        const interfaces::VotingSubmitResult result =
            interfaces::MakeVotingManager()->submitVote(HASH_HEX, {uint8_t {0}});
        BOOST_CHECK(result.status == interfaces::VotingSubmitStatus::POLL_NOT_FOUND);

        DriverOutcome probed = Probe("VoteBuilder::BuildContractTx", fx);
        probed.detail = "probe VoteBuilder::BuildContractTx: " + probed.detail;
        return probed;
    };

    drivers["ResearcherContext.advertiseBeacon"] = [](KeyReachWallet& fx) {
        const interfaces::BeaconAdvertiseResult result = interfaces::MakeResearcherContext(&fx.wallet)->advertiseBeacon();
        const std::string name = BeaconStatusName(result.status);
        return DriverOutcome{result.status == interfaces::BeaconStatus::ERROR_WALLET_LOCKED,
                             result.status == interfaces::BeaconStatus::ACTIVE, name, name};
    };

    drivers["ResearcherContext.generateBeaconKeyForV3"] = [](KeyReachWallet& fx) {
        const std::string public_key = interfaces::MakeResearcherContext(&fx.wallet)->generateBeaconKeyForV3();
        const std::string said = public_key.empty() ? "no key" : "key " + public_key;
        return DriverOutcome{public_key.empty(), !public_key.empty(), said, public_key.empty() ? "no key" : "key"};
    };

    drivers["ResearcherContext.advertiseBeaconV3"] = [](KeyReachWallet& fx) {
        const std::string proof = "<master_url>https://gridcoin.us/</master_url>"
                                  "<msg>1 " + HexStr(fx.own.GetPubKey()) + "</msg>"
                                  "<signature>AAAA</signature>";
        const interfaces::BeaconAdvertiseResult result =
            interfaces::MakeResearcherContext(&fx.wallet)->advertiseBeaconV3(proof);
        const std::string name = BeaconStatusName(result.status);
        return DriverOutcome{result.status == interfaces::BeaconStatus::ERROR_WALLET_LOCKED,
                             result.status == interfaces::BeaconStatus::ACTIVE, name, name};
    };

    drivers["Node.executeRpcConsoleCommand"] = [](KeyReachWallet& fx) {
        const interfaces::RpcConsoleResult result = interfaces::MakeNode()->executeRpcConsoleCommand(
            "signmessage", {EncodeDestination(CTxDestination(fx.own_id)), "x"});
        return DriverOutcome{result.wallet_unlock_needed, result.ok,
                             strprintf("ok %d: %s", result.ok, result.output), result.ok ? "ok" : result.output};
    };

    drivers["Wallet.getNewReceiveAddress"] = [](KeyReachWallet& fx) {
        std::string address;
        const bool drawn = interfaces::MakeWallet(&fx.wallet)->getNewReceiveAddress(address);
        return DriverOutcome{!drawn, drawn, drawn ? "drew " + address : "no address", drawn ? "drew" : "no address"};
    };

    drivers["Wallet.getNewReceiveAddressWithLabel"] = [](KeyReachWallet& fx) {
        std::string address;
        const bool drawn = interfaces::MakeWallet(&fx.wallet)->getNewReceiveAddressWithLabel("key entry points", address);
        return DriverOutcome{!drawn, drawn, drawn ? "drew " + address : "no address", drawn ? "drew" : "no address"};
    };

    drivers["Wallet.getKeyFromPool"] = [](KeyReachWallet& fx) {
        CPubKey key;
        const bool drawn = interfaces::MakeWallet(&fx.wallet)->getKeyFromPool("", key);
        const std::string said = drawn ? "drew a key" : "no key";
        return DriverOutcome{!drawn, drawn, said, said};
    };

    return drivers;
}

//! Drive the rows of one IPC section: refused without a capability and under a
//! capability of another kind, and not refused under one of the row's kind,
//! each begun in the IPC slot as an IPC client begins one. Under the row's kind
//! the method must also do what it was asked, and a row that reaches
//! AddKeyPubKey must have added a key, unless the row says in
//! stops_before_key why the unit fixture cannot take it there. A row without a
//! kind is only checked for its refusal. Rows that want the same wallet share
//! one, as the RPC rows do.
void DriveIpcRows(const UniValue& rows)
{
    BOOST_REQUIRE(rows.isArray());

    const std::map<std::string, IpcDriver> drivers = IpcDrivers();

    std::vector<const UniValue*> all;
    for (size_t i = 0; i < rows.size(); ++i) all.push_back(&rows[i]);

    for (const auto& [options, wallet_rows] : ByWallet(all)) {
        KeyReachWallet fx(options);
        fx.UnlockFor(UnlockScope::StakingOnly);

        const std::unique_ptr<interfaces::Wallet> client = interfaces::MakeWallet(&fx.wallet);

        for (const UniValue* row : wallet_rows) {
            const std::string name = IpcName(*row);
            const std::set<std::string> needs = Needs(*row);
            const std::string stops_before_key = Field(*row, "stops_before_key");
            const bool adds_a_key = Field(*row, "reaches") == "AddKeyPubKey";

            const auto driver = drivers.find(name);
            BOOST_REQUIRE_MESSAGE(driver != drivers.end(), name << ": no driver");

            Environment environment(needs, fx, std::string());

            if (needs.count("empty_pool")) fx.DrainPool();

            BOOST_REQUIRE_MESSAGE(fx.wallet.IsUnlockedForStakingOnly() && !fx.wallet.CanUseKeys(),
                                  name << ": the wallet is not unlocked for staking only");

            const size_t keys_before_refusal = fx.KeyCount();
            const DriverOutcome refused = driver->second(fx);

            BOOST_CHECK_MESSAGE(refused.refused, name << ": not refused while unlocked for staking only: "
                                << refused.detail);
            BOOST_CHECK_MESSAGE(fx.KeyCount() == keys_before_refusal,
                                name << ": a refused call changed the wallet's keys");

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
            const DriverOutcome other_kind = driver->second(fx);
            client->endElevation();

            BOOST_CHECK_MESSAGE(other_kind.refused, name << ": not refused under a Backup capability: "
                                << other_kind.detail);

            BOOST_REQUIRE(client->beginElevation(KeyReachWallet::PASSPHRASE, *kind));
            const size_t keys_before = fx.KeyCount();
            const DriverOutcome allowed = driver->second(fx);
            const size_t keys_after = fx.KeyCount();
            client->endElevation();

            BOOST_CHECK_MESSAGE(!allowed.refused, name << ": refused under a " << kind_value.get_str()
                                << " capability: " << allowed.detail);

            if (!stops_before_key.empty()) {
                const std::string expected_stop = Field(*row, "expected_stop");
                BOOST_CHECK_MESSAGE(allowed.stop == expected_stop, name << ": no longer stops as its mark says ("
                                    << expected_stop << "): " << allowed.stop
                                    << "; update stops_before_key and expected_stop");
            } else {
                BOOST_CHECK_MESSAGE(allowed.succeeded, name << ": did not reach the key code under a "
                                    << kind_value.get_str() << " capability, and the table gives no reason: "
                                    << allowed.detail);

                if (adds_a_key) {
                    BOOST_CHECK_MESSAGE(keys_after > keys_before, name << ": added no key under a "
                                        << kind_value.get_str() << " capability (" << keys_before
                                        << " keys before, " << keys_after << " after)");
                }
            }

            BOOST_TEST_MESSAGE(name << ": refused (" << refused.detail << "); under the capability: "
                               << allowed.detail
                               << (stops_before_key.empty() ? std::string() : "; stops before the key: " + stops_before_key)
                               << (adds_a_key ? strprintf("; keys %u -> %u", keys_before, keys_after) : std::string()));
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

            const UniValue stops = find_value(rows[i], "stops_before_key");
            const UniValue after = find_value(rows[i], "error_after_key");
            const UniValue expected_stop = find_value(rows[i], "expected_stop");
            BOOST_CHECK_MESSAGE(stops.isNull() || (stops.isStr() && !stops.get_str().empty()),
                                name << ": stops_before_key is not a reason");
            BOOST_CHECK_MESSAGE(stops.isNull() == expected_stop.isNull(),
                                name << ": expected_stop goes with stops_before_key, and only with it");
            BOOST_CHECK_MESSAGE(expected_stop.isNull() || expected_stop.isNum()
                                    || (expected_stop.isStr() && expected_stop.get_str() == "returned"),
                                name << ": expected_stop is neither an RPC error code nor \"returned\"");
            BOOST_CHECK_MESSAGE(after.isNull() || after.isNum(), name << ": error_after_key is not an error code");
            BOOST_CHECK_MESSAGE(stops.isNull() || after.isNull(),
                                name << ": a row that stops before the key cannot fail after it");
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

            const UniValue stops = find_value(rows[i], "stops_before_key");
            const UniValue expected_stop = find_value(rows[i], "expected_stop");
            BOOST_CHECK_MESSAGE(stops.isNull() || (stops.isStr() && !stops.get_str().empty()),
                                name << ": stops_before_key is not a reason");
            BOOST_CHECK_MESSAGE(stops.isNull() == expected_stop.isNull(),
                                name << ": expected_stop goes with stops_before_key, and only with it");
            BOOST_CHECK_MESSAGE(expected_stop.isNull() || (expected_stop.isStr() && !expected_stop.get_str().empty()),
                                name << ": expected_stop is not a status");
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

//! What a call that should fail threw: its RPC error code and message, or
//! nullopt and the exception's text when it threw something else.
template <typename Fn>
std::pair<std::optional<int>, std::string> RpcError(Fn&& fn)
{
    try {
        fn();
    } catch (const UniValue& error) {
        return {find_value(error, "code").get_int(), find_value(error, "message").getValStr()};
    } catch (const std::exception& e) {
        return {std::nullopt, e.what()};
    }

    return {std::nullopt, "returned"};
}

//! dumpwallet writes every key the wallet holds or no dump at all: a key it
//! cannot read fails the dump, and the file it had begun is removed.
BOOST_AUTO_TEST_CASE(dumpwallet_writes_no_dump_with_a_key_missing)
{
    const fs::path path = GetDataDir() / DUMP_FILE;
    fs::remove(path);

    // Unlocked for staking only, and past the RPC's own unlock check, which
    // refuses before the writer runs: the writer refuses at the first key it
    // cannot read.
    {
        KeyReachWallet fx(WalletOptions{});
        fx.UnlockFor(UnlockScope::StakingOnly);

        // A file already there is replaced by a dump or by nothing.
        fsbridge::ofstream(path) << "an earlier file\n";

        const auto [code, message] = RpcError([&] { WriteWalletDump(fx.wallet, DUMP_FILE); });

        BOOST_CHECK(code == RPC_WALLET_UNLOCK_NEEDED);
        BOOST_CHECK_MESSAGE(message.rfind("dumpwallet: cannot read the key for ", 0) == 0, message);
        BOOST_CHECK(!fs::exists(path));
    }

    // Fully unlocked, with one key that does not decrypt: the RPC itself fails,
    // names the key, and leaves no file.
    {
        KeyReachWallet fx(WalletOptions{});
        fx.UnlockFor(UnlockScope::Full);

        // Two blocks of ciphertext decrypt to at most 31 bytes, never a key.
        CKey broken;
        broken.MakeNewKey(true);
        const CKeyID broken_id = broken.GetPubKey().GetID();
        {
            LOCK(fx.wallet.cs_wallet);
            BOOST_REQUIRE(fx.wallet.AddCryptedKey(broken.GetPubKey(), std::vector<unsigned char>(32, 0x5a)));
        }

        CKey read_back;
        BOOST_REQUIRE(fx.wallet.HaveKey(broken_id));
        BOOST_REQUIRE(!fx.wallet.GetKey(broken_id, read_back));

        const Outcome outcome = Call("dumpwallet", {DUMP_FILE});

        BOOST_CHECK_MESSAGE(outcome.code == RPC_WALLET_ERROR, outcome.Describe());
        BOOST_CHECK_EQUAL(outcome.message, "dumpwallet: cannot read the key for "
                                           + EncodeDestination(CTxDestination(broken_id)));
        BOOST_CHECK(!fs::exists(path));
    }

    // Fully unlocked with every key readable: the dump holds each of them, the
    // HD master's extended key and the end marker.
    {
        KeyReachWallet fx(WalletOptions{});
        fx.UnlockFor(UnlockScope::Full);

        const Outcome outcome = Call("dumpwallet", {DUMP_FILE});
        BOOST_REQUIRE_MESSAGE(outcome.returned, outcome.Describe());

        std::string dump;
        {
            fsbridge::ifstream file(path);
            dump.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
        fs::remove(path);

        std::set<CKeyID> ids;
        fx.wallet.GetKeys(ids);
        BOOST_REQUIRE(ids.size() > 1);

        for (const CKeyID& id : ids) {
            CKey key;
            BOOST_REQUIRE(fx.wallet.GetKey(id, key));
            BOOST_CHECK_MESSAGE(dump.find(EncodeSecret(key) + " ") != std::string::npos,
                                "the dump leaves out " << EncodeDestination(CTxDestination(id)));
        }

        BOOST_CHECK(dump.find("# extended private masterkey: ") != std::string::npos);
        BOOST_CHECK(dump.size() > 14 && dump.compare(dump.size() - 14, 14, "# End of dump\n") == 0);
    }

    // Metadata for a key the wallet does not hold, as a failed import can leave,
    // is no key to dump: the dump succeeds and leaves it out.
    {
        KeyReachWallet fx(WalletOptions{});
        fx.UnlockFor(UnlockScope::Full);

        CKey orphan;
        orphan.MakeNewKey(true);
        const CKeyID orphan_id = orphan.GetPubKey().GetID();
        {
            LOCK(fx.wallet.cs_wallet);
            fx.wallet.mapKeyMetadata[orphan_id] = CKeyMetadata(GetTime());
        }
        BOOST_REQUIRE(!fx.wallet.HaveKey(orphan_id));

        const Outcome outcome = Call("dumpwallet", {DUMP_FILE});
        BOOST_CHECK_MESSAGE(outcome.returned, outcome.Describe());

        std::string dump;
        {
            fsbridge::ifstream file(path);
            dump.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
        fs::remove(path);

        BOOST_CHECK(dump.find("addr=" + EncodeDestination(CTxDestination(orphan_id))) == std::string::npos);
        BOOST_CHECK(dump.find("# End of dump\n") != std::string::npos);
    }

    // A dump that cannot be written is no success, and what the path names is
    // removed only when it is a regular file. /dev/full takes the dump and fails
    // it at the flush; where there is no /dev/full this part is skipped.
    if (fs::exists("/dev/full")) {
        KeyReachWallet fx(WalletOptions{});
        fx.UnlockFor(UnlockScope::Full);

        const Outcome outcome = Call("dumpwallet", {"/dev/full"});

        BOOST_CHECK_MESSAGE(outcome.code == RPC_WALLET_ERROR, outcome.Describe());
        BOOST_CHECK_EQUAL(outcome.message, "dumpwallet: cannot write the dump file");
        BOOST_CHECK(fs::exists("/dev/full"));
    } else {
        BOOST_TEST_MESSAGE("no /dev/full: the write failure is not checked here");
    }

    // A symbolic link at the path is not removed when the dump fails. Where links
    // cannot be made this part is skipped.
    {
        const fs::path target = GetDataDir() / "key_entry_points_dump_target.txt";
        fs::remove(target);
        fsbridge::ofstream(target) << "the link's target\n";

        bool linked{false};
        try {
            fs::create_symlink(target, path);
            linked = true;
        } catch (const std::exception& e) {
            BOOST_TEST_MESSAGE("no symbolic link: " << e.what());
        }

        if (linked) {
            KeyReachWallet fx(WalletOptions{});
            fx.UnlockFor(UnlockScope::StakingOnly);

            const auto [code, message] = RpcError([&] { WriteWalletDump(fx.wallet, DUMP_FILE); });

            BOOST_CHECK(code == RPC_WALLET_UNLOCK_NEEDED);
            BOOST_CHECK(fs::is_symlink(fs::symlink_status(path)));
        }

        fs::remove(path);
        fs::remove(target);
    }
}

BOOST_AUTO_TEST_SUITE_END()
