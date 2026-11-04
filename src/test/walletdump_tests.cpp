// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "clientversion.h"
#include "fs.h"
#include "init.h"
#include "key.h"
#include "key_io.h"
#include "logging.h"
#include "rpc/client.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "support/allocators/secure.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "test/mock_wallet_db.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "tinyformat.h"
#include "util/time.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "wallet/walletdump.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// The dump dumpwallet writes is made by WalletDumpGenerator, which also feeds
// the file transfer's dump stream, and importwallet parses through
// ImportWalletFromStream, which the transfer's upload imports with. The dump
// header names the best block, so the suite runs on a real regtest chain.
namespace {

using grc_test::EncryptedWallet;
using grc_test::MockWalletDbScope;
using grc_test::SwapMainWallet;

//! The case's own directory under the data directory, removed when the case
//! ends.
struct DumpCase
{
    const fs::path dir = GetDataDir() / "walletdump_tests";

    DumpCase() { fs::create_directories(dir); }

    ~DumpCase()
    {
        boost::system::error_code ec;
        fs::remove_all(dir, ec);
    }
};

//! An unencrypted, file-backed HD wallet with every kind of line a dump has: the
//! HD master, a key pool, a labelled key whose label needs escaping, a change
//! key, and metadata for a key the wallet does not hold, which the dump leaves
//! out. Swapped into pwalletMain, so the RPCs reach it. File-backed on the
//! mocked database, so the key pool is written as on a node; the scope isolates
//! its records from every other case's.
struct DumpWallet
{
    static inline const std::string WALLET_FILE{"wallet_walletdump_tests.dat"};
    static inline const std::string LABEL{"a label, with % and spaces"};

    grc_test::ForcedArgGuard m_pool_size{"keypool", "3"};
    MockWalletDbScope m_db{WALLET_FILE};
    CWallet wallet{WALLET_FILE};
    SwapMainWallet m_swap{&wallet};

    CKeyID labelled_id;
    CKeyID change_id;
    CKeyID orphan_id;
    std::set<CKeyID> pool;

    //! Every key it makes is born now, the labelled one included: AddKey()
    //! gives a key no birth time of its own, so the case sets it.
    DumpWallet()
    {
        BOOST_REQUIRE(m_db.isolated());

        {
            LOCK(wallet.cs_wallet);

            BOOST_REQUIRE(wallet.SetHDMasterKey(wallet.GenerateNewHDMasterKey()));
            BOOST_REQUIRE(wallet.TopUpKeyPool());

            CKey labelled;
            labelled.MakeNewKey(true);
            labelled_id = labelled.GetPubKey().GetID();
            BOOST_REQUIRE(wallet.AddKey(labelled));
            wallet.mapKeyMetadata[labelled_id].nCreateTime = GetTime();
            BOOST_REQUIRE(wallet.SetAddressBookName(labelled_id, LABEL));

            // Made fresh, and neither in the pool nor in the address book.
            const CPubKey change = wallet.GenerateNewKey();
            BOOST_REQUIRE(change.IsValid());
            change_id = change.GetID();

            CKey orphan;
            orphan.MakeNewKey(true);
            orphan_id = orphan.GetPubKey().GetID();
            wallet.mapKeyMetadata[orphan_id] = CKeyMetadata(GetTime());
        }

        // Outside cs_wallet: it takes cs_main and then cs_wallet itself.
        wallet.GetAllReserveKeys(pool);
        BOOST_REQUIRE(!pool.empty());
    }

    DumpWallet(const DumpWallet&) = delete;
    DumpWallet& operator=(const DumpWallet&) = delete;
};

std::string ReadAll(const fs::path& path)
{
    fsbridge::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::set<CKeyID> Keys(const CWallet& wallet)
{
    std::set<CKeyID> ids;
    wallet.GetKeys(ids);
    return ids;
}

//! Everything a generator made now writes, in Next() calls of max_bytes.
std::string Generate(CWallet& wallet, size_t max_bytes)
{
    WalletDumpGenerator generator(wallet);
    std::string all;
    std::string part;

    // Bounds a generator that never ends.
    for (int i = 0; i < 100000; ++i) {
        const bool more = generator.Next(part, max_bytes);
        all += part;
        part.clear();
        if (!more) return all;
    }

    BOOST_FAIL("the generator never ended");
    return all;
}

//! An in-memory wallet keeps key pool entries with no database to read them
//! from, so a dump of it (GetAllReserveKeys()) would fail; a dump case on one
//! drops the pool first. A node's wallet is file-backed.
void DropKeyPool(CWallet& wallet)
{
    LOCK(wallet.cs_wallet);
    wallet.setKeyPool.clear();
}

//! The dump \p fx's wallet should give, written out from what the fixture
//! knows, in the dump format: the header for the best block and \p now, the HD
//! master's extended key, and one line for each key it made, with its kind and
//! key path. Every key was born at \p now, so the lines are in key-id order.
std::string ExpectedDump(DumpWallet& fx, int64_t now)
{
    const auto time = [](int64_t t) { return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", t); };

    std::string out;
    {
        LOCK(cs_main);
        out += "# Wallet dump created by Gridcoin " + FormatFullVersion() + "\n";
        out += "# * Created on " + time(now) + "\n";
        out += strprintf("# * Best block at time of backup was %i (%s),\n", nBestHeight, hashBestChain.ToString());
        out += "#   mined on " + time(pindexBest->nTime) + "\n";
        out += "\n";
    }

    LOCK(fx.wallet.cs_wallet);

    const CKeyID master = fx.wallet.GetHDChain().masterKeyID;
    CKey master_key;
    BOOST_REQUIRE(fx.wallet.GetKey(master, master_key));
    CExtKey extended;
    extended.SetSeed(master_key);
    out += "# extended private masterkey: " + EncodeExtKey(extended) + "\n\n";

    // The pool, the master, the labelled key and the change key.
    std::set<CKeyID> ids;
    fx.wallet.GetKeys(ids);
    BOOST_REQUIRE_EQUAL(ids.size(), fx.pool.size() + 3);

    for (const CKeyID& id : ids) {
        CKey key;
        BOOST_REQUIRE(fx.wallet.GetKey(id, key));

        std::string kind;
        if (id == fx.labelled_id) {
            kind = "label=a%20label,%20with%20%25%20and%20spaces";
        } else if (id == master) {
            kind = "hdmaster=1";
        } else if (fx.pool.count(id)) {
            kind = "reserve=1";
        } else if (id == fx.change_id) {
            kind = "change=1";
        } else {
            BOOST_FAIL("the wallet holds a key the fixture did not make");
        }

        const std::string path = fx.wallet.mapKeyMetadata[id].hdKeypath;
        out += EncodeSecret(key) + " " + time(now) + " " + kind + " # addr=" + EncodeDestination(CTxDestination(id))
               + (path.empty() ? "" : " hdkeypath=" + path) + "\n";
    }

    out += "\n# End of dump\n";
    return out;
}

//! What \p fn threw: an RPC error as "message (code n)", any other exception
//! as its text, or nullopt when it returned.
std::optional<std::string> Thrown(const std::function<void()>& fn)
{
    try {
        fn();
    } catch (const UniValue& error) {
        return find_value(error, "message").getValStr() + strprintf(" (code %d)", find_value(error, "code").get_int());
    } catch (const std::exception& e) {
        return std::string(e.what());
    }

    return std::nullopt;
}

} // namespace

BOOST_AUTO_TEST_SUITE(walletdump_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

// The generator and dumpwallet both write the dump the fixture's wallet should
// give, byte for byte, as written out from the fixture's own keys and labels:
// every kind of line, and none for the metadata without a key. The clock is
// fixed, so the "Created on" line and the birth times are known.
BOOST_FIXTURE_TEST_CASE(generator_matches_rpc_dumpwallet_output, DumpCase)
{
    const grc_test::StateGuard state;
    const int64_t now = 1700000000;
    SetMockTime(now);

    DumpWallet fx;
    const std::string expected = ExpectedDump(fx, now);

    BOOST_CHECK(expected.find(" hdkeypath=m/") != std::string::npos);
    BOOST_CHECK(expected.find("addr=" + EncodeDestination(CTxDestination(fx.orphan_id))) == std::string::npos);

    const fs::path path = dir / "rpc.txt";
    tableRPC.execute("dumpwallet", RPCConvertValues("dumpwallet", {path.string()}));
    BOOST_CHECK_EQUAL(ReadAll(path), expected);

    BOOST_CHECK_EQUAL(Generate(fx.wallet, 100), expected);
}

// Next() appends whole lines, at least one a call, and no more than it is asked
// for unless one line is longer than that.
BOOST_FIXTURE_TEST_CASE(generator_respects_chunk_bound, DumpCase)
{
    const grc_test::StateGuard state;
    SetMockTime(1700000000);

    DumpWallet fx;

    WalletDumpGenerator generator(fx.wallet);
    std::string all;
    size_t calls = 0;

    for (bool more = true; more;) {
        BOOST_REQUIRE(++calls < 10000);

        std::string part;
        more = generator.Next(part, 300);

        BOOST_REQUIRE(!part.empty());
        BOOST_CHECK_EQUAL(part.back(), '\n');

        const size_t lines = std::count(part.begin(), part.end(), '\n');
        BOOST_CHECK_MESSAGE(part.size() <= 300 || lines == 1,
                            "call " << calls << " appended " << part.size() << " bytes in " << lines << " lines");

        all += part;
    }

    BOOST_CHECK(calls > 1);
    BOOST_REQUIRE(all.size() > 14);
    BOOST_CHECK_EQUAL(all.substr(all.size() - 15), "\n# End of dump\n");

    // Nothing after the end.
    std::string after;
    BOOST_CHECK(!generator.Next(after, 300));
    BOOST_CHECK(after.empty());

    // The estimate covers the header and at least one key.
    const size_t header_end = all.find("\n\n");
    BOOST_REQUIRE(header_end != std::string::npos);
    BOOST_CHECK_GE(generator.SizeHint(), header_end + 2 + 160);

    // A bound below every line's length: one line a call, and the same text.
    WalletDumpGenerator one_line(fx.wallet);
    std::string again;
    for (bool more = true; more;) {
        std::string part;
        more = one_line.Next(part, 1);
        BOOST_CHECK_EQUAL(std::count(part.begin(), part.end(), '\n'), 1);
        again += part;
    }
    BOOST_CHECK_EQUAL(again, all);
}

// Without the keys the generator refuses, at construction and at each Next(): a
// wallet unlocked for staking only, unless a capability scope is open on the
// calling thread.
BOOST_AUTO_TEST_CASE(generator_refuses_without_keys)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    DropKeyPool(fx.wallet);

    const std::string refusal = strprintf("keys are not available (code %d)", RPC_WALLET_UNLOCK_NEEDED);

    BOOST_CHECK_EQUAL(Thrown([&] { WalletDumpGenerator generator(fx.wallet); }).value_or("returned"), refusal);

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(EncryptedWallet::PASSPHRASE,
                                                                    CapabilityKind::FileTransfer);
    BOOST_REQUIRE(id.has_value());

    std::optional<WalletDumpGenerator> generator;
    std::string dump;
    {
        const CapabilityScope scope(fx.wallet, id, CapabilityKind::FileTransfer);
        BOOST_REQUIRE(scope.Active());

        generator.emplace(fx.wallet);
        BOOST_CHECK(generator->Next(dump, 1));
        BOOST_CHECK(!dump.empty());
    }

    // The scope has closed: the next line is refused, and nothing is appended.
    std::string after;
    BOOST_CHECK_EQUAL(Thrown([&] { generator->Next(after, 100000); }).value_or("returned"), refusal);
    BOOST_CHECK(after.empty());

    // Under a scope again, the rest is written, the wallet's key among it.
    {
        const CapabilityScope scope(fx.wallet, id, CapabilityKind::FileTransfer);
        while (generator->Next(dump, 100000)) {
        }
    }
    BOOST_CHECK(dump.find(EncodeSecret(fx.key) + " ") != std::string::npos);

    fx.wallet.EndElevation(*id);
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

// A dump imported from a stream gives the wallet the keys, the labels and the
// result that importing the same dump from its file gives.
BOOST_FIXTURE_TEST_CASE(import_from_stream_matches_file_import, DumpCase)
{
    const grc_test::StateGuard state;

    std::string dump;
    std::set<CKeyID> dumped;
    CKeyID labelled_id;
    {
        DumpWallet a;
        dump = Generate(a.wallet, 4096);
        dumped = Keys(a.wallet);
        labelled_id = a.labelled_id;
    }
    BOOST_REQUIRE(dumped.size() > 3);

    const fs::path file = dir / "a.txt";
    {
        fsbridge::ofstream out(file);
        out << dump;
    }

    CWallet b;
    std::istringstream in(dump);
    const UniValue from_stream = ImportWalletFromStream(b, in);

    CWallet c;
    UniValue from_file;
    {
        SwapMainWallet swap(&c);
        from_file = tableRPC.execute("importwallet", RPCConvertValues("importwallet", {file.string()}));
    }

    BOOST_CHECK(Keys(b) == dumped);
    BOOST_CHECK(Keys(c) == dumped);
    BOOST_CHECK_EQUAL(from_stream.write(), from_file.write());

    // The imported dump had an HD seed in it, which the result says.
    BOOST_CHECK(from_stream.isStr());

    LOCK2(b.cs_wallet, c.cs_wallet);
    BOOST_CHECK_EQUAL(b.mapAddressBook.size(), c.mapAddressBook.size());
    for (const auto& [destination, entry] : b.mapAddressBook) {
        const auto it = c.mapAddressBook.find(destination);
        BOOST_REQUIRE(it != c.mapAddressBook.end());
        BOOST_CHECK_EQUAL(entry.name, it->second.name);
    }
    BOOST_CHECK_EQUAL(b.mapAddressBook[labelled_id].name, DumpWallet::LABEL);
}

// A stream the importer cannot use is refused as the RPC refuses a file: a
// wallet unlocked for staking only, with no capability scope open.
BOOST_AUTO_TEST_CASE(import_from_stream_refuses_without_keys)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    CKey fresh;
    fresh.MakeNewKey(true);
    std::istringstream in(EncodeSecret(fresh) + " 2026-01-01T00:00:00Z label=x\n");

    const std::optional<std::string> thrown = Thrown([&] { ImportWalletFromStream(fx.wallet, in); });
    BOOST_REQUIRE(thrown.has_value());
    BOOST_CHECK_MESSAGE(thrown->find(strprintf("(code %d)", RPC_WALLET_UNLOCK_NEEDED)) != std::string::npos, *thrown);
    BOOST_CHECK(!fx.wallet.HaveKey(fresh.GetPubKey().GetID()));
}

BOOST_AUTO_TEST_SUITE_END()
