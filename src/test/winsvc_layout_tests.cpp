// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "ipc/win_endpoint.h"
#include "tinyformat.h"
#include "util/win_acl.h"
#include "winsvc/layout.h"
#include "winsvc/scm.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace winsvc;
namespace winacl = util::winacl;
using winacl::AceSpec;
using winacl::Inherit;

namespace {

// The single rights by their bare names, which are <windows.h>'s macros where it was included
// first (see util/win_acl.h).
using namespace util::winacl::mask;

Sid SidOf(const std::string& text)
{
    const std::optional<Sid> sid = winacl::ParseSidString(text);
    BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
    return *sid;
}

//! \p text with every character outside printable ASCII as \uXXXX, for a message.
std::string Shown(const std::wstring& text)
{
    std::string shown;
    for (const wchar_t c : text) {
        if (c >= 0x20 && c < 0x7f) {
            shown += static_cast<char>(c);
        } else {
            shown += strprintf("\\u%04x", static_cast<unsigned>(c));
        }
    }
    return shown;
}

bool Contains(const std::wstring& text, const std::wstring& part)
{
    return text.find(part) != std::wstring::npos;
}

bool Contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

fs::path ProgramData()
{
    return fs::path(L"C:\\ProgramData");
}

fs::path DaemonExe()
{
    return fs::path(L"C:\\Program Files\\GridcoinResearch\\daemon\\gridcoinresearchd.exe");
}

//! NT SERVICE\gridcoinresearchd and NT SERVICE\gridcointestnetd, as `sc showsid` gives them.
const char* const SVC_MAIN = "S-1-5-80-3522172824-109097731-499206491-3348895147-1114477730";
const char* const SVC_TEST = "S-1-5-80-2622462711-3254838260-194753155-2927861636-1882283010";

LayoutSids Sids()
{
    LayoutSids sids;
    sids.admins = winacl::wellknown::Administrators();
    sids.system = winacl::wellknown::System();
    sids.svc_main = SidOf(SVC_MAIN);
    sids.svc_test = SidOf(SVC_TEST);
    sids.grp_main = SidOf("S-1-5-21-1004336348-1177238915-682003330-1001");
    sids.grp_test = SidOf("S-1-5-21-1004336348-1177238915-682003330-1002");
    return sids;
}

std::vector<LayoutEntry> Layout()
{
    return ServiceLayout(ProgramData(), Sids());
}

LayoutEntry EntryAt(const std::vector<LayoutEntry>& layout, const fs::path& path)
{
    const auto it = std::find_if(layout.begin(), layout.end(),
                                 [&path](const LayoutEntry& entry) { return entry.path == path; });
    BOOST_REQUIRE_MESSAGE(it != layout.end(), "no layout entry for " << path.string());
    return *it;
}

std::vector<AceSpec> AcesFor(const LayoutEntry& entry, const Sid& trustee)
{
    std::vector<AceSpec> aces;
    for (const AceSpec& ace : entry.acl.aces) {
        if (ace.trustee == trustee) aces.push_back(ace);
    }
    return aces;
}

//! The one entry for \p trustee on \p entry; fails the case when there is none or more than one.
AceSpec OnlyAceFor(const LayoutEntry& entry, const Sid& trustee)
{
    const std::vector<AceSpec> aces = AcesFor(entry, trustee);
    BOOST_REQUIRE_MESSAGE(aces.size() == 1, entry.path.string() << " has " << aces.size() << " entries for "
                                                                << winacl::FormatSid(trustee) << ", not one");
    return aces.front();
}

//! What \p trustee may do at \p path by the layout: the rights of its entries there when \p path
//! is a layout entry, otherwise the rights its inherited entries on the nearest layout entry above
//! \p path pass down. Every layout entry is protected, so nothing above that one counts.
uint32_t GrantedTo(const std::vector<LayoutEntry>& layout, const fs::path& path, const Sid& trustee)
{
    for (fs::path at = path; !at.empty(); at = at.parent_path()) {
        const auto it = std::find_if(layout.begin(), layout.end(),
                                     [&at](const LayoutEntry& entry) { return entry.path == at; });
        if (it == layout.end()) {
            if (!at.has_parent_path() || at.parent_path() == at) break;
            continue;
        }

        uint32_t granted = 0;
        for (const AceSpec& ace : it->acl.aces) {
            if (ace.trustee != trustee) continue;
            if (at == path || ace.inherit == Inherit::ContainerAndObject) granted |= ace.mask;
        }
        return granted;
    }
    return 0;
}

std::vector<Network> Networks()
{
    return {Network::Main, Network::Test};
}

const Sid& ServiceOf(const LayoutSids& sids, Network net)
{
    return net == Network::Main ? sids.svc_main : sids.svc_test;
}

const Sid& OtherServiceOf(const LayoutSids& sids, Network net)
{
    return net == Network::Main ? sids.svc_test : sids.svc_main;
}

const Sid& GroupOf(const LayoutSids& sids, Network net)
{
    return net == Network::Main ? sids.grp_main : sids.grp_test;
}

const Sid& OtherGroupOf(const LayoutSids& sids, Network net)
{
    return net == Network::Main ? sids.grp_test : sids.grp_main;
}

//! Every right that changes a file or what a directory holds.
constexpr uint32_t ANY_WRITE = FILE_WRITEISH | FILE_DELETE_CHILD | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY;
//! The rights compared below as uint32_t: the SDK's macros are signed.
constexpr uint32_t FULL_ACCESS = FILE_ALL_ACCESS;
//! What boost::filesystem opens a path with to read its status (and canonical() to resolve it).
constexpr uint32_t BOOST_STAT = FILE_READ_ATTRIBUTES | FILE_READ_EA;
//! The rights on Gridcoin\ and on Gridcoin\ipc: their attributes, as a status query reads them,
//! and their access lists.
constexpr uint32_t STAT_AND_ACL = READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE;

} // namespace

BOOST_AUTO_TEST_SUITE(winsvc_layout_tests)

BOOST_AUTO_TEST_CASE(names_for_each_network)
{
    const NetworkNames main = NamesFor(Network::Main);
    BOOST_CHECK(main.service == L"gridcoinresearchd");
    BOOST_CHECK(main.display == L"Gridcoin Research core (mainnet)");
    BOOST_CHECK(main.account == L"NT SERVICE\\gridcoinresearchd");
    BOOST_CHECK(main.group == L"Gridcoin GUI Users");
    BOOST_CHECK(main.conf_file == L"mainnet.conf");
    BOOST_CHECK(main.datadir_leaf == L"mainnet");
    BOOST_CHECK(main.ipc_leaf == L"mainnet");
    BOOST_CHECK(!main.testnet);

    const NetworkNames test = NamesFor(Network::Test);
    BOOST_CHECK(test.service == L"gridcointestnetd");
    BOOST_CHECK(test.display == L"Gridcoin Research core (testnet)");
    BOOST_CHECK(test.account == L"NT SERVICE\\gridcointestnetd");
    BOOST_CHECK(test.group == L"Gridcoin Testnet GUI Users");
    BOOST_CHECK(test.conf_file == L"testnet.conf");
    BOOST_CHECK(test.datadir_leaf == L"testnet");
    BOOST_CHECK(test.ipc_leaf == L"testnet");
    BOOST_CHECK(test.testnet);
}

BOOST_AUTO_TEST_CASE(layout_paths)
{
    const fs::path pd = ProgramData();
    const fs::path root = pd / "Gridcoin";

    BOOST_CHECK(GridcoinRoot(pd) == root);
    BOOST_CHECK(DataDir(pd, Network::Main) == root / "mainnet");
    BOOST_CHECK(DataDir(pd, Network::Test) == root / "testnet");
    BOOST_CHECK(NetDataDir(pd, Network::Main) == root / "mainnet");
    BOOST_CHECK(NetDataDir(pd, Network::Test) == root / "testnet" / "testnet");
    BOOST_CHECK(IpcDir(pd, Network::Main) == root / "ipc" / "mainnet");
    BOOST_CHECK(IpcDir(pd, Network::Test) == root / "ipc" / "testnet");
    BOOST_CHECK(ConfPath(pd, Network::Main) == root / "conf" / "mainnet.conf");
    BOOST_CHECK(ConfPath(pd, Network::Test) == root / "conf" / "testnet.conf");
    BOOST_CHECK(QuarantineDir(pd) == root / "conf" / "quarantine");
    BOOST_CHECK(StatePath(pd, Network::Main) == root / "conf" / "mainnet.state");
    BOOST_CHECK(StatePath(pd, Network::Test) == root / "conf" / "testnet.state");
}

BOOST_AUTO_TEST_CASE(layout_entries_in_order)
{
    const fs::path pd = ProgramData();
    const fs::path root = GridcoinRoot(pd);
    const std::vector<std::pair<fs::path, bool>> expected{
        {root, true},
        {root / "conf", true},
        {ConfPath(pd, Network::Main), false},
        {ConfPath(pd, Network::Test), false},
        {QuarantineDir(pd), true},
        {DataDir(pd, Network::Main), true},
        {DataDir(pd, Network::Test), true},
        {root / "ipc", true},
        {IpcDir(pd, Network::Main), true},
        {IpcDir(pd, Network::Test), true},
    };

    const std::vector<LayoutEntry> layout = Layout();
    BOOST_REQUIRE_EQUAL(layout.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        BOOST_CHECK_MESSAGE(layout[i].path == expected[i].first, "entry " << i << " is " << layout[i].path.string()
                                                                           << ", not " << expected[i].first.string());
        BOOST_CHECK_EQUAL(layout[i].directory, expected[i].second);
    }
}

BOOST_AUTO_TEST_CASE(layout_parent_first_protected)
{
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();
    const fs::path root = GridcoinRoot(ProgramData());

    for (size_t i = 0; i < layout.size(); ++i) {
        const LayoutEntry& entry = layout[i];

        // Every entry but the root is inside one listed before it, which is a directory.
        if (entry.path != root) {
            const auto parent = std::find_if(layout.begin(), layout.begin() + i, [&entry](const LayoutEntry& e) {
                return e.path == entry.path.parent_path();
            });
            BOOST_CHECK_MESSAGE(parent != layout.begin() + i, entry.path.string() << " comes before its parent");
            if (parent != layout.begin() + i) BOOST_CHECK(parent->directory);
        }

        // An explicit owner, Administrators, and allow entries (the only kind an AclSpec holds)
        // that each name a trustee, grant something, and name a trustee once.
        BOOST_CHECK_MESSAGE(entry.acl.owner == sids.admins, entry.path.string() << " is not owned by Administrators");
        for (const AceSpec& ace : entry.acl.aces) {
            BOOST_CHECK(!winacl::FormatSid(ace.trustee).empty());
            BOOST_CHECK(ace.mask != 0);
            BOOST_CHECK_EQUAL(AcesFor(entry, ace.trustee).size(), 1U);
        }

        // Administrators and SYSTEM have full access everywhere.
        BOOST_CHECK_EQUAL(OnlyAceFor(entry, sids.admins).mask, FULL_ACCESS);
        BOOST_CHECK_EQUAL(OnlyAceFor(entry, sids.system).mask, FULL_ACCESS);

        // A file's entries are not inherited.
        if (!entry.directory) {
            for (const AceSpec& ace : entry.acl.aces) BOOST_CHECK(ace.inherit == Inherit::None);
        }
    }
}

BOOST_AUTO_TEST_CASE(layout_datadir_grants_only_its_service)
{
    const fs::path pd = ProgramData();
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();

    for (const Network net : Networks()) {
        const LayoutEntry datadir = EntryAt(layout, DataDir(pd, net));
        BOOST_CHECK(datadir.directory);
        BOOST_CHECK_EQUAL(datadir.acl.aces.size(), 3U);

        const AceSpec svc = OnlyAceFor(datadir, ServiceOf(sids, net));
        BOOST_CHECK_EQUAL(svc.mask, FULL_ACCESS);
        BOOST_CHECK(svc.inherit == Inherit::ContainerAndObject);
        BOOST_CHECK(OnlyAceFor(datadir, sids.admins).inherit == Inherit::ContainerAndObject);
        BOOST_CHECK(OnlyAceFor(datadir, sids.system).inherit == Inherit::ContainerAndObject);

        BOOST_CHECK(AcesFor(datadir, OtherServiceOf(sids, net)).empty());
        BOOST_CHECK(AcesFor(datadir, sids.grp_main).empty());
        BOOST_CHECK(AcesFor(datadir, sids.grp_test).empty());
    }
}

BOOST_AUTO_TEST_CASE(layout_ipc_net_dir_nothing_inheritable)
{
    const fs::path pd = ProgramData();
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();

    for (const Network net : Networks()) {
        const LayoutEntry ipcdir = EntryAt(layout, IpcDir(pd, net));
        BOOST_CHECK_EQUAL(ipcdir.acl.aces.size(), 4U);
        for (const AceSpec& ace : ipcdir.acl.aces) {
            BOOST_CHECK_MESSAGE(ace.inherit == Inherit::None,
                                ipcdir.path.string() << ": " << winacl::FormatSid(ace.trustee) << " is inherited");
        }

        BOOST_CHECK_EQUAL(OnlyAceFor(ipcdir, ServiceOf(sids, net)).mask,
                          ipc::win::DIR_VIEW_MASK | FILE_READ_EA | FILE_ADD_FILE | FILE_DELETE_CHILD);
        BOOST_CHECK_EQUAL(IPC_SERVICE_MASK, ipc::win::DIR_VIEW_MASK | FILE_READ_EA | FILE_ADD_FILE | FILE_DELETE_CHILD);
        BOOST_CHECK_EQUAL(OnlyAceFor(ipcdir, GroupOf(sids, net)).mask, ipc::win::DIR_VIEW_MASK);
        BOOST_CHECK(AcesFor(ipcdir, OtherServiceOf(sids, net)).empty());
        BOOST_CHECK(AcesFor(ipcdir, OtherGroupOf(sids, net)).empty());
    }
}

BOOST_AUTO_TEST_CASE(layout_conf_readable_only_by_its_service)
{
    const fs::path pd = ProgramData();
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();

    for (const Network net : Networks()) {
        const LayoutEntry conf = EntryAt(layout, ConfPath(pd, net));
        BOOST_CHECK(!conf.directory);
        BOOST_CHECK_EQUAL(conf.acl.aces.size(), 3U);

        const AceSpec svc = OnlyAceFor(conf, ServiceOf(sids, net));
        BOOST_CHECK_EQUAL(svc.mask & ANY_WRITE, 0U);
        BOOST_CHECK(svc.mask & FILE_READ_DATA);

        BOOST_CHECK(AcesFor(conf, OtherServiceOf(sids, net)).empty());
        BOOST_CHECK(AcesFor(conf, sids.grp_main).empty());
        BOOST_CHECK(AcesFor(conf, sids.grp_test).empty());
    }

    // The directory that holds them, and the quarantine, are the administrators' alone.
    for (const fs::path& dir : {GridcoinRoot(pd) / "conf", QuarantineDir(pd)}) {
        const LayoutEntry entry = EntryAt(layout, dir);
        BOOST_CHECK_EQUAL(entry.acl.aces.size(), 2U);
        for (const AceSpec& ace : entry.acl.aces) BOOST_CHECK(ace.inherit == Inherit::ContainerAndObject);
    }
}

// Ruling 1: a GENERIC_READ open of the conf, as the core's, needs FILE_READ_EA too.
BOOST_AUTO_TEST_CASE(layout_conf_grants_generic_read)
{
    const fs::path pd = ProgramData();
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();

    // FILE_GENERIC_READ: STANDARD_RIGHTS_READ, FILE_READ_DATA, FILE_READ_ATTRIBUTES, FILE_READ_EA
    // and SYNCHRONIZE.
    BOOST_CHECK_EQUAL(CONF_READ_MASK, 0x120089U);

    for (const Network net : Networks()) {
        const AceSpec svc = OnlyAceFor(EntryAt(layout, ConfPath(pd, net)), ServiceOf(sids, net));
        BOOST_CHECK_EQUAL(svc.mask, CONF_READ_MASK);
        BOOST_CHECK(svc.mask & FILE_READ_EA);
        BOOST_CHECK(svc.inherit == Inherit::None);
    }
}

BOOST_AUTO_TEST_CASE(layout_groups_view_only)
{
    const fs::path pd = ProgramData();
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();

    std::vector<std::pair<fs::path, const Sid*>> seen;
    for (const LayoutEntry& entry : layout) {
        for (const AceSpec& ace : entry.acl.aces) {
            if (ace.trustee != sids.grp_main && ace.trustee != sids.grp_test) continue;
            BOOST_CHECK_MESSAGE(ace.mask == ipc::win::DIR_VIEW_MASK && ace.inherit == Inherit::None,
                                entry.path.string() << ": a group entry that is not DIR_VIEW_MASK alone");
            BOOST_CHECK(entry.directory);
            seen.emplace_back(entry.path, ace.trustee == sids.grp_main ? &sids.grp_main : &sids.grp_test);
        }
    }

    // Where the groups appear: Gridcoin and ipc for both, and each network's IPC directory for
    // its own.
    const fs::path root = GridcoinRoot(pd);
    const std::vector<std::pair<fs::path, const Sid*>> expected{
        {root, &sids.grp_main},
        {root, &sids.grp_test},
        {root / "ipc", &sids.grp_main},
        {root / "ipc", &sids.grp_test},
        {IpcDir(pd, Network::Main), &sids.grp_main},
        {IpcDir(pd, Network::Test), &sids.grp_test},
    };
    BOOST_CHECK(seen == expected);
}

// Ruling 2: every path the core's startup check opens (fsmodes::CheckOwnerOnlyModes, with
// READ_CONTROL | FILE_READ_ATTRIBUTES), and the IPC directory the core checks before it serves
// (ServerIpcDirRefusal, the same rights), lets the service read its attributes and access list.
BOOST_AUTO_TEST_CASE(layout_startup_check_paths_readable)
{
    const fs::path pd = ProgramData();
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();
    constexpr uint32_t opened = READ_CONTROL | FILE_READ_ATTRIBUTES;

    for (const Network net : Networks()) {
        const fs::path netdir = NetDataDir(pd, net);
        const std::vector<fs::path> checked{
            netdir,
            netdir / "wallet.dat",
            netdir / "database",
            netdir / "database" / "log.0000000001",
            ConfPath(pd, net),
            netdir / "gridcoinsettings.json",
            netdir / "walletbackups",
            netdir / "walletbackups" / "wallet-2026-11-06T00-00-00.dat",
            netdir / "debug.log",
            DataDir(pd, net),
            IpcDir(pd, net),
        };

        for (const fs::path& path : checked) {
            const uint32_t granted = GrantedTo(layout, path, ServiceOf(sids, net));
            BOOST_CHECK_MESSAGE((granted & opened) == opened,
                                path.string() << ": the service gets " << strprintf("0x%08x", granted)
                                              << ", without READ_CONTROL and FILE_READ_ATTRIBUTES");
        }

        // The other network's service reaches none of them except through Gridcoin itself.
        for (const fs::path& path : checked) {
            BOOST_CHECK_EQUAL(GrantedTo(layout, path, OtherServiceOf(sids, net)), 0U);
        }

        // The directory above the IPC directory, which the core's preparation of that directory
        // makes sure exists (MakeIpcDirectory: create_directories reads its status), shared by
        // both networks' IPC directories.
        const fs::path ipc_parent = IpcDir(pd, net).parent_path();
        BOOST_CHECK_MESSAGE(GrantedTo(layout, ipc_parent, ServiceOf(sids, net)) & FILE_READ_ATTRIBUTES,
                            ipc_parent.string() << ": the service cannot read its attributes");
    }
}

// Ruling 3, as amended in fix round 1: the service's autounlock check reads the access list of
// every directory above its file (FindForeignWriterOnDataChain), Gridcoin\ included, and the
// creation path of each data directory reads Gridcoin\'s status at every start (a boost status
// query opens it with FILE_READ_ATTRIBUTES | FILE_READ_EA). So each service has READ_CONTROL |
// FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE there, and nothing that lists or changes it.
BOOST_AUTO_TEST_CASE(layout_root_lets_each_service_read_its_access_list)
{
    const fs::path pd = ProgramData();
    const fs::path root = GridcoinRoot(pd);
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();

    BOOST_CHECK_EQUAL(ROOT_SERVICE_MASK, STAT_AND_ACL);
    for (const Network net : Networks()) {
        const AceSpec svc = OnlyAceFor(EntryAt(layout, root), ServiceOf(sids, net));
        BOOST_CHECK_EQUAL(svc.mask, STAT_AND_ACL);
        BOOST_CHECK(svc.inherit == Inherit::None);
    }

    // Every directory on the way from the default autounlock file up to Gridcoin\ lets the
    // service read its access list. Those above Gridcoin\ are Windows' own.
    for (const Network net : Networks()) {
        const fs::path blob = NetDataDir(pd, net) / "autounlock" / "passphrase.dpapi";
        size_t levels = 0;
        for (fs::path dir = blob.parent_path(); dir != pd; dir = dir.parent_path()) {
            BOOST_REQUIRE(!dir.empty());
            BOOST_CHECK_MESSAGE(GrantedTo(layout, dir, ServiceOf(sids, net)) & READ_CONTROL,
                                dir.string() << ": the service cannot read its access list");
            ++levels;
        }
        // autounlock, the data directory (and the network one below it for Test), Gridcoin.
        BOOST_CHECK_EQUAL(levels, net == Network::Main ? 3U : 4U);
    }

    // Nothing on Gridcoin\ is passed down to the service: what it has in the tree comes from the
    // entries below. On ipc it has its own entry, for the status the core reads there, and nothing
    // there is passed down either.
    BOOST_CHECK_EQUAL(GrantedTo(layout, root / "conf", sids.svc_main), 0U);
    BOOST_CHECK_EQUAL(GrantedTo(layout, root / "elsewhere", sids.svc_test), 0U);
    BOOST_CHECK_EQUAL(IPC_PARENT_SERVICE_MASK, STAT_AND_ACL);
    for (const Network net : Networks()) {
        const AceSpec svc = OnlyAceFor(EntryAt(layout, root / "ipc"), ServiceOf(sids, net));
        BOOST_CHECK_EQUAL(svc.mask, IPC_PARENT_SERVICE_MASK);
        BOOST_CHECK(svc.inherit == Inherit::None);
        BOOST_CHECK_EQUAL(GrantedTo(layout, root / "ipc" / "elsewhere", ServiceOf(sids, net)), 0U);
    }
}

// I-1 and the start-path table: every call the core's -service -multiprocess start makes on a
// layout path, with the rights it needs there. Each must be granted by the layout itself.
BOOST_AUTO_TEST_CASE(layout_start_path_calls_succeed)
{
    const fs::path pd = ProgramData();
    const fs::path root = GridcoinRoot(pd);
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();

    for (const Network net : Networks()) {
        const fs::path ipcdir = IpcDir(pd, net);
        const std::vector<std::pair<fs::path, uint32_t>> calls{
            // CreateOwnerOnlyDirectory(<datadir>), from GetDataDirPath: create_directories(Gridcoin\).
            {root, BOOST_STAT},
            // The autounlock chain check (GetNamedSecurityInfoW).
            {root, READ_CONTROL},
            // CheckDataDirOption and GetDataDirPath: is_directory(-datadir); canonical() of it in
            // AttachGroupIpcDirRefusal; the file ID in DataDirectoryIds.
            {DataDir(pd, net), BOOST_STAT | READ_CONTROL},
            // The network data directory and everything the core makes in it.
            {NetDataDir(pd, net), FULL_ACCESS},
            // MakeIpcDirectory: create_directories(Gridcoin\ipc).
            {ipcdir.parent_path(), BOOST_STAT},
            // canonical(-ipcdir) in AttachGroupIpcDirRefusal; CreateDirectoryWithAcl's open of an
            // existing directory; OpenNoFollow in ApplyAttachGroupDirectoryAccess and
            // ServerIpcDirRefusal.
            {ipcdir, BOOST_STAT | READ_CONTROL},
            // bind() makes node.sock; the cookie is written beside it and renamed over the old one;
            // a stale socket is removed.
            {ipcdir, FILE_ADD_FILE | FILE_DELETE_CHILD},
            // ReadConfigFiles (_wfopen "r"), CleanConfig and IsConfigFileEmpty (ifstream).
            {ConfPath(pd, net), CONF_READ_MASK},
        };

        for (const auto& [path, needed] : calls) {
            const uint32_t granted = GrantedTo(layout, path, ServiceOf(sids, net));
            BOOST_CHECK_MESSAGE((granted & needed) == needed,
                                path.string() << ": the service gets " << strprintf("0x%08x", granted)
                                              << ", not all of " << strprintf("0x%08x", needed));
        }
    }
}

BOOST_AUTO_TEST_CASE(virtual_service_sid_vectors)
{
    // TrustedInstaller is the one service whose SID Windows also names as a well-known account.
    const std::optional<Sid> trusted_installer = VirtualServiceSid(L"TrustedInstaller");
    BOOST_REQUIRE(trusted_installer.has_value());
    BOOST_CHECK(*trusted_installer == winacl::wellknown::TrustedInstaller());

    BOOST_CHECK(VirtualServiceSid(L"gridcoinresearchd") == SidOf(SVC_MAIN));
    BOOST_CHECK(VirtualServiceSid(L"gridcointestnetd") == SidOf(SVC_TEST));
    BOOST_CHECK(VirtualServiceSid(L"GridcoinResearchD") == SidOf(SVC_MAIN));
    BOOST_CHECK(VirtualServiceSid(NamesFor(Network::Main).service) == SidOf(SVC_MAIN));

    BOOST_CHECK(!VirtualServiceSid(L"").has_value());
    BOOST_CHECK(!VirtualServiceSid(L"gridcoin\u00e9").has_value());
}

BOOST_AUTO_TEST_CASE(quote_arg_vectors)
{
    const std::vector<std::pair<std::wstring, std::wstring>> vectors{
        {L"C:\\Program Files\\x", L"\"C:\\Program Files\\x\""},
        {L"a\"b", L"\"a\\\"b\""},
        {L"C:\\my dir\\", L"\"C:\\my dir\\\\\""},
        {L"C:\\my dir\\\\", L"\"C:\\my dir\\\\\\\\\""},
        {L"a\\\"b", L"\"a\\\\\\\"b\""},
        {L"", L"\"\""},
        {L"plain", L"plain"},
        {L"C:\\plain\\path\\", L"C:\\plain\\path\\"},
        {L"tab\there", L"\"tab\there\""},
        {L"C:\\Users\\Jos\u00e9 Garc\u00eda\\x", L"\"C:\\Users\\Jos\u00e9 Garc\u00eda\\x\""},
        {L"C:\\Users\\Jos\u00e9\\x", L"C:\\Users\\Jos\u00e9\\x"},
    };

    for (const auto& [arg, quoted] : vectors) {
        BOOST_CHECK_MESSAGE(QuoteArg(arg) == quoted,
                            "QuoteArg(" << Shown(arg) << ") is " << Shown(QuoteArg(arg)) << ", not " << Shown(quoted));
    }
}

BOOST_AUTO_TEST_CASE(image_path_mainnet_exact)
{
    const std::wstring expected =
        L"\"C:\\Program Files\\GridcoinResearch\\daemon\\gridcoinresearchd.exe\" -service -multiprocess "
        L"-conf=\"C:\\ProgramData\\Gridcoin\\conf\\mainnet.conf\" -datadir=\"C:\\ProgramData\\Gridcoin\\mainnet\" "
        L"-ipcdir=\"C:\\ProgramData\\Gridcoin\\ipc\\mainnet\" -ipcattachgroup=\"Gridcoin GUI Users\" -autounlockblob";

    const std::wstring image_path = BuildImagePath(DaemonExe(), Network::Main, ProgramData());
    BOOST_CHECK_MESSAGE(image_path == expected, Shown(image_path));
}

BOOST_AUTO_TEST_CASE(image_path_testnet_flags)
{
    const std::wstring image_path = BuildImagePath(DaemonExe(), Network::Test, ProgramData());

    BOOST_CHECK(Contains(image_path, L" -testnet"));
    BOOST_CHECK(Contains(image_path, L"testnet.conf"));
    BOOST_CHECK(Contains(image_path, L"ipc\\testnet"));
    BOOST_CHECK(Contains(image_path, L"\"Gridcoin Testnet GUI Users\""));
    BOOST_CHECK(!Contains(image_path, L"mainnet"));

    const std::wstring expected =
        L"\"C:\\Program Files\\GridcoinResearch\\daemon\\gridcoinresearchd.exe\" -service -multiprocess -testnet "
        L"-conf=\"C:\\ProgramData\\Gridcoin\\conf\\testnet.conf\" -datadir=\"C:\\ProgramData\\Gridcoin\\testnet\" "
        L"-ipcdir=\"C:\\ProgramData\\Gridcoin\\ipc\\testnet\" -ipcattachgroup=\"Gridcoin Testnet GUI Users\" "
        L"-autounlockblob";
    BOOST_CHECK_MESSAGE(image_path == expected, Shown(image_path));
}

// Review Focus 2: a custom install folder with a space, a non-ASCII name and a ProgramData moved
// to another drive are quoted whole, never split.
BOOST_AUTO_TEST_CASE(image_path_quotes_spaced_and_non_ascii_paths)
{
    const fs::path exe(L"D:\\Apps\\Grid coin\\daemon\\gridcoinresearchd.exe");
    const fs::path spaced(L"E:\\Data\\Grid coin");

    std::wstring image_path = BuildImagePath(exe, Network::Main, spaced);
    BOOST_CHECK(image_path.rfind(L"\"D:\\Apps\\Grid coin\\daemon\\gridcoinresearchd.exe\" ", 0) == 0);
    BOOST_CHECK(Contains(image_path, L"-conf=\"E:\\Data\\Grid coin\\Gridcoin\\conf\\mainnet.conf\""));
    BOOST_CHECK(Contains(image_path, L"-datadir=\"E:\\Data\\Grid coin\\Gridcoin\\mainnet\""));
    BOOST_CHECK(Contains(image_path, L"-ipcdir=\"E:\\Data\\Grid coin\\Gridcoin\\ipc\\mainnet\""));

#ifdef WIN32
    // Elsewhere a path converts between wide and narrow text through the process
    // locale, which a test run may leave at "C", and the image path is built only
    // on Windows.
    const fs::path pd(L"E:\\Data\\Jos\u00e9 Garc\u00eda");

    image_path = BuildImagePath(exe, Network::Main, pd);
    BOOST_CHECK(Contains(image_path, L"-conf=\"E:\\Data\\Jos\u00e9 Garc\u00eda\\Gridcoin\\conf\\mainnet.conf\""));
    BOOST_CHECK(Contains(image_path, L"-datadir=\"E:\\Data\\Jos\u00e9 Garc\u00eda\\Gridcoin\\mainnet\""));
    BOOST_CHECK(Contains(image_path, L"-ipcdir=\"E:\\Data\\Jos\u00e9 Garc\u00eda\\Gridcoin\\ipc\\mainnet\""));
#endif
}

BOOST_AUTO_TEST_CASE(image_path_executable_is_the_daemon)
{
    const std::optional<std::wstring> exe =
        ImagePathExecutable(BuildImagePath(DaemonExe(), Network::Test, ProgramData()));
    BOOST_REQUIRE(exe.has_value());
    BOOST_CHECK(*exe == L"C:\\Program Files\\GridcoinResearch\\daemon\\gridcoinresearchd.exe");

    BOOST_CHECK(ImagePathExecutable(L"C:\\Gridcoin\\gridcoinresearchd.exe -service") ==
                std::optional<std::wstring>(L"C:\\Gridcoin\\gridcoinresearchd.exe"));
    BOOST_CHECK(ImagePathExecutable(L"C:\\x.exe") == std::optional<std::wstring>(L"C:\\x.exe"));
    BOOST_CHECK(!ImagePathExecutable(L"").has_value());
    BOOST_CHECK(!ImagePathExecutable(L"\"C:\\Program Files\\x.exe -service").has_value());
}

BOOST_AUTO_TEST_CASE(service_conf_contents)
{
    const std::string main = BuildServiceConf(Network::Main, "PW");
    BOOST_CHECK_EQUAL(main,
                      "# Generated by gridcoin-service.exe install. Owned by Administrators; the service can only read "
                      "it.\n"
                      "rpcuser=gridcoinrpc\n"
                      "rpcpassword=PW\n"
                      "server=1\n"
                      "rpcserviceuser=NT SERVICE\\gridcoinresearchd\n");

    const std::string test = BuildServiceConf(Network::Test, "PW");
    BOOST_CHECK_EQUAL(test,
                      "# Generated by gridcoin-service.exe install. Owned by Administrators; the service can only read "
                      "it.\n"
                      "rpcuser=gridcoinrpc\n"
                      "rpcpassword=PW\n"
                      "server=1\n"
                      "rpcserviceuser=NT SERVICE\\gridcointestnetd\n"
                      "testnet=1\n");

    for (const std::string& conf : {main, test}) {
        BOOST_CHECK(!Contains(conf, "rpcallowip"));
        BOOST_CHECK(!Contains(conf, "rpcbind"));
        BOOST_CHECK(!Contains(conf, "includeconf"));
        BOOST_CHECK(!Contains(conf, "\r"));
    }
    BOOST_CHECK(!Contains(main, "testnet=1"));
}

BOOST_AUTO_TEST_CASE(existing_conf_with_includeconf_refused)
{
    for (const Network net : Networks()) {
        BOOST_CHECK(ServiceConfAcceptable(BuildServiceConf(net, "PW")));
    }

    const std::vector<std::string> refused{
        "includeconf=other.conf\n",
        "rpcuser=a\n  includeconf=other.conf\n",
        "\tincludeconf=other.conf\r\n",
        "IncludeConf=other.conf\n",
        "INCLUDECONF = other.conf\n",
        "main.includeconf=other.conf\n",
        "test.IncludeConf=other.conf\n",
        "[test]\nincludeconf=other.conf\n",
        "-includeconf=other.conf\n",
        "noincludeconf=1\n",
        "rpcuser=a\nincludeconf=other.conf",
        "includeconf\n",
    };
    for (const std::string& text : refused) {
        BOOST_CHECK_MESSAGE(!ServiceConfAcceptable(text), "accepted: " << text);
    }

    const std::vector<std::string> accepted{
        "",
        "# includeconf=other.conf\n",
        "rpcuser=a # includeconf=other.conf\n",
        "rpcpassword=includeconf\n",
        "includeconfx=1\n",
        "myincludeconf=1\n",
        "[main]\nrpcuser=a\r\n",
    };
    for (const std::string& text : accepted) {
        BOOST_CHECK_MESSAGE(ServiceConfAcceptable(text), "refused: " << text);
    }
}

BOOST_AUTO_TEST_CASE(missing_conf_lines)
{
    // install's own file needs nothing.
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main, BuildServiceConf(Network::Main, "PW"), "NEW"), "");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Test, BuildServiceConf(Network::Test, "PW"), "NEW"), "");

    // A file without any of the three gets all three, after a line break when it lacks one.
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main, "server=1\n", "NEW"),
                      "rpcuser=gridcoinrpc\nrpcpassword=NEW\nrpcserviceuser=NT SERVICE\\gridcoinresearchd\n");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Test, "server=1", "NEW"),
                      "\nrpcuser=gridcoinrpc\nrpcpassword=NEW\nrpcserviceuser=NT SERVICE\\gridcointestnetd\n");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main, "", "NEW"),
                      "rpcuser=gridcoinrpc\nrpcpassword=NEW\nrpcserviceuser=NT SERVICE\\gridcoinresearchd\n");

    // A name set at the top or in the network's section, with spaces or empty, is left as it is.
    // A "test." name under [main] is in the main section, as ArgsManager reads it.
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main,
                                              "rpcuser=admin\n[main]\nrpcpassword = secret\r\n"
                                              "test.rpcserviceuser=NT SERVICE\\other\n",
                                              "NEW"),
                      "[main]\nrpcserviceuser=NT SERVICE\\gridcoinresearchd\n");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main,
                                              "rpcuser=admin\n[main]\nrpcpassword = secret\r\n"
                                              "rpcserviceuser=NT SERVICE\\other\n",
                                              "NEW"),
                      "");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Test,
                                              "test.rpcuser=a\n[test]\nrpcpassword=b\nnorpcserviceuser=1\n", "NEW"),
                      "");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main, "rpcuser=\nrpcserviceuser=x\n", "NEW"),
                      "rpcpassword=NEW\n");

    // A comment does not set a name.
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main, "# rpcpassword=old\nrpcuser=a\nrpcserviceuser=x\n", "NEW"),
                      "rpcpassword=NEW\n");
}

// M-2, the review's first case: a name set only for the other network does not count.
BOOST_AUTO_TEST_CASE(missing_conf_lines_counts_only_the_networks_own_section)
{
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main,
                                              "rpcuser=a\nrpcpassword=b\n[test]\nrpcserviceuser=x\n", "NEW"),
                      "[main]\nrpcserviceuser=NT SERVICE\\gridcoinresearchd\n");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main, "rpcuser=a\nrpcpassword=b\ntest.rpcserviceuser=x\n",
                                              "NEW"),
                      "rpcserviceuser=NT SERVICE\\gridcoinresearchd\n");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Test, "[main]\nrpcuser=a\nrpcpassword=b\nrpcserviceuser=c\n",
                                              "NEW"),
                      "[test]\nrpcuser=gridcoinrpc\nrpcpassword=NEW\nrpcserviceuser=NT SERVICE\\gridcointestnetd\n");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Test, "main.rpcuser=a\nrpcpassword=b\nrpcserviceuser=c\n",
                                              "NEW"),
                      "rpcuser=gridcoinrpc\n");
}

// M-2, the review's second case: the lines go under the network's own header, not into whatever
// section the file ends in.
BOOST_AUTO_TEST_CASE(missing_conf_lines_go_under_the_networks_own_header)
{
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Test, "[main]\nrpcport=1\n", "NEW"),
                      "[test]\nrpcuser=gridcoinrpc\nrpcpassword=NEW\nrpcserviceuser=NT SERVICE\\gridcointestnetd\n");
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Main, "rpcuser=a\n[test]\nrpcport=1", "NEW"),
                      "\n[main]\nrpcpassword=NEW\nrpcserviceuser=NT SERVICE\\gridcoinresearchd\n");
    // Without a header the lines are at the top, where they count for both networks.
    BOOST_CHECK_EQUAL(MissingServiceConfLines(Network::Test, "rpcport=1\n", "NEW"),
                      "rpcuser=gridcoinrpc\nrpcpassword=NEW\nrpcserviceuser=NT SERVICE\\gridcointestnetd\n");
}

BOOST_AUTO_TEST_CASE(state_file_value)
{
    const std::string text = "user_sid=S-1-5-21-1-2-3-1001\r\nfrom=C:\\Users\\a\\AppData\\Roaming\\GridcoinResearch\n"
                             "enabled_at=2026-11-06T05:00:00Z\ntask_disabled=1\nuser_sid=S-1-5-21-9\n";

    BOOST_CHECK(StateFileValue(text, "user_sid") == std::optional<std::string>("S-1-5-21-1-2-3-1001"));
    BOOST_CHECK(StateFileValue(text, "from") ==
                std::optional<std::string>("C:\\Users\\a\\AppData\\Roaming\\GridcoinResearch"));
    BOOST_CHECK(StateFileValue(text, "task_disabled") == std::optional<std::string>("1"));
    BOOST_CHECK(!StateFileValue(text, "user").has_value());
    BOOST_CHECK(!StateFileValue(text, "sid").has_value());
    BOOST_CHECK(!StateFileValue("", "user_sid").has_value());
}

BOOST_AUTO_TEST_CASE(firewall_rule_args_exact)
{
    BOOST_CHECK(FirewallRuleName(Network::Main) == L"Gridcoin service (gridcoinresearchd)");
    BOOST_CHECK(FirewallRuleName(Network::Test) == L"Gridcoin service (gridcointestnetd)");

    BOOST_CHECK(FirewallStagingRuleName(Network::Main) == L"Gridcoin service (gridcoinresearchd) (new)");
    BOOST_CHECK(FirewallStagingRuleName(Network::Test) == L"Gridcoin service (gridcointestnetd) (new)");

    const std::wstring add = FirewallAddRuleArgs(Network::Main, DaemonExe(), FirewallRuleName(Network::Main));
    BOOST_CHECK_MESSAGE(add == L"advfirewall firewall add rule name=\"Gridcoin service (gridcoinresearchd)\" dir=in "
                               L"action=allow "
                               L"program=\"C:\\Program Files\\GridcoinResearch\\daemon\\gridcoinresearchd.exe\" "
                               L"service=gridcoinresearchd protocol=TCP profile=domain,private",
                        Shown(add));

    const std::wstring staged = FirewallAddRuleArgs(Network::Test, DaemonExe(), FirewallStagingRuleName(Network::Test));
    BOOST_CHECK_MESSAGE(staged == L"advfirewall firewall add rule name=\"Gridcoin service (gridcointestnetd) (new)\" "
                                  L"dir=in action=allow "
                                  L"program=\"C:\\Program Files\\GridcoinResearch\\daemon\\gridcoinresearchd.exe\" "
                                  L"service=gridcointestnetd protocol=TCP profile=domain,private",
                        Shown(staged));

    const std::wstring show = FirewallShowRuleArgs(FirewallRuleName(Network::Main));
    BOOST_CHECK_MESSAGE(show == L"advfirewall firewall show rule name=\"Gridcoin service (gridcoinresearchd)\"",
                        Shown(show));

    const std::wstring remove = FirewallDeleteRuleArgs(FirewallRuleName(Network::Test));
    BOOST_CHECK_MESSAGE(remove == L"advfirewall firewall delete rule name=\"Gridcoin service (gridcointestnetd)\"",
                        Shown(remove));

    const std::wstring rename =
        FirewallRenameRuleArgs(FirewallStagingRuleName(Network::Main), FirewallRuleName(Network::Main));
    BOOST_CHECK_MESSAGE(rename == L"advfirewall firewall set rule name=\"Gridcoin service (gridcoinresearchd) (new)\" "
                                  L"new name=\"Gridcoin service (gridcoinresearchd)\"",
                        Shown(rename));
}

// N-3: the firewall steps never leave the machine without a rule when it had one, whichever step
// fails, and end with the one rule under its own name.
BOOST_AUTO_TEST_CASE(firewall_steps_never_delete_the_last_rule)
{
    using Step = FirewallStep;
    BOOST_CHECK(FirewallSteps(false, false) == std::vector<Step>{Step::AddRule});
    BOOST_CHECK(FirewallSteps(true, false) ==
                (std::vector<Step>{Step::AddStaging, Step::DeleteRule, Step::RenameStagingToRule}));
    BOOST_CHECK(FirewallSteps(true, true) == (std::vector<Step>{Step::DeleteStaging, Step::AddStaging,
                                                                Step::DeleteRule, Step::RenameStagingToRule}));
    BOOST_CHECK(FirewallSteps(false, true) ==
                (std::vector<Step>{Step::RenameStagingToRule, Step::AddStaging, Step::DeleteRule,
                                   Step::RenameStagingToRule}));

    // Every start, run to every possible failure: the run stops at the failing step, which
    // changes nothing.
    for (const bool rule_at_start : {false, true}) {
        for (const bool staging_at_start : {false, true}) {
            const std::vector<Step> steps = FirewallSteps(rule_at_start, staging_at_start);
            for (size_t failing = 0; failing <= steps.size(); ++failing) {
                bool rule = rule_at_start;
                bool staging = staging_at_start;
                for (size_t i = 0; i < failing && i < steps.size(); ++i) {
                    switch (steps[i]) {
                    case Step::AddRule: rule = true; break;
                    case Step::AddStaging: staging = true; break;
                    case Step::DeleteRule: rule = false; break;
                    case Step::DeleteStaging: staging = false; break;
                    case Step::RenameStagingToRule:
                        BOOST_CHECK_MESSAGE(staging, "a rename with no staged rule");
                        rule = rule || staging;
                        staging = false;
                        break;
                    }
                    if (rule_at_start || staging_at_start) {
                        BOOST_CHECK_MESSAGE(rule || staging, "no rule left after step " << i << " of a run that "
                                                                                       << "started with "
                                                                                       << rule_at_start << "/"
                                                                                       << staging_at_start);
                    }
                }
                // A run that completes ends with the rule alone.
                if (failing == steps.size()) BOOST_CHECK(rule && !staging);
            }
        }
    }
}

// W-1: status, run by an account the service's security descriptor does not let read its settings
// (a standard user over ssh is not INTERACTIVE), reports that and goes on.
BOOST_AUTO_TEST_CASE(status_classifies_opening_the_service)
{
    BOOST_CHECK(ClassifyServiceOpen(0) == ServiceOpen::Opened);
    BOOST_CHECK(ClassifyServiceOpen(1060) == ServiceOpen::NotInstalled); // ERROR_SERVICE_DOES_NOT_EXIST
    BOOST_CHECK(ClassifyServiceOpen(5) == ServiceOpen::Unreadable);      // ERROR_ACCESS_DENIED
    BOOST_CHECK(ClassifyServiceOpen(6) == ServiceOpen::Failed);          // ERROR_INVALID_HANDLE
    BOOST_CHECK(ClassifyServiceOpen(1072) == ServiceOpen::Failed);       // ERROR_SERVICE_MARKED_FOR_DELETE

    const std::string text = ServiceUnreadableText(Network::Test);
    BOOST_CHECK(Contains(text, "gridcointestnetd"));
    BOOST_CHECK(Contains(text, "this account cannot read the service's settings"));
    BOOST_CHECK(Contains(text, "elevated"));
}

// M-1: who may own what install finds in a layout directory that was there before it.
BOOST_AUTO_TEST_CASE(level_entry_owners_and_refusals)
{
    const fs::path pd = ProgramData();
    const fs::path root = GridcoinRoot(pd);
    const LayoutSids sids = Sids();
    const std::vector<LayoutEntry> layout = Layout();
    const Sid user = SidOf("S-1-5-21-1004336348-1177238915-682003330-1005");
    const std::vector<Sid> admin_owners{sids.admins, sids.system, winacl::wellknown::TrustedInstaller()};

    BOOST_CHECK(LevelEntryOwners(pd, root, sids) == admin_owners);
    BOOST_CHECK(LevelEntryOwners(pd, root / "conf", sids) == admin_owners);
    BOOST_CHECK(LevelEntryOwners(pd, root / "ipc", sids) == admin_owners);
    for (const Network net : Networks()) {
        std::vector<Sid> service_owners = admin_owners;
        service_owners.push_back(ServiceOf(sids, net));
        BOOST_CHECK(LevelEntryOwners(pd, DataDir(pd, net), sids) == service_owners);
        BOOST_CHECK(LevelEntryOwners(pd, IpcDir(pd, net), sids) == service_owners);
    }

    const auto facts = [](const fs::path& path, std::optional<Sid> owner, bool link = false) {
        LevelEntryFacts entry;
        entry.path = path;
        entry.link = link;
        entry.owner = std::move(owner);
        return entry;
    };
    const auto refusal = [&](const fs::path& level, const LevelEntryFacts& entry) {
        return LevelEntryRefusal(layout, pd, level, entry, sids);
    };

    // A file the administrators or the system made is fine, one another account made is refused,
    // with its path and owner.
    BOOST_CHECK(!refusal(root / "conf", facts(StatePath(pd, Network::Main), sids.admins)));
    BOOST_CHECK(!refusal(root / "conf", facts(root / "conf" / "notes.txt", sids.system)));
    LevelEntryFacts planted = facts(StatePath(pd, Network::Test), user);
    planted.owner_name = "W10-1\\svcsec-b";
    const std::optional<std::string> refused = refusal(root / "conf", planted);
    BOOST_REQUIRE(refused.has_value());
    BOOST_CHECK(Contains(*refused, StatePath(pd, Network::Test).string()));
    BOOST_CHECK(Contains(*refused, "W10-1\\svcsec-b (S-1-5-21-1004336348-1177238915-682003330-1005)"));

    // A service's entries are fine in its own data and IPC directories only.
    BOOST_CHECK(!refusal(DataDir(pd, Network::Main), facts(DataDir(pd, Network::Main) / "wallet.dat", sids.svc_main)));
    BOOST_CHECK(refusal(DataDir(pd, Network::Main), facts(DataDir(pd, Network::Main) / "wallet.dat", sids.svc_test)));
    BOOST_CHECK(!refusal(IpcDir(pd, Network::Test), facts(IpcDir(pd, Network::Test) / "node.sock", sids.svc_test)));
    BOOST_CHECK(refusal(root / "conf", facts(root / "conf" / "x.conf", sids.svc_main)));
    BOOST_CHECK(refusal(root, facts(root / "x", sids.svc_test)));

    // A link is refused whoever owns it, and so is an entry whose owner cannot be read.
    const std::optional<std::string> link = refusal(root / "ipc", facts(root / "ipc" / "x", sids.admins, true));
    BOOST_REQUIRE(link.has_value());
    BOOST_CHECK(Contains(*link, "link"));
    BOOST_CHECK(refusal(QuarantineDir(pd), facts(QuarantineDir(pd) / "x", std::nullopt)));

    // A layout entry is checked as a level of its own, whoever owns it, unless it is a link: no
    // layout path is ever one, and propagation from its parent must not reach through it.
    BOOST_CHECK(!refusal(root / "conf", facts(ConfPath(pd, Network::Main), user)));
    BOOST_CHECK(!refusal(root, facts(DataDir(pd, Network::Test), user)));
    const std::optional<std::string> layout_link = refusal(root, facts(DataDir(pd, Network::Test), user, true));
    BOOST_REQUIRE(layout_link.has_value());
    BOOST_CHECK(Contains(*layout_link, DataDir(pd, Network::Test).string()));
    BOOST_CHECK(Contains(*layout_link, "link"));
    BOOST_CHECK(refusal(root / "conf", facts(ConfPath(pd, Network::Test), sids.admins, true)));
}

// N-1: every directory level found in place has its entries checked, whether or not its list is
// already the layout's: a matching list says who can add an entry now, not who added one before.
BOOST_AUTO_TEST_CASE(every_existing_level_has_its_entries_checked)
{
    for (const LayoutEntry& entry : Layout()) {
        for (const bool list_matches : {false, true}) {
            BOOST_CHECK_MESSAGE(ChecksLevelEntries(entry, list_matches) == entry.directory,
                                entry.path.string() << (list_matches ? ", list matching" : ", list differing"));
        }
    }
}

// M-1: status shows the state file only when the administrators' alone.
BOOST_AUTO_TEST_CASE(state_file_refusal)
{
    using winacl::AceKind;
    using winacl::ObservedAce;
    using winacl::ObservedAcl;
    const auto ace = [](AceKind kind, const Sid& trustee, uint32_t mask, bool inherited) {
        ObservedAce entry;
        entry.kind = kind;
        entry.trustee = trustee;
        entry.mask = mask;
        entry.inherited = inherited;
        return entry;
    };
    const Sid admins = winacl::wellknown::Administrators();
    const Sid system = winacl::wellknown::System();
    const Sid user = SidOf("S-1-5-21-1004336348-1177238915-682003330-1005");

    // What conf\ passes down to a file made there.
    ObservedAcl inherited;
    inherited.owner = admins;
    inherited.dacl_present = true;
    inherited.aces = {ace(AceKind::Allow, admins, FULL_ACCESS, true), ace(AceKind::Allow, system, FULL_ACCESS, true)};
    BOOST_CHECK(!StateFileRefusal(inherited));

    ObservedAcl by_system = inherited;
    by_system.owner = system;
    BOOST_CHECK(!StateFileRefusal(by_system));

    ObservedAcl denied = inherited;
    denied.aces.push_back(ace(AceKind::Deny, user, FULL_ACCESS, false));
    BOOST_CHECK(!StateFileRefusal(denied));

    ObservedAcl by_user = inherited;
    by_user.owner = user;
    BOOST_CHECK(StateFileRefusal(by_user));

    ObservedAcl no_owner = inherited;
    no_owner.owner.reset();
    BOOST_CHECK(StateFileRefusal(no_owner));

    ObservedAcl readable = inherited;
    readable.aces.push_back(ace(AceKind::Allow, winacl::wellknown::Users(), FILE_READ_DATA, false));
    BOOST_CHECK(StateFileRefusal(readable));

    ObservedAcl no_dacl = inherited;
    no_dacl.dacl_present = false;
    no_dacl.aces.clear();
    BOOST_CHECK(StateFileRefusal(no_dacl));
}

BOOST_AUTO_TEST_CASE(service_description_names_the_account)
{
    BOOST_CHECK(Contains(ServiceDescription(Network::Main), L"NT SERVICE\\gridcoinresearchd"));
    BOOST_CHECK(Contains(ServiceDescription(Network::Test), L"NT SERVICE\\gridcointestnetd"));
    BOOST_CHECK(Contains(ServiceDescription(Network::Main), L"gridcoin-service.exe"));
}

BOOST_AUTO_TEST_CASE(tool_args_parse)
{
    std::string error;

    std::optional<ToolOptions> options = ParseToolArgs({"install"}, error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->verb == Verb::Install);
    BOOST_CHECK(options->net == Network::Main);
    BOOST_CHECK(options->restricted_sid);
    BOOST_CHECK(!options->reconfigure);

    options = ParseToolArgs({"install", "--network=test", "--sid-type=unrestricted", "--reconfigure"}, error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->verb == Verb::Install);
    BOOST_CHECK(options->net == Network::Test);
    BOOST_CHECK(!options->restricted_sid);
    BOOST_CHECK(options->reconfigure);

    options = ParseToolArgs({"--network=main", "install", "--sid-type=restricted"}, error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->net == Network::Main);
    BOOST_CHECK(options->restricted_sid);

    options = ParseToolArgs({"status", "--network=test"}, error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->verb == Verb::Status);
    BOOST_CHECK(options->net == Network::Test);

    options = ParseToolArgs({"uninstall"}, error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->verb == Verb::Uninstall);

    for (const std::vector<std::string>& help :
         std::vector<std::vector<std::string>>{{"help"}, {"--help"}, {"-h"}, {"/?"}, {"install", "--help"}}) {
        options = ParseToolArgs(help, error);
        BOOST_REQUIRE(options.has_value());
        BOOST_CHECK(options->verb == Verb::Help);
    }

    const std::vector<std::vector<std::string>> refused{
        {},
        {"start"},
        {"install", "uninstall"},
        {"install", "--network=regtest"},
        {"install", "--network="},
        {"install", "--network"},
        {"install", "--sid-type=none"},
        {"status", "--reconfigure"},
        {"uninstall", "--sid-type=restricted"},
        {"install", "--force"},
        {"install", "-network=test"},
        {"Install"},
    };
    for (const std::vector<std::string>& args : refused) {
        error.clear();
        BOOST_CHECK(!ParseToolArgs(args, error).has_value());
        BOOST_CHECK(!error.empty());
    }

    BOOST_CHECK(Contains(ToolUsage(), "install"));
    BOOST_CHECK(Contains(ToolUsage(), "--network=main|test"));
}

BOOST_AUTO_TEST_CASE(tool_args_parse_enable_and_disable)
{
    std::string error;

    std::optional<ToolOptions> options = ParseToolArgs({"enable", "--user=svcsec-b"}, error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->verb == Verb::Enable);
    BOOST_CHECK(options->net == Network::Main);
    BOOST_CHECK(options->user == std::optional<std::string>("svcsec-b"));
    BOOST_CHECK(!options->from.has_value());
    BOOST_CHECK(!options->to.has_value());

    options = ParseToolArgs({"enable", "--network=test", "--user=DOMAIN\\Jos\xc3\xa9 Garc\xc3\xad"
                                                         "a",
                             "--from=D:\\Grid coin\\data"},
                            error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->net == Network::Test);
    BOOST_CHECK(options->user == std::optional<std::string>("DOMAIN\\Jos\xc3\xa9 Garc\xc3\xad"
                                                            "a"));
    BOOST_CHECK(options->from == std::optional<std::string>("D:\\Grid coin\\data"));

    options = ParseToolArgs({"disable"}, error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->verb == Verb::Disable);
    BOOST_CHECK(!options->to.has_value());

    options = ParseToolArgs({"disable", "--to=C:\\Users\\svcsec-b\\AppData\\Roaming\\GridcoinResearch",
                             "--network=test"},
                            error);
    BOOST_REQUIRE(options.has_value());
    BOOST_CHECK(options->net == Network::Test);
    BOOST_CHECK(options->to == std::optional<std::string>("C:\\Users\\svcsec-b\\AppData\\Roaming\\GridcoinResearch"));

    const std::vector<std::vector<std::string>> refused{
        {"enable"},
        {"enable", "--user="},
        {"enable", "--user"},
        {"enable", "--user=a", "--from="},
        {"enable", "--user=a", "--to=C:\\x"},
        {"enable", "--user=a", "--reconfigure"},
        {"disable", "--user=a"},
        {"disable", "--from=C:\\x"},
        {"disable", "--to="},
        {"install", "--user=a"},
        {"status", "--from=C:\\x"},
        {"enable", "disable", "--user=a"},
    };
    for (const std::vector<std::string>& args : refused) {
        error.clear();
        BOOST_CHECK(!ParseToolArgs(args, error).has_value());
        BOOST_CHECK(!error.empty());
    }

    BOOST_CHECK(Contains(ToolUsage(), "enable"));
    BOOST_CHECK(Contains(ToolUsage(), "--user=<account>"));
    BOOST_CHECK(Contains(ToolUsage(), "disable"));
    BOOST_CHECK(Contains(ToolUsage(), "--to=<directory>"));
}

BOOST_AUTO_TEST_CASE(disable_target_owner_follows_the_core_rule)
{
    // A data directory an elevated start created belongs to Administrators; the core's own startup
    // check (fsmodes) accepts this account, SYSTEM and Administrators as owners, and so does disable.
    const Sid user = SidOf("S-1-5-21-1547161642-583907252-1801674531-1110");
    const Sid other = SidOf("S-1-5-21-1547161642-583907252-1801674531-1111");
    BOOST_CHECK(MovedRootOwnerAllowed(user, user, false));
    BOOST_CHECK(MovedRootOwnerAllowed(winacl::wellknown::Administrators(), user, false));
    BOOST_CHECK(MovedRootOwnerAllowed(winacl::wellknown::System(), user, false));
    BOOST_CHECK(!MovedRootOwnerAllowed(other, user, false));
    BOOST_CHECK(!MovedRootOwnerAllowed(SidOf("S-1-5-32-545"), user, false));
    BOOST_CHECK(!MovedRootOwnerAllowed(std::nullopt, user, false));

    // Found elsewhere than the path given, only the account's own directory is taken.
    BOOST_CHECK(MovedRootOwnerAllowed(user, user, true));
    BOOST_CHECK(!MovedRootOwnerAllowed(winacl::wellknown::Administrators(), user, true));
    BOOST_CHECK(!MovedRootOwnerAllowed(winacl::wellknown::System(), user, true));
    BOOST_CHECK(!MovedRootOwnerAllowed(other, user, true));
}

BOOST_AUTO_TEST_CASE(launcher_core_line_gives_network_flag_and_datadir)
{
    const std::string head = "@echo off\r\nrem BEGIN GENERATED\r\nrem Gridcoin Core-Start launcher. Do not edit it.\r\n";
    const std::string tail = "if %ERRORLEVEL% EQU 0 goto end\r\n\"%SystemRoot%\\System32\\PING.EXE\" -n 121 127.0.0.1 >nul\r\n"
                             ":end\r\nrem END GENERATED\r\n";
    const std::string err = " 2>>\"%TEMP%\\gridcoin-core-start.err.log\"\r\n";

    // The default data directory: no -datadir.
    std::optional<LauncherCore> core = ParseLauncherCoreLine(
        head + "\"C:\\Program Files\\Gridcoin\\gridcoinresearchd.exe\" -multiprocess -autounlockblob" + err + tail);
    BOOST_REQUIRE(core.has_value());
    BOOST_CHECK(!core->testnet);
    BOOST_CHECK(!core->datadir.has_value());

    // A -datadir with a space and doubled percent signs.
    core = ParseLauncherCoreLine(head + "\"C:\\G\\gridcoinresearchd.exe\" -datadir=\"D:\\My 100%%\\gdata\" -multiprocess "
                                        "-autounlockblob" + err + tail);
    BOOST_REQUIRE(core.has_value());
    BOOST_CHECK(!core->testnet);
    BOOST_REQUIRE(core->datadir.has_value());
    BOOST_CHECK_EQUAL(*core->datadir, "D:\\My 100%\\gdata");

    // -testnet, with and without a data directory, and LF line ends.
    core = ParseLauncherCoreLine(head + "\"C:\\G\\gridcoinresearchd.exe\" -testnet -multiprocess -autounlockblob\n" + tail);
    BOOST_REQUIRE(core.has_value());
    BOOST_CHECK(core->testnet);
    BOOST_CHECK(!core->datadir.has_value());
    core = ParseLauncherCoreLine(head + "\"C:\\G\\g.exe\" -datadir=\"E:\\t\" -testnet -multiprocess" + err + tail);
    BOOST_REQUIRE(core.has_value());
    BOOST_CHECK(core->testnet);
    BOOST_CHECK_EQUAL(core->datadir.value_or(""), "E:\\t");

    // A -testnet inside the quoted data directory is no flag.
    core = ParseLauncherCoreLine(head + "\"C:\\G\\g.exe\" -datadir=\"E:\\x -testnet y\" -multiprocess" + err + tail);
    BOOST_REQUIRE(core.has_value());
    BOOST_CHECK(!core->testnet);

    // The first core line counts, and one outside the generated block does not.
    core = ParseLauncherCoreLine("\"C:\\G\\g.exe\" -testnet\r\nrem BEGIN GENERATED\r\n\"C:\\G\\g.exe\" -multiprocess\r\n"
                                 "\"C:\\G\\g.exe\" -testnet\r\nrem END GENERATED\r\n");
    BOOST_REQUIRE(core.has_value());
    BOOST_CHECK(!core->testnet);

    // No generated block, an empty block, or no core line in it.
    BOOST_CHECK(!ParseLauncherCoreLine("").has_value());
    BOOST_CHECK(!ParseLauncherCoreLine("\"C:\\G\\g.exe\" -testnet\r\n").has_value());
    BOOST_CHECK(!ParseLauncherCoreLine("rem BEGIN GENERATED\r\nrem END GENERATED\r\n").has_value());
    BOOST_CHECK(!ParseLauncherCoreLine("rem BEGIN GENERATED\r\necho hi\r\nrem END GENERATED\r\n").has_value());
    BOOST_CHECK(!ParseLauncherCoreLine("rem BEGIN GENERATED\r\n\"C:\\G\\g.exe\" -testnet\r\n").has_value());
}

BOOST_AUTO_TEST_CASE(launcher_path_comes_from_the_action_arguments)
{
    BOOST_CHECK_EQUAL(LauncherPathOfArguments("/s /c \"\"C:\\Program Files\\Gridcoin\\windows\\core-autostart.cmd\"\"")
                          .value_or("-"),
                      "C:\\Program Files\\Gridcoin\\windows\\core-autostart.cmd");
    BOOST_CHECK(!LauncherPathOfArguments("").has_value());
    BOOST_CHECK(!LauncherPathOfArguments("/s /c").has_value());
    BOOST_CHECK(!LauncherPathOfArguments("/s /c \"\"\"\"").has_value());
    BOOST_CHECK(!LauncherPathOfArguments("stop").has_value());
}

BOOST_AUTO_TEST_CASE(conf_network_follows_the_last_testnet_line)
{
    BOOST_CHECK(NetworkOfConf("") == Network::Main);
    BOOST_CHECK(NetworkOfConf("server=1\nrpcuser=x\n") == Network::Main);
    BOOST_CHECK(NetworkOfConf("testnet=1\n") == Network::Test);
    BOOST_CHECK(NetworkOfConf("testnet=0\n") == Network::Main);
    BOOST_CHECK(NetworkOfConf("  testnet = 1  \r\nserver=1\r\n") == Network::Test);
    BOOST_CHECK(NetworkOfConf("testnet=1 # the test network\n") == Network::Test);
    // A comment line is no setting, and the last setting wins.
    BOOST_CHECK(NetworkOfConf("#testnet=1\n") == Network::Main);
    BOOST_CHECK(NetworkOfConf("testnet=1\ntestnet=0\n") == Network::Main);
    BOOST_CHECK(NetworkOfConf("testnet=0\ntestnet=1") == Network::Test);
    // Another name that holds the text, and a section header, are not the setting.
    BOOST_CHECK(NetworkOfConf("mytestnet=1\n") == Network::Main);
    BOOST_CHECK(NetworkOfConf("[test]\nserver=1\n") == Network::Main);
    BOOST_CHECK(NetworkOfConf("testnet=1\n[test]\nserver=1\n") == Network::Test);
}

BOOST_AUTO_TEST_SUITE_END()
