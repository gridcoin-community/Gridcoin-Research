// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/fsmodes.h"

#include "util/dir_permissions.h"
#include "util/win_acl.h"

#include "test/state_guard.h"
#include <fs.h>
#include <util/string.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32
#include "test/umask_guard.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#else
#include "test/win_fs_util.h"
#include "test/wine.h"

#include <io.h>
#include <windows.h>
#endif

using namespace util::winacl;

// The mask constants by their bare names. On Windows <windows.h> defines each of them
// as a macro of the same value (win_acl.cpp checks that), so these cases read the same
// on every platform.
using namespace util::winacl::mask;

#ifndef WIN32
using grc_test::UmaskGuard;
#endif

namespace {

bool HasFinding(const std::vector<fsmodes::ModeFinding>& findings, const fs::path& path)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&path](const fsmodes::ModeFinding& f) { return f.path == path; });
}

//! The contents of the file at \p path.
std::string ReadAll(const fs::path& path)
{
    fsbridge::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

const fsmodes::ModeFinding& FindingFor(const std::vector<fsmodes::ModeFinding>& findings, const fs::path& path)
{
    const auto it = std::find_if(findings.begin(), findings.end(),
                                 [&path](const fsmodes::ModeFinding& f) { return f.path == path; });
    BOOST_REQUIRE(it != findings.end());
    return *it;
}

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

//! A service SID (NT SERVICE\...), standing in for the token user of a core run as a service.
Sid ServiceSid()
{
    return SidOf("S-1-5-80-1-2-3-4-5");
}

Sid AuthenticatedUsers()
{
    return SidOf("S-1-5-11");
}

Sid CreatorOwner()
{
    return SidOf("S-1-3-0");
}

// Windows' standard rights of a file as its dialogs grant them: Read, Read and execute, and
// Modify.
constexpr uint32_t READ_RIGHTS = 0x120089;
constexpr uint32_t READ_EXECUTE_RIGHTS = 0x1200a9;
constexpr uint32_t MODIFY_RIGHTS = 0x1301bf;

ObservedAce Ace(const Sid& trustee, uint32_t access, bool inherited = false, AceKind kind = AceKind::Allow,
                bool inherit_only = false)
{
    ObservedAce ace;
    ace.kind = kind;
    ace.trustee = trustee;
    ace.mask = access;
    ace.inherited = inherited;
    ace.inherit_only = inherit_only;

    return ace;
}

//! A DACL owned by \p owner, holding \p aces, protected unless \p is_protected is false.
ObservedAcl Acl(const std::optional<Sid>& owner, std::vector<ObservedAce> aces, bool is_protected = true)
{
    ObservedAcl acl;
    acl.owner = owner;
    acl.dacl_present = true;
    acl.is_protected = is_protected;
    acl.aces = std::move(aces);

    return acl;
}

//! An access list owned by \p owner with no DACL, which gives everyone full access.
ObservedAcl NoDacl(const Sid& owner)
{
    ObservedAcl acl;
    acl.owner = owner;

    return acl;
}

fsmodes::WinAclVerdict Classify(const ObservedAcl& acl, const Sid& self = Self())
{
    return fsmodes::ClassifyWinAcl(acl, self);
}

void CheckHas(const std::string& text, const std::string& part)
{
    BOOST_CHECK_MESSAGE(text.find(part) != std::string::npos, "'" << text << "' does not contain '" << part << "'");
}

void CheckHasNot(const std::string& text, const std::string& part)
{
    BOOST_CHECK_MESSAGE(text.find(part) == std::string::npos, "'" << text << "' contains '" << part << "'");
}

//! How many times \p part occurs in \p text.
size_t Occurrences(const std::string& text, const std::string& part)
{
    size_t count = 0;
    for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++count;
    return count;
}

//! Whether \p text is one line of printable ASCII.
bool IsAsciiLine(const std::string& text)
{
    return std::all_of(text.begin(), text.end(), [](char c) { return c >= 0x20 && c < 0x7f; });
}

} // anonymous namespace

#ifndef WIN32

namespace {

//! A scratch directory under the test data directory, removed when the case ends.
struct ScratchDir
{
    fs::path m_path;

    ScratchDir()
    {
        std::string templ = (GetDataDir() / "fsmodes_XXXXXX").string();
        BOOST_REQUIRE(::mkdtemp(templ.data()) != nullptr);
        m_path = templ;
    }

    ~ScratchDir()
    {
        boost::system::error_code ec;
        fs::remove_all(m_path, ec);
    }
};

void MakeFile(const fs::path& path, mode_t mode)
{
    {
        fsbridge::ofstream stream(path);
        stream << "fsmodes test\n";
    }
    BOOST_REQUIRE_EQUAL(::chmod(path.string().c_str(), mode), 0);
}

void MakeDir(const fs::path& path, mode_t mode)
{
    fs::create_directories(path);
    BOOST_REQUIRE_EQUAL(::chmod(path.string().c_str(), mode), 0);
}

unsigned ModeOf(const fs::path& path)
{
    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat(path.string().c_str(), &st), 0);

    // st_mode is mode_t; the permission bits fit an unsigned.
    return static_cast<unsigned>(st.st_mode & 07777);
}

} // anonymous namespace

#endif // !WIN32

BOOST_AUTO_TEST_SUITE(fsmodes_tests)

// The Windows rule (ClassifyWinAcl) and its warning (WinAclWarning) are pure, so they are
// tested on every platform.

// A file this account owns that the Users group can read is a finding this account can fix.
BOOST_AUTO_TEST_CASE(win_acl_owned_users_read_is_wide)
{
    const ObservedAcl acl = Acl(Self(), {
        Ace(Self(), FILE_ALL_ACCESS),
        Ace(wellknown::System(), FILE_ALL_ACCESS),
        Ace(wellknown::Users(), FILE_READ_DATA),
    });

    BOOST_CHECK(Classify(acl) == fsmodes::WinAclVerdict::Wide);
}

// A data directory an earlier release made under %APPDATA% inherits this account, SYSTEM and
// Administrators, each with full access. Nobody else can read or change it, so there is
// nothing to report.
BOOST_AUTO_TEST_CASE(win_acl_owned_inherited_appdata_is_clean)
{
    const ObservedAcl acl = Acl(Self(), {
        Ace(wellknown::System(), FILE_ALL_ACCESS, /*inherited=*/true),
        Ace(wellknown::Administrators(), FILE_ALL_ACCESS, /*inherited=*/true),
        Ace(Self(), FILE_ALL_ACCESS, /*inherited=*/true),
    }, /*is_protected=*/false);

    BOOST_CHECK(Classify(acl) == fsmodes::WinAclVerdict::Clean);
}

// The service layout's configuration file: owned by Administrators, controlled by them and
// SYSTEM, and read by the service.
BOOST_AUTO_TEST_CASE(win_acl_service_conf_is_clean)
{
    const ObservedAcl acl = Acl(wellknown::Administrators(), {
        Ace(wellknown::Administrators(), FILE_ALL_ACCESS),
        Ace(wellknown::System(), FILE_ALL_ACCESS),
        Ace(ServiceSid(), READ_RIGHTS),
    });

    BOOST_CHECK(Classify(acl, ServiceSid()) == fsmodes::WinAclVerdict::Clean);
}

// Each right that changes a path, given to the Users group on a path this account does not
// own, is reported as not ours to fix. FILE_DELETE_CHILD is among them: on a directory it
// deletes any entry inside, such as wallet.dat.
BOOST_AUTO_TEST_CASE(win_acl_not_owned_users_write_warns)
{
    for (const uint32_t right : {uint32_t{FILE_WRITE_DATA}, uint32_t{FILE_APPEND_DATA}, uint32_t{FILE_WRITE_EA},
                                 uint32_t{FILE_WRITE_ATTRIBUTES}, uint32_t{DELETE}, uint32_t{WRITE_DAC},
                                 uint32_t{WRITE_OWNER}, uint32_t{GENERIC_WRITE}, uint32_t{GENERIC_ALL},
                                 uint32_t{FILE_DELETE_CHILD}}) {
        const ObservedAcl acl = Acl(wellknown::Administrators(), {
            Ace(wellknown::Administrators(), FILE_ALL_ACCESS),
            Ace(wellknown::System(), FILE_ALL_ACCESS),
            Ace(Self(), READ_RIGHTS),
            Ace(wellknown::Users(), right),
        });

        BOOST_CHECK_MESSAGE(Classify(acl) == fsmodes::WinAclVerdict::WideNotOwned, "right 0x" << std::hex << right);
    }
}

// The same for each right that reads it.
BOOST_AUTO_TEST_CASE(win_acl_not_owned_users_read_warns)
{
    for (const uint32_t right : {uint32_t{FILE_READ_DATA}, uint32_t{GENERIC_READ}, READ_RIGHTS}) {
        const ObservedAcl acl = Acl(wellknown::Administrators(), {
            Ace(wellknown::Administrators(), FILE_ALL_ACCESS),
            Ace(wellknown::System(), FILE_ALL_ACCESS),
            Ace(Self(), READ_RIGHTS),
            Ace(wellknown::Users(), right),
        });

        BOOST_CHECK_MESSAGE(Classify(acl) == fsmodes::WinAclVerdict::WideNotOwned, "right 0x" << std::hex << right);
    }
}

// No DACL at all gives every account full access.
BOOST_AUTO_TEST_CASE(win_acl_missing_dacl_owned_is_wide)
{
    BOOST_CHECK(Classify(NoDacl(Self())) == fsmodes::WinAclVerdict::Wide);
    BOOST_CHECK(Classify(NoDacl(wellknown::Administrators())) == fsmodes::WinAclVerdict::WideNotOwned);
}

// An owner can always change the access list, so an owner outside the allowed accounts is a
// finding even with nobody else on the list, and so is an owner that could not be read.
// SYSTEM and Administrators are allowed owners.
BOOST_AUTO_TEST_CASE(win_acl_foreign_owner_is_a_finding)
{
    const std::vector<ObservedAce> owner_only{Ace(Self(), FILE_ALL_ACCESS), Ace(wellknown::System(), FILE_ALL_ACCESS)};

    BOOST_CHECK(Classify(Acl(OtherUser(), owner_only)) == fsmodes::WinAclVerdict::WideNotOwned);
    BOOST_CHECK(Classify(Acl(std::nullopt, owner_only)) == fsmodes::WinAclVerdict::WideNotOwned);
    BOOST_CHECK(Classify(Acl(wellknown::System(), owner_only)) == fsmodes::WinAclVerdict::Clean);
    BOOST_CHECK(Classify(Acl(wellknown::Administrators(), owner_only)) == fsmodes::WinAclVerdict::Clean);
    BOOST_CHECK(Classify(Acl(Self(), owner_only)) == fsmodes::WinAclVerdict::Clean);
}

// Rights that neither read nor change what a file holds, a deny entry and an entry that only
// what is created inside inherits are not findings. A conditional or object allow entry counts
// as an allow entry.
BOOST_AUTO_TEST_CASE(win_acl_counts_only_read_and_change_rights)
{
    const std::vector<ObservedAce> owner_only{Ace(Self(), FILE_ALL_ACCESS), Ace(wellknown::System(), FILE_ALL_ACCESS)};

    const auto with = [&owner_only](const ObservedAce& extra) {
        std::vector<ObservedAce> aces = owner_only;
        aces.push_back(extra);
        return Acl(Self(), std::move(aces));
    };

    const uint32_t harmless = FILE_READ_ATTRIBUTES | FILE_READ_EA | READ_CONTROL | SYNCHRONIZE | FILE_EXECUTE;
    BOOST_CHECK(Classify(with(Ace(wellknown::Users(), harmless))) == fsmodes::WinAclVerdict::Clean);
    BOOST_CHECK(Classify(with(Ace(wellknown::Users(), FILE_ALL_ACCESS, false, AceKind::Deny))) ==
                fsmodes::WinAclVerdict::Clean);
    BOOST_CHECK(Classify(with(Ace(CreatorOwner(), FILE_ALL_ACCESS, false, AceKind::Allow, /*inherit_only=*/true))) ==
                fsmodes::WinAclVerdict::Clean);
    BOOST_CHECK(Classify(with(Ace(wellknown::Users(), FILE_READ_DATA, false, AceKind::OtherAllow))) ==
                fsmodes::WinAclVerdict::Wide);
}

// The warning for a directory this account owns that inherits a volume root's list names each
// other account and what it can do, and gives one icacls command that makes the directory
// owner-only, as the core creates one, for what inherits from it too.
BOOST_AUTO_TEST_CASE(win_acl_warning_for_an_owned_directory_makes_it_owner_only)
{
    const fs::path path = "D:\\Gridcoin";
    const ObservedAcl acl = Acl(Self(), {
        Ace(wellknown::Administrators(), FILE_ALL_ACCESS, /*inherited=*/true),
        Ace(wellknown::System(), FILE_ALL_ACCESS, /*inherited=*/true),
        Ace(Self(), FILE_ALL_ACCESS, /*inherited=*/true),
        Ace(wellknown::Users(), READ_EXECUTE_RIGHTS, /*inherited=*/true),
        Ace(AuthenticatedUsers(), MODIFY_RIGHTS, /*inherited=*/true),
        Ace(CreatorOwner(), FILE_ALL_ACCESS, /*inherited=*/true, AceKind::Allow, /*inherit_only=*/true),
    }, /*is_protected=*/false);
    BOOST_REQUIRE(Classify(acl) == fsmodes::WinAclVerdict::Wide);

    const std::string warning = fsmodes::WinAclWarning(path, acl, Self(), /*directory=*/true);
    BOOST_TEST_MESSAGE(warning);

    CheckHas(warning, "WARNING: D:\\Gridcoin ");
    CheckHas(warning, "S-1-5-32-545 (Users) can read it (access mask 0x001200a9)");
    CheckHas(warning, "S-1-5-11 (Authenticated Users) can read and change it (access mask 0x001301bf)");
    CheckHasNot(warning, "S-1-3-0");
    CheckHasNot(warning, "S-1-5-32-544");
    CheckHas(warning, "icacls \"D:\\Gridcoin\" /inheritance:r /grant:r \"*" + FormatSid(Self()) +
                          ":(OI)(CI)F\" \"*S-1-5-18:(OI)(CI)F\" /remove:g *S-1-5-32-545 *S-1-5-11");
    BOOST_CHECK_MESSAGE(IsAsciiLine(warning), warning);
}

// A directory entry whose only right that changes anything is FILE_DELETE_CHILD (icacls
// "(DC)") lets that account delete what the directory holds. It is reported, the account is
// named, and the command removes its entry.
BOOST_AUTO_TEST_CASE(win_acl_warning_for_a_delete_child_entry)
{
    const fs::path path = "D:\\Gridcoin";
    const ObservedAcl acl = Acl(Self(), {
        Ace(Self(), FILE_ALL_ACCESS),
        Ace(wellknown::System(), FILE_ALL_ACCESS),
        Ace(wellknown::Users(), FILE_DELETE_CHILD | SYNCHRONIZE),
    });
    BOOST_REQUIRE(Classify(acl) == fsmodes::WinAclVerdict::Wide);

    const std::string warning = fsmodes::WinAclWarning(path, acl, Self(), /*directory=*/true);
    BOOST_TEST_MESSAGE(warning);

    CheckHas(warning, "S-1-5-32-545 (Users) can change it (access mask 0x00100040)");
    CheckHas(warning, "/remove:g *S-1-5-32-545");
    BOOST_CHECK_MESSAGE(IsAsciiLine(warning), warning);

    // With read and execute too (icacls "RX" and "DC") the account can read and change it.
    const ObservedAcl read_and_delete = Acl(Self(), {
        Ace(Self(), FILE_ALL_ACCESS),
        Ace(wellknown::System(), FILE_ALL_ACCESS),
        Ace(wellknown::Users(), READ_EXECUTE_RIGHTS | FILE_DELETE_CHILD),
    });
    CheckHas(fsmodes::WinAclWarning(path, read_and_delete, Self(), /*directory=*/true),
             "S-1-5-32-545 (Users) can read and change it (access mask 0x001200e9)");
}

// For a file, nothing inherits the fix. Two entries for one account are named once, with the
// rights they give together.
BOOST_AUTO_TEST_CASE(win_acl_warning_for_an_owned_file)
{
    const fs::path path = "C:\\Users\\me\\AppData\\Roaming\\GridcoinResearch\\wallet.dat";
    const ObservedAcl acl = Acl(Self(), {
        Ace(Self(), FILE_ALL_ACCESS),
        Ace(wellknown::System(), FILE_ALL_ACCESS),
        Ace(wellknown::Users(), FILE_READ_DATA),
        Ace(wellknown::Users(), FILE_WRITE_DATA, false, AceKind::OtherAllow),
    });

    const std::string warning = fsmodes::WinAclWarning(path, acl, Self(), /*directory=*/false);

    CheckHas(warning, "S-1-5-32-545 (Users) can read and change it (access mask 0x00000003)");
    CheckHas(warning, "icacls \"" + path.string() + "\" /inheritance:r /grant:r \"*" + FormatSid(Self()) +
                          ":F\" \"*S-1-5-18:F\" /remove:g *S-1-5-32-545");
    CheckHasNot(warning, "(OI)(CI)");
    BOOST_CHECK_MESSAGE(IsAsciiLine(warning), warning);

    // With no DACL there is nobody's entry to remove.
    const std::string no_dacl = fsmodes::WinAclWarning(path, NoDacl(Self()), Self(), /*directory=*/false);
    CheckHas(no_dacl, "no DACL");
    CheckHas(no_dacl, "icacls \"" + path.string() + "\" /inheritance:r /grant:r \"*" + FormatSid(Self()) +
                          ":F\" \"*S-1-5-18:F\"");
    CheckHasNot(no_dacl, "/remove");
}

// For a path another account owns, the fix takes the other accounts' access away and gives this
// account no entry. With an allowed owner (here Administrators) the owner stays, and its owner,
// an administrator, or this account where it may change the permissions can run the fix. An
// entry that is inherited is first made the path's own, so that it can be removed. With an owner
// outside the allowed accounts, or one that could not be read, the fix makes this account the
// owner, which an administrator can do, and this account too where it may take ownership.
BOOST_AUTO_TEST_CASE(win_acl_warning_for_a_path_another_account_owns)
{
    const fs::path conf = "C:\\ProgramData\\Gridcoin\\gridcoinresearch.conf";
    const ObservedAcl inherited = Acl(wellknown::Administrators(), {
        Ace(wellknown::Administrators(), FILE_ALL_ACCESS),
        Ace(wellknown::System(), FILE_ALL_ACCESS),
        Ace(ServiceSid(), READ_RIGHTS),
        Ace(wellknown::Users(), READ_RIGHTS, /*inherited=*/true),
    }, /*is_protected=*/false);
    BOOST_REQUIRE(Classify(inherited, ServiceSid()) == fsmodes::WinAclVerdict::WideNotOwned);

    const std::string warning = fsmodes::WinAclWarning(conf, inherited, ServiceSid(), /*directory=*/false);
    BOOST_TEST_MESSAGE(warning);

    CheckHas(warning, "S-1-5-32-545 (Users) can read it");
    CheckHas(warning, "owned by S-1-5-32-544 (Administrators)");
    CheckHas(warning, "icacls \"" + conf.string() + "\" /inheritance:d && icacls \"" + conf.string() +
                          "\" /remove:g *S-1-5-32-545");
    CheckHas(warning, "It is owned by S-1-5-32-544 (Administrators). To fix it, in a command prompt run as its owner, "
                      "as an administrator (elevated), or as this account where it may change the permissions, remove "
                      "the other accounts' access: icacls \"" + conf.string() + "\" /inheritance:d");
    CheckHasNot(warning, "only its owner");
    CheckHasNot(warning, "/grant");
    CheckHasNot(warning, "/inheritance:r");
    CheckHasNot(warning, "/setowner");
    BOOST_CHECK_MESSAGE(IsAsciiLine(warning), warning);

    // An entry of the path's own needs no copy of the inherited ones.
    ObservedAcl own = inherited;
    own.aces.back().inherited = false;
    const std::string explicit_entry = fsmodes::WinAclWarning(conf, own, ServiceSid(), /*directory=*/false);
    CheckHas(explicit_entry, "icacls \"" + conf.string() + "\" /remove:g *S-1-5-32-545");
    CheckHasNot(explicit_entry, "/inheritance");

    // An owner that is another user can change the access list. The warning says who can make
    // this account the owner instead: an administrator, and this account where it may take
    // ownership (here it holds FILE_ALL_ACCESS, WRITE_OWNER included).
    const ObservedAcl foreign_owner = Acl(OtherUser(), {
        Ace(Self(), FILE_ALL_ACCESS),
        Ace(wellknown::System(), FILE_ALL_ACCESS),
    });
    const std::string other = fsmodes::WinAclWarning(conf, foreign_owner, Self(), /*directory=*/false);
    BOOST_TEST_MESSAGE(other);
    CheckHas(other, "owned by " + FormatSid(OtherUser()) + ", which can change its access list");
    BOOST_CHECK_EQUAL(Occurrences(other, FormatSid(OtherUser())), 1U);
    CheckHas(other, "To fix it, in a command prompt run as an administrator (elevated), or as this account where it "
                    "may take ownership, make this account its owner: icacls \"" + conf.string() + "\" /setowner *" +
                        FormatSid(Self()));
    CheckHasNot(other, "Only an administrator");
    CheckHasNot(other, "/remove");
    CheckHasNot(other, "/grant");
    BOOST_CHECK_MESSAGE(IsAsciiLine(other), other);

    // With other accounts on its list as well, they are removed after the owner is changed.
    ObservedAcl foreign_and_users = foreign_owner;
    foreign_and_users.aces.push_back(Ace(wellknown::Users(), READ_RIGHTS));
    const std::string both = fsmodes::WinAclWarning(conf, foreign_and_users, Self(), /*directory=*/false);
    CheckHas(both, "/setowner *" + FormatSid(Self()) + ", then remove the other accounts' access: icacls \"" +
                       conf.string() + "\" /remove:g *S-1-5-32-545");

    // So is an owner that could not be read.
    ObservedAcl unknown_owner = foreign_owner;
    unknown_owner.owner.reset();
    const std::string unknown = fsmodes::WinAclWarning(conf, unknown_owner, Self(), /*directory=*/false);
    CheckHas(unknown, "its owner could not be read");
    CheckHas(unknown, "/setowner *" + FormatSid(Self()));
}

// A path on a volume that keeps no access lists is open to every account, and the warning
// points to an NTFS volume rather than to an access list that cannot be set.
BOOST_AUTO_TEST_CASE(win_no_acl_volume_warning_points_to_ntfs)
{
    const fs::path path = "E:\\GridcoinResearch";
    const std::string warning = fsmodes::WinNoAclVolumeWarning(path);
    BOOST_TEST_MESSAGE(warning);

    CheckHas(warning, "WARNING: E:\\GridcoinResearch ");
    CheckHas(warning, "FAT or exFAT");
    CheckHas(warning, "NTFS");
    CheckHasNot(warning, "/grant");
    CheckHasNot(warning, "/remove");
    BOOST_CHECK_MESSAGE(IsAsciiLine(warning), warning);
}

// Why a Windows path is not this account's, worded for the conf writers' log lines and the
// backup note: the owner by name and SID and the elevated icacls command that makes this
// account the owner, or that a link is not followed, or that a FAT volume shows no owner.
BOOST_AUTO_TEST_CASE(win_not_owned_text_names_the_owner_and_the_fix)
{
    const fs::path conf = "C:\\Users\\jim\\AppData\\Roaming\\GridcoinResearch\\gridcoinresearch.conf";

    fsmodes::WinOwnership foreign;
    foreign.kind = fsmodes::WinOwnership::Kind::Foreign;
    foreign.owner = wellknown::Administrators();
    foreign.owner_name = "BUILTIN\\Administrators";
    const std::string admins = fsmodes::WinNotOwnedText(conf, foreign, Self(), "PC\\jim", /*service=*/false);
    BOOST_TEST_MESSAGE(admins);
    CheckHas(admins, "owned by BUILTIN\\Administrators (S-1-5-32-544)");
    CheckHas(admins, "not by this account, PC\\jim (" + FormatSid(Self()) + ")");
    CheckHas(admins, "in a command prompt run as an administrator (elevated), or as this account where it may "
                     "change the owner");
    CheckHas(admins, "icacls \"" + conf.string() + "\" /setowner *" + FormatSid(Self()));
    BOOST_CHECK_MESSAGE(IsAsciiLine(admins), admins);

    // The service's own configuration file belongs to the administrators by design, and the service
    // only reads it: no text gives the service its ownership.
    const fs::path service_conf = "C:\\ProgramData\\Gridcoin\\conf\\mainnet.conf";
    const Sid service_sid = SidOf("S-1-5-80-3522172824-109097731-499206491-3348895147-1114477730");
    const std::string service =
        fsmodes::WinNotOwnedText(service_conf, foreign, service_sid, "NT SERVICE\\gridcoinresearchd", /*service=*/true);
    BOOST_TEST_MESSAGE(service);
    CheckHas(service, "owned by BUILTIN\\Administrators (S-1-5-32-544)");
    CheckHas(service, "belongs to the administrators by design");
    CheckHasNot(service, "/setowner");
    CheckHasNot(service, "icacls");
    BOOST_CHECK_MESSAGE(IsAsciiLine(service), service);
    for (const fsmodes::WinOwnership::Kind kind :
         {fsmodes::WinOwnership::Kind::Link, fsmodes::WinOwnership::Kind::NoAclVolume,
          fsmodes::WinOwnership::Kind::Unreadable, fsmodes::WinOwnership::Kind::Unopened}) {
        fsmodes::WinOwnership other;
        other.kind = kind;
        CheckHasNot(fsmodes::WinNotOwnedText(service_conf, other, service_sid, "", /*service=*/true), "/setowner");
    }

    // Without names, the SIDs alone.
    foreign.owner_name.clear();
    const std::string unnamed = fsmodes::WinNotOwnedText(conf, foreign, Self(), "", /*service=*/false);
    CheckHas(unnamed, "owned by S-1-5-32-544,");
    CheckHas(unnamed, "not by this account, " + FormatSid(Self()));
    CheckHasNot(unnamed, "()");

    fsmodes::WinOwnership link;
    link.kind = fsmodes::WinOwnership::Kind::Link;
    const std::string linked = fsmodes::WinNotOwnedText(conf, link, Self(), "PC\\jim", /*service=*/false);
    CheckHas(linked, "link");
    CheckHasNot(linked, "/setowner");

    fsmodes::WinOwnership fat;
    fat.kind = fsmodes::WinOwnership::Kind::NoAclVolume;
    const std::string no_owner = fsmodes::WinNotOwnedText(conf, fat, Self(), "PC\\jim", /*service=*/false);
    CheckHas(no_owner, "FAT or exFAT");
    CheckHasNot(no_owner, "/setowner");

    fsmodes::WinOwnership unreadable;
    unreadable.kind = fsmodes::WinOwnership::Kind::Unreadable;
    unreadable.error = "GetSecurityInfo failed with error 5";
    CheckHas(fsmodes::WinNotOwnedText(conf, unreadable, Self(), "", /*service=*/false), "GetSecurityInfo failed with error 5");

    fsmodes::WinOwnership unopened;
    unopened.kind = fsmodes::WinOwnership::Kind::Unopened;
    unopened.error = "The system cannot find the file specified";
    CheckHas(fsmodes::WinNotOwnedText(conf, unopened, Self(), "", /*service=*/false), "The system cannot find the file specified");

    fsmodes::WinOwnership owned;
    owned.kind = fsmodes::WinOwnership::Kind::Owned;
    BOOST_CHECK(fsmodes::WinNotOwnedText(conf, owned, Self(), "PC\\jim", /*service=*/false).empty());
}

// Only a reparse point whose tag is a name surrogate (a symbolic link, a junction, a mount
// point) is a link to the check. Others, such as a cloud file's placeholder, a deduplicated
// file or an AF_UNIX socket, are the file itself. Without the reparse attribute the tag means
// nothing.
BOOST_AUTO_TEST_CASE(classify_win_entry_counts_name_surrogates_as_links)
{
    constexpr uint32_t DIRECTORY = 0x10, ARCHIVE = 0x20, REPARSE_POINT = 0x400;
    constexpr uint32_t TAG_SYMLINK = 0xA000000C, TAG_MOUNT_POINT = 0xA0000003, TAG_CLOUD = 0x9000001A,
                       TAG_DEDUP = 0x80000013, TAG_AF_UNIX = 0x80000023;

    const auto check = [](uint32_t attributes, uint32_t tag, bool directory, bool reparse, bool link) {
        const fsmodes::WinEntry entry = fsmodes::ClassifyWinEntry(attributes, tag);
        BOOST_CHECK_MESSAGE(entry.directory == directory && entry.reparse == reparse && entry.link == link,
                            "attributes 0x" << std::hex << attributes << ", tag 0x" << tag);
    };

    check(ARCHIVE, 0, false, false, false);
    check(DIRECTORY, 0, true, false, false);
    check(ARCHIVE | REPARSE_POINT, TAG_SYMLINK, false, true, true);
    check(DIRECTORY | REPARSE_POINT, TAG_SYMLINK, true, true, true);
    check(DIRECTORY | REPARSE_POINT, TAG_MOUNT_POINT, true, true, true);
    check(ARCHIVE | REPARSE_POINT, TAG_CLOUD, false, true, false);
    check(DIRECTORY | REPARSE_POINT, TAG_CLOUD, true, true, false);
    check(ARCHIVE | REPARSE_POINT, TAG_DEDUP, false, true, false);
    check(ARCHIVE | REPARSE_POINT, TAG_AF_UNIX, false, true, false);
    check(ARCHIVE, TAG_SYMLINK, false, false, false);
}

// A new file whose creation with the owner-only list failed is created again without it only
// where the list itself was refused, and the reason says by whom. Access denied counts only from
// a network share; any other failure creates nothing.
BOOST_AUTO_TEST_CASE(acl_refused_by_names_the_volume_or_the_share)
{
    constexpr uint32_t WIN32_ACCESS_DENIED = 5, WIN32_PATH_NOT_FOUND = 3, WIN32_NOT_SUPPORTED = 50,
                       WIN32_FILE_EXISTS = 80, WIN32_INVALID_PARAMETER = 87, WIN32_DISK_FULL = 112,
                       WIN32_INVALID_OWNER = 1307, WIN32_INVALID_PRIMARY_GROUP = 1308, WIN32_PRIVILEGE_NOT_HELD = 1314,
                       WIN32_INVALID_ACL = 1336, WIN32_INVALID_SECURITY_DESCR = 1338;
#ifdef WIN32
    static_assert(WIN32_ACCESS_DENIED == ERROR_ACCESS_DENIED && WIN32_PATH_NOT_FOUND == ERROR_PATH_NOT_FOUND &&
                  WIN32_NOT_SUPPORTED == ERROR_NOT_SUPPORTED && WIN32_FILE_EXISTS == ERROR_FILE_EXISTS &&
                  WIN32_INVALID_PARAMETER == ERROR_INVALID_PARAMETER && WIN32_DISK_FULL == ERROR_DISK_FULL &&
                  WIN32_INVALID_OWNER == ERROR_INVALID_OWNER &&
                  WIN32_INVALID_PRIMARY_GROUP == ERROR_INVALID_PRIMARY_GROUP &&
                  WIN32_PRIVILEGE_NOT_HELD == ERROR_PRIVILEGE_NOT_HELD && WIN32_INVALID_ACL == ERROR_INVALID_ACL &&
                  WIN32_INVALID_SECURITY_DESCR == ERROR_INVALID_SECURITY_DESCR);
#endif

    for (const uint32_t error : {WIN32_NOT_SUPPORTED, WIN32_INVALID_OWNER, WIN32_INVALID_PRIMARY_GROUP,
                                 WIN32_PRIVILEGE_NOT_HELD, WIN32_INVALID_ACL, WIN32_INVALID_SECURITY_DESCR}) {
        BOOST_CHECK_EQUAL(fsmodes::AclRefusedBy(error, /*remote=*/false).value_or("nothing"), "the volume");
        BOOST_CHECK_EQUAL(fsmodes::AclRefusedBy(error, /*remote=*/true).value_or("nothing"), "the network share");
    }

    BOOST_CHECK(!fsmodes::AclRefusedBy(WIN32_ACCESS_DENIED, /*remote=*/false));
    BOOST_CHECK_EQUAL(fsmodes::AclRefusedBy(WIN32_ACCESS_DENIED, /*remote=*/true).value_or("nothing"),
                      "the network share");

    for (const uint32_t error : {WIN32_PATH_NOT_FOUND, WIN32_FILE_EXISTS, WIN32_INVALID_PARAMETER, WIN32_DISK_FULL}) {
        BOOST_CHECK_MESSAGE(!fsmodes::AclRefusedBy(error, /*remote=*/false), "error " << error);
        BOOST_CHECK_MESSAGE(!fsmodes::AclRefusedBy(error, /*remote=*/true), "error " << error);
    }
}

// The directory's final path decides whether it is on a share: the UNC form is what a network
// redirector serves, however the directory was reached. Where that path could not be read, the
// check on the name decides, so a refusal of access on a share is still retried.
BOOST_AUTO_TEST_CASE(network_share_from_the_final_path_or_else_the_name)
{
    int asked = 0;
    const auto by_name = [&asked](bool answer) {
        return std::function<bool()>([&asked, answer] {
            ++asked;
            return answer;
        });
    };

    BOOST_CHECK(fsmodes::OnNetworkShare(std::wstring{L"\\\\?\\UNC\\server\\share\\backups"}, by_name(false)));
    BOOST_CHECK(!fsmodes::OnNetworkShare(std::wstring{L"\\\\?\\C:\\Users\\x\\backups"}, by_name(true)));
    BOOST_CHECK(!fsmodes::OnNetworkShare(std::wstring{L"\\\\?\\Volume{0}\\backups"}, by_name(true)));
    BOOST_CHECK_EQUAL(asked, 0);

    BOOST_CHECK(fsmodes::OnNetworkShare(std::nullopt, by_name(true)));
    BOOST_CHECK(!fsmodes::OnNetworkShare(std::nullopt, by_name(false)));
    BOOST_CHECK_EQUAL(asked, 2);
}

// A path the Windows check cannot open is reported with the reason, unless it does not exist.
// A data or backup directory moved behind a junction that Windows does not follow is named,
// with what to do, rather than passed over in silence.
BOOST_AUTO_TEST_CASE(unopened_path_is_reported_unless_it_does_not_exist)
{
    constexpr uint32_t WIN32_FILE_NOT_FOUND = 2, WIN32_PATH_NOT_FOUND = 3, WIN32_ACCESS_DENIED = 5;
    const fs::path moved{"C:/Users/x/AppData/Roaming/GridcoinResearch"};

    BOOST_CHECK(!fsmodes::UnopenedPathFinding(moved, WIN32_FILE_NOT_FOUND, "not found"));
    BOOST_CHECK(!fsmodes::UnopenedPathFinding(moved, WIN32_PATH_NOT_FOUND, "not found"));

    const std::optional<fsmodes::ModeFinding> untrusted =
        fsmodes::UnopenedPathFinding(moved, util::winacl::WIN32_UNTRUSTED_MOUNT_POINT, "ignored");
    BOOST_REQUIRE(untrusted);
    BOOST_CHECK(untrusted->path == moved);
    BOOST_CHECK(untrusted->kind == fsmodes::ModeFinding::Kind::Unreadable);
    BOOST_CHECK_EQUAL(untrusted->mode, 0U);
    for (const std::string& part : {"WARNING: " + moved.string(), std::string{"junction"},
                                    std::string{"administrator"}, std::string{"directory the junction leads to"},
                                    std::string{"not checked"}}) {
        CheckHas(untrusted->message, part);
    }
    CheckHasNot(untrusted->message, "ignored");
    BOOST_CHECK_MESSAGE(IsAsciiLine(untrusted->message), untrusted->message);

    const std::optional<fsmodes::ModeFinding> denied =
        fsmodes::UnopenedPathFinding(moved, WIN32_ACCESS_DENIED, "Access is denied.");
    BOOST_REQUIRE(denied);
    BOOST_CHECK(denied->kind == fsmodes::ModeFinding::Kind::Unreadable);
    CheckHas(denied->message, "WARNING: " + moved.string());
    CheckHas(denied->message, "(Access is denied.)");
    CheckHas(denied->message, "not checked");
}

// A directory the Windows check opened but could not list is reported, as the files in it were
// not checked, unless it is gone since it was opened.
BOOST_AUTO_TEST_CASE(unlisted_directory_is_reported_unless_it_is_gone)
{
    constexpr uint32_t WIN32_FILE_NOT_FOUND = 2, WIN32_PATH_NOT_FOUND = 3, WIN32_ACCESS_DENIED = 5;
    const fs::path backups{"C:/Users/x/AppData/Roaming/GridcoinResearch/walletbackups"};

    BOOST_CHECK(!fsmodes::UnlistedDirectoryFinding(backups, WIN32_FILE_NOT_FOUND, "not found"));
    BOOST_CHECK(!fsmodes::UnlistedDirectoryFinding(backups, WIN32_PATH_NOT_FOUND, "not found"));

    const std::optional<fsmodes::ModeFinding> denied =
        fsmodes::UnlistedDirectoryFinding(backups, WIN32_ACCESS_DENIED, "Access is denied.");
    BOOST_REQUIRE(denied);
    BOOST_CHECK(denied->path == backups);
    BOOST_CHECK(denied->kind == fsmodes::ModeFinding::Kind::Unreadable);
    BOOST_CHECK_EQUAL(denied->mode, 0U);
    CheckHas(denied->message, "WARNING: " + backups.string());
    CheckHas(denied->message, "could not be listed (Access is denied.)");
    CheckHas(denied->message, "not checked");
    BOOST_CHECK_MESSAGE(IsAsciiLine(denied->message), denied->message);
}

// The startup warning for a backup directory whose files cannot be owner-only names the
// cause and says what it means: backups are still written, and other users may read them.
// It does not tell the user to make a writable directory writable.
BOOST_AUTO_TEST_CASE(backup_dir_warning_names_the_cause)
{
    const fs::path dir = "/media/stick/backups";

    BOOST_CHECK(!fsmodes::BackupDirWarning(dir, {}));

    fsmodes::BackupDirCheck unrestricted;
    unrestricted.not_owner_only = "created /media/stick/backups but could not chmod it to 0700: "
                                  "Operation not permitted (1)";
    const std::optional<std::string> warning = fsmodes::BackupDirWarning(dir, unrestricted);
    BOOST_REQUIRE(warning);
    BOOST_CHECK_MESSAGE(warning->find(*unrestricted.not_owner_only) != std::string::npos, *warning);
    BOOST_CHECK_MESSAGE(warning->find(dir.string()) != std::string::npos, *warning);
    BOOST_CHECK_MESSAGE(warning->find("still written") != std::string::npos, *warning);
    BOOST_CHECK_MESSAGE(warning->find("other users") != std::string::npos, *warning);
    BOOST_CHECK_MESSAGE(warning->find("not writable") == std::string::npos, *warning);
    BOOST_CHECK_MESSAGE(warning->find("Make it writable") == std::string::npos, *warning);
#ifdef WIN32
    // On Windows the cause may be a volume without access lists or a share that refuses one.
    BOOST_CHECK_MESSAGE(warning->find("FAT or exFAT") != std::string::npos, *warning);
    BOOST_CHECK_MESSAGE(warning->find("network share") != std::string::npos, *warning);
#endif

    fsmodes::BackupDirCheck locked;
    locked.not_writable = "Permission denied (13)";
    const std::optional<std::string> refused = fsmodes::BackupDirWarning(dir, locked);
    BOOST_REQUIRE(refused);
    BOOST_CHECK_MESSAGE(refused->find("not writable") != std::string::npos, *refused);
    BOOST_CHECK_MESSAGE(refused->find("Permission denied (13)") != std::string::npos, *refused);
    BOOST_CHECK_MESSAGE(refused->find("will fail") != std::string::npos, *refused);
}

#ifndef WIN32

BOOST_AUTO_TEST_CASE(warns_on_wide_existing_files_and_changes_nothing)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;

    MakeDir(datadir, 0755);
    MakeFile(datadir / "wallet.dat", 0644);
    MakeDir(datadir / "database", 0755);
    MakeFile(datadir / "database" / "log.0000000001", 0644);
    MakeDir(datadir / "walletbackups", 0755);
    MakeFile(datadir / "walletbackups" / "wallet.dat-x", 0644);
    MakeFile(datadir / "gridcoinsettings.json", 0664);
    MakeFile(datadir / "debug.log", 0644);

    // A group-readable file and an owner-only file pass silently.
    MakeFile(datadir / "gridcoinresearch.conf", 0640);
    MakeFile(datadir / "walletbackups" / "wallet.dat-ok", 0600);

    const std::set<fs::path> all = {
        datadir,
        datadir / "wallet.dat",
        datadir / "database",
        datadir / "database" / "log.0000000001",
        datadir / "walletbackups",
        datadir / "walletbackups" / "wallet.dat-x",
        datadir / "walletbackups" / "wallet.dat-ok",
        datadir / "gridcoinsettings.json",
        datadir / "debug.log",
        datadir / "gridcoinresearch.conf",
    };

    std::vector<std::pair<fs::path, unsigned>> before;
    for (const auto& path : all) before.emplace_back(path, ModeOf(path));

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, datadir / "gridcoinresearch.conf",
                                                       datadir / "gridcoinsettings.json", datadir / "wallet.dat",
                                                       datadir / "walletbackups", datadir / "debug.log");

    // Nothing changed.
    for (const auto& [path, mode] : before) {
        BOOST_CHECK_MESSAGE(ModeOf(path) == mode, path.string() << " changed mode");
    }

    const std::set<fs::path> dirs = {datadir, datadir / "database", datadir / "walletbackups"};
    const std::set<fs::path> wide = {
        datadir,
        datadir / "wallet.dat",
        datadir / "database",
        datadir / "database" / "log.0000000001",
        datadir / "walletbackups",
        datadir / "walletbackups" / "wallet.dat-x",
        datadir / "gridcoinsettings.json",
        datadir / "debug.log",
    };

    BOOST_CHECK_EQUAL(findings.size(), wide.size());

    for (const auto& path : all) {
        BOOST_CHECK_EQUAL(HasFinding(findings, path), wide.count(path) == 1);
    }

    for (const auto& path : wide) {
        const auto& finding = FindingFor(findings, path);
        BOOST_CHECK(finding.kind == fsmodes::ModeFinding::Kind::WideMode);

        const std::string fix = std::string("chmod ") + (dirs.count(path) ? "700 " : "600 ") + path.string();
        BOOST_CHECK_MESSAGE(finding.message.find(fix) != std::string::npos,
                            "'" << finding.message << "' does not name '" << fix << "'");
    }
}

BOOST_AUTO_TEST_CASE(an_owner_only_layout_has_no_findings)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;

    MakeDir(datadir, 0700);
    MakeFile(datadir / "wallet.dat", 0600);
    MakeDir(datadir / "database", 0700);
    MakeFile(datadir / "database" / "log.0000000001", 0600);

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, datadir / "gridcoinresearch.conf",
                                                       datadir / "gridcoinsettings.json", datadir / "wallet.dat",
                                                       datadir / "walletbackups", datadir / "debug.log");

    BOOST_CHECK(findings.empty());
}

BOOST_AUTO_TEST_CASE(root_owned_conf_is_check_only)
{
    if (::geteuid() == 0) return;

    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;
    MakeDir(datadir, 0700);

    const fs::path passwd = "/etc/passwd";
    BOOST_REQUIRE(!fsmodes::OwnedByUs(passwd));

    const unsigned before = ModeOf(passwd);
    BOOST_REQUIRE(!fsmodes::NotOwnedModeAcceptable(before));

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, passwd, {}, {}, {}, {});

    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == passwd);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::WideModeNotOwned);
    BOOST_CHECK_EQUAL(ModeOf(passwd), before);

    // The pure rule: a root:group 0640 file passes silently.
    BOOST_CHECK(fsmodes::NotOwnedModeAcceptable(0640));
    BOOST_CHECK(fsmodes::NotOwnedModeAcceptable(0600));
    BOOST_CHECK(!fsmodes::NotOwnedModeAcceptable(0644));
    BOOST_CHECK(!fsmodes::NotOwnedModeAcceptable(0660));
    BOOST_CHECK(!fsmodes::NotOwnedModeAcceptable(0604));
}

BOOST_AUTO_TEST_CASE(symlink_not_followed)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;
    MakeDir(datadir, 0700);

    const fs::path target = datadir / "real_wallet";
    MakeFile(target, 0644);

    const fs::path wallet = datadir / "wallet.dat";
    fs::create_symlink(target, wallet);

    BOOST_CHECK(!fsmodes::OwnedByUs(wallet));

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, {}, {}, wallet, {}, {});

    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == wallet);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::Symlink);
    BOOST_CHECK_EQUAL(ModeOf(target), 0644U);
}

BOOST_AUTO_TEST_CASE(symlinked_datadir_root_is_followed)
{
    const ScratchDir scratch;

    // A data directory moved to another disk through a link is a legitimate setup. The link
    // itself is not reported, and the normal rule applies to its target.
    const fs::path narrow = scratch.m_path / "narrow";
    MakeDir(narrow, 0700);
    const fs::path narrow_link = scratch.m_path / "narrow_link";
    fs::create_symlink(narrow, narrow_link);

    BOOST_CHECK(fsmodes::CheckOwnerOnlyModes(narrow_link, {}, {}, {}, {}, {}).empty());

    const fs::path wide = scratch.m_path / "wide";
    MakeDir(wide, 0755);
    const fs::path wide_link = scratch.m_path / "wide_link";
    fs::create_symlink(wide, wide_link);

    const auto findings = fsmodes::CheckOwnerOnlyModes(wide_link, {}, {}, {}, {}, {});

    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == wide_link);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::WideMode);
    BOOST_CHECK_EQUAL(findings[0].mode, 0755U);
    BOOST_CHECK(findings[0].message.find("chmod 700 " + wide_link.string()) != std::string::npos);

    // Inside a linked data directory a symlinked wallet file is still reported and not followed.
    const fs::path real_wallet = narrow / "real_wallet";
    MakeFile(real_wallet, 0644);
    const fs::path wallet = narrow / "wallet.dat";
    fs::create_symlink(real_wallet, wallet);

    const auto inner = fsmodes::CheckOwnerOnlyModes(narrow_link, {}, {}, wallet, {}, {});

    BOOST_REQUIRE_EQUAL(inner.size(), 1U);
    BOOST_CHECK(inner[0].path == wallet);
    BOOST_CHECK(inner[0].kind == fsmodes::ModeFinding::Kind::Symlink);
}

// The backup directory, the configuration file and debug.log may be links to another
// disk, as the data directory may: the link is not reported, and the rule applies to
// its target. The files inside a linked backup directory keep the rule that a link is
// reported, and so does the settings file.
BOOST_AUTO_TEST_CASE(symlinked_backupdir_conf_and_debug_log_are_followed)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path / "data";
    MakeDir(datadir, 0700);

    const auto link = [](const fs::path& target, const fs::path& path) {
        fs::create_symlink(target, path);
        return path;
    };

    MakeDir(scratch.m_path / "backups_narrow", 0700);
    MakeFile(scratch.m_path / "conf_narrow", 0600);
    MakeFile(scratch.m_path / "log_narrow", 0600);
    const fs::path backups_narrow = link(scratch.m_path / "backups_narrow", scratch.m_path / "backups_narrow_link");
    const fs::path conf_narrow = link(scratch.m_path / "conf_narrow", scratch.m_path / "conf_narrow_link");
    const fs::path log_narrow = link(scratch.m_path / "log_narrow", scratch.m_path / "log_narrow_link");

    BOOST_CHECK(fsmodes::CheckOwnerOnlyModes(datadir, conf_narrow, {}, {}, backups_narrow, log_narrow).empty());

    MakeDir(scratch.m_path / "backups_wide", 0755);
    MakeFile(scratch.m_path / "conf_wide", 0644);
    MakeFile(scratch.m_path / "log_wide", 0604);
    const fs::path backups_wide = link(scratch.m_path / "backups_wide", scratch.m_path / "backups_wide_link");
    const fs::path conf_wide = link(scratch.m_path / "conf_wide", scratch.m_path / "conf_wide_link");
    const fs::path log_wide = link(scratch.m_path / "log_wide", scratch.m_path / "log_wide_link");

    const auto wide = fsmodes::CheckOwnerOnlyModes(datadir, conf_wide, {}, {}, backups_wide, log_wide);
    BOOST_CHECK_EQUAL(wide.size(), 3U);
    for (const auto& [path, mode] : std::vector<std::pair<fs::path, unsigned>>{
             {backups_wide, 0755U}, {conf_wide, 0644U}, {log_wide, 0604U}}) {
        const fsmodes::ModeFinding& finding = FindingFor(wide, path);
        BOOST_CHECK(finding.kind == fsmodes::ModeFinding::Kind::WideMode);
        BOOST_CHECK_EQUAL(finding.mode, mode);
    }

    // Inside the linked backup directory.
    MakeFile(scratch.m_path / "backups_narrow" / "wallet.dat-wide", 0644);
    MakeFile(scratch.m_path / "elsewhere", 0600);
    fs::create_symlink(scratch.m_path / "elsewhere", scratch.m_path / "backups_narrow" / "wallet.dat-link");
    const fs::path settings = link(scratch.m_path / "elsewhere", datadir / "gridcoinsettings.json");

    const auto inner = fsmodes::CheckOwnerOnlyModes(datadir, {}, settings, {}, backups_narrow, {});
    BOOST_CHECK_EQUAL(inner.size(), 3U);
    BOOST_CHECK(FindingFor(inner, backups_narrow / "wallet.dat-wide").kind == fsmodes::ModeFinding::Kind::WideMode);
    BOOST_CHECK(FindingFor(inner, backups_narrow / "wallet.dat-link").kind == fsmodes::ModeFinding::Kind::Symlink);
    BOOST_CHECK(FindingFor(inner, settings).kind == fsmodes::ModeFinding::Kind::Symlink);
}

BOOST_AUTO_TEST_CASE(backupdir_not_writable_reported)
{
    const ScratchDir scratch;

    // A directory that does not exist yet is created, and is writable.
    const fs::path fresh = scratch.m_path / "fresh" / "backups";
    BOOST_CHECK(!fsmodes::CheckBackupDir(fresh).not_writable);
    BOOST_CHECK(fs::is_directory(fresh));
    BOOST_CHECK_EQUAL(ModeOf(fresh), 0700U);
    BOOST_CHECK(fs::is_empty(fresh));

    if (::geteuid() == 0) return;

    const fs::path locked = scratch.m_path / "locked";
    MakeDir(locked, 0500);

    const auto reason = fsmodes::CheckBackupDir(locked).not_writable;
    BOOST_CHECK(reason.has_value() && !reason->empty());

    // Let the scratch directory be removed.
    BOOST_REQUIRE_EQUAL(::chmod(locked.string().c_str(), 0700), 0);
}

// A probe left behind by an earlier start that was killed between creating and
// removing it does not make the directory look unwritable. Under PrivatePIDs= every
// start has the same pid, so a probe named by pid and count would collide.
BOOST_AUTO_TEST_CASE(a_leftover_write_probe_does_not_fail_the_check)
{
    const ScratchDir scratch;
    const fs::path backups = scratch.m_path / "backups";
    MakeDir(backups, 0700);

    for (int count = 0; count < 200; ++count) {
        MakeFile(backups / (".gridcoin-write-test-" + ToString(::getpid()) + "-" + ToString(count)), 0600);
    }

    BOOST_CHECK(!fsmodes::CheckBackupDir(backups).not_writable);
    BOOST_CHECK(!fsmodes::CheckBackupDir(backups).not_writable);
}

// A probe name that is already taken is removed and the probe tried once more. A
// name that cannot be freed is reported.
BOOST_AUTO_TEST_CASE(a_taken_probe_name_is_freed_and_tried_once_more)
{
    const ScratchDir scratch;
    const fs::path backups = scratch.m_path / "backups";
    MakeDir(backups, 0700);

    const auto fixed = [] { return std::string{"probe"}; };

    MakeFile(backups / "probe", 0600);
    BOOST_CHECK(!fsmodes::CheckBackupDir(backups, fixed).not_writable);
    BOOST_CHECK(!fs::exists(backups / "probe"));

    // A directory of that name is not removed by unlink, so the second try fails too.
    MakeDir(backups / "probe", 0700);
    const auto reason = fsmodes::CheckBackupDir(backups, fixed).not_writable;
    BOOST_CHECK(reason.has_value() && !reason->empty());
    BOOST_CHECK(fs::is_directory(backups / "probe"));
}

// A backup directory that cannot be restricted is kept only when chmod's failure means the
// filesystem keeps no POSIX modes. Any other failure is refused as before.
BOOST_AUTO_TEST_CASE(only_a_filesystem_without_modes_is_tolerated)
{
    for (const int err : {EPERM, ENOTSUP, EOPNOTSUPP, EROFS}) {
        BOOST_CHECK_MESSAGE(util::RestrictErrorMeansNoModes(err), "errno " << err);
    }
    for (const int err : {EIO, EACCES, ENOENT, ENOSPC, EINVAL, ENAMETOOLONG, ELOOP, EFAULT}) {
        BOOST_CHECK_MESSAGE(!util::RestrictErrorMeansNoModes(err), "errno " << err);
    }
}

// A backup directory whose files cannot be owner-only is kept and reported, and still
// accepts files. On a filesystem with POSIX modes the directory is created owner-only
// and nothing is reported. The other branch runs where a path named "unrestrictable"
// behaves like FAT.
BOOST_AUTO_TEST_CASE(an_unrestrictable_backup_directory_is_kept_and_reported)
{
    const ScratchDir scratch;
    const UmaskGuard permissive(0);
    const fs::path backups = scratch.m_path / "unrestrictable" / "backups";

    for (int pass = 0; pass < 2; ++pass) {
        // The first pass creates the directory, the second finds it there.
        const fsmodes::BackupDirCheck check = fsmodes::CheckBackupDir(backups);
        BOOST_CHECK(!check.not_writable);
        BOOST_REQUIRE(fs::is_directory(backups));
        if (ModeOf(backups) == 0700U) {
            BOOST_CHECK_MESSAGE(!check.not_owner_only, *check.not_owner_only);
        } else {
            BOOST_REQUIRE_MESSAGE(check.not_owner_only, "pass " << pass);
            BOOST_TEST_MESSAGE("not owner-only: " << *check.not_owner_only);
        }
    }
}

// AppendToOwnedFile writes at the end of a regular file this account owns and leaves what
// was there.
BOOST_AUTO_TEST_CASE(append_to_owned_file_appends_to_a_regular_file)
{
    const ScratchDir scratch;
    const fs::path conf = scratch.m_path / "gridcoinresearch.conf";
    MakeFile(conf, 0600);

    const fsmodes::AppendResult result = fsmodes::AppendToOwnedFile(conf, "rpcuser=fsmodes\n");

    BOOST_CHECK(result.status == fsmodes::AppendStatus::Appended);
    BOOST_CHECK_EQUAL(ReadAll(conf), "fsmodes test\nrpcuser=fsmodes\n");
    BOOST_CHECK_EQUAL(ModeOf(conf), 0600U);
}

// A conf that is a symbolic link is not written, and neither is the file it leads to: the
// open does not follow the link. That is also what happens to a conf swapped for a link
// after AddMissingRpcCredentials() checked it with OwnedByUs(), since the open does not rely
// on that check.
BOOST_AUTO_TEST_CASE(append_to_owned_file_refuses_a_symlinked_conf)
{
    const ScratchDir scratch;
    const fs::path target = scratch.m_path / "elsewhere.conf";
    MakeFile(target, 0600);

    const fs::path conf = scratch.m_path / "gridcoinresearch.conf";
    fs::create_symlink(target, conf);

    const fsmodes::AppendResult result = fsmodes::AppendToOwnedFile(conf, "rpcpassword=fsmodes\n");

    BOOST_CHECK(result.status == fsmodes::AppendStatus::NotOwned);
    BOOST_CHECK_EQUAL(ReadAll(target), "fsmodes test\n");
    BOOST_CHECK(fs::is_symlink(conf));
}

// A FIFO is not a regular file. With a reader on it the open succeeds, and the check of the
// open descriptor refuses it, so nothing goes down the pipe.
BOOST_AUTO_TEST_CASE(append_to_owned_file_refuses_a_fifo)
{
    const ScratchDir scratch;
    const fs::path fifo = scratch.m_path / "gridcoinresearch.conf";
    BOOST_REQUIRE_EQUAL(::mkfifo(fifo.string().c_str(), 0600), 0);

    const int reader = ::open(fifo.string().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    BOOST_REQUIRE(reader >= 0);

    const fsmodes::AppendResult result = fsmodes::AppendToOwnedFile(fifo, "rpcpassword=fsmodes\n");

    char buffer[64];
    const ssize_t read = ::read(reader, buffer, sizeof(buffer));
    ::close(reader);

    BOOST_CHECK(result.status == fsmodes::AppendStatus::NotOwned);
    BOOST_CHECK_MESSAGE(read <= 0, "the FIFO received " << read << " bytes");
}

// A new file for secret material is created exclusively, 0600 from creation whatever the
// umask. A name that is taken, by a file, a link to one or a link that leads nowhere, is
// refused in the same step, and nothing there or where a link leads is opened or changed.
BOOST_AUTO_TEST_CASE(create_new_owner_only_file_is_exclusive_and_0600)
{
    const ScratchDir scratch;
    const UmaskGuard permissive(0);

    bool taken = true;
    std::optional<std::string> not_owner_only{"not set"};
    std::string error;

    const fs::path fresh = scratch.m_path / "fresh";
    std::FILE* file = fsmodes::CreateNewOwnerOnlyFile(fresh, taken, not_owner_only, error);
    BOOST_REQUIRE_MESSAGE(file, error);
    BOOST_CHECK(!taken);
    BOOST_CHECK_MESSAGE(!not_owner_only, not_owner_only.value_or(""));
    BOOST_CHECK(std::fputs("dump\n", file) >= 0);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);
    BOOST_CHECK_EQUAL(ModeOf(fresh), 0600U);
    BOOST_CHECK_EQUAL(ReadAll(fresh), "dump\n");

    const fs::path existing = scratch.m_path / "existing";
    MakeFile(existing, 0644);
    const fs::path target = scratch.m_path / "target";
    MakeFile(target, 0644);
    const fs::path link = scratch.m_path / "link";
    fs::create_symlink(target, link);
    const fs::path nowhere = scratch.m_path / "nowhere";
    const fs::path dangling = scratch.m_path / "dangling";
    fs::create_symlink(nowhere, dangling);

    for (const fs::path& name : {existing, link, dangling}) {
        taken = false;
        error.clear();
        std::FILE* refused = fsmodes::CreateNewOwnerOnlyFile(name, taken, not_owner_only, error);
        BOOST_CHECK_MESSAGE(!refused, name.string() << " was opened");
        if (refused) std::fclose(refused);
        BOOST_CHECK_MESSAGE(taken, name.string());
        BOOST_CHECK_MESSAGE(!error.empty(), name.string());
    }
    BOOST_CHECK_EQUAL(ReadAll(existing), "fsmodes test\n");
    BOOST_CHECK_EQUAL(ModeOf(existing), 0644U);
    BOOST_CHECK_EQUAL(ReadAll(target), "fsmodes test\n");
    BOOST_CHECK_EQUAL(ModeOf(target), 0644U);
    BOOST_CHECK(fs::is_symlink(fs::symlink_status(link)));
    BOOST_CHECK(!fs::exists(fs::symlink_status(nowhere)));

    // A directory that is not there: refused, and the name is not taken.
    taken = true;
    error.clear();
    BOOST_CHECK(!fsmodes::CreateNewOwnerOnlyFile(scratch.m_path / "absent" / "file", taken, not_owner_only, error));
    BOOST_CHECK(!taken);
    BOOST_CHECK(!error.empty());
}

// A file that replaces another keeps the permission bits of the one it replaces, set on the open
// temporary file before the rename, and the old file is not touched. Production change that would
// fail this: a KeepPermissions that does not fchmod the temporary file.
BOOST_AUTO_TEST_CASE(keep_permissions_carries_the_mode_onto_the_temp_file)
{
    const ScratchDir scratch;
    const UmaskGuard permissive(0);

    for (const mode_t mode : {mode_t{0640}, mode_t{0664}, mode_t{0604}, mode_t{0400}}) {
        const fs::path old_file = scratch.m_path / "old";
        const fs::path temp = scratch.m_path / "old.tmp";
        fs::remove(old_file);
        fs::remove(temp);
        MakeFile(old_file, mode);

        bool taken = false;
        std::optional<std::string> not_owner_only;
        std::string error;
        std::FILE* file = fsmodes::CreateNewOwnerOnlyFile(temp, taken, not_owner_only, error);
        BOOST_REQUIRE_MESSAGE(file, error);
        BOOST_CHECK_EQUAL(ModeOf(temp), 0600U);

        const fsmodes::KeepResult result = fsmodes::KeepPermissions(old_file, file);
        BOOST_CHECK(result.kind == fsmodes::KeepResult::Kind::Kept);
        BOOST_CHECK_MESSAGE(result.error.empty(), result.error);
        BOOST_CHECK_EQUAL(std::fclose(file), 0);

        BOOST_CHECK_EQUAL(ModeOf(temp), static_cast<unsigned>(mode));
        BOOST_CHECK_EQUAL(ModeOf(old_file), static_cast<unsigned>(mode));
    }
}

// Only the permission bits are carried: a set-group-ID bit on the old file is not, and a link at
// the old name is read as the link, not as what it leads to (a link has no bits to carry).
BOOST_AUTO_TEST_CASE(keep_permissions_copies_only_the_permission_bits)
{
    const ScratchDir scratch;
    const UmaskGuard permissive(0);

    const fs::path old_file = scratch.m_path / "old";
    MakeFile(old_file, 0640);
    BOOST_REQUIRE_EQUAL(::chmod(old_file.c_str(), 02640), 0);
    const fs::path temp = scratch.m_path / "old.tmp";
    MakeFile(temp, 0600);
    BOOST_CHECK(fsmodes::KeepPermissions(old_file, temp).kind == fsmodes::KeepResult::Kind::Kept);
    BOOST_CHECK_EQUAL(ModeOf(temp), 0640U);

    // A link at the old name is not followed and has no permissions to carry: the new file stays
    // as created.
    const fs::path link = scratch.m_path / "link";
    fs::create_symlink(old_file, link);
    const fs::path temp2 = scratch.m_path / "link.tmp";
    MakeFile(temp2, 0600);
    BOOST_CHECK(fsmodes::KeepPermissions(link, temp2).kind == fsmodes::KeepResult::Kind::NoOldFile);
    BOOST_CHECK_EQUAL(ModeOf(temp2), 0600U);
}

// Asked for this account's files only, KeepPermissions does not carry the mode of a file another
// account owns: the new file stays owner-only, and the result says why. This account's own file
// still lends its mode. Production change that would fail this: a KeepPermissions that does not
// compare the old file's owner with the effective user.
BOOST_AUTO_TEST_CASE(keep_permissions_own_only_skips_another_accounts_file)
{
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("run as root, which owns the system's files too; skipping");
        return;
    }
    const ScratchDir scratch;
    const UmaskGuard permissive(0);

    // A file every system has, which another account (root) owns and others may read.
    const fs::path foreign = "/etc/passwd";
    struct stat foreign_st;
    BOOST_REQUIRE_EQUAL(::lstat(foreign.c_str(), &foreign_st), 0);
    BOOST_REQUIRE(foreign_st.st_uid != ::geteuid());

    const fs::path temp = scratch.m_path / "new.tmp";
    MakeFile(temp, 0600);
    const int fd = ::open(temp.c_str(), O_RDWR | O_CLOEXEC);
    BOOST_REQUIRE(fd >= 0);
    BOOST_CHECK(fsmodes::KeepPermissions(foreign, fd, /*own_only=*/true).kind == fsmodes::KeepResult::Kind::NotOurs);
    BOOST_CHECK_EQUAL(ModeOf(temp), 0600U);

    const fs::path own = scratch.m_path / "own";
    MakeFile(own, 0640);
    BOOST_CHECK(fsmodes::KeepPermissions(own, fd, /*own_only=*/true).kind == fsmodes::KeepResult::Kind::Kept);
    BOOST_CHECK_EQUAL(ModeOf(temp), 0640U);
    BOOST_CHECK_EQUAL(::close(fd), 0);
}

// The group's permissions belong to the old file's group. When the new file cannot have that group
// they are not given to the group it has instead: nothing is carried, and the result says why.
// Production change that would fail this: a KeepPermissions that applies the old mode after a
// failed fchown of the group.
BOOST_AUTO_TEST_CASE(keep_permissions_carries_no_group_bits_to_another_group)
{
    // /etc/passwd is root's group's, with group read; a process that is not root and not in that
    // group cannot give a file that group.
    const fs::path old_file = "/etc/passwd";
    struct stat old_st;
    BOOST_REQUIRE_EQUAL(::lstat(old_file.c_str(), &old_st), 0);
    std::vector<gid_t> groups(static_cast<size_t>(std::max(::getgroups(0, nullptr), 0)));
    groups.resize(static_cast<size_t>(std::max(::getgroups(static_cast<int>(groups.size()), groups.data()), 0)));
    if (::geteuid() == 0 || ::getegid() == old_st.st_gid ||
        std::find(groups.begin(), groups.end(), old_st.st_gid) != groups.end() || (old_st.st_mode & 0070) == 0) {
        BOOST_TEST_MESSAGE("this process may take the group of /etc/passwd, or it gives its group nothing; skipping");
        return;
    }
    const ScratchDir scratch;
    const UmaskGuard permissive(0);
    const fs::path temp = scratch.m_path / "new.tmp";
    MakeFile(temp, 0600);

    const fsmodes::KeepResult result = fsmodes::KeepPermissions(old_file, temp);
    BOOST_CHECK(result.kind == fsmodes::KeepResult::Kind::NotKept);
    BOOST_CHECK_MESSAGE(result.error.find("group") != std::string::npos, result.error);
    BOOST_CHECK_EQUAL(ModeOf(temp), 0600U);
}

// With nothing at the old name the temporary file stays as created, 0600 whatever the umask.
BOOST_AUTO_TEST_CASE(keep_permissions_with_no_old_file_leaves_the_temp_file_owner_only)
{
    const ScratchDir scratch;
    const UmaskGuard permissive(0);

    const fs::path temp = scratch.m_path / "new.tmp";
    bool taken = false;
    std::optional<std::string> not_owner_only;
    std::string error;
    std::FILE* file = fsmodes::CreateNewOwnerOnlyFile(temp, taken, not_owner_only, error);
    BOOST_REQUIRE_MESSAGE(file, error);

    const fsmodes::KeepResult result = fsmodes::KeepPermissions(scratch.m_path / "absent", file);
    BOOST_CHECK(result.kind == fsmodes::KeepResult::Kind::NoOldFile);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);
    BOOST_CHECK_EQUAL(ModeOf(temp), 0600U);
}

// The group of the old file is carried when this process may set it, and the form that takes a
// path does the same. A group this process is not in stays as it is (EPERM is not a failure).
BOOST_AUTO_TEST_CASE(keep_permissions_carries_the_group_and_works_on_a_path)
{
    const ScratchDir scratch;
    const UmaskGuard permissive(0);

    const int count = ::getgroups(0, nullptr);
    std::vector<gid_t> groups(count > 0 ? static_cast<size_t>(count) : 0);
    if (count > 0) BOOST_REQUIRE_EQUAL(::getgroups(count, groups.data()), count);
    const auto other = std::find_if(groups.begin(), groups.end(), [](gid_t g) { return g != ::getegid(); });

    const fs::path old_file = scratch.m_path / "old";
    MakeFile(old_file, 0640);
    if (other != groups.end()) BOOST_REQUIRE_EQUAL(::chown(old_file.c_str(), static_cast<uid_t>(-1), *other), 0);

    const fs::path temp = scratch.m_path / "old.tmp";
    MakeFile(temp, 0600);
    BOOST_CHECK(fsmodes::KeepPermissions(old_file, temp).kind == fsmodes::KeepResult::Kind::Kept);
    BOOST_CHECK_EQUAL(ModeOf(temp), 0640U);

    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat(temp.c_str(), &st), 0);
    BOOST_CHECK_EQUAL(st.st_gid, other != groups.end() ? *other : ::getegid());

    // A link at the temporary name is not followed.
    const fs::path victim = scratch.m_path / "victim";
    MakeFile(victim, 0600);
    const fs::path link = scratch.m_path / "link";
    fs::create_symlink(victim, link);
    BOOST_CHECK(fsmodes::KeepPermissions(old_file, link).kind == fsmodes::KeepResult::Kind::NotKept);
    BOOST_CHECK_EQUAL(ModeOf(victim), 0600U);
}

// CreateReplacementFile: a stale temporary name (a link too) is removed and never followed, the
// new file is created owner-only and then takes the mode of the file it will replace.
BOOST_AUTO_TEST_CASE(create_replacement_file_keeps_the_mode_of_the_target)
{
    const ScratchDir scratch;
    const UmaskGuard permissive(0);

    const fs::path target = scratch.m_path / "target";
    const fs::path temp = scratch.m_path / "target.tmp";
    const fs::path victim = scratch.m_path / "victim";
    MakeFile(target, 0640);
    MakeFile(victim, 0644);
    fs::create_symlink(victim, temp);

    std::string error;
    std::optional<std::string> warning;
    std::FILE* file = fsmodes::CreateReplacementFile(target, temp, error, warning);
    BOOST_REQUIRE_MESSAGE(file, error);
    BOOST_CHECK_MESSAGE(!warning, warning.value_or(""));
    BOOST_CHECK(std::fputs("new\n", file) >= 0);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);

    BOOST_CHECK_EQUAL(ModeOf(temp), 0640U);
    BOOST_CHECK_EQUAL(ModeOf(victim), 0644U);
    BOOST_CHECK_EQUAL(ReadAll(victim), "fsmodes test\n");
    BOOST_CHECK_EQUAL(ModeOf(target), 0640U);

    // No target: owner-only.
    fs::remove(target);
    fs::remove(temp);
    file = fsmodes::CreateReplacementFile(target, temp, error, warning);
    BOOST_REQUIRE_MESSAGE(file, error);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);
    BOOST_CHECK_EQUAL(ModeOf(temp), 0600U);
}

#else // WIN32

namespace {

//! Wine keeps no protected DACL, and its owners are not Windows' own, so the cases that read
//! back what Windows keeps run on Windows only.
bool SkipUnderWine()
{
    if (!grc_test::RunningUnderWine()) return false;
    BOOST_TEST_MESSAGE("Wine keeps no protected DACL; this case runs on Windows");
    return true;
}

//! A fresh directory under the system temp directory, removed when the case ends.
struct WinTempDir
{
    fs::path path{fs::temp_directory_path() / fs::unique_path("fsmodes_%%%%-%%%%-%%%%")};

    WinTempDir()
    {
        fs::create_directories(path);
    }

    ~WinTempDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

//! grc_test::CreateFileWithAcl, with this suite's usual contents.
void CreateWithAcl(const fs::path& path, const AclSpec& spec, const std::string& text = "fsmodes test\n")
{
    grc_test::CreateFileWithAcl(path, spec, text);
}

//! The owner-only access list (this account and SYSTEM) with \p extra after it.
AclSpec OwnerOnlyPlus(const AceSpec& extra, bool directory)
{
    AclSpec spec = util::OwnerOnlyAclSpec(directory);
    spec.aces.push_back(extra);
    return spec;
}

//! The Users group reading a file, or, on a directory, reading it and what is created inside it.
AceSpec UsersRead(bool directory)
{
    return {wellknown::Users(), FILE_GENERIC_READ, directory ? Inherit::ContainerAndObject : Inherit::None};
}

//! %SystemRoot%\win.ini, a file the system owns.
fs::path WinIni()
{
    wchar_t windows[MAX_PATH + 1];
    const UINT length = ::GetSystemWindowsDirectoryW(windows, MAX_PATH + 1);
    BOOST_REQUIRE(length > 0 && length <= MAX_PATH);

    return fs::path(std::wstring(windows, length)) / "win.ini";
}

} // anonymous namespace

// A file this account created with itself as the owner is ours, and AppendToOwnedFile writes at
// its end. A path that does not exist is not.
BOOST_AUTO_TEST_CASE(owned_by_us_own_file)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path file = dir.path / "gridcoinresearch.conf";
    CreateWithAcl(file, util::OwnerOnlyAclSpec(/*inheritable=*/false));

    BOOST_CHECK(fsmodes::OwnedByUs(file));

    const fsmodes::AppendResult result = fsmodes::AppendToOwnedFile(file, "rpcuser=fsmodes\n");
    BOOST_CHECK(result.status == fsmodes::AppendStatus::Appended);
    BOOST_CHECK_EQUAL(ReadAll(file), "fsmodes test\nrpcuser=fsmodes\n");

    BOOST_CHECK(!fsmodes::OwnedByUs(dir.path / "absent"));
    BOOST_CHECK(fsmodes::AppendToOwnedFile(dir.path / "absent", "x").status == fsmodes::AppendStatus::OpenFailed);
}

// %SystemRoot%\win.ini is owned by the system, not by this account.
BOOST_AUTO_TEST_CASE(owned_by_us_false_for_foreign_owner)
{
    if (SkipUnderWine()) return;

    const fs::path ini = WinIni();
    BOOST_REQUIRE(fs::exists(ini));
    const std::optional<Sid> owner = ReadAcl(ini).owner;
    BOOST_REQUIRE(owner && *owner != CurrentTokenUser());

    BOOST_CHECK(!fsmodes::OwnedByUs(ini));

    // What the conf writers' messages say about it: its owner, and the fix.
    const fsmodes::WinOwnership ownership = fsmodes::ReadWinOwnership(ini);
    BOOST_CHECK(ownership.kind == fsmodes::WinOwnership::Kind::Foreign);
    BOOST_CHECK(ownership.owner == *owner);
    const std::string text = fsmodes::WinNotOwnedText(ini, /*service=*/false);
    BOOST_TEST_MESSAGE(text);
    CheckHas(text, FormatSid(*owner));
    CheckHas(text, "/setowner *" + FormatSid(CurrentTokenUser()));
}

// A file whose owner this account set is this account's, also under an elevated token, whose
// new files otherwise belong to Administrators.
BOOST_AUTO_TEST_CASE(read_win_ownership_agrees_with_owned_by_us)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path own = dir.path / "own.conf";
    CreateWithAcl(own, util::OwnerOnlyAclSpec(/*inheritable=*/false));
    BOOST_CHECK(fsmodes::ReadWinOwnership(own).kind == fsmodes::WinOwnership::Kind::Owned);
    BOOST_CHECK(fsmodes::OwnedByUs(own));
    BOOST_CHECK(fsmodes::WinNotOwnedText(own, /*service=*/false).empty());

    BOOST_CHECK(fsmodes::ReadWinOwnership(dir.path / "absent").kind == fsmodes::WinOwnership::Kind::Unopened);
    BOOST_CHECK(!fsmodes::WinNotOwnedText(dir.path / "absent", /*service=*/false).empty());
}

// A file that replaces another keeps its access list: a Users read entry survives, and a
// protected list stays protected. Production change that would fail this: a KeepPermissions that
// leaves the temporary file with the owner-only list it was created with.
BOOST_AUTO_TEST_CASE(keep_permissions_carries_the_users_entry_and_protection)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path old_file = dir.path / "gridcoinsettings.json";
    CreateWithAcl(old_file, OwnerOnlyPlus(UsersRead(/*directory=*/false), /*directory=*/false));
    const ObservedAcl before = ReadAcl(old_file);
    BOOST_REQUIRE(before.is_protected);

    const fs::path temp = dir.path / "gridcoinsettings.json.tmp";
    std::string error;
    std::optional<std::string> warning;
    std::FILE* file = fsmodes::CreateReplacementFile(old_file, temp, error, warning);
    BOOST_REQUIRE_MESSAGE(file, error);
    BOOST_CHECK_MESSAGE(!warning, warning.value_or(""));
    BOOST_CHECK(std::fputs("{}\n", file) >= 0);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);

    const ObservedAcl after = ReadAcl(temp);
    BOOST_CHECK(after.is_protected);
    BOOST_CHECK(after.owner == CurrentTokenUser());
    BOOST_CHECK_EQUAL(after.aces.size(), before.aces.size());
    const auto has_users = [](const ObservedAcl& acl) {
        return std::any_of(acl.aces.begin(), acl.aces.end(), [](const ObservedAce& ace) {
            return ace.kind == AceKind::Allow && ace.trustee == wellknown::Users();
        });
    };
    BOOST_CHECK(has_users(after));

    // The old file is not changed.
    BOOST_CHECK(grc_test::SameAcl(ReadAcl(old_file), before));
}

// Asked for this account's files only, KeepPermissions does not carry the access list of a file
// another account owns (the system's hosts file): the new file keeps its owner-only list.
// Production change that would fail this: a KeepPermissions that does not compare the old file's
// owner with the token user.
BOOST_AUTO_TEST_CASE(keep_permissions_own_only_skips_another_accounts_file)
{
    if (SkipUnderWine()) return;

    wchar_t system_dir[MAX_PATH];
    const UINT length = ::GetSystemDirectoryW(system_dir, MAX_PATH);
    BOOST_REQUIRE(length > 0 && length < MAX_PATH);
    const fs::path foreign = fs::path(std::wstring(system_dir, length)) / "drivers" / "etc" / "hosts";
    BOOST_REQUIRE(ReadAcl(foreign).owner != CurrentTokenUser());

    const WinTempDir dir;
    const fs::path temp = dir.path / "new.tmp";
    bool taken = false;
    std::optional<std::string> not_owner_only;
    std::string error;
    std::FILE* file = fsmodes::CreateNewOwnerOnlyFile(temp, taken, not_owner_only, error);
    BOOST_REQUIRE_MESSAGE(file, error);
    const ObservedAcl before = ReadAcl(temp);

    void* const handle = reinterpret_cast<void*>(::_get_osfhandle(::_fileno(file)));
    BOOST_CHECK(fsmodes::KeepPermissions(foreign, handle, /*own_only=*/true).kind ==
                fsmodes::KeepResult::Kind::NotOurs);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);
    BOOST_CHECK(grc_test::SameAcl(ReadAcl(temp), before));
}

// A list that was not protected stays unprotected, and its explicit entries stay.
BOOST_AUTO_TEST_CASE(keep_permissions_keeps_an_unprotected_list_unprotected)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path folder = dir.path / "data";
    BOOST_REQUIRE(CreateDirectoryWithAcl(folder, OwnerOnlyPlus(UsersRead(/*directory=*/true), /*directory=*/true)));

    // Created plainly in a directory that hands down entries, so its list is not protected.
    const fs::path old_file = folder / "peers.dat";
    const HANDLE created = ::CreateFileW(old_file.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                         FILE_ATTRIBUTE_NORMAL, nullptr);
    BOOST_REQUIRE(created != INVALID_HANDLE_VALUE);
    ::CloseHandle(created);
    const ObservedAcl before = ReadAcl(old_file);
    BOOST_REQUIRE(!before.is_protected);

    const fs::path temp = folder / "peers.dat.tmp";
    std::string error;
    std::optional<std::string> warning;
    std::FILE* file = fsmodes::CreateReplacementFile(old_file, temp, error, warning);
    BOOST_REQUIRE_MESSAGE(file, error);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);

    const ObservedAcl after = ReadAcl(temp);
    BOOST_CHECK(!after.is_protected);
    BOOST_CHECK_EQUAL(after.aces.size(), before.aces.size());
}

// With nothing at the old name the new file is owner-only from creation.
BOOST_AUTO_TEST_CASE(create_replacement_file_without_a_target_is_owner_only)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path temp = dir.path / "new.tmp";
    std::string error;
    std::optional<std::string> warning;
    std::FILE* file = fsmodes::CreateReplacementFile(dir.path / "absent", temp, error, warning);
    BOOST_REQUIRE_MESSAGE(file, error);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);

    BOOST_CHECK(!util::winacl::CheckMatchesSpec(ReadAcl(temp), util::OwnerOnlyAclSpec(/*inheritable=*/false)));
}

// A junction is not ours, whoever made it: OwnedByUs does not follow it to the directory it
// leads to, which is.
BOOST_AUTO_TEST_CASE(owned_by_us_false_for_a_junction)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path real = dir.path / "real";
    BOOST_REQUIRE(CreateDirectoryWithAcl(real, util::OwnerOnlyAclSpec(/*inheritable=*/true)));
    const fs::path link = dir.path / "link";
    grc_test::MakeJunction(link, real);

    BOOST_CHECK(fsmodes::OwnedByUs(real));
    BOOST_CHECK(!fsmodes::OwnedByUs(link));

    // The junction itself is removed, so the cleanup does not go through it.
    BOOST_CHECK(::RemoveDirectoryW(link.wstring().c_str()));
}

// A file symbolic link is not ours and nothing is appended through it; in the checked set it is
// reported, not followed. A copy to a name that is a dangling link creates nothing where the
// link leads: the new-target open does not follow it, and the existing-target open finds no
// file. Creating the links needs Developer Mode or the privilege to create symbolic links;
// without either the case says so and stops.
BOOST_AUTO_TEST_CASE(file_symlinks_are_not_followed)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path target = dir.path / "real_wallet";
    CreateWithAcl(target, util::OwnerOnlyAclSpec(/*inheritable=*/false));

    // SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE: honoured in Developer Mode.
    const fs::path link = dir.path / "wallet.dat";
    if (!::CreateSymbolicLinkW(link.wstring().c_str(), target.wstring().c_str(),
                               SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        BOOST_TEST_MESSAGE("file_symlinks_are_not_followed: cannot create a symbolic link here (error "
                           << ::GetLastError() << "); enable Developer Mode or run elevated to cover it");
        return;
    }

    BOOST_CHECK(fsmodes::OwnedByUs(target));
    BOOST_CHECK(!fsmodes::OwnedByUs(link));
    BOOST_CHECK(fsmodes::AppendToOwnedFile(link, "rpcpassword=fsmodes\n").status == fsmodes::AppendStatus::NotOwned);
    BOOST_CHECK_EQUAL(ReadAll(target), "fsmodes test\n");

    const auto findings = fsmodes::CheckOwnerOnlyModes({}, {}, {}, link, {}, {});
    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::Symlink);

    const fs::path source = dir.path / "source";
    {
        fsbridge::ofstream stream(source, std::ios::binary);
        stream << "wallet bytes\n";
    }
    const fs::path nowhere = dir.path / "nowhere";
    const fs::path dangling = dir.path / "dangling";
    BOOST_REQUIRE(::CreateSymbolicLinkW(dangling.wstring().c_str(), nowhere.wstring().c_str(),
                                        SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE));
    BOOST_CHECK_THROW(fsmodes::CopyFileOwnerOnly(source, dangling), fs::filesystem_error);
    BOOST_CHECK(!fs::exists(nowhere));
}

// A wallet file this account owns that the Users group can read is reported, naming Users and
// the command that fixes it, and its access list is left exactly as it was: it still does not
// match the owner-only list.
BOOST_AUTO_TEST_CASE(check_warns_on_a_wallet_readable_by_users_and_changes_nothing)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path datadir = dir.path / "data";
    BOOST_REQUIRE(CreateDirectoryWithAcl(datadir, util::OwnerOnlyAclSpec(/*inheritable=*/true)));

    const fs::path wallet = datadir / "wallet.dat";
    CreateWithAcl(wallet, util::OwnerOnlyAclSpec(/*inheritable=*/false));
    ApplyAcl(wallet, OwnerOnlyPlus(UsersRead(/*directory=*/false), /*directory=*/false));

    const ObservedAcl before = ReadAcl(wallet);
    BOOST_REQUIRE(CheckMatchesSpec(before, util::OwnerOnlyAclSpec(/*inheritable=*/false)).has_value());

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, datadir / "gridcoinresearch.conf",
                                                       datadir / "gridcoinsettings.json", wallet,
                                                       datadir / "walletbackups", datadir / "debug.log");

    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == wallet);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::WideMode);
    BOOST_CHECK_EQUAL(findings[0].mode, 0U);
    CheckHas(findings[0].message, "S-1-5-32-545 (Users) can read it");
    CheckHas(findings[0].message, "icacls \"" + wallet.string() + "\" /inheritance:r /grant:r");
    BOOST_CHECK_MESSAGE(IsAsciiLine(findings[0].message), findings[0].message);

    const ObservedAcl after = ReadAcl(wallet);
    BOOST_CHECK(grc_test::SameAcl(after, before));
    BOOST_CHECK(CheckMatchesSpec(after, util::OwnerOnlyAclSpec(/*inheritable=*/false)).has_value());
}

// The whole set is walked: a data directory whose list lets the Users group read it, and what
// inherits that list inside it, are each reported, as ours or not as OwnedByUs says, and
// nothing is changed. A file with its own owner-only list, a database file other than a log,
// and a directory inside the backup directory are not reported.
BOOST_AUTO_TEST_CASE(check_walks_the_wallet_files_and_changes_nothing)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path datadir = dir.path / "data";
    BOOST_REQUIRE(CreateDirectoryWithAcl(datadir, OwnerOnlyPlus(UsersRead(/*directory=*/true), /*directory=*/true)));

    // Created as the core creates them, with the list they inherit.
    const auto make_file = [](const fs::path& path) {
        fsbridge::ofstream stream(path);
        stream << "fsmodes test\n";
    };
    make_file(datadir / "wallet.dat");
    fs::create_directories(datadir / "database");
    make_file(datadir / "database" / "log.0000000001");
    make_file(datadir / "database" / "other");
    fs::create_directories(datadir / "walletbackups" / "subdir");
    make_file(datadir / "walletbackups" / "wallet.dat-x");
    make_file(datadir / "gridcoinsettings.json");
    make_file(datadir / "debug.log");

    CreateWithAcl(datadir / "gridcoinresearch.conf", util::OwnerOnlyAclSpec(/*inheritable=*/false));
    CreateWithAcl(datadir / "walletbackups" / "wallet.dat-ok", util::OwnerOnlyAclSpec(/*inheritable=*/false));

    const std::set<fs::path> wide = {
        datadir,
        datadir / "wallet.dat",
        datadir / "database",
        datadir / "database" / "log.0000000001",
        datadir / "walletbackups",
        datadir / "walletbackups" / "wallet.dat-x",
        datadir / "gridcoinsettings.json",
        datadir / "debug.log",
    };
    const std::set<fs::path> dirs = {datadir, datadir / "database", datadir / "walletbackups"};

    std::vector<std::pair<fs::path, ObservedAcl>> before;
    for (const auto& path : wide) before.emplace_back(path, ReadAcl(path));

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, datadir / "gridcoinresearch.conf",
                                                       datadir / "gridcoinsettings.json", datadir / "wallet.dat",
                                                       datadir / "walletbackups", datadir / "debug.log");

    BOOST_CHECK_EQUAL(findings.size(), wide.size());
    for (const auto& path : wide) {
        const fsmodes::ModeFinding& finding = FindingFor(findings, path);
        const auto kind = fsmodes::OwnedByUs(path) ? fsmodes::ModeFinding::Kind::WideMode
                                                   : fsmodes::ModeFinding::Kind::WideModeNotOwned;
        BOOST_CHECK_MESSAGE(finding.kind == kind, path.string());
        CheckHas(finding.message, "S-1-5-32-545 (Users) can read it");
        if (kind == fsmodes::ModeFinding::Kind::WideMode) {
            CheckHas(finding.message, dirs.count(path) ? ":(OI)(CI)F\"" : ":F\"");
        }
    }
    BOOST_CHECK(!HasFinding(findings, datadir / "gridcoinresearch.conf"));
    BOOST_CHECK(!HasFinding(findings, datadir / "walletbackups" / "wallet.dat-ok"));
    BOOST_CHECK(!HasFinding(findings, datadir / "database" / "other"));
    BOOST_CHECK(!HasFinding(findings, datadir / "walletbackups" / "subdir"));

    for (const auto& [path, acl] : before) {
        BOOST_CHECK_MESSAGE(grc_test::SameAcl(ReadAcl(path), acl), path.string() << " changed its access list");
    }
}

// A configuration file another account owns, here %SystemRoot%\win.ini, is at most reported
// as not ours, with a fix that gives this account no entry on it, and its access list is left
// as it is.
BOOST_AUTO_TEST_CASE(check_leaves_a_foreign_owned_conf)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path datadir = dir.path / "data";
    BOOST_REQUIRE(CreateDirectoryWithAcl(datadir, util::OwnerOnlyAclSpec(/*inheritable=*/true)));

    const fs::path ini = WinIni();
    const ObservedAcl before = ReadAcl(ini);

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, ini, {}, {}, {}, {});

    BOOST_CHECK_LE(findings.size(), 1U);
    for (const fsmodes::ModeFinding& finding : findings) {
        BOOST_CHECK(finding.path == ini);
        BOOST_CHECK(finding.kind == fsmodes::ModeFinding::Kind::WideModeNotOwned);
        CheckHasNot(finding.message, "/grant");
        BOOST_TEST_MESSAGE(finding.message);
    }
    BOOST_CHECK(grc_test::SameAcl(ReadAcl(ini), before));
}

// A junction in the set is reported and not followed, so nothing behind it is checked. A data
// directory or a backup directory moved to another disk behind a junction is followed, and the
// rule applies to what it leads to; inside it, the files are checked as before. Current Windows
// 11 builds do not follow a junction an account that is not an administrator made: there the
// moved directory is reported, once, as one that could not be checked and why. Which applies is
// read from the system, and a junction made with an elevated token is always followed.
BOOST_AUTO_TEST_CASE(check_reports_a_junction_and_follows_a_moved_directory)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path wide = dir.path / "wide";
    BOOST_REQUIRE(CreateDirectoryWithAcl(wide, OwnerOnlyPlus(UsersRead(/*directory=*/true), /*directory=*/true)));
    CreateWithAcl(wide / "log.0000000001", OwnerOnlyPlus(UsersRead(/*directory=*/false), /*directory=*/false));

    const fs::path datadir = dir.path / "data";
    BOOST_REQUIRE(CreateDirectoryWithAcl(datadir, util::OwnerOnlyAclSpec(/*inheritable=*/true)));
    const fs::path database = datadir / "database";
    grc_test::MakeJunction(database, wide);

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, {}, {}, {}, {}, {});
    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == database);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::Symlink);
    BOOST_CHECK_EQUAL(findings[0].mode, 0U);

    const fs::path moved = dir.path / "moved";
    grc_test::MakeJunction(moved, wide);
    const bool untrusted = grc_test::JunctionUntrustedOn(moved);
    BOOST_TEST_MESSAGE("Windows " << (untrusted ? "does not follow" : "follows") << " a junction this token made");
    // An elevated token stands in for "an administrator made it"; that holds for an elevated
    // administrator and a standard user, not for every elevated token (Backup Operators).
    if (CurrentTokenIsElevated()) BOOST_CHECK_MESSAGE(!untrusted, "a junction an elevated token made is not followed");

    const auto followed = fsmodes::CheckOwnerOnlyModes(moved, {}, {}, {}, {}, {});
    BOOST_REQUIRE_EQUAL(followed.size(), 1U);
    BOOST_CHECK(followed[0].path == moved);
    if (untrusted) {
        BOOST_CHECK(followed[0].kind == fsmodes::ModeFinding::Kind::Unreadable);
        CheckHas(followed[0].message, "WARNING: " + moved.string());
        CheckHas(followed[0].message, "junction");
        CheckHas(followed[0].message, "administrator");
    } else {
        BOOST_CHECK(followed[0].kind == fsmodes::ModeFinding::Kind::WideMode);
        CheckHas(followed[0].message, "icacls \"" + moved.string() + "\"");
        CheckHas(followed[0].message, ":(OI)(CI)F\"");
    }

    const fs::path backups = dir.path / "backups";
    BOOST_REQUIRE(CreateDirectoryWithAcl(backups, util::OwnerOnlyAclSpec(/*inheritable=*/true)));
    CreateWithAcl(backups / "wallet.dat-x", OwnerOnlyPlus(UsersRead(/*directory=*/false), /*directory=*/false));
    CreateWithAcl(backups / "wallet.dat-ok", util::OwnerOnlyAclSpec(/*inheritable=*/false));
    grc_test::MakeJunction(backups / "linked", wide);
    const fs::path backups_link = dir.path / "backups_link";
    grc_test::MakeJunction(backups_link, backups);

    const auto inner = fsmodes::CheckOwnerOnlyModes({}, {}, {}, {}, backups_link, {});
    if (untrusted) {
        BOOST_REQUIRE_EQUAL(inner.size(), 1U);
        BOOST_CHECK(inner[0].path == backups_link);
        BOOST_CHECK(inner[0].kind == fsmodes::ModeFinding::Kind::Unreadable);
        CheckHas(inner[0].message, "junction");
    } else {
        BOOST_CHECK_EQUAL(inner.size(), 2U);
        BOOST_CHECK(FindingFor(inner, backups_link / "wallet.dat-x").kind == fsmodes::ModeFinding::Kind::WideMode);
        BOOST_CHECK(FindingFor(inner, backups_link / "linked").kind == fsmodes::ModeFinding::Kind::Symlink);
    }

    // The junctions themselves are removed, so the cleanup does not go through them.
    BOOST_CHECK(::RemoveDirectoryW(backups_link.wstring().c_str()));
    BOOST_CHECK(::RemoveDirectoryW((backups / "linked").wstring().c_str()));
    BOOST_CHECK(::RemoveDirectoryW(moved.wstring().c_str()));
    BOOST_CHECK(::RemoveDirectoryW(database.wstring().c_str()));
}

// A backup directory that opens but cannot be listed is reported, as the files in it were not
// checked. With SeBackupPrivilege enabled it can still be listed, and the case says so.
BOOST_AUTO_TEST_CASE(check_reports_a_backup_directory_it_cannot_list)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path backups = dir.path / "backups";
    const Sid self = CurrentTokenUser();
    BOOST_REQUIRE(CreateDirectoryWithAcl(backups, {self, {{self, FILE_ALL_ACCESS & ~FILE_LIST_DIRECTORY, Inherit::None}}}));

    const auto findings = fsmodes::CheckOwnerOnlyModes({}, {}, {}, {}, backups, {});
    // Empty, so it is removed without being listed.
    boost::system::error_code ec;
    fs::remove(backups, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());

    if (findings.empty() && grc_test::TokenPrivilegeState(L"SeBackupPrivilege") == grc_test::PrivilegeState::Enabled) {
        BOOST_WARN_MESSAGE(false, "SeBackupPrivilege is enabled in this token, so "
                                      << backups.string() << " can be listed; the report is not checked");
        return;
    }
    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == backups);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::Unreadable);
    CheckHas(findings[0].message, "could not be listed");
}

// A backup directory that does not exist is created owner-only from creation, a file can be
// written there, and the check leaves it empty. A probe name that is taken is freed and tried
// once more; one that cannot be freed, and a directory this account cannot write, are
// reported.
BOOST_AUTO_TEST_CASE(backupdir_writable_probe)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path fresh = dir.path / "fresh" / "backups";

    const fsmodes::BackupDirCheck check = fsmodes::CheckBackupDir(fresh);
    BOOST_CHECK_MESSAGE(!check.not_writable, check.not_writable.value_or(""));
    BOOST_CHECK_MESSAGE(!check.not_owner_only, check.not_owner_only.value_or(""));
    BOOST_REQUIRE(fs::is_directory(fresh));
    BOOST_CHECK(fs::is_empty(fresh));
    const std::optional<std::string> reason = CheckMatchesSpec(ReadAcl(fresh), util::OwnerOnlyAclSpec(true));
    BOOST_CHECK_MESSAGE(!reason, reason.value_or(""));

    const auto fixed = [] { return std::string{"probe"}; };
    CreateWithAcl(fresh / "probe", util::OwnerOnlyAclSpec(/*inheritable=*/false));
    BOOST_CHECK(!fsmodes::CheckBackupDir(fresh, fixed).not_writable);
    BOOST_CHECK(!fs::exists(fresh / "probe"));

    fs::create_directory(fresh / "probe");
    const std::optional<std::string> taken = fsmodes::CheckBackupDir(fresh, fixed).not_writable;
    BOOST_CHECK(taken.has_value() && !taken->empty());
    BOOST_CHECK(fs::is_directory(fresh / "probe"));

    // This account may list and read the directory, and not add to it.
    const fs::path locked = dir.path / "locked";
    const Sid self = CurrentTokenUser();
    BOOST_REQUIRE(CreateDirectoryWithAcl(locked, {self, {{self, FILE_GENERIC_READ | FILE_TRAVERSE, Inherit::None}}}));
    const std::optional<std::string> refused = fsmodes::CheckBackupDir(locked).not_writable;
    BOOST_REQUIRE(refused.has_value());
    BOOST_CHECK(!refused->empty());
    BOOST_CHECK_MESSAGE(IsAsciiLine(*refused), *refused);
    BOOST_CHECK(fs::is_empty(locked));
}

// A copy that creates its target gives it the owner-only list from creation. A target that
// exists keeps its own list and receives the new content in full. The source is never
// truncated as its own target, and a source that cannot be read creates nothing.
BOOST_AUTO_TEST_CASE(copy_file_owner_only_creates_the_target_owner_only)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path source = dir.path / "source";
    {
        fsbridge::ofstream stream(source, std::ios::binary);
        stream << "wallet bytes\n";
    }

    const fs::path target = dir.path / "target";
    const std::optional<std::string> unrestricted = fsmodes::CopyFileOwnerOnly(source, target);
    BOOST_CHECK_MESSAGE(!unrestricted, unrestricted.value_or(""));
    BOOST_CHECK_EQUAL(ReadAll(target), "wallet bytes\n");
    const std::optional<std::string> reason = CheckMatchesSpec(ReadAcl(target), util::OwnerOnlyAclSpec(false));
    BOOST_CHECK_MESSAGE(!reason, reason.value_or(""));

    const fs::path existing = dir.path / "existing";
    CreateWithAcl(existing, OwnerOnlyPlus(UsersRead(/*directory=*/false), /*directory=*/false),
                  "an older and longer content, none of which may be left\n");
    const ObservedAcl before = ReadAcl(existing);
    BOOST_CHECK(!fsmodes::CopyFileOwnerOnly(source, existing));
    BOOST_CHECK_EQUAL(ReadAll(existing), "wallet bytes\n");
    BOOST_CHECK(grc_test::SameAcl(ReadAcl(existing), before));

    BOOST_CHECK_THROW(fsmodes::CopyFileOwnerOnly(source, source), fs::filesystem_error);
    BOOST_CHECK_EQUAL(ReadAll(source), "wallet bytes\n");

    BOOST_CHECK_THROW(fsmodes::CopyFileOwnerOnly(dir.path / "absent", dir.path / "never"), fs::filesystem_error);
    BOOST_CHECK(!fs::exists(dir.path / "never"));
}

// A new file for secret material is created exclusively, with the owner-only list from
// creation. A name that is taken, by a file or by a link that leads nowhere, is refused in the
// same step, and nothing there or where the link leads is opened or changed.
BOOST_AUTO_TEST_CASE(create_new_owner_only_file_is_exclusive_and_owner_only)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    bool taken = true;
    std::optional<std::string> not_owner_only{"not set"};
    std::string error;

    const fs::path fresh = dir.path / "fresh";
    std::FILE* file = fsmodes::CreateNewOwnerOnlyFile(fresh, taken, not_owner_only, error);
    BOOST_REQUIRE_MESSAGE(file, error);
    BOOST_CHECK(!taken);
    BOOST_CHECK_MESSAGE(!not_owner_only, not_owner_only.value_or(""));
    BOOST_CHECK(std::fputs("dump\n", file) >= 0);
    BOOST_CHECK_EQUAL(std::fclose(file), 0);
    const std::optional<std::string> reason = CheckMatchesSpec(ReadAcl(fresh), util::OwnerOnlyAclSpec(false));
    BOOST_CHECK_MESSAGE(!reason, reason.value_or(""));
    BOOST_CHECK_EQUAL(ReadAll(fresh), "dump\n");

    const fs::path existing = dir.path / "existing";
    CreateWithAcl(existing, util::OwnerOnlyAclSpec(/*inheritable=*/false), "earlier\n");
    const fs::path nowhere = dir.path / "nowhere";
    const fs::path dangling = dir.path / "dangling";
    BOOST_REQUIRE(::CreateSymbolicLinkW(dangling.wstring().c_str(), nowhere.wstring().c_str(),
                                        SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE));

    for (const fs::path& name : {existing, dangling}) {
        taken = false;
        error.clear();
        std::FILE* refused = fsmodes::CreateNewOwnerOnlyFile(name, taken, not_owner_only, error);
        BOOST_CHECK_MESSAGE(!refused, name.string() << " was opened");
        if (refused) std::fclose(refused);
        BOOST_CHECK_MESSAGE(taken, name.string());
        BOOST_CHECK_MESSAGE(!error.empty(), name.string());
    }
    BOOST_CHECK_EQUAL(ReadAll(existing), "earlier\n");
    BOOST_CHECK(!fs::exists(nowhere));
}

// A directory the core creates is owner-only from creation, and the parents it needs get the
// lists they inherit. A directory that exists is left as it is, and a file in the way is an
// error.
BOOST_AUTO_TEST_CASE(create_owner_only_directory_is_owner_only_from_creation)
{
    if (SkipUnderWine()) return;

    const WinTempDir dir;
    const fs::path made = dir.path / "parent" / "made";
    std::string error;
    BOOST_REQUIRE_MESSAGE(util::CreateOwnerOnlyDirectory(made, error), error);
    const std::optional<std::string> reason = CheckMatchesSpec(ReadAcl(made), util::OwnerOnlyAclSpec(true));
    BOOST_CHECK_MESSAGE(!reason, reason.value_or(""));
    BOOST_CHECK(!ReadAcl(dir.path / "parent").is_protected);

    const fs::path existing = dir.path / "existing";
    BOOST_REQUIRE(CreateDirectoryWithAcl(existing, OwnerOnlyPlus(UsersRead(/*directory=*/true), /*directory=*/true)));
    const ObservedAcl before = ReadAcl(existing);
    BOOST_CHECK_MESSAGE(util::CreateOwnerOnlyDirectory(existing, error), error);
    BOOST_CHECK(grc_test::SameAcl(ReadAcl(existing), before));

    const fs::path file = dir.path / "file";
    CreateWithAcl(file, util::OwnerOnlyAclSpec(/*inheritable=*/false));
    BOOST_CHECK(!util::CreateOwnerOnlyDirectory(file, error));
    BOOST_CHECK(!error.empty());
    BOOST_CHECK(fs::is_regular_file(file));
}

#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
