// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/win_endpoint.h"

#include <boost/test/unit_test.hpp>

#include <optional>
#include <string>
#include <vector>

#ifdef WIN32
#include "ipc/peercred.h"
#include "random.h"
#include "test/state_guard.h"
#include "test/wine.h"
#include "tinyformat.h"
#include "util.h"
#include "util/dir_permissions.h"
#include "util/strencodings.h"

#include <system_error>

#include <windows.h>
#endif

using namespace ipc::win;
using namespace util::winacl;

// The mask constants by their bare names. On Windows <windows.h> defines each of them
// as a macro of the same value (win_acl.cpp checks that), so these cases read the same
// on every platform.
using namespace util::winacl::mask;

namespace {

Sid SidOf(const std::string& text)
{
    const std::optional<Sid> sid = ParseSidString(text);
    BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
    return *sid;
}

//! A local account, standing in for the token user.
Sid Self()
{
    return SidOf("S-1-5-21-1004336348-1177238915-682003330-1001");
}

//! Another local account on the same computer.
Sid OtherUser()
{
    return SidOf("S-1-5-21-1004336348-1177238915-682003330-1002");
}

//! A local group, standing in for "Gridcoin GUI Users".
Sid Group()
{
    return SidOf("S-1-5-21-1004336348-1177238915-682003330-1005");
}

//! A service SID (NT SERVICE\...), standing in for the core's service account.
Sid Service()
{
    return SidOf("S-1-5-80-1111111111-2222222222-3333333333-4111111111-555555555");
}

Sid AuthenticatedUsers()
{
    return SidOf("S-1-5-11");
}

ObservedAce Ace(const Sid& trustee, uint32_t access, Inherit inherit = Inherit::None, bool inherited = false,
                AceKind kind = AceKind::Allow)
{
    ObservedAce ace;
    ace.kind = kind;
    ace.trustee = trustee;
    ace.mask = access;
    ace.inherited = inherited;
    ace.inherit = inherit;

    return ace;
}

ObservedAcl Acl(const Sid& owner, std::vector<ObservedAce> aces, bool is_protected = true)
{
    ObservedAcl acl;
    acl.owner = owner;
    acl.dacl_present = true;
    acl.is_protected = is_protected;
    acl.aces = std::move(aces);

    return acl;
}

EndpointEntry Entry(const std::string& path, EntryKind kind, ObservedAcl acl)
{
    EndpointEntry entry;
    entry.path = fs::path(path);
    entry.kind = kind;
    entry.acl = std::move(acl);

    return entry;
}

//! Every allow entry of \p spec, the trustee given.
std::vector<AceSpec> EntriesFor(const AclSpec& spec, const Sid& trustee)
{
    std::vector<AceSpec> found;
    for (const AceSpec& ace : spec.aces) {
        if (ace.trustee == trustee) found.push_back(ace);
    }

    return found;
}

//! A same-account endpoint in a data directory created by an earlier release: the
//! directory and its parent inherit %APPDATA%'s list (this account, SYSTEM and
//! Administrators), unprotected.
EndpointFacts SameAccountFacts()
{
    const auto inherited = [] {
        return Acl(Self(),
                   {Ace(Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject, true),
                    Ace(wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject, true),
                    Ace(wellknown::Administrators(), FILE_ALL_ACCESS, Inherit::ContainerAndObject, true)},
                   /*is_protected=*/false);
    };

    EndpointFacts facts;
    facts.socket = Entry("C:\\Users\\a\\AppData\\Roaming\\GridcoinResearch\\node.sock", EntryKind::File,
                         Acl(Self(), {Ace(Self(), FILE_ALL_ACCESS), Ace(wellknown::System(), FILE_ALL_ACCESS)}));
    facts.dir = Entry("C:\\Users\\a\\AppData\\Roaming\\GridcoinResearch", EntryKind::Directory, inherited());
    facts.parent = Entry("C:\\Users\\a\\AppData\\Roaming", EntryKind::Directory, inherited());

    return facts;
}

//! The service endpoint as gridcoin-service.exe lays it out: Administrators-owned,
//! protected directories, the service may add and remove entries in its own, and
//! the group may only view them.
EndpointFacts ServiceFacts()
{
    EndpointFacts facts;
    facts.socket = Entry("C:\\ProgramData\\Gridcoin\\ipc\\mainnet\\node.sock", EntryKind::File,
                         Acl(Service(), {Ace(Service(), FILE_ALL_ACCESS), Ace(wellknown::System(), FILE_ALL_ACCESS),
                                         Ace(Group(), SOCKET_CONNECT_MASK)}));
    facts.dir = Entry("C:\\ProgramData\\Gridcoin\\ipc\\mainnet", EntryKind::Directory,
                      Acl(wellknown::Administrators(),
                          {Ace(wellknown::Administrators(), FILE_ALL_ACCESS),
                           Ace(wellknown::System(), FILE_ALL_ACCESS),
                           Ace(Service(), DIR_VIEW_MASK | FILE_ADD_FILE | FILE_DELETE_CHILD),
                           Ace(Group(), DIR_VIEW_MASK)}));
    facts.parent = Entry("C:\\ProgramData\\Gridcoin\\ipc", EntryKind::Directory,
                         Acl(wellknown::Administrators(),
                             {Ace(wellknown::Administrators(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
                              Ace(wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
                              Ace(Group(), DIR_VIEW_MASK)}));

    return facts;
}

//! Check that \p reason is a refusal that contains every one of \p words.
void CheckRefusal(const std::optional<std::string>& reason, const std::vector<std::string>& words)
{
    BOOST_REQUIRE_MESSAGE(reason.has_value(), "expected a refusal containing '" << words.front() << "'");
    for (const std::string& word : words) {
        BOOST_CHECK_MESSAGE(reason->find(word) != std::string::npos, "'" << word << "' is not in: " << *reason);
    }
}

} // namespace

BOOST_AUTO_TEST_SUITE(ipc_win_endpoint_tests)

// Same-account mode: each object is owned by this account, and only this account and
// SYSTEM may use it.
BOOST_AUTO_TEST_CASE(endpoint_same_account_owner_only)
{
    for (const IpcObject obj : {IpcObject::Socket, IpcObject::Cookie}) {
        const AclSpec spec = EndpointAclSpec(obj, Self(), std::nullopt);

        BOOST_CHECK(spec.owner == Self());
        BOOST_REQUIRE_EQUAL(spec.aces.size(), 2U);
        BOOST_CHECK(spec.aces[0].trustee == Self());
        BOOST_CHECK(spec.aces[1].trustee == wellknown::System());
        for (const AceSpec& ace : spec.aces) {
            BOOST_CHECK_EQUAL(ace.mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
            BOOST_CHECK(ace.inherit == Inherit::None);
        }
    }
}

// The group may connect to the socket, and read what the GUI checks before it
// connects, and nothing else: it can neither read nor append to the stream as a
// file, nor delete, re-own or re-ACL the socket.
BOOST_AUTO_TEST_CASE(endpoint_socket_group_gets_connect_mask_only)
{
    const AclSpec spec = EndpointAclSpec(IpcObject::Socket, Self(), Group());

    BOOST_CHECK(spec.owner == Self());
    BOOST_CHECK_EQUAL(spec.aces.size(), 3U);

    const std::vector<AceSpec> group = EntriesFor(spec, Group());
    BOOST_REQUIRE_EQUAL(group.size(), 1U);
    BOOST_CHECK_EQUAL(group[0].mask, SOCKET_CONNECT_MASK);
    BOOST_CHECK_EQUAL(group[0].mask & static_cast<uint32_t>(DELETE | WRITE_DAC | WRITE_OWNER | FILE_APPEND_DATA |
                                                            FILE_READ_DATA),
                      0U);

    // The owner's and SYSTEM's entries are those of the same-account mode.
    BOOST_REQUIRE_EQUAL(EntriesFor(spec, Self()).size(), 1U);
    BOOST_CHECK_EQUAL(EntriesFor(spec, Self())[0].mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
    BOOST_REQUIRE_EQUAL(EntriesFor(spec, wellknown::System()).size(), 1U);
    BOOST_CHECK_EQUAL(EntriesFor(spec, wellknown::System())[0].mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
}

// The group may read the cookie and nothing that changes it.
BOOST_AUTO_TEST_CASE(endpoint_cookie_group_read_only)
{
    const AclSpec spec = EndpointAclSpec(IpcObject::Cookie, Self(), Group());

    BOOST_CHECK(spec.owner == Self());
    BOOST_CHECK_EQUAL(spec.aces.size(), 3U);

    const std::vector<AceSpec> group = EntriesFor(spec, Group());
    BOOST_REQUIRE_EQUAL(group.size(), 1U);
    BOOST_CHECK_EQUAL(group[0].mask, COOKIE_READ_MASK);
    BOOST_CHECK_EQUAL(group[0].mask & FILE_WRITEISH, 0U);
    BOOST_CHECK_EQUAL(group[0].mask & static_cast<uint32_t>(FILE_DELETE_CHILD | FILE_EXECUTE), 0U);
}

// Nothing created inside receives an entry from either object, in either mode.
BOOST_AUTO_TEST_CASE(endpoint_never_inheritable)
{
    for (const IpcObject obj : {IpcObject::Socket, IpcObject::Cookie}) {
        for (const std::optional<Sid>& group : {std::optional<Sid>{}, std::optional<Sid>{Group()}}) {
            const AclSpec spec = EndpointAclSpec(obj, Self(), group);
            BOOST_CHECK(!spec.aces.empty());
            for (const AceSpec& ace : spec.aces) {
                BOOST_CHECK(ace.inherit == Inherit::None);
            }
        }
    }
}

// A core running as SYSTEM names it once, as the owner-only list does.
BOOST_AUTO_TEST_CASE(endpoint_names_system_once_when_it_is_the_owner)
{
    const AclSpec spec = EndpointAclSpec(IpcObject::Socket, wellknown::System(), std::nullopt);

    BOOST_CHECK(spec.owner == wellknown::System());
    BOOST_REQUIRE_EQUAL(spec.aces.size(), 1U);
    BOOST_CHECK(spec.aces[0].trustee == wellknown::System());
    BOOST_CHECK_EQUAL(spec.aces[0].mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
}

// The group's directory rights let it see the directory and nothing more, so a
// directory that grants them passes the directory rule.
BOOST_AUTO_TEST_CASE(ipc_dir_spec_gives_the_group_view_only)
{
    BOOST_CHECK_EQUAL(DIR_VIEW_MASK & IPC_DIR_WRITEISH, 0U);

    const AclSpec spec = IpcDirAclSpec(Self(), Group());
    BOOST_CHECK(spec.owner == Self());
    BOOST_REQUIRE_EQUAL(spec.aces.size(), 3U);

    BOOST_REQUIRE_EQUAL(EntriesFor(spec, Self()).size(), 1U);
    BOOST_CHECK_EQUAL(EntriesFor(spec, Self())[0].mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
    BOOST_CHECK(EntriesFor(spec, Self())[0].inherit == Inherit::ContainerAndObject);

    BOOST_REQUIRE_EQUAL(EntriesFor(spec, wellknown::System()).size(), 1U);
    BOOST_CHECK_EQUAL(EntriesFor(spec, wellknown::System())[0].mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
    BOOST_CHECK(EntriesFor(spec, wellknown::System())[0].inherit == Inherit::ContainerAndObject);

    BOOST_REQUIRE_EQUAL(EntriesFor(spec, Group()).size(), 1U);
    BOOST_CHECK_EQUAL(EntriesFor(spec, Group())[0].mask, DIR_VIEW_MASK);
    BOOST_CHECK(EntriesFor(spec, Group())[0].inherit == Inherit::None);

    // The directory as it reads back after the list is applied.
    std::vector<ObservedAce> aces;
    for (const AceSpec& ace : spec.aces) aces.push_back(Ace(ace.trustee, ace.mask, ace.inherit));
    const EndpointEntry dir = Entry("C:\\gridcoin-ipc", EntryKind::Directory, Acl(spec.owner, aces));

    BOOST_CHECK(!CheckMatchesSpec(dir.acl, spec));
    BOOST_CHECK(!DirectoryRefusal(dir, Self(), /*require_protected=*/true));
}

// Only a name surrogate sends a path elsewhere. The socket's own AF_UNIX reparse point
// is not one, nor is a reparse point that only stores a file's data elsewhere.
BOOST_AUTO_TEST_CASE(classify_entry_by_reparse_tag)
{
    const uint32_t symlink = 0xA000000C;
    const uint32_t mount_point = 0xA0000003; // a junction
    const uint32_t af_unix = 0x80000023;
    const uint32_t cloud_files = 0x9000001A;

    BOOST_CHECK(ClassifyEntry(/*directory=*/true, std::nullopt) == EntryKind::Directory);
    BOOST_CHECK(ClassifyEntry(/*directory=*/false, std::nullopt) == EntryKind::File);
    BOOST_CHECK(ClassifyEntry(/*directory=*/false, af_unix) == EntryKind::File);
    BOOST_CHECK(ClassifyEntry(/*directory=*/true, cloud_files) == EntryKind::Directory);
    BOOST_CHECK(ClassifyEntry(/*directory=*/false, cloud_files) == EntryKind::File);

    for (const bool directory : {true, false}) {
        BOOST_CHECK(ClassifyEntry(directory, symlink) == EntryKind::Redirect);
        BOOST_CHECK(ClassifyEntry(directory, mount_point) == EntryKind::Redirect);
    }
}

// A same-account endpoint in an existing data directory passes: its list is inherited
// and unprotected, which the rule asks for only at a service -ipcdir.
BOOST_AUTO_TEST_CASE(endpoint_same_account_inherited_directories_accepted)
{
    const EndpointFacts facts = SameAccountFacts();
    BOOST_CHECK(!EndpointRefusal(facts, Self(), /*require_protected=*/false));

    // The same directories at a service -ipcdir are refused for not being protected.
    CheckRefusal(EndpointRefusal(facts, Self(), /*require_protected=*/true), {"protected", facts.dir.path.string()});
}

// The service layout passes the service rule. Its directories are owned by
// Administrators, the service may add and remove entries in its own, and the
// group's view does not count as a write.
BOOST_AUTO_TEST_CASE(endpoint_service_layout_accepted)
{
    BOOST_CHECK(!EndpointRefusal(ServiceFacts(), Service(), /*require_protected=*/true));
}

// The socket must be the expected server's: another account's, even in a directory
// that is otherwise clean, is refused before anything is sent to it.
BOOST_AUTO_TEST_CASE(endpoint_socket_of_another_owner_refused)
{
    EndpointFacts facts = SameAccountFacts();
    facts.socket.acl.owner = OtherUser();
    CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.socket.path.string(), "owner"});

    // In the service mode, a socket of this GUI's own account is not the service's.
    EndpointFacts service = ServiceFacts();
    service.socket.acl.owner = Self();
    CheckRefusal(EndpointRefusal(service, Service(), true), {service.socket.path.string(), "owner"});

    // So is a socket whose owner could not be read.
    facts = SameAccountFacts();
    facts.socket.acl.owner.reset();
    CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.socket.path.string()});
}

// A link or a directory at the socket's name is not the socket.
BOOST_AUTO_TEST_CASE(endpoint_socket_redirect_or_directory_refused)
{
    for (const EntryKind kind : {EntryKind::Redirect, EntryKind::Directory}) {
        EndpointFacts facts = SameAccountFacts();
        facts.socket.kind = kind;
        CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.socket.path.string()});
    }
}

// A junction or link at the directory or its parent is refused: through one, what is
// checked is not what connect reaches.
BOOST_AUTO_TEST_CASE(endpoint_redirected_directory_refused)
{
    EndpointFacts facts = SameAccountFacts();
    facts.dir.kind = EntryKind::Redirect;
    CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.dir.path.string()});

    facts = SameAccountFacts();
    facts.parent.kind = EntryKind::Redirect;
    CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.parent.path.string()});

    // And the directory must be one.
    facts = SameAccountFacts();
    facts.dir.kind = EntryKind::File;
    CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.dir.path.string()});
}

// Another account that can add, remove or replace an entry in the socket's directory
// could swap the socket between this check and the connect.
BOOST_AUTO_TEST_CASE(endpoint_directory_with_foreign_writer_refused)
{
    for (const uint32_t right : {static_cast<uint32_t>(FILE_ADD_FILE), static_cast<uint32_t>(FILE_ADD_SUBDIRECTORY),
                                 static_cast<uint32_t>(FILE_DELETE_CHILD), static_cast<uint32_t>(DELETE),
                                 static_cast<uint32_t>(WRITE_DAC), static_cast<uint32_t>(WRITE_OWNER),
                                 static_cast<uint32_t>(GENERIC_WRITE), static_cast<uint32_t>(GENERIC_ALL)}) {
        EndpointFacts facts = SameAccountFacts();
        facts.dir.acl.aces.push_back(Ace(OtherUser(), right));
        CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.dir.path.string()});

        facts = SameAccountFacts();
        facts.parent.acl.aces.push_back(Ace(AuthenticatedUsers(), right, Inherit::None, /*inherited=*/true));
        CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.parent.path.string()});
    }

    // An entry that only applies to what is created inside, or that denies, is not a
    // grant on the directory.
    EndpointFacts facts = SameAccountFacts();
    ObservedAce inherit_only = Ace(AuthenticatedUsers(), FILE_ALL_ACCESS, Inherit::ContainerAndObject);
    inherit_only.inherit_only = true;
    facts.dir.acl.aces.push_back(inherit_only);
    facts.dir.acl.aces.push_back(Ace(OtherUser(), FILE_ALL_ACCESS, Inherit::None, false, AceKind::Deny));
    BOOST_CHECK(!EndpointRefusal(facts, Self(), false));

    // Nor is the right to read and traverse it.
    facts = SameAccountFacts();
    facts.dir.acl.aces.push_back(Ace(wellknown::Users(), DIR_VIEW_MASK));
    BOOST_CHECK(!EndpointRefusal(facts, Self(), false));
}

// A directory another account owns, such as a %ProgramData%\Gridcoin that a standard
// user created before the service was installed, is refused, at the directory or at
// its parent.
BOOST_AUTO_TEST_CASE(endpoint_directory_of_another_owner_refused)
{
    EndpointFacts facts = ServiceFacts();
    facts.parent.acl.owner = OtherUser();
    CheckRefusal(EndpointRefusal(facts, Service(), true), {facts.parent.path.string(), "owner"});

    facts = ServiceFacts();
    facts.dir.acl.owner = OtherUser();
    CheckRefusal(EndpointRefusal(facts, Service(), true), {facts.dir.path.string(), "owner"});

    // The owner may be the expected server, SYSTEM or Administrators.
    for (const Sid& owner : {Service(), wellknown::System(), wellknown::Administrators()}) {
        facts = ServiceFacts();
        facts.dir.acl.owner = owner;
        facts.parent.acl.owner = owner;
        BOOST_CHECK(!EndpointRefusal(facts, Service(), true));
    }
}

// A directory with no DACL gives everyone full access.
BOOST_AUTO_TEST_CASE(endpoint_directory_without_dacl_refused)
{
    EndpointFacts facts = SameAccountFacts();
    facts.dir.acl.dacl_present = false;
    facts.dir.acl.aces.clear();
    CheckRefusal(EndpointRefusal(facts, Self(), false), {facts.dir.path.string(), "DACL"});
}

// The service mode asks for protected directories, at the directory and at its
// parent alike.
BOOST_AUTO_TEST_CASE(endpoint_service_directories_must_be_protected)
{
    EndpointFacts facts = ServiceFacts();
    facts.dir.acl.is_protected = false;
    CheckRefusal(EndpointRefusal(facts, Service(), true), {facts.dir.path.string(), "protected"});
    BOOST_CHECK(!EndpointRefusal(facts, Service(), false));

    facts = ServiceFacts();
    facts.parent.acl.is_protected = false;
    CheckRefusal(EndpointRefusal(facts, Service(), true), {facts.parent.path.string(), "protected"});
}

// What the server checks at an -ipcdir is the directory rule with itself as the
// server: its own list, the owner-only list and the service layout pass; a junction,
// another owner or another account's write is refused.
BOOST_AUTO_TEST_CASE(directory_refusal_for_the_server)
{
    const EndpointEntry owner_only = Entry("D:\\ipc", EntryKind::Directory,
                                           Acl(Self(), {Ace(Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
                                                        Ace(wellknown::System(), FILE_ALL_ACCESS,
                                                            Inherit::ContainerAndObject)}));
    BOOST_CHECK(!DirectoryRefusal(owner_only, Self(), false));
    BOOST_CHECK(!DirectoryRefusal(ServiceFacts().dir, Service(), false));

    EndpointEntry redirected = owner_only;
    redirected.kind = EntryKind::Redirect;
    CheckRefusal(DirectoryRefusal(redirected, Self(), false), {"D:\\ipc"});

    EndpointEntry other_owner = owner_only;
    other_owner.acl.owner = OtherUser();
    CheckRefusal(DirectoryRefusal(other_owner, Self(), false), {"D:\\ipc", "owner"});

    EndpointEntry users_write = owner_only;
    users_write.acl.aces.push_back(Ace(wellknown::Users(), FILE_ADD_FILE, Inherit::None, /*inherited=*/true));
    CheckRefusal(DirectoryRefusal(users_write, Self(), false), {"D:\\ipc"});

    // The service account's write in the service layout is its own, not another
    // account's; to any other server it is another account's.
    CheckRefusal(DirectoryRefusal(ServiceFacts().dir, Self(), false), {ServiceFacts().dir.path.string()});
}

// The attach group is looked up on this computer only.
BOOST_AUTO_TEST_CASE(local_group_lookup_name_is_qualified_by_the_computer)
{
    BOOST_CHECK_EQUAL(LocalGroupLookupName("DESKTOP-1", "Gridcoin GUI Users").value_or(""),
                      "DESKTOP-1\\Gridcoin GUI Users");
    BOOST_CHECK_EQUAL(LocalGroupLookupName("PC", "Gridcoin Testnet GUI Users").value_or(""),
                      "PC\\Gridcoin Testnet GUI Users");
    // UTF-8 passes through unchanged ("Grupo de Jos" and e-acute).
    BOOST_CHECK_EQUAL(LocalGroupLookupName("PC", "Grupo de Jos\xC3\xA9").value_or(""), "PC\\Grupo de Jos\xC3\xA9");

    BOOST_CHECK(!LocalGroupLookupName("PC", ""));
    BOOST_CHECK(!LocalGroupLookupName("", "Gridcoin GUI Users"));

    // A name that brings its own domain or computer, or that no account name can be.
    const std::vector<std::string> refused{"CORP\\Gridcoin GUI Users", "Gridcoin GUI Users@corp.example", "a/b",
                                           "a[b]", "a:b", "a;b", "a|b", "a=b", "a,b", "a+b", "a*b", "a?b", "a<b",
                                           "a>b", "a\"b", "a\tb", "a\nb", std::string("a\0b", 3)};
    for (const std::string& name : refused) {
        BOOST_CHECK_MESSAGE(!LocalGroupLookupName("PC", name), "accepted '" << name << "'");
    }
}

#ifdef WIN32

namespace {

//! A directory of its own under the temp directory, removed when the case ends.
struct WinTempDir
{
    fs::path path;

    WinTempDir()
    {
        path = fs::temp_directory_path() / strprintf("grc_ipc_win_endpoint_%s", GetRandHash().GetHex().substr(0, 16));
        fs::create_directories(path);
    }

    ~WinTempDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

void WriteFile(const fs::path& path)
{
    fsbridge::ofstream out(path, std::ios::binary);
    out << "x";
}

} // namespace

// The access list given to an endpoint object reads back as specified, with the owner
// this account.
BOOST_AUTO_TEST_CASE(win_apply_endpoint_acl_reads_back)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; skipping");
        return;
    }
    WinTempDir dir;
    for (const IpcObject obj : {IpcObject::Socket, IpcObject::Cookie}) {
        const fs::path file = dir.path / (obj == IpcObject::Socket ? "node.sock" : "ipc.cookie");
        WriteFile(file);

        ApplyEndpointAcl(file, obj);
        const ObservedAcl acl = ReadAcl(file);
        BOOST_CHECK(!CheckMatchesSpec(acl, EndpointAclSpec(obj, CurrentTokenUser(), std::nullopt)));
        BOOST_CHECK(!CheckMatchesSpec(acl, ThisProcessEndpointAclSpec(obj, std::nullopt)));
    }
}

// No socket means no server is listening, which the GUI keeps waiting on rather than
// refusing.
BOOST_AUTO_TEST_CASE(win_check_endpoint_missing_socket_is_not_listening)
{
    WinTempDir dir;
    try {
        CheckServerEndpointBeforeConnect(dir.path / "node.sock", CurrentTokenUser());
        BOOST_ERROR("a missing socket was accepted");
    } catch (const std::system_error& e) {
        BOOST_CHECK_MESSAGE(e.code() == std::errc::no_such_file_or_directory, e.what());
    }

    try {
        CheckServerEndpointBeforeConnect(dir.path / "absent" / "node.sock", CurrentTokenUser());
        BOOST_ERROR("a socket in a missing directory was accepted");
    } catch (const std::system_error& e) {
        BOOST_CHECK_MESSAGE(e.code() == std::errc::no_such_file_or_directory, e.what());
    }
}

// An endpoint of this account's own, in an owner-only directory under this account's
// temp directory, passes; one checked against another expected owner does not.
BOOST_AUTO_TEST_CASE(win_check_endpoint_own_socket_accepted)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; skipping");
        return;
    }
    WinTempDir dir;
    const fs::path ipcdir = dir.path / "ipc";
    std::string error;
    BOOST_REQUIRE_MESSAGE(util::CreateOwnerOnlyDirectory(ipcdir, error), error);
    WriteFile(ipcdir / "node.sock");
    ApplyEndpointAcl(ipcdir / "node.sock", IpcObject::Socket);

    BOOST_CHECK_NO_THROW(CheckServerEndpointBeforeConnect(ipcdir / "node.sock", CurrentTokenUser()));
    BOOST_CHECK_THROW(CheckServerEndpointBeforeConnect(ipcdir / "node.sock", wellknown::System()),
                      std::runtime_error);
}

// An attach group that does not exist on this computer is refused at configuration,
// and the reason names it. The real group is checked only where it exists.
BOOST_FIXTURE_TEST_CASE(win_attach_group_must_be_a_local_group, grc_test::StateGuard)
{
    const std::string missing = strprintf("gridcoin-test-no-such-group-%s", GetRandHash().GetHex().substr(0, 8));
    BOOST_CHECK(!ipc::ConfigureAttachGroup(missing));
    BOOST_CHECK(!AttachGroupSid());
    const std::optional<std::string> problem = ipc::AttachGroupProblem(missing);
    BOOST_REQUIRE(problem);
    BOOST_CHECK_MESSAGE(problem->find(missing) != std::string::npos, *problem);

    BOOST_CHECK(!ipc::ConfigureAttachGroup("CORP\\Gridcoin GUI Users"));
    BOOST_CHECK(ipc::AttachGroupProblem("CORP\\Gridcoin GUI Users"));

    if (ipc::ConfigureAttachGroup("Gridcoin GUI Users")) {
        const std::optional<std::string> sid = AttachGroupSid();
        BOOST_REQUIRE(sid);
        BOOST_CHECK_MESSAGE(sid->rfind("S-1-5-21-", 0) == 0, *sid);
        BOOST_CHECK(!ipc::AttachGroupProblem("Gridcoin GUI Users"));
    } else {
        BOOST_TEST_MESSAGE("no local group 'Gridcoin GUI Users' on this computer; its lookup is not checked");
    }

    BOOST_CHECK(ipc::ConfigureAttachGroup(""));
    BOOST_CHECK(!AttachGroupSid());
}

// Without -ipcserviceuser the GUI expects a core of its own account; a named account
// that does not exist is an error, not a fallback.
BOOST_FIXTURE_TEST_CASE(win_expected_server_sid, grc_test::StateGuard)
{
    gArgs.ClearForcedArg("-ipcserviceuser");
    BOOST_CHECK(ExpectedServerSid() == CurrentTokenUser());

    gArgs.ForceSetArg("-ipcserviceuser", "NT SERVICE\\gridcoin-test-no-such-service");
    BOOST_CHECK_THROW(ExpectedServerSid(), std::runtime_error);

    // An account that exists, named as Windows names it here (well-known names are
    // localized, so this account's own is used): it resolves to its SID.
    Sid self = CurrentTokenUser();
    std::wstring name(512, L'\0');
    std::wstring domain(512, L'\0');
    DWORD name_length = static_cast<DWORD>(name.size());
    DWORD domain_length = static_cast<DWORD>(domain.size());
    SID_NAME_USE use{};
    BOOST_REQUIRE(::LookupAccountSidW(nullptr, self.data(), name.data(), &name_length, domain.data(),
                                      &domain_length, &use));
    name.resize(name_length);
    domain.resize(domain_length);

    gArgs.ForceSetArg("-ipcserviceuser", WideToUtf8(domain + L"\\" + name));
    BOOST_CHECK(ExpectedServerSid() == self);
}

// The attach group's directory is never the data directory, however it is reached;
// it is refused before its list is changed.
BOOST_AUTO_TEST_CASE(win_attach_group_directory_refuses_the_data_directory)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; skipping");
        return;
    }
    const fs::path datadir = GetDataDir();
    const ObservedAcl before = ReadAcl(datadir);

    try {
        ApplyAttachGroupDirectoryAccess(datadir, wellknown::Users(), /*created=*/false);
        BOOST_ERROR("the data directory was given the attach group's access");
    } catch (const std::runtime_error& e) {
        const std::string what = e.what();
        BOOST_CHECK_MESSAGE(what.find("-ipcattachgroup") != std::string::npos, what);
        BOOST_CHECK_MESSAGE(what.find("-ipcdir") != std::string::npos, what);
    }

    const ObservedAcl after = ReadAcl(datadir);
    BOOST_CHECK(after.owner == before.owner);
    BOOST_CHECK_EQUAL(after.aces.size(), before.aces.size());
    BOOST_CHECK_EQUAL(after.is_protected, before.is_protected);
}

// A directory of this account's own gets the attach group's view, and the list reads
// back through the directory's handle.
BOOST_AUTO_TEST_CASE(win_attach_group_directory_access_applied)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; skipping");
        return;
    }
    // Created with an explicit owner of this account, which an elevated process does
    // not get from a plain create.
    WinTempDir dir;
    const fs::path ipcdir = dir.path / "ipc";
    std::string error;
    BOOST_REQUIRE_MESSAGE(util::CreateOwnerOnlyDirectory(ipcdir, error), error);

    ApplyAttachGroupDirectoryAccess(ipcdir, wellknown::Users(), /*created=*/true);
    BOOST_CHECK(!CheckMatchesSpec(ReadAcl(ipcdir), IpcDirAclSpec(CurrentTokenUser(), wellknown::Users())));
    BOOST_CHECK(!ServerIpcDirRefusal(ipcdir));
}

#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
