// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/win_endpoint.h"

#include <boost/test/unit_test.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef WIN32
#include "ipc/peercred.h"
#include "ipc/process.h"
#include "random.h"
#include "test/state_guard.h"
#include "test/win_fs_util.h"
#include "test/wine.h"
#include "tinyformat.h"
#include "util.h"
#include "util/dir_permissions.h"
#include "util/strencodings.h"

#include <functional>
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

//! The Modify permission as Explorer grants it (FILE_GENERIC_READ, FILE_GENERIC_WRITE,
//! FILE_GENERIC_EXECUTE and DELETE).
constexpr uint32_t MODIFY = 0x1301BF;

//! A socket as the core binds it: an AF_UNIX reparse point owned by \p owner, with
//! \p owner and SYSTEM at full access and, when given, \p group with the connect
//! right.
EndpointEntry Socket(const std::string& path, const Sid& owner, const std::optional<Sid>& group = std::nullopt)
{
    std::vector<ObservedAce> aces{Ace(owner, FILE_ALL_ACCESS), Ace(wellknown::System(), FILE_ALL_ACCESS)};
    if (group) aces.push_back(Ace(*group, SOCKET_CONNECT_MASK));

    EndpointEntry entry = Entry(path, EntryKind::UnixSocket, Acl(owner, std::move(aces)));
    entry.reparse_tag = REPARSE_TAG_AF_UNIX;

    return entry;
}

//! An endpoint: its socket, and the directories the rule reads when it applies.
struct Endpoint
{
    EndpointEntry socket;
    EndpointDirs dirs;
};

//! A same-account endpoint in a data directory created by an earlier release: the
//! directory and its parent inherit %APPDATA%'s list (this account, SYSTEM and
//! Administrators), unprotected.
Endpoint SameAccount()
{
    const auto inherited = [] {
        return Acl(Self(),
                   {Ace(Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject, true),
                    Ace(wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject, true),
                    Ace(wellknown::Administrators(), FILE_ALL_ACCESS, Inherit::ContainerAndObject, true)},
                   /*is_protected=*/false);
    };

    Endpoint endpoint;
    endpoint.socket = Socket("C:\\Users\\a\\AppData\\Roaming\\GridcoinResearch\\node.sock", Self());
    endpoint.dirs.dir = Entry("C:\\Users\\a\\AppData\\Roaming\\GridcoinResearch", EntryKind::Directory, inherited());
    endpoint.dirs.parent = Entry("C:\\Users\\a\\AppData\\Roaming", EntryKind::Directory, inherited());

    return endpoint;
}

//! The same-account endpoint with owner-only, protected directories: where the
//! directory rule applies it has nothing to refuse in them, so a case built on it
//! is refused only for what it adds.
Endpoint OwnerOnlySameAccount()
{
    Endpoint endpoint = SameAccount();
    for (EndpointEntry* entry : {&endpoint.dirs.dir, &endpoint.dirs.parent}) {
        entry->acl = Acl(Self(), {Ace(Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
                                  Ace(wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject)});
    }

    return endpoint;
}

//! The same endpoint with the attach group's connect entry on its socket, as a core
//! of this account binds it with -ipcattachgroup: another account can write the
//! socket, so the directory rule applies without -ipcserviceuser.
Endpoint AttachGroup()
{
    Endpoint endpoint = SameAccount();
    endpoint.socket = Socket(endpoint.socket.path.string(), Self(), Group());

    return endpoint;
}

//! The service endpoint as gridcoin-service.exe lays it out: Administrators-owned,
//! protected directories, the service may add and remove entries in its own, and
//! the group may only view them.
Endpoint ServiceLayout()
{
    Endpoint endpoint;
    endpoint.socket = Socket("C:\\ProgramData\\Gridcoin\\ipc\\mainnet\\node.sock", Service(), Group());
    endpoint.dirs.dir = Entry("C:\\ProgramData\\Gridcoin\\ipc\\mainnet", EntryKind::Directory,
                              Acl(wellknown::Administrators(),
                                  {Ace(wellknown::Administrators(), FILE_ALL_ACCESS),
                                   Ace(wellknown::System(), FILE_ALL_ACCESS),
                                   Ace(Service(), DIR_VIEW_MASK | FILE_ADD_FILE | FILE_DELETE_CHILD),
                                   Ace(Group(), DIR_VIEW_MASK)}));
    endpoint.dirs.parent = Entry("C:\\ProgramData\\Gridcoin\\ipc", EntryKind::Directory,
                                 Acl(wellknown::Administrators(),
                                     {Ace(wellknown::Administrators(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
                                      Ace(wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
                                      Ace(Group(), DIR_VIEW_MASK)}));

    return endpoint;
}

//! EndpointRefusal over \p endpoint. \p reads, when given, counts the calls that read
//! the directories.
std::optional<std::string> Refusal(const Endpoint& endpoint, const Sid& expected, bool service_user_set,
                                   int* reads = nullptr)
{
    return EndpointRefusal(endpoint.socket, expected, service_user_set, [&] {
        if (reads != nullptr) ++*reads;
        return endpoint.dirs;
    });
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

// Only a name surrogate sends a path elsewhere, and only the AF_UNIX tag on a file
// that is not a directory makes a socket. A directory whose tag is not a surrogate,
// such as a cloud-files directory, stays a directory.
BOOST_AUTO_TEST_CASE(classify_entry_by_reparse_tag)
{
    const uint32_t symlink = 0xA000000C;
    const uint32_t mount_point = 0xA0000003; // a junction
    const uint32_t cloud_files = 0x9000001A;
    BOOST_CHECK_EQUAL(REPARSE_TAG_AF_UNIX, 0x80000023U);

    BOOST_CHECK(ClassifyEntry(/*directory=*/false, REPARSE_TAG_AF_UNIX) == EntryKind::UnixSocket);

    BOOST_CHECK(ClassifyEntry(/*directory=*/false, std::nullopt) == EntryKind::File);
    BOOST_CHECK(ClassifyEntry(/*directory=*/false, cloud_files) == EntryKind::File);

    BOOST_CHECK(ClassifyEntry(/*directory=*/true, std::nullopt) == EntryKind::Directory);
    BOOST_CHECK(ClassifyEntry(/*directory=*/true, cloud_files) == EntryKind::Directory);
    BOOST_CHECK(ClassifyEntry(/*directory=*/true, REPARSE_TAG_AF_UNIX) == EntryKind::Directory);

    for (const bool directory : {true, false}) {
        BOOST_CHECK(ClassifyEntry(directory, symlink) == EntryKind::Redirect);
        BOOST_CHECK(ClassifyEntry(directory, mount_point) == EntryKind::Redirect);
    }
}

// A junction on the endpoint's path that Windows does not follow is a refusal that names
// the path and says what to do, not a failure to open. Every other error stays one.
BOOST_AUTO_TEST_CASE(untrusted_junction_on_the_path_is_a_refusal)
{
    constexpr uint32_t WIN32_FILE_NOT_FOUND = 2, WIN32_PATH_NOT_FOUND = 3, WIN32_ACCESS_DENIED = 5;
    BOOST_CHECK_EQUAL(WIN32_UNTRUSTED_MOUNT_POINT, 448U);

    const fs::path socket{"C:/Users/x/AppData/Roaming/GridcoinResearch/node.sock"};
    const std::optional<std::string> refusal = UntrustedPathRefusal(WIN32_UNTRUSTED_MOUNT_POINT, socket);
    BOOST_REQUIRE(refusal);
    for (const std::string& part : {socket.string(), std::string{"junction"}, std::string{"administrator"},
                                    std::string{"untrusted mount point"}, std::string{"directory the junction leads to"}}) {
        BOOST_CHECK_MESSAGE(refusal->find(part) != std::string::npos, *refusal << " lacks '" << part << "'");
    }

    for (const uint32_t error : {WIN32_FILE_NOT_FOUND, WIN32_PATH_NOT_FOUND, WIN32_ACCESS_DENIED, 0U}) {
        BOOST_CHECK_MESSAGE(!UntrustedPathRefusal(error, socket), "error " << error);
    }
}

// In every mode the socket must be an AF_UNIX socket with a DACL, owned by the
// expected server: a plain file, a file with another tag, a directory or a link at
// its name is not the socket.
BOOST_AUTO_TEST_CASE(socket_must_be_the_expected_servers_socket)
{
    const EndpointEntry socket = Socket("C:\\gr\\node.sock", Self());
    BOOST_CHECK(!SocketRefusal(socket, Self()));

    const std::vector<std::pair<EntryKind, std::optional<uint32_t>>> others{
        {EntryKind::File, std::nullopt},
        {EntryKind::File, 0x9000001A},
        {EntryKind::Directory, std::nullopt},
        {EntryKind::Redirect, 0xA0000003},
        {EntryKind::Redirect, 0xA000000C},
    };
    for (const auto& [kind, tag] : others) {
        EndpointEntry other = socket;
        other.kind = kind;
        other.reparse_tag = tag;
        CheckRefusal(SocketRefusal(other, Self()), {"C:\\gr\\node.sock"});
    }

    // No DACL gives everyone full access.
    EndpointEntry no_dacl = socket;
    no_dacl.acl.dacl_present = false;
    no_dacl.acl.aces.clear();
    CheckRefusal(SocketRefusal(no_dacl, Self()), {"C:\\gr\\node.sock", "DACL"});

    // Another owner than the expected one, or one that could not be read.
    CheckRefusal(SocketRefusal(socket, OtherUser()), {"C:\\gr\\node.sock", "owner"});
    EndpointEntry unread = socket;
    unread.acl.owner.reset();
    CheckRefusal(SocketRefusal(unread, Self()), {"C:\\gr\\node.sock", "owner"});
}

// The directory rule applies when someone other than the expected account and SYSTEM
// can write the socket, as the attach group's connect right does, or when
// -ipcserviceuser is set.
BOOST_AUTO_TEST_CASE(directory_rule_applies_when_another_can_write_the_socket)
{
    const ObservedAcl own = Socket("C:\\gr\\node.sock", Self()).acl;
    BOOST_CHECK(!DirectoryRuleApplies(own, Self(), /*service_user_set=*/false));
    BOOST_CHECK(DirectoryRuleApplies(own, Self(), /*service_user_set=*/true));

    BOOST_CHECK(DirectoryRuleApplies(Socket("C:\\gr\\node.sock", Self(), Group()).acl, Self(), false));

    for (const uint32_t right : {static_cast<uint32_t>(FILE_WRITE_DATA), static_cast<uint32_t>(GENERIC_WRITE),
                                 static_cast<uint32_t>(GENERIC_ALL)}) {
        ObservedAcl acl = own;
        acl.aces.push_back(Ace(OtherUser(), right));
        BOOST_CHECK_MESSAGE(DirectoryRuleApplies(acl, Self(), false), "right " << right);
    }

    // Reading the socket's list or attributes is not the right to connect, and an
    // entry that denies, or one that only applies to what would be created inside,
    // grants nothing.
    ObservedAcl read_only = own;
    read_only.aces.push_back(Ace(wellknown::Users(), READ_CONTROL | FILE_READ_ATTRIBUTES | SYNCHRONIZE));
    BOOST_CHECK(!DirectoryRuleApplies(read_only, Self(), false));

    ObservedAcl denied = own;
    denied.aces.push_back(Ace(OtherUser(), FILE_ALL_ACCESS, Inherit::None, false, AceKind::Deny));
    BOOST_CHECK(!DirectoryRuleApplies(denied, Self(), false));

    ObservedAcl inherit_only = own;
    ObservedAce entry = Ace(wellknown::Users(), FILE_ALL_ACCESS, Inherit::ContainerAndObject);
    entry.inherit_only = true;
    inherit_only.aces.push_back(entry);
    BOOST_CHECK(!DirectoryRuleApplies(inherit_only, Self(), false));

    // An inherited entry and an allow entry of another kind (a callback or object
    // entry) grant the right as well.
    ObservedAcl inherited = own;
    inherited.aces.push_back(Ace(OtherUser(), FILE_WRITE_DATA, Inherit::None, /*inherited=*/true));
    BOOST_CHECK(DirectoryRuleApplies(inherited, Self(), false));

    ObservedAcl other_allow = own;
    other_allow.aces.push_back(Ace(OtherUser(), FILE_WRITE_DATA, Inherit::None, false, AceKind::OtherAllow));
    BOOST_CHECK(DirectoryRuleApplies(other_allow, Self(), false));

    // Asked about a socket another account owns, which SocketRefusal refuses first,
    // the rule applies.
    BOOST_CHECK(DirectoryRuleApplies(own, OtherUser(), false));
}

// A same-account endpoint passes on its socket alone, and its directories are not
// read: a parent at a volume root that lets Authenticated Users add a directory, a
// directory with an inherited Modify entry, and a directory behind a junction are
// all accepted. With -ipcserviceuser set the same directories are read, and each is
// refused for what it adds to owner-only, protected directories.
BOOST_AUTO_TEST_CASE(endpoint_same_account_does_not_read_the_directories)
{
    int reads = 0;
    BOOST_CHECK(!Refusal(SameAccount(), Self(), false, &reads));

    Endpoint root_parent = OwnerOnlySameAccount();
    root_parent.dirs.parent.acl.aces.push_back(Ace(AuthenticatedUsers(), FILE_ADD_SUBDIRECTORY));
    BOOST_CHECK(!Refusal(root_parent, Self(), false, &reads));

    Endpoint inherited_modify = OwnerOnlySameAccount();
    inherited_modify.dirs.dir.acl.aces.push_back(
        Ace(AuthenticatedUsers(), MODIFY, Inherit::ContainerAndObject, /*inherited=*/true));
    BOOST_CHECK(!Refusal(inherited_modify, Self(), false, &reads));

    Endpoint junction = OwnerOnlySameAccount();
    junction.dirs.dir.kind = EntryKind::Redirect;
    junction.dirs.dir.reparse_tag = 0xA0000003;
    BOOST_CHECK(!Refusal(junction, Self(), false, &reads));

    BOOST_CHECK_EQUAL(reads, 0);

    CheckRefusal(Refusal(SameAccount(), Self(), true, &reads), {"protected", SameAccount().dirs.dir.path.string()});
    BOOST_CHECK_EQUAL(reads, 1);

    BOOST_CHECK(!Refusal(OwnerOnlySameAccount(), Self(), true));
    CheckRefusal(Refusal(root_parent, Self(), true), {root_parent.dirs.parent.path.string(), "S-1-5-11"});
    CheckRefusal(Refusal(inherited_modify, Self(), true), {inherited_modify.dirs.dir.path.string(), "S-1-5-11"});
    CheckRefusal(Refusal(junction, Self(), true), {junction.dirs.dir.path.string(), "junction"});
}

// With -ipcattachgroup the socket's group entry lets another account write it, so the
// directory rule applies with this account as the expected server and no
// -ipcserviceuser: the directories are read once, and another account's write on the
// directory is refused.
BOOST_AUTO_TEST_CASE(endpoint_attach_group_applies_the_directory_rule)
{
    int reads = 0;
    BOOST_CHECK(!Refusal(AttachGroup(), Self(), false, &reads));
    BOOST_CHECK_EQUAL(reads, 1);

    Endpoint endpoint = AttachGroup();
    endpoint.dirs.dir.acl.aces.push_back(Ace(OtherUser(), FILE_ADD_FILE));
    CheckRefusal(Refusal(endpoint, Self(), false), {endpoint.dirs.dir.path.string()});
}

// The service layout passes the service rule. Its directories are owned by
// Administrators, the service may add and remove entries in its own, and the
// group's view does not count as a write.
BOOST_AUTO_TEST_CASE(endpoint_service_layout_accepted)
{
    BOOST_CHECK(!Refusal(ServiceLayout(), Service(), /*service_user_set=*/true));
}

// The socket must be the expected server's: another account's, even in a directory
// that is otherwise clean, is refused before anything is sent to it, and before its
// directories are read.
BOOST_AUTO_TEST_CASE(endpoint_socket_of_another_owner_refused)
{
    int reads = 0;
    Endpoint endpoint = SameAccount();
    endpoint.socket.acl.owner = OtherUser();
    CheckRefusal(Refusal(endpoint, Self(), false, &reads), {endpoint.socket.path.string(), "owner"});

    // In the service mode, a socket of this GUI's own account is not the service's.
    Endpoint service = ServiceLayout();
    service.socket.acl.owner = Self();
    CheckRefusal(Refusal(service, Service(), true, &reads), {service.socket.path.string(), "owner"});

    // A link at the socket's name is refused in the same-account mode too.
    endpoint = SameAccount();
    endpoint.socket.kind = EntryKind::Redirect;
    endpoint.socket.reparse_tag = 0xA000000C;
    CheckRefusal(Refusal(endpoint, Self(), false, &reads), {endpoint.socket.path.string()});

    BOOST_CHECK_EQUAL(reads, 0);
}

// Where the rule applies, a junction or link at the directory or its parent is
// refused: through one, what is checked is not what connect reaches.
BOOST_AUTO_TEST_CASE(endpoint_redirected_directory_refused)
{
    Endpoint endpoint = AttachGroup();
    endpoint.dirs.dir.kind = EntryKind::Redirect;
    CheckRefusal(Refusal(endpoint, Self(), false), {endpoint.dirs.dir.path.string()});

    endpoint = AttachGroup();
    endpoint.dirs.parent.kind = EntryKind::Redirect;
    CheckRefusal(Refusal(endpoint, Self(), false), {endpoint.dirs.parent.path.string()});

    // And the directory must be one.
    endpoint = AttachGroup();
    endpoint.dirs.dir.kind = EntryKind::File;
    CheckRefusal(Refusal(endpoint, Self(), false), {endpoint.dirs.dir.path.string()});
}

// Where the rule applies, another account that can add, remove or replace an entry in
// the socket's directory, or in its parent, is refused: it could swap the socket
// between this check and the connect.
BOOST_AUTO_TEST_CASE(endpoint_directory_with_foreign_writer_refused)
{
    for (const uint32_t right : {static_cast<uint32_t>(FILE_ADD_FILE), static_cast<uint32_t>(FILE_ADD_SUBDIRECTORY),
                                 static_cast<uint32_t>(FILE_DELETE_CHILD), static_cast<uint32_t>(DELETE),
                                 static_cast<uint32_t>(WRITE_DAC), static_cast<uint32_t>(WRITE_OWNER),
                                 static_cast<uint32_t>(GENERIC_WRITE), static_cast<uint32_t>(GENERIC_ALL)}) {
        Endpoint endpoint = AttachGroup();
        endpoint.dirs.dir.acl.aces.push_back(Ace(OtherUser(), right));
        CheckRefusal(Refusal(endpoint, Self(), false), {endpoint.dirs.dir.path.string()});

        endpoint = AttachGroup();
        endpoint.dirs.parent.acl.aces.push_back(Ace(AuthenticatedUsers(), right, Inherit::None, /*inherited=*/true));
        CheckRefusal(Refusal(endpoint, Self(), false), {endpoint.dirs.parent.path.string()});
    }

    // An entry that only applies to what is created inside, or that denies, is not a
    // grant on the directory.
    Endpoint endpoint = AttachGroup();
    ObservedAce inherit_only = Ace(AuthenticatedUsers(), FILE_ALL_ACCESS, Inherit::ContainerAndObject);
    inherit_only.inherit_only = true;
    endpoint.dirs.dir.acl.aces.push_back(inherit_only);
    endpoint.dirs.dir.acl.aces.push_back(Ace(OtherUser(), FILE_ALL_ACCESS, Inherit::None, false, AceKind::Deny));
    BOOST_CHECK(!Refusal(endpoint, Self(), false));

    // Nor is the right to read and traverse it.
    endpoint = AttachGroup();
    endpoint.dirs.dir.acl.aces.push_back(Ace(wellknown::Users(), DIR_VIEW_MASK));
    BOOST_CHECK(!Refusal(endpoint, Self(), false));
}

// A directory another account owns, such as a %ProgramData%\Gridcoin that a standard
// user created before the service was installed, is refused, at the directory or at
// its parent.
BOOST_AUTO_TEST_CASE(endpoint_directory_of_another_owner_refused)
{
    Endpoint endpoint = ServiceLayout();
    endpoint.dirs.parent.acl.owner = OtherUser();
    CheckRefusal(Refusal(endpoint, Service(), true), {endpoint.dirs.parent.path.string(), "owner"});

    endpoint = ServiceLayout();
    endpoint.dirs.dir.acl.owner = OtherUser();
    CheckRefusal(Refusal(endpoint, Service(), true), {endpoint.dirs.dir.path.string(), "owner"});

    // The owner may be the expected server, SYSTEM or Administrators.
    for (const Sid& owner : {Service(), wellknown::System(), wellknown::Administrators()}) {
        endpoint = ServiceLayout();
        endpoint.dirs.dir.acl.owner = owner;
        endpoint.dirs.parent.acl.owner = owner;
        BOOST_CHECK(!Refusal(endpoint, Service(), true));
    }
}

// Where the rule applies, a directory with no DACL, which gives everyone full access,
// is refused.
BOOST_AUTO_TEST_CASE(endpoint_directory_without_dacl_refused)
{
    Endpoint endpoint = AttachGroup();
    endpoint.dirs.dir.acl.dacl_present = false;
    endpoint.dirs.dir.acl.aces.clear();
    CheckRefusal(Refusal(endpoint, Self(), false), {endpoint.dirs.dir.path.string(), "DACL"});
}

// The service mode asks for protected directories, at the directory and at its
// parent alike; the attach-group rule without -ipcserviceuser does not.
BOOST_AUTO_TEST_CASE(endpoint_service_directories_must_be_protected)
{
    Endpoint endpoint = ServiceLayout();
    endpoint.dirs.dir.acl.is_protected = false;
    CheckRefusal(Refusal(endpoint, Service(), true), {endpoint.dirs.dir.path.string(), "protected"});
    BOOST_CHECK(!Refusal(endpoint, Service(), false));

    endpoint = ServiceLayout();
    endpoint.dirs.parent.acl.is_protected = false;
    CheckRefusal(Refusal(endpoint, Service(), true), {endpoint.dirs.parent.path.string(), "protected"});
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
    BOOST_CHECK(!DirectoryRefusal(ServiceLayout().dirs.dir, Service(), false));

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
    CheckRefusal(DirectoryRefusal(ServiceLayout().dirs.dir, Self(), false),
                 {ServiceLayout().dirs.dir.path.string()});
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

// The -ipcserviceuser account is named with its domain, so the lookup is made there
// alone.
BOOST_AUTO_TEST_CASE(service_user_must_name_its_domain)
{
    for (const char* name : {"NT SERVICE\\gridcoinresearchd", "NT SERVICE\\gridcointestnetd", "PC\\alice"}) {
        BOOST_CHECK_MESSAGE(!ServiceUserNameRefusal(name), "refused '" << name << "'");
    }
    for (const char* name : {"gridcoinresearchd", "\\gridcoinresearchd", "NT SERVICE\\", "a\\b\\c",
                             "alice@corp.example", ""}) {
        CheckRefusal(ServiceUserNameRefusal(name), {"-ipcserviceuser"});
    }
}

// The expected server is a user account or a service SID (NT SERVICE, S-1-5-80); a
// group of any kind, SYSTEM's well-known group and anything else are refused.
BOOST_AUTO_TEST_CASE(service_user_must_be_a_user_or_a_service)
{
    const auto account = [](const Sid& sid, AccountType type, const std::string& domain) {
        AccountLookup lookup;
        lookup.sid = sid;
        lookup.type = type;
        lookup.domain = domain;
        return lookup;
    };

    BOOST_CHECK(!ServiceUserAccountRefusal(account(Self(), AccountType::User, "PC")));
    BOOST_CHECK(!ServiceUserAccountRefusal(account(Service(), AccountType::WellKnownGroup, "NT SERVICE")));

    CheckRefusal(ServiceUserAccountRefusal(account(wellknown::System(), AccountType::WellKnownGroup, "NT AUTHORITY")),
                 {"S-1-5-18"});
    for (const AccountType type : {AccountType::Group, AccountType::Alias, AccountType::Domain, AccountType::Computer,
                                   AccountType::DeletedAccount, AccountType::Unknown, AccountType::Other}) {
        CheckRefusal(ServiceUserAccountRefusal(account(Group(), type, "PC")), {FormatSid(Group())});
    }
    // A service SID found as another kind is not a service.
    CheckRefusal(ServiceUserAccountRefusal(account(Service(), AccountType::Alias, "NT SERVICE")),
                 {FormatSid(Service())});

    // A service's SID is S-1-5-80 and the five words of its name's hash. Other SIDs
    // under S-1-5-80, such as NT SERVICE\ALL SERVICES (S-1-5-80-0), name no one
    // service and are refused.
    for (const char* refused : {"S-1-5-80-0", "S-1-5-80-1-2-3-4", "S-1-5-80-1-2-3-4-5-6", "S-1-5-81-1-2-3-4-5",
                                "S-1-5-21-1-2-3-4-5", "S-1-16-80-1-2-3-4-5"}) {
        CheckRefusal(ServiceUserAccountRefusal(account(SidOf(refused), AccountType::WellKnownGroup, "NT SERVICE")),
                     {refused});
    }
    BOOST_CHECK(!ServiceUserAccountRefusal(
        account(SidOf("S-1-5-80-1-2-3-4-5"), AccountType::WellKnownGroup, "NT SERVICE")));
}

// The attach group is a local group of this computer, found in the computer's own
// domain: BUILTIN\Users found under the computer's name, a domain group, or a user is
// refused.
BOOST_AUTO_TEST_CASE(attach_group_must_be_an_alias_of_this_computer)
{
    const auto account = [](AccountType type, const std::string& domain) {
        AccountLookup lookup;
        lookup.sid = Group();
        lookup.type = type;
        lookup.domain = domain;
        return lookup;
    };

    BOOST_CHECK(!AttachGroupAccountRefusal(account(AccountType::Alias, "DESKTOP-1"), "DESKTOP-1"));
    BOOST_CHECK(!AttachGroupAccountRefusal(account(AccountType::Alias, "desktop-1"), "DESKTOP-1"));

    CheckRefusal(AttachGroupAccountRefusal(account(AccountType::Alias, "BUILTIN"), "DESKTOP-1"), {"BUILTIN"});
    CheckRefusal(AttachGroupAccountRefusal(account(AccountType::Alias, "CORP"), "DESKTOP-1"), {"CORP"});
    CheckRefusal(AttachGroupAccountRefusal(account(AccountType::Alias, ""), "DESKTOP-1"), {"DESKTOP-1"});
    for (const AccountType type : {AccountType::Group, AccountType::User, AccountType::WellKnownGroup}) {
        CheckRefusal(AttachGroupAccountRefusal(account(type, "DESKTOP-1"), "DESKTOP-1"), {"local group"});
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

//! Closes a socket descriptor from Process::bind or Process::connect.
struct SocketCloser
{
    int fd{-1};
    ~SocketCloser()
    {
        if (fd == -1) return;
        SOCKET s = static_cast<SOCKET>(fd);
        closesocket(s);
    }
};

//! Check that \p check throws the endpoint rule's refusal, a std::runtime_error that
//! is not a std::system_error, naming \p word.
void CheckRefusedByRule(const std::function<void()>& check, const std::string& word)
{
    try {
        check();
    } catch (const std::system_error& e) {
        BOOST_ERROR("a call failed rather than the rule refusing: " << e.what());
        return;
    } catch (const std::runtime_error& e) {
        BOOST_CHECK_MESSAGE(std::string(e.what()).find(word) != std::string::npos, e.what());
        return;
    }
    BOOST_ERROR("the endpoint was not refused");
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

// A plain file of this account's own at the socket's name, even with the socket's
// access list, is not a socket: it carries no AF_UNIX reparse tag, and is refused by
// the rule rather than by a failing call.
BOOST_AUTO_TEST_CASE(win_check_endpoint_plain_file_is_not_a_socket)
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

    const EndpointEntry entry = ReadEntryNoFollow(ipcdir / "node.sock");
    BOOST_CHECK(entry.kind == EntryKind::File);
    BOOST_CHECK(!entry.reparse_tag);

    CheckRefusedByRule([&] { CheckServerEndpointBeforeConnect(ipcdir / "node.sock", CurrentTokenUser()); },
                       "AF_UNIX");
}

// A socket bound by the IPC process layer carries the AF_UNIX reparse tag and passes
// the check. With only this account and SYSTEM on its list the directories are not
// held; with another account's connect right on it they are, and a connect made
// through Process::connect works while they are held.
BOOST_FIXTURE_TEST_CASE(win_bound_socket_passes_the_check, grc_test::StateGuard)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; skipping");
        return;
    }
    gArgs.ClearForcedArg("-ipcdir");
    gArgs.ClearForcedArg("-ipcattachgroup");
    gArgs.ClearForcedArg("-ipcserviceuser");

    WinTempDir dir;
    const fs::path ipcdir = dir.path / "ipc";
    std::string address = "unix";
    SocketCloser server{ipc::MakeProcess()->bind(ipcdir, address)};
    BOOST_REQUIRE(server.fd != -1);
    BOOST_REQUIRE_EQUAL(::listen(static_cast<SOCKET>(server.fd), 1), 0);

    const fs::path socket = ipcdir / "node.sock";
    const EndpointEntry entry = ReadEntryNoFollow(socket);
    BOOST_CHECK(entry.kind == EntryKind::UnixSocket);
    BOOST_REQUIRE(entry.reparse_tag);
    BOOST_CHECK_EQUAL(*entry.reparse_tag, REPARSE_TAG_AF_UNIX);

    const Sid self = CurrentTokenUser();
    BOOST_CHECK_EQUAL(CheckServerEndpointBeforeConnect(socket, self).size(), 0U);

    // Expected to be another account's, here SYSTEM's, the same socket is refused.
    CheckRefusedByRule([&] { CheckServerEndpointBeforeConnect(socket, wellknown::System()); }, "owner");

    ApplyEndpointAcl(socket, EndpointAclSpec(IpcObject::Socket, self, wellknown::Users()));
    BOOST_CHECK_EQUAL(CheckServerEndpointBeforeConnect(socket, self).size(), 2U);

    std::string client_address = "unix";
    SocketCloser client{ipc::MakeProcess()->connect(ipcdir, client_address)};
    BOOST_CHECK(client.fd != -1);
}

// The socket's path reaches afunix as UTF-8: ParseAddress (ipc/process.cpp) fills sun_path
// from path.string(). A data directory under a profile with a non-ASCII name puts such bytes
// there. The bind and the connect must use the directory under its intended name, which they
// do only if afunix reads sun_path as UTF-8 and not in the system code page; this case is what
// shows which. Where the system code page is UTF-8 (65001) the two readings agree, so a pass
// there does not settle it, and the case warns. The name is short, two characters (one in
// code page 1252, one outside it), so the path keeps a margin under sun_path's 108 bytes for a
// longer user name.
BOOST_FIXTURE_TEST_CASE(win_socket_under_a_non_ascii_directory, grc_test::StateGuard)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine's AF_UNIX is not Windows' own; skipping");
        return;
    }
    gArgs.ClearForcedArg("-ipcdir");
    gArgs.ClearForcedArg("-ipcattachgroup");
    gArgs.ClearForcedArg("-ipcserviceuser");

    const unsigned code_page = ::GetACP();
    BOOST_TEST_MESSAGE("the system code page is " << code_page);
    BOOST_WARN_MESSAGE(code_page != CP_UTF8, "the system code page is UTF-8, so this pass does not show how afunix "
                                             "reads sun_path");

    WinTempDir dir;
    const std::wstring name = L"\u00e4\u6d4b";
    const fs::path ipcdir = dir.path / fs::path(name);
    std::string address = "unix";
    SocketCloser server{ipc::MakeProcess()->bind(ipcdir, address)};
    BOOST_REQUIRE(server.fd != -1);
    BOOST_REQUIRE_EQUAL(::listen(static_cast<SOCKET>(server.fd), 1), 0);

    std::vector<std::wstring> names;
    for (fs::directory_iterator it(dir.path); it != fs::directory_iterator(); ++it) {
        names.push_back(it->path().filename().wstring());
    }
    BOOST_CHECK_MESSAGE(names.size() == 1 && names[0] == name, "the IPC directory is not there under its name alone");

    bool socket_found = false;
    for (fs::directory_iterator it(ipcdir); it != fs::directory_iterator(); ++it) {
        socket_found = socket_found || it->path().filename() == "node.sock";
    }
    BOOST_CHECK_MESSAGE(socket_found, "no node.sock in the IPC directory under its intended name");
    BOOST_CHECK(ReadEntryNoFollow(ipcdir / "node.sock").kind == EntryKind::UnixSocket);

    std::string client_address = "unix";
    SocketCloser client{ipc::MakeProcess()->connect(ipcdir, client_address)};
    BOOST_CHECK(client.fd != -1);
}

// A real junction: in the same-account mode a junction above the socket is followed
// as the system resolves it; where the directory rule applies it is refused, and a
// junction at the socket's own name is refused in every mode. Current Windows 11 builds
// do not follow a junction an account that is not an administrator made; there the one
// above the socket is a refusal that says so, never a failure to open. Which applies is
// read from the system, and a junction made with an elevated token is always followed.
BOOST_FIXTURE_TEST_CASE(win_junction_followed_above_the_socket_only_in_same_account_mode, grc_test::StateGuard)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; skipping");
        return;
    }
    gArgs.ClearForcedArg("-ipcdir");
    gArgs.ClearForcedArg("-ipcattachgroup");
    gArgs.ClearForcedArg("-ipcserviceuser");

    WinTempDir dir;
    const fs::path real = dir.path / "real";
    std::string address = "unix";
    SocketCloser server{ipc::MakeProcess()->bind(real, address)};
    BOOST_REQUIRE(server.fd != -1);

    const fs::path linked = dir.path / "linked";
    grc_test::MakeJunction(linked, real);
    BOOST_CHECK(ReadEntryNoFollow(linked).kind == EntryKind::Redirect);

    const bool untrusted = grc_test::JunctionUntrustedOn(linked);
    BOOST_TEST_MESSAGE("Windows " << (untrusted ? "does not follow" : "follows") << " a junction this token made");
    // An elevated token stands in for "an administrator made it"; that holds for an elevated
    // administrator and a standard user, not for every elevated token (Backup Operators).
    if (CurrentTokenIsElevated()) BOOST_CHECK_MESSAGE(!untrusted, "a junction an elevated token made is not followed");

    const Sid self = CurrentTokenUser();
    if (untrusted) {
        CheckRefusedByRule([&] { CheckServerEndpointBeforeConnect(linked / "node.sock", self); }, "junction");
        CheckRefusedByRule([&] { CheckServerEndpointBeforeConnect(linked / "node.sock", self); },
                           (linked / "node.sock").string());
    } else {
        BOOST_CHECK_NO_THROW(CheckServerEndpointBeforeConnect(linked / "node.sock", self));
    }

    ApplyEndpointAcl(real / "node.sock", EndpointAclSpec(IpcObject::Socket, self, wellknown::Users()));
    CheckRefusedByRule([&] { CheckServerEndpointBeforeConnect(linked / "node.sock", self); },
                       untrusted ? "junction" : linked.string());

    const fs::path other = dir.path / "other";
    std::string error;
    BOOST_REQUIRE_MESSAGE(util::CreateOwnerOnlyDirectory(other, error), error);
    grc_test::MakeJunction(other / "node.sock", real);
    CheckRefusedByRule([&] { CheckServerEndpointBeforeConnect(other / "node.sock", self); },
                       (other / "node.sock").string());

    // The junctions themselves are removed, so the cleanup does not go through them.
    BOOST_CHECK(::RemoveDirectoryW((other / "node.sock").wstring().c_str()));
    BOOST_CHECK(::RemoveDirectoryW(linked.wstring().c_str()));
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

    // A name without its domain is refused before any lookup.
    gArgs.ForceSetArg("-ipcserviceuser", "gridcoinresearchd");
    BOOST_CHECK_THROW(ExpectedServerSid(), std::runtime_error);

    // So is a group, here the local Administrators alias by its English builtin name
    // (where the names are localized it does not resolve, which is refused too).
    gArgs.ForceSetArg("-ipcserviceuser", "BUILTIN\\Administrators");
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
//
// It needs no skip under Wine: the refusal comes before any access list is written.
BOOST_AUTO_TEST_CASE(win_attach_group_directory_refuses_the_data_directory)
{
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

    BOOST_CHECK(grc_test::SameAcl(ReadAcl(datadir), before));
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
