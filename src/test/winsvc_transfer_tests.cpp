// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "fs.h"
#include "univalue.h"
#include "util/strencodings.h"
#include "util/system.h"
#include "util/win_acl.h"
#include "winsvc/layout.h"
#include "winsvc/transfer.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace winsvc;
namespace winacl = util::winacl;
using winacl::AceSpec;
using winacl::AclSpec;
using winacl::Inherit;
using winacl::ObservedAce;
using winacl::ObservedAcl;

namespace {

// The single rights by their bare names, which are <windows.h>'s macros where it was included
// first (see util/win_acl.h).
using namespace util::winacl::mask;

//! A directory of its own below the suite's data directory, removed when the case ends.
struct TempTree
{
    fs::path root{GetDataDir() / fs::unique_path("winsvc_transfer_%%%%-%%%%-%%%%")};

    TempTree()
    {
        fs::create_directories(root);
    }

    ~TempTree()
    {
        boost::system::error_code ec;
        fs::remove_all(root, ec);
    }
};

void WriteFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    fsbridge::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    BOOST_REQUIRE_MESSAGE(!out.fail(), "could not write " << path.string());
}

std::string ReadFile(const fs::path& path)
{
    fsbridge::ifstream in(path, std::ios::binary);
    BOOST_REQUIRE_MESSAGE(in.is_open(), "could not open " << path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string Sha256Hex(const std::string& data)
{
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(reinterpret_cast<const unsigned char*>(data.data()), data.size()).Finalize(hash);
    return HexStr(hash);
}

//! \p size bytes that are not all alike, so a copy that drops or moves a block shows.
std::string Pattern(size_t size)
{
    std::string data(size, '\0');
    uint32_t x = 2463534242U;
    for (char& c : data) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        c = static_cast<char>(x & 0xff);
    }
    return data;
}

//! Every entry below \p root, by its relative path in generic form, as "dir", "file:<sha256>",
//! "link:<target>" or "other", read without following links.
std::map<std::string, std::string> Snapshot(const fs::path& root)
{
    std::map<std::string, std::string> entries;
    if (!fs::exists(fs::symlink_status(root))) return entries;
    for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
        const fs::path path = it->path();
        const std::string relative = path.lexically_relative(root).generic_string();
        const fs::file_status status = fs::symlink_status(path);
        if (fs::is_symlink(status)) {
            entries[relative] = "link:" + fs::read_symlink(path).generic_string();
            it.disable_recursion_pending();
        } else if (fs::is_directory(status)) {
            entries[relative] = "dir";
        } else if (fs::is_regular_file(status)) {
#ifndef WIN32
            // An unreadable file is recorded by its mode, which the transfer must not change.
            struct stat st{};
            BOOST_REQUIRE(::lstat(path.c_str(), &st) == 0);
            if ((st.st_mode & 0777) == 0) {
                entries[relative] = "unreadable";
                continue;
            }
#endif
            entries[relative] = "file:" + Sha256Hex(ReadFile(path));
        } else {
            entries[relative] = "other";
        }
    }
    return entries;
}

bool Contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

//! A user's data directory as a core leaves it: a wallet, blocks with a nested index, an empty
//! file, and a 3 MiB file.
void MakeWalletTree(const fs::path& root)
{
    WriteFile(root / "wallet.dat", "wallet bytes");
    WriteFile(root / "blocks" / "blk0001.dat", Pattern(3 * 1024 * 1024));
    WriteFile(root / "blocks" / "index" / "000001.ldb", "index");
    WriteFile(root / "blocks" / "index" / "LOCK", "");
    WriteFile(root / "peers.dat", "peers");
    fs::create_directories(root / "walletbackups" / "empty");
}

EntryFacts Facts(const std::string& relative, EntryFacts::Type type, uint64_t links = 1, uint64_t size = 0)
{
    EntryFacts facts;
    facts.relative = fs::path(relative);
    facts.type = type;
    facts.link_count = links;
    facts.size = size;
    return facts;
}

EntryDecision DecideFor(const std::string& relative, EntryFacts::Type type, const TransferRules& rules,
                        uint64_t links = 1, uint64_t size = 0)
{
    std::string reason;
    return Decide(Facts(relative, type, links, size), rules, reason);
}

Sid SidOf(const std::string& text)
{
    const std::optional<Sid> sid = winacl::ParseSidString(text);
    BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
    return *sid;
}

//! NT SERVICE\gridcoinresearchd and NT SERVICE\gridcointestnetd, as `sc showsid` gives them.
const char* const SVC_MAIN = "S-1-5-80-3522172824-109097731-499206491-3348895147-1114477730";
const char* const SVC_TEST = "S-1-5-80-2622462711-3254838260-194753155-2927861636-1882283010";
const char* const USER = "S-1-5-21-1004336348-1177238915-682003330-1001";

LayoutSids Sids()
{
    LayoutSids sids;
    sids.admins = winacl::wellknown::Administrators();
    sids.system = winacl::wellknown::System();
    sids.svc_main = SidOf(SVC_MAIN);
    sids.svc_test = SidOf(SVC_TEST);
    sids.grp_main = SidOf("S-1-5-21-1004336348-1177238915-682003330-1010");
    sids.grp_test = SidOf("S-1-5-21-1004336348-1177238915-682003330-1011");
    return sids;
}

fs::path ProgramData()
{
    return fs::path(L"C:\\ProgramData");
}

//! An inherited allow entry as Windows writes it below a directory whose entry is passed down.
ObservedAce InheritedAce(const Sid& trustee, uint32_t mask, bool directory)
{
    ObservedAce ace;
    ace.kind = winacl::AceKind::Allow;
    ace.trustee = trustee;
    ace.mask = mask;
    ace.inherited = true;
    ace.inherit = directory ? Inherit::ContainerAndObject : Inherit::None;
    return ace;
}

} // namespace

BOOST_AUTO_TEST_SUITE(winsvc_transfer_tests)

BOOST_AUTO_TEST_CASE(transfer_copies_tree_and_verifies)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    const std::map<std::string, std::string> before = Snapshot(src);

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), tree.root / "quarantine");
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);

    // Every file and directory, each file hashing as its source does.
    BOOST_CHECK(Snapshot(dst) == before);
    BOOST_CHECK(Snapshot(src) == before);
    BOOST_CHECK_EQUAL(result.files, 5U);
    BOOST_CHECK_EQUAL(result.bytes, 12U + 3U * 1024 * 1024 + 5U + 5U);
    BOOST_CHECK(result.created_dst);

    std::set<fs::path> created(result.created_top.begin(), result.created_top.end());
    BOOST_CHECK(created == std::set<fs::path>({"wallet.dat", "blocks", "peers.dat", "walletbackups"}));
    std::set<fs::path> taken(result.source_remove.begin(), result.source_remove.end());
    BOOST_CHECK(taken == created);
    BOOST_CHECK(result.quarantined.empty());
    BOOST_CHECK(!fs::exists(tree.root / "quarantine"));

#ifndef WIN32
    // Owner-only, whatever the umask.
    struct stat st{};
    BOOST_REQUIRE(::lstat((dst / "wallet.dat").c_str(), &st) == 0);
    BOOST_CHECK_EQUAL(st.st_mode & 07777, 0600U);
    BOOST_REQUIRE(::lstat((dst / "blocks" / "index").c_str(), &st) == 0);
    BOOST_CHECK_EQUAL(st.st_mode & 07777, 0700U);
    BOOST_REQUIRE(::lstat(dst.c_str(), &st) == 0);
    BOOST_CHECK_EQUAL(st.st_mode & 07777, 0700U);
#endif
}

BOOST_AUTO_TEST_CASE(transfer_into_an_existing_empty_destination)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    fs::create_directories(dst);

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), tree.root / "quarantine");
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);
    BOOST_CHECK(!result.created_dst);
    BOOST_CHECK(Snapshot(dst) == Snapshot(src));
}

#ifndef WIN32
//! A refused transfer: ok false, the error names the path, the destination as it was and the
//! source byte for byte as it was.
void CheckRefused(const fs::path& src, const fs::path& dst, const TransferRules& rules, const fs::path& quarantine,
                  const std::string& named)
{
    const std::map<std::string, std::string> src_before = Snapshot(src);
    const bool dst_existed = fs::exists(dst);
    const std::map<std::string, std::string> dst_before = Snapshot(dst);

    const TransferResult result = TransferTree(src, dst, rules, quarantine);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK_MESSAGE(Contains(result.error, named), "'" << result.error << "' does not name " << named);
    BOOST_CHECK_EQUAL(fs::exists(dst), dst_existed);
    BOOST_CHECK(Snapshot(dst) == dst_before);
    BOOST_CHECK(Snapshot(src) == src_before);
    BOOST_CHECK(!fs::exists(quarantine));
}

BOOST_AUTO_TEST_CASE(transfer_refuses_symlink_file)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    MakeWalletTree(src);
    WriteFile(tree.root / "outside.txt", "outside");
    BOOST_REQUIRE(::symlink((tree.root / "outside.txt").c_str(), (src / "blocks" / "link.dat").c_str()) == 0);

    CheckRefused(src, tree.root / "dst", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "blocks" / "link.dat").string());

    // Into an empty directory too, which stays empty.
    fs::create_directories(tree.root / "empty");
    CheckRefused(src, tree.root / "empty", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "blocks" / "link.dat").string());
}

BOOST_AUTO_TEST_CASE(transfer_refuses_symlink_dir)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    MakeWalletTree(src);
    fs::create_directories(tree.root / "outside");
    WriteFile(tree.root / "outside" / "secret", "secret");
    BOOST_REQUIRE(::symlink((tree.root / "outside").c_str(), (src / "chainstate").c_str()) == 0);

    CheckRefused(src, tree.root / "dst", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "chainstate").string());
}

BOOST_AUTO_TEST_CASE(transfer_refuses_hardlinked_file)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    MakeWalletTree(src);
    WriteFile(tree.root / "outside.dat", "outside");
    BOOST_REQUIRE(::link((tree.root / "outside.dat").c_str(), (src / "blocks" / "hard.dat").c_str()) == 0);

    CheckRefused(src, tree.root / "dst", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "blocks" / "hard.dat").string());
}

BOOST_AUTO_TEST_CASE(transfer_refuses_fifo)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    MakeWalletTree(src);
    BOOST_REQUIRE(::mkfifo((src / "blocks" / "pipe").c_str(), 0600) == 0);

    CheckRefused(src, tree.root / "dst", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "blocks" / "pipe").string());
}
#endif // WIN32

BOOST_AUTO_TEST_CASE(transfer_skips_runtime_files)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    WriteFile(src / "ipc.cookie", "cookie");
    WriteFile(src / ".lock", "");
    WriteFile(src / "gridcoinresearchd.pid", "1234");
    WriteFile(src / "autounlock" / "passphrase.dpapi", "blob");
#ifndef WIN32
    // Names are decided before kinds: a socket left behind is skipped, not refused.
    BOOST_REQUIRE(::mkfifo((src / "node.sock").c_str(), 0600) == 0);
#else
    WriteFile(src / "node.sock", "");
#endif

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), tree.root / "quarantine");
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);
    for (const char* name : {"node.sock", "ipc.cookie", ".lock", "gridcoinresearchd.pid", "autounlock"}) {
        BOOST_CHECK_MESSAGE(!fs::exists(fs::symlink_status(dst / name)), name << " was copied");
    }
    BOOST_CHECK(fs::exists(dst / "wallet.dat"));

    // Skipped entries are taken from the source with the rest: the user's autounlock file goes.
    std::set<fs::path> taken(result.source_remove.begin(), result.source_remove.end());
    for (const char* name : {"node.sock", "ipc.cookie", "gridcoinresearchd.pid", "autounlock"}) {
        BOOST_CHECK_MESSAGE(taken.count(name) == 1, name << " is not among the entries taken");
    }
    // The caller holds the lock file and removes it last, so it is not among them.
    BOOST_CHECK(taken.count(".lock") == 0);
    std::set<fs::path> created(result.created_top.begin(), result.created_top.end());
    BOOST_CHECK(created.count("autounlock") == 0);
}

BOOST_AUTO_TEST_CASE(transfer_quarantines_conf_and_db_config)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    const fs::path quarantine = tree.root / "quarantine";
    MakeWalletTree(src);
    WriteFile(src / "gridcoinresearch.conf", "blocknotify=evil\n");
    WriteFile(src / "DB_CONFIG", "set_lg_dir /elsewhere\n");

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), quarantine);
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);
    BOOST_CHECK(!fs::exists(dst / "gridcoinresearch.conf"));
    BOOST_CHECK(!fs::exists(dst / "DB_CONFIG"));
    BOOST_CHECK_EQUAL(ReadFile(quarantine / "gridcoinresearch.conf"), "blocknotify=evil\n");
    BOOST_CHECK_EQUAL(ReadFile(quarantine / "DB_CONFIG"), "set_lg_dir /elsewhere\n");

    std::set<fs::path> quarantined(result.quarantined.begin(), result.quarantined.end());
    BOOST_CHECK(quarantined == std::set<fs::path>({quarantine / "gridcoinresearch.conf", quarantine / "DB_CONFIG"}));

    // A quarantined file stays in the source: it is not among the entries taken.
    std::set<fs::path> taken(result.source_remove.begin(), result.source_remove.end());
    BOOST_CHECK(taken.count("gridcoinresearch.conf") == 0);
    BOOST_CHECK(taken.count("DB_CONFIG") == 0);
    BOOST_CHECK(fs::exists(src / "gridcoinresearch.conf"));
}

BOOST_AUTO_TEST_CASE(transfer_filters_settings)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    const fs::path quarantine = tree.root / "quarantine";
    MakeWalletTree(src);
    const std::string settings = "{\"blocknotify\":\"x\",\"staking\":true}";
    WriteFile(src / "gridcoinsettings.json", settings);

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), quarantine);
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);

    UniValue copied;
    BOOST_REQUIRE(copied.read(ReadFile(dst / "gridcoinsettings.json")));
    BOOST_REQUIRE(copied.isObject());
    BOOST_CHECK(copied.getKeys() == std::vector<std::string>({"staking"}));
    BOOST_CHECK(copied["staking"].isTrue());
    BOOST_CHECK(result.dropped_settings == std::vector<std::string>({"blocknotify"}));

    // The file as it was is kept where the service does not read it.
    BOOST_CHECK_EQUAL(ReadFile(quarantine / "gridcoinsettings.json"), settings);
    BOOST_CHECK_EQUAL(ReadFile(src / "gridcoinsettings.json"), settings);
}

BOOST_AUTO_TEST_CASE(transfer_main_excludes_testnet)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    MakeWalletTree(src / "testnet");

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), tree.root / "quarantine");
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);
    BOOST_CHECK(fs::exists(dst / "wallet.dat"));
    BOOST_CHECK(!fs::exists(dst / "testnet"));
    BOOST_CHECK(std::find(result.source_remove.begin(), result.source_remove.end(), fs::path("testnet")) ==
                result.source_remove.end());
}

BOOST_AUTO_TEST_CASE(transfer_test_takes_only_testnet)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    WriteFile(src / "gridcoinresearch.conf", "testnet=1\n");
    MakeWalletTree(src / "testnet");
    WriteFile(src / "testnet" / ".lock", "");

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Test), tree.root / "quarantine");
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);

    std::map<std::string, std::string> expected;
    for (const auto& [name, kind] : Snapshot(src / "testnet")) {
        if (name != ".lock") expected["testnet/" + name] = kind;
    }
    expected["testnet"] = "dir";
    BOOST_CHECK(Snapshot(dst) == expected);
    BOOST_CHECK(result.created_top == std::vector<fs::path>({"testnet"}));
    // Not the directory as a whole, which holds the lock file the caller removes last, but what is in it.
    std::set<fs::path> taken(result.source_remove.begin(), result.source_remove.end());
    BOOST_CHECK(taken.count("testnet") == 0);
    BOOST_CHECK(taken.count(fs::path("testnet") / "wallet.dat") == 1);
    BOOST_CHECK(taken.count(fs::path("testnet") / ".lock") == 0);
    // The configuration file at the top is the main network's, outside the test network's tree.
    BOOST_CHECK(result.quarantined.empty());
}

BOOST_AUTO_TEST_CASE(transfer_test_without_testnet_refused)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    MakeWalletTree(src);

    const TransferResult result =
        TransferTree(src, tree.root / "dst", RulesForEnable(Network::Test), tree.root / "quarantine");
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(Contains(result.error, "testnet"));
    BOOST_CHECK(!fs::exists(tree.root / "dst"));
}

BOOST_AUTO_TEST_CASE(transfer_refuses_nonempty_destination)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    // What an interrupted run left: a name the transfer creates.
    WriteFile(dst / "wallet.dat", "a partial copy");
    const std::map<std::string, std::string> src_before = Snapshot(src);
    const std::map<std::string, std::string> dst_before = Snapshot(dst);

    const std::optional<std::string> refusal = DestinationRefusal(src, dst, RulesForEnable(Network::Main));
    BOOST_REQUIRE(refusal.has_value());
    BOOST_CHECK(Contains(*refusal, (dst / "wallet.dat").string()));

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), tree.root / "quarantine");
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(Contains(result.error, (dst / "wallet.dat").string()));
    BOOST_CHECK(Snapshot(dst) == dst_before);
    BOOST_CHECK(Snapshot(src) == src_before);

    // A file at the destination itself.
    WriteFile(tree.root / "file", "x");
    BOOST_CHECK(DestinationRefusal(src, tree.root / "file", RulesForEnable(Network::Main)).has_value());
    BOOST_CHECK(!TransferTree(src, tree.root / "file", RulesForEnable(Network::Main), tree.root / "q").ok);
}

BOOST_AUTO_TEST_CASE(transfer_keeps_unrelated_destination_entries)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    // A user's directory that kept its configuration file and its test network.
    WriteFile(dst / "gridcoinresearch.conf", "addnode=x\n");
    WriteFile(dst / "testnet" / "wallet.dat", "test wallet");

    BOOST_CHECK(!DestinationRefusal(src, dst, RulesForDisable(Network::Main)).has_value());
    const TransferResult result = TransferTree(src, dst, RulesForDisable(Network::Main), fs::path());
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);
    BOOST_CHECK(!result.created_dst);
    BOOST_CHECK_EQUAL(ReadFile(dst / "gridcoinresearch.conf"), "addnode=x\n");
    BOOST_CHECK_EQUAL(ReadFile(dst / "testnet" / "wallet.dat"), "test wallet");
    BOOST_CHECK_EQUAL(ReadFile(dst / "wallet.dat"), "wallet bytes");
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(transfer_test_leaves_the_users_conf_at_the_source)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    const fs::path quarantine = tree.root / "quarantine";
    MakeWalletTree(src / "testnet");
    WriteFile(src / "testnet" / ".lock", "");
    WriteFile(src / "testnet" / "gridcoinresearch.conf", "testnet=1\n");
    WriteFile(src / "testnet" / "DB_CONFIG", "set_lg_dir /elsewhere\n");
    WriteFile(src / "testnet" / "blocks" / "autounlock", "blob");

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Test), quarantine);
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);
    BOOST_CHECK_EQUAL(ReadFile(quarantine / "testnet" / "gridcoinresearch.conf"), "testnet=1\n");
    std::set<fs::path> kept(result.source_kept.begin(), result.source_kept.end());
    BOOST_CHECK(kept == std::set<fs::path>({fs::path("testnet") / "gridcoinresearch.conf",
                                            fs::path("testnet") / "DB_CONFIG"}));

    // The removal step: the wallet and the rest go, the quarantined files stay where the user keeps them.
    std::string error;
    BOOST_REQUIRE_MESSAGE(RemoveTreeNoFollow(src, result.source_remove, error), error);
    BOOST_CHECK(!fs::exists(src / "testnet" / "wallet.dat"));
    BOOST_CHECK(!fs::exists(src / "testnet" / "blocks"));
    BOOST_CHECK(!fs::exists(src / "testnet" / "peers.dat"));
    BOOST_CHECK_EQUAL(ReadFile(src / "testnet" / "gridcoinresearch.conf"), "testnet=1\n");
    BOOST_CHECK_EQUAL(ReadFile(src / "testnet" / "DB_CONFIG"), "set_lg_dir /elsewhere\n");
    BOOST_CHECK(fs::exists(src / "testnet" / ".lock"));

    // The lock file goes last; the directory it was in stays, as it holds the kept files.
    BOOST_REQUIRE(::unlink((src / "testnet" / ".lock").c_str()) == 0);
    bool removed = true;
    BOOST_REQUIRE_MESSAGE(RemoveDirIfEmpty(src, "testnet", removed, error), error);
    BOOST_CHECK(!removed);
    BOOST_CHECK(fs::exists(src / "testnet" / "gridcoinresearch.conf"));
}

BOOST_AUTO_TEST_CASE(transfer_test_removes_the_emptied_source_directory)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    MakeWalletTree(src / "testnet");
    WriteFile(src / "testnet" / ".lock", "");

    const TransferResult result = TransferTree(src, tree.root / "dst", RulesForEnable(Network::Test),
                                               tree.root / "quarantine");
    BOOST_REQUIRE_MESSAGE(result.ok, result.error);
    BOOST_CHECK(result.source_kept.empty());
    BOOST_CHECK(RulesForEnable(Network::Test).hold == fs::path("testnet") / ".lock");
    BOOST_CHECK(RulesForEnable(Network::Main).hold == fs::path(".lock"));
    BOOST_CHECK(RulesForDisable(Network::Test).hold == fs::path("testnet") / ".lock");

    std::string error;
    BOOST_REQUIRE_MESSAGE(RemoveTreeNoFollow(src, result.source_remove, error), error);
    // Only the held lock file is left, so the directory is not empty yet.
    bool removed = true;
    BOOST_REQUIRE_MESSAGE(RemoveDirIfEmpty(src, "testnet", removed, error), error);
    BOOST_CHECK(!removed);
    BOOST_CHECK(fs::exists(src / "testnet" / ".lock"));
    BOOST_REQUIRE(::unlink((src / "testnet" / ".lock").c_str()) == 0);
    BOOST_REQUIRE_MESSAGE(RemoveDirIfEmpty(src, "testnet", removed, error), error);
    BOOST_CHECK(removed);
    BOOST_CHECK(!fs::exists(src / "testnet"));
    BOOST_CHECK(fs::exists(src));
    // Nothing there is not an error, and a path that is not plain is refused.
    BOOST_CHECK(RemoveDirIfEmpty(src, "testnet", removed, error));
    BOOST_CHECK(!removed);
    BOOST_CHECK(!RemoveDirIfEmpty(src, "..", removed, error));
}

BOOST_AUTO_TEST_CASE(transfer_refuses_malformed_settings_before_writing)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    WriteFile(src / "gridcoinresearch.conf", "conf\n");
    WriteFile(src / "gridcoinsettings.json", "{\"a\":1,\"a\":2}");

    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), tree.root / "quarantine");
    BOOST_CHECK(!result.ok);
    // Found in the planning pass, which writes nothing, and not part way through the copy.
    BOOST_CHECK_MESSAGE(Contains(result.error, "Nothing was copied"), result.error);
    BOOST_CHECK(Contains(result.error, "gridcoinsettings.json"));
    BOOST_CHECK(!fs::exists(dst));
    BOOST_CHECK(!fs::exists(tree.root / "quarantine"));
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(transfer_notices_a_destination_that_does_not_read_back)
{
    const TempTree tree;
    const fs::path src = tree.root / "src";
    const fs::path dst = tree.root / "dst";
    MakeWalletTree(src);
    const std::map<std::string, std::string> before = Snapshot(src);

    // Corrupts the first byte of each file written, after it is written and before it is read back.
    int corrupted = 0;
    winsvc::testing::after_write = [&corrupted](const fs::path& path) {
        std::string content = ReadFile(path);
        if (content.empty()) return;
        content[0] = static_cast<char>(content[0] ^ 0x01);
        WriteFile(path, content);
        ++corrupted;
    };
    const TransferResult result = TransferTree(src, dst, RulesForEnable(Network::Main), tree.root / "quarantine");
    winsvc::testing::after_write = nullptr;

    BOOST_CHECK_GT(corrupted, 0);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK_MESSAGE(Contains(result.error, "does not read back as it was written"), result.error);
    BOOST_CHECK(!fs::exists(dst));
    BOOST_CHECK(Snapshot(src) == before);
}
#endif

BOOST_AUTO_TEST_CASE(follow_ups_run_after_a_failure)
{
    std::vector<std::string> ran;
    const auto step = [&ran](const std::string& name, bool fail) {
        return [&ran, name, fail] {
            ran.push_back(name);
            if (fail) throw std::runtime_error(name + " broke");
        };
    };

    // A failing step is named with what to do by hand, and the steps after it still run.
    std::vector<FollowUp> steps = {
        {"one", "do one", step("one", false), -1},
        {"two", "do two", step("two", true), -1},
        {"three", "do three", step("three", false), -1},
    };
    std::vector<std::string> warnings = RunFollowUps(steps);
    BOOST_CHECK((ran == std::vector<std::string>{"one", "two", "three"}));
    BOOST_REQUIRE_EQUAL(warnings.size(), 1U);
    BOOST_CHECK(Contains(warnings[0], "two"));
    BOOST_CHECK(Contains(warnings[0], "two broke"));
    BOOST_CHECK(Contains(warnings[0], "do two"));

    // A step that needs another is not run when that one failed, and says so.
    ran.clear();
    steps = {
        {"config", "sc config", step("config", true), -1},
        {"start", "sc start", step("start", false), 0},
        {"other", "do other", step("other", false), -1},
    };
    warnings = RunFollowUps(steps);
    BOOST_CHECK((ran == std::vector<std::string>{"config", "other"}));
    BOOST_REQUIRE_EQUAL(warnings.size(), 2U);
    BOOST_CHECK(Contains(warnings[1], "start"));
    BOOST_CHECK(Contains(warnings[1], "not attempted"));
    BOOST_CHECK(Contains(warnings[1], "sc start"));

    // Nothing failing gives nothing to say.
    ran.clear();
    steps = {{"a", "x", step("a", false), -1}, {"b", "y", step("b", false), 0}};
    BOOST_CHECK(RunFollowUps(steps).empty());
    BOOST_CHECK((ran == std::vector<std::string>{"a", "b"}));
}

BOOST_AUTO_TEST_CASE(transfer_failure_midway_removes_partial_destination)
{
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("running as root, which reads a mode 000 file; case skipped");
        return;
    }
    const TempTree tree;
    const fs::path src = tree.root / "src";
    MakeWalletTree(src);
    WriteFile(src / "gridcoinresearch.conf", "conf\n");
    // Read after the entries before it are copied: the copy fails part way.
    WriteFile(src / "zz_unreadable.dat", "unreadable");
    BOOST_REQUIRE(::chmod((src / "zz_unreadable.dat").c_str(), 0) == 0);

    // Into a destination that is absent, and into one that is an empty directory.
    CheckRefused(src, tree.root / "dst", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "zz_unreadable.dat").string());
    fs::create_directories(tree.root / "empty");
    CheckRefused(src, tree.root / "empty", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "zz_unreadable.dat").string());

    // Into one that holds other entries, which are kept.
    WriteFile(tree.root / "kept" / "notes.txt", "notes");
    CheckRefused(src, tree.root / "kept", RulesForEnable(Network::Main), tree.root / "quarantine",
                 (src / "zz_unreadable.dat").string());

    BOOST_REQUIRE(::chmod((src / "zz_unreadable.dat").c_str(), 0600) == 0);
}

BOOST_AUTO_TEST_CASE(remove_tree_does_not_follow_links)
{
    const TempTree tree;
    const fs::path root = tree.root / "root";
    MakeWalletTree(root);
    WriteFile(tree.root / "outside" / "file.txt", "outside");
    BOOST_REQUIRE(::symlink((tree.root / "outside" / "file.txt").c_str(), (root / "blocks" / "link").c_str()) == 0);
    BOOST_REQUIRE(::symlink((tree.root / "outside").c_str(), (root / "dirlink").c_str()) == 0);

    std::string error;
    BOOST_REQUIRE_MESSAGE(RemoveTreeNoFollow(root, {"blocks", "dirlink", "wallet.dat", "absent"}, error), error);
    BOOST_CHECK(!fs::exists(fs::symlink_status(root / "blocks")));
    BOOST_CHECK(!fs::exists(fs::symlink_status(root / "dirlink")));
    BOOST_CHECK(!fs::exists(root / "wallet.dat"));
    BOOST_CHECK(fs::exists(root / "peers.dat"));
    BOOST_CHECK(fs::exists(root));
    BOOST_CHECK_EQUAL(ReadFile(tree.root / "outside" / "file.txt"), "outside");

    // A path deeper than the top, and the paths it refuses.
    BOOST_REQUIRE_MESSAGE(RemoveTreeNoFollow(root, {fs::path("walletbackups") / "empty"}, error), error);
    BOOST_CHECK(!fs::exists(root / "walletbackups" / "empty"));
    BOOST_CHECK(fs::exists(root / "walletbackups"));
    BOOST_CHECK(!RemoveTreeNoFollow(root, {".."}, error));
    BOOST_CHECK(!RemoveTreeNoFollow(root, {fs::path("walletbackups") / ".." / "peers.dat"}, error));
    BOOST_CHECK(!RemoveTreeNoFollow(root, {tree.root / "outside"}, error));
    BOOST_CHECK(fs::exists(root / "peers.dat"));
    BOOST_CHECK(fs::exists(tree.root / "outside" / "file.txt"));
}
#endif // WIN32

BOOST_AUTO_TEST_CASE(decide_table)
{
    using Type = EntryFacts::Type;
    const TransferRules main_rules = RulesForEnable(Network::Main);

    BOOST_CHECK(DecideFor("blocks", Type::Directory, main_rules) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("wallet.dat", Type::Regular, main_rules) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("blocks/blk0001.dat", Type::Regular, main_rules) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("wallet.dat", Type::Regular, main_rules, 2) == EntryDecision::Refuse);
    BOOST_CHECK(DecideFor("wallet.dat", Type::Symlink, main_rules) == EntryDecision::Refuse);
    BOOST_CHECK(DecideFor("blocks", Type::Reparse, main_rules) == EntryDecision::Refuse);
    BOOST_CHECK(DecideFor("blocks/pipe", Type::Other, main_rules) == EntryDecision::Refuse);

    // Each reason says what the entry is.
    std::string reason;
    BOOST_CHECK(Decide(Facts("wallet.dat", Type::Regular, 3), main_rules, reason) == EntryDecision::Refuse);
    BOOST_CHECK(Contains(reason, "3"));
    BOOST_CHECK(Decide(Facts("x", Type::Symlink), main_rules, reason) == EntryDecision::Refuse);
    BOOST_CHECK(Contains(reason, "link"));
    BOOST_CHECK(Decide(Facts("x", Type::Reparse), main_rules, reason) == EntryDecision::Refuse);
    BOOST_CHECK(Contains(reason, "reparse point"));

    // Runtime files and the autounlock directory are skipped whatever they are, at any depth.
    for (const char* name : {"node.sock", "ipc.cookie", ".lock", "gridcoinresearchd.pid"}) {
        BOOST_CHECK(DecideFor(name, Type::Regular, main_rules) == EntryDecision::Skip);
        BOOST_CHECK(DecideFor(name, Type::Other, main_rules) == EntryDecision::Skip);
        BOOST_CHECK(DecideFor(name, Type::Reparse, main_rules) == EntryDecision::Skip);
    }
    BOOST_CHECK(DecideFor("autounlock", Type::Directory, main_rules) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("NODE.SOCK", Type::Reparse, main_rules) == EntryDecision::Skip);

    // The files the service must not read, in any case, as Windows opens them.
    BOOST_CHECK(DecideFor("gridcoinresearch.conf", Type::Regular, main_rules) == EntryDecision::Quarantine);
    BOOST_CHECK(DecideFor("DB_CONFIG", Type::Regular, main_rules) == EntryDecision::Quarantine);
    BOOST_CHECK(DecideFor("db_config", Type::Regular, main_rules) == EntryDecision::Quarantine);
    BOOST_CHECK(DecideFor("gridcoinsettings.json", Type::Regular, main_rules) == EntryDecision::FilterSettings);
    BOOST_CHECK(DecideFor("GridcoinSettings.JSON", Type::Regular, main_rules) == EntryDecision::FilterSettings);
    BOOST_CHECK(DecideFor("gridcoinsettings.json", Type::Regular, main_rules, 1, SETTINGS_SIZE_LIMIT) ==
                EntryDecision::FilterSettings);
    BOOST_CHECK(DecideFor("gridcoinsettings.json", Type::Regular, main_rules, 1, SETTINGS_SIZE_LIMIT + 1) ==
                EntryDecision::Refuse);
    // Not a file where a file is expected, or a link where one is.
    BOOST_CHECK(DecideFor("gridcoinresearch.conf", Type::Directory, main_rules) == EntryDecision::Refuse);
    BOOST_CHECK(DecideFor("gridcoinsettings.json", Type::Directory, main_rules) == EntryDecision::Refuse);
    BOOST_CHECK(DecideFor("gridcoinresearch.conf", Type::Symlink, main_rules) == EntryDecision::Refuse);
    BOOST_CHECK(DecideFor("gridcoinsettings.json", Type::Regular, main_rules, 2) == EntryDecision::Refuse);

    // Scope: the main network leaves the test network's tree; the test network takes only it.
    BOOST_CHECK(DecideFor("testnet", Type::Directory, main_rules) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("testnet/wallet.dat", Type::Regular, main_rules) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("testnet", Type::Symlink, main_rules) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("blocks/testnet", Type::Directory, main_rules) == EntryDecision::Copy);
    const TransferRules test_rules = RulesForEnable(Network::Test);
    BOOST_CHECK(DecideFor("testnet", Type::Directory, test_rules) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("TestNet", Type::Directory, test_rules) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("testnet/wallet.dat", Type::Regular, test_rules) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("testnet/gridcoinsettings.json", Type::Regular, test_rules) ==
                EntryDecision::FilterSettings);
    BOOST_CHECK(DecideFor("testnet/autounlock", Type::Directory, test_rules) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("wallet.dat", Type::Regular, test_rules) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("gridcoinresearch.conf", Type::Regular, test_rules) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("testnet", Type::Reparse, test_rules) == EntryDecision::Refuse);
    BOOST_CHECK(InScope("testnet/blocks", test_rules));
    BOOST_CHECK(!InScope("blocks", test_rules));
    BOOST_CHECK(InScope("blocks", main_rules));
    BOOST_CHECK(!InScope("testnet", main_rules));

    // Moving back out quarantines and filters nothing.
    const TransferRules back = RulesForDisable(Network::Main);
    BOOST_CHECK(DecideFor("gridcoinresearch.conf", Type::Regular, back) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("gridcoinsettings.json", Type::Regular, back) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("autounlock", Type::Directory, back) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("testnet", Type::Directory, back) == EntryDecision::Skip);
    BOOST_CHECK(DecideFor("wallet.dat", Type::Regular, back, 2) == EntryDecision::Refuse);
    const TransferRules back_test = RulesForDisable(Network::Test);
    BOOST_CHECK(DecideFor("testnet/wallet.dat", Type::Regular, back_test) == EntryDecision::Copy);
    BOOST_CHECK(DecideFor("wallet.dat", Type::Regular, back_test) == EntryDecision::Skip);
}

BOOST_AUTO_TEST_CASE(rules_for_each_direction)
{
    const std::vector<std::string> runtime{"node.sock", "ipc.cookie", ".lock", "gridcoinresearchd.pid", "autounlock"};
    for (const Network net : {Network::Main, Network::Test}) {
        const TransferRules in = RulesForEnable(net);
        BOOST_CHECK(in.skip_names == runtime);
        BOOST_CHECK(in.quarantine_names == std::vector<std::string>({"gridcoinresearch.conf", "DB_CONFIG"}));
        BOOST_CHECK(in.filter_settings_names == std::vector<std::string>({"gridcoinsettings.json"}));

        const TransferRules out = RulesForDisable(net);
        BOOST_CHECK(out.skip_names == runtime);
        BOOST_CHECK(out.quarantine_names.empty());
        BOOST_CHECK(out.filter_settings_names.empty());

        for (const TransferRules& rules : {in, out}) {
            if (net == Network::Main) {
                BOOST_CHECK(rules.exclude_top_dirs == std::vector<std::string>({"testnet"}));
                BOOST_CHECK(!rules.only_top_dir.has_value());
            } else {
                BOOST_CHECK(rules.exclude_top_dirs.empty());
                BOOST_CHECK(rules.only_top_dir == std::optional<std::string>("testnet"));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(filter_settings_json)
{
    // The core's own rule decides: a section prefix is looked through, the "no" prefix is not.
    const auto [filtered, dropped] =
        FilterSettingsJson("{\"blocknotify\":\"x\",\"staking\":true,\"test.walletnotify\":\"y\",\"rpcport\":1}");
    UniValue value;
    BOOST_REQUIRE(value.read(filtered));
    BOOST_CHECK(value.getKeys() == std::vector<std::string>({"staking"}));
    BOOST_CHECK(dropped == std::vector<std::string>({"blocknotify", "test.walletnotify", "rpcport"}));
    BOOST_CHECK(!filtered.empty() && filtered.back() == '\n');

    // Nothing to drop: the text as it was.
    const std::string clean = "{\n    \"staking\": true,\n    \"enablesidestaking\": false\n}\n";
    const auto [same, none] = FilterSettingsJson(clean);
    BOOST_CHECK_EQUAL(same, clean);
    BOOST_CHECK(none.empty());

    BOOST_CHECK_THROW(FilterSettingsJson("not json"), std::runtime_error);
    BOOST_CHECK_THROW(FilterSettingsJson("[1, 2]"), std::runtime_error);
    BOOST_CHECK_THROW(FilterSettingsJson("{\"staking\":true,\"staking\":false}"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(local_tree_paths)
{
    BOOST_CHECK(!LocalTreePathProblem(L"C:\\Users\\svcsec-b\\AppData\\Roaming\\GridcoinResearch").has_value());
    BOOST_CHECK(!LocalTreePathProblem(L"D:/Grid coin/data").has_value());
    BOOST_CHECK(!LocalTreePathProblem(L"C:\\Users\\Jos\u00e9 Garc\u00eda\\AppData\\Roaming\\GridcoinResearch").has_value());
    for (const std::wstring path : {L"", L"GridcoinResearch", L"\\Users\\x", L"C:", L"C:relative",
                                    L"\\\\server\\share\\dir", L"\\\\?\\C:\\x", L"C:\\a\\..\\b", L"C:\\a\\.\\b",
                                    L"1:\\x", L"C:\\x\\", L"C:\\a\\\\b"}) {
        BOOST_CHECK_MESSAGE(LocalTreePathProblem(path).has_value(), "accepted a path it must refuse");
    }
}

BOOST_AUTO_TEST_CASE(moved_in_tree_is_the_services_and_install_accepts_it)
{
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = ServiceLayout(ProgramData(), sids);
    for (const Network net : {Network::Main, Network::Test}) {
        const MovedTreeSecurity security = EnableSecurity(ProgramData(), net, sids);
        const Sid& service = net == Network::Main ? sids.svc_main : sids.svc_test;
        BOOST_CHECK(security.owner == service);

        // The root's list is the layout's for the service's data directory, owner included.
        const auto datadir = std::find_if(layout.begin(), layout.end(), [net](const LayoutEntry& entry) {
            return entry.path == DataDir(ProgramData(), net);
        });
        BOOST_REQUIRE(datadir != layout.end());
        BOOST_CHECK(security.root_spec.owner == datadir->acl.owner);
        BOOST_REQUIRE_EQUAL(security.root_spec.aces.size(), datadir->acl.aces.size());
        for (size_t i = 0; i < security.root_spec.aces.size(); ++i) {
            BOOST_CHECK(security.root_spec.aces[i].trustee == datadir->acl.aces[i].trustee);
            BOOST_CHECK_EQUAL(security.root_spec.aces[i].mask, datadir->acl.aces[i].mask);
            BOOST_CHECK(security.root_spec.aces[i].inherit == datadir->acl.aces[i].inherit);
        }

        // What a later install --reconfigure finds directly inside the data directory: for the
        // main network the moved entries, which the service owns; for the test network the
        // network data directory, owned as the root's list says.
        const fs::path level = DataDir(ProgramData(), net);
        LevelEntryFacts moved;
        moved.path = net == Network::Main ? level / L"wallet.dat" : NetDataDir(ProgramData(), net);
        moved.owner = net == Network::Main ? security.owner : security.root_spec.owner;
        BOOST_CHECK(!LevelEntryRefusal(layout, ProgramData(), level, moved, sids).has_value());
        // Every entry below is the service's, which the scan accepts in the data directory too.
        moved.path = level / L"blocks";
        moved.owner = security.owner;
        BOOST_CHECK(!LevelEntryRefusal(layout, ProgramData(), level, moved, sids).has_value());

        // The user's ownership, kept, would be refused.
        moved.owner = SidOf(USER);
        BOOST_CHECK(LevelEntryRefusal(layout, ProgramData(), level, moved, sids).has_value());
    }
}

BOOST_AUTO_TEST_CASE(owner_only_spec_for_the_user)
{
    const Sid user = SidOf(USER);
    const AclSpec spec = OwnerOnlySpec(user);
    BOOST_CHECK(spec.owner == user);
    BOOST_REQUIRE_EQUAL(spec.aces.size(), 2U);
    BOOST_CHECK(spec.aces[0].trustee == user);
    BOOST_CHECK(spec.aces[1].trustee == winacl::wellknown::System());
    for (const AceSpec& ace : spec.aces) {
        BOOST_CHECK_EQUAL(ace.mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
        BOOST_CHECK(ace.inherit == Inherit::ContainerAndObject);
    }

    const AclSpec file = SpecForFile(spec);
    BOOST_CHECK(file.owner == user);
    BOOST_REQUIRE_EQUAL(file.aces.size(), 2U);
    for (size_t i = 0; i < file.aces.size(); ++i) {
        BOOST_CHECK(file.aces[i].trustee == spec.aces[i].trustee);
        BOOST_CHECK_EQUAL(file.aces[i].mask, spec.aces[i].mask);
        BOOST_CHECK(file.aces[i].inherit == Inherit::None);
    }
}

BOOST_AUTO_TEST_CASE(inherited_only_read_back)
{
    const LayoutSids sids = Sids();
    const MovedTreeSecurity security = EnableSecurity(ProgramData(), Network::Main, sids);
    BOOST_REQUIRE(!security.root_spec.aces.empty());

    // What Windows gives an entry below the root: the root's passed-down entries, inherited.
    for (const bool directory : {true, false}) {
        ObservedAcl acl;
        acl.owner = security.owner;
        acl.dacl_present = true;
        acl.is_protected = false;
        for (const AceSpec& ace : security.root_spec.aces) {
            acl.aces.push_back(InheritedAce(ace.trustee, ace.mask, directory));
        }
        BOOST_CHECK(!InheritedOnlyRefusal(acl, security.owner, security.root_spec).has_value());

        ObservedAcl other_owner = acl;
        other_owner.owner = sids.admins;
        BOOST_CHECK(InheritedOnlyRefusal(other_owner, security.owner, security.root_spec).has_value());

        ObservedAcl no_owner = acl;
        no_owner.owner.reset();
        BOOST_CHECK(InheritedOnlyRefusal(no_owner, security.owner, security.root_spec).has_value());

        ObservedAcl protected_list = acl;
        protected_list.is_protected = true;
        BOOST_CHECK(InheritedOnlyRefusal(protected_list, security.owner, security.root_spec).has_value());

        ObservedAcl no_dacl = acl;
        no_dacl.dacl_present = false;
        no_dacl.aces.clear();
        BOOST_CHECK(InheritedOnlyRefusal(no_dacl, security.owner, security.root_spec).has_value());

        // An entry of its own, even one the root also passes down.
        ObservedAcl explicit_entry = acl;
        explicit_entry.aces.back().inherited = false;
        BOOST_CHECK(InheritedOnlyRefusal(explicit_entry, security.owner, security.root_spec).has_value());

        ObservedAcl extra = acl;
        extra.aces.push_back(InheritedAce(SidOf(USER), FILE_ALL_ACCESS, directory));
        BOOST_CHECK(InheritedOnlyRefusal(extra, security.owner, security.root_spec).has_value());

        ObservedAcl missing = acl;
        missing.aces.pop_back();
        BOOST_CHECK(InheritedOnlyRefusal(missing, security.owner, security.root_spec).has_value());

        ObservedAcl narrower = acl;
        narrower.aces.back().mask = FILE_READ_DATA;
        BOOST_CHECK(InheritedOnlyRefusal(narrower, security.owner, security.root_spec).has_value());

        // A deny entry is not a grant, and does not count.
        ObservedAcl with_deny = acl;
        ObservedAce deny = InheritedAce(SidOf(USER), FILE_ALL_ACCESS, directory);
        deny.kind = winacl::AceKind::Deny;
        with_deny.aces.push_back(deny);
        BOOST_CHECK(!InheritedOnlyRefusal(with_deny, security.owner, security.root_spec).has_value());
    }

    // An entry the root does not pass down is not expected below it.
    AclSpec root{sids.admins, {{sids.admins, FILE_ALL_ACCESS, Inherit::ContainerAndObject},
                               {sids.system, FILE_ALL_ACCESS, Inherit::None}}};
    ObservedAcl below;
    below.owner = sids.svc_main;
    below.dacl_present = true;
    below.aces.push_back(InheritedAce(sids.admins, FILE_ALL_ACCESS, false));
    BOOST_CHECK(!InheritedOnlyRefusal(below, sids.svc_main, root).has_value());
    below.aces.push_back(InheritedAce(sids.system, FILE_ALL_ACCESS, false));
    BOOST_CHECK(InheritedOnlyRefusal(below, sids.svc_main, root).has_value());
}

BOOST_AUTO_TEST_CASE(state_file_text_reads_back)
{
    const std::string text = StateFileText(USER, "C:\\Users\\svcsec-b\\AppData\\Roaming\\GridcoinResearch",
                                           "2026-11-06T12:00:00Z", true);
    BOOST_CHECK_EQUAL(text, std::string("user_sid=") + USER +
                                "\nfrom=C:\\Users\\svcsec-b\\AppData\\Roaming\\GridcoinResearch\n"
                                "enabled_at=2026-11-06T12:00:00Z\ntask_disabled=1\n");
    BOOST_CHECK(StateFileValue(text, "user_sid") == std::optional<std::string>(USER));
    BOOST_CHECK(StateFileValue(text, "task_disabled") == std::optional<std::string>("1"));
    BOOST_CHECK(StateFileValue(StateFileText(USER, "x", "y", false), "task_disabled") ==
                std::optional<std::string>("0"));
}

BOOST_AUTO_TEST_CASE(state_file_spec_is_accepted_as_written)
{
    const LayoutSids sids = Sids();
    const AclSpec spec = StateFileSpec(sids);
    BOOST_CHECK(spec.owner == sids.admins);

    // The file as Windows reads it back after creating it with the spec.
    ObservedAcl acl;
    acl.owner = spec.owner;
    acl.dacl_present = true;
    acl.is_protected = true;
    for (const AceSpec& ace : spec.aces) {
        BOOST_CHECK(ace.inherit == Inherit::None);
        ObservedAce observed;
        observed.kind = winacl::AceKind::Allow;
        observed.trustee = ace.trustee;
        observed.mask = ace.mask;
        acl.aces.push_back(observed);
    }
    BOOST_CHECK(!winacl::CheckMatchesSpec(acl, spec).has_value());
    BOOST_CHECK(!StateFileRefusal(acl).has_value());

    // Owned by the user, as one the user planted would be, it is refused.
    acl.owner = SidOf(USER);
    BOOST_CHECK(StateFileRefusal(acl).has_value());
}

BOOST_AUTO_TEST_CASE(core_image_names)
{
    BOOST_CHECK(IsCoreImageName(L"gridcoinresearchd.exe"));
    BOOST_CHECK(IsCoreImageName(L"gridcoinresearch.exe"));
    BOOST_CHECK(IsCoreImageName(L"GridcoinResearchD.EXE"));
    BOOST_CHECK(!IsCoreImageName(L"gridcoin-service.exe"));
    BOOST_CHECK(!IsCoreImageName(L"gridcoinresearchd"));
    BOOST_CHECK(!IsCoreImageName(L"xgridcoinresearchd.exe"));
    BOOST_CHECK(!IsCoreImageName(L""));
}

BOOST_AUTO_TEST_SUITE_END()
