// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/fsmodes.h"

#include "util/dir_permissions.h"
#include "random.h"
#include "span.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/string.h"

#include <algorithm>
#include <set>
#include <utility>

// open() on POSIX, _O_TEXT on Windows.
#include <fcntl.h>

#ifdef WIN32
#include <system_error>

#include <io.h>
#include <windows.h>
#else
#include "util/syserror.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace fsmodes {

std::string WinNotOwnedText(const fs::path& path, const WinOwnership& ownership, const util::winacl::Sid& self,
                            const std::string& self_name)
{
    const auto shown = [](const util::winacl::Sid& sid, const std::string& name) {
        std::string text = util::winacl::FormatSid(sid);
        if (text.empty()) text = "an unreadable SID";
        return name.empty() ? text : name + " (" + text + ")";
    };

    switch (ownership.kind) {
    case WinOwnership::Kind::Owned:
        return {};
    case WinOwnership::Kind::Unopened:
        return "it could not be opened (" + ownership.error + ")";
    case WinOwnership::Kind::Link:
        return "it is a link (a reparse point), which is not followed; use the file it leads to instead";
    case WinOwnership::Kind::NoAclVolume:
        return "it is on a volume that keeps no owners or access lists, such as FAT or exFAT, so it cannot be shown "
               "to be this account's; keep it on an NTFS volume";
    case WinOwnership::Kind::Unreadable:
        return "its owner could not be read (" + ownership.error + ")";
    case WinOwnership::Kind::Foreign:
        break;
    }

    // As icacls reads a SID: marked with an asterisk. Setting another account as the owner
    // needs an administrator's privileges, so the command is run elevated.
    return "it is owned by " + shown(ownership.owner, ownership.owner_name) + ", not by this account, " +
           shown(self, self_name) + ". To make it this account's, in a command prompt opened as an administrator "
           "(elevated) run: icacls \"" + path.string() + "\" /setowner *" + util::winacl::FormatSid(self);
}

namespace {

using util::winacl::AceKind;
using util::winacl::ObservedAce;
using util::winacl::ObservedAcl;
using util::winacl::Sid;

//! The paths CheckOwnerOnlyModes checks, in the same order on every platform, through the
//! platform's checker.
//!
//! follow_link is for the paths an operator may move to another disk behind a link: the data
//! directory, the backup directory, the configuration file and debug.log. Every other path
//! keeps the rule that a link is reported and not followed.
template <typename Checker>
void CheckWalletFiles(Checker& checker, const fs::path& datadir, const fs::path& conf, const fs::path& settings,
                      const fs::path& wallet_file, const fs::path& backupdir, const fs::path& debuglog)
{
    checker.Check(datadir, /*follow_link=*/true);
    checker.Check(wallet_file);

    if (!datadir.empty()) {
        const fs::path database = datadir / "database";
        checker.Check(database);
        checker.CheckEntries(database, [](const std::string& name) { return name.compare(0, 4, "log.") == 0; });
    }

    checker.Check(conf, /*follow_link=*/true);
    checker.Check(settings);

    checker.Check(backupdir, /*follow_link=*/true);
    checker.CheckEntries(backupdir, [](const std::string&) { return true; }, /*follow_link=*/true);

    checker.Check(debuglog, /*follow_link=*/true);
}

// The single rights by their bare names: <windows.h>'s macros where it is included,
// util::winacl::mask's constants elsewhere, with the same values.
namespace rights {
using namespace util::winacl::mask;

//! The rights that change a path: FILE_WRITEISH, and FILE_DELETE_CHILD, with which an account
//! can delete any entry of a directory, such as wallet.dat, a backup or the conf. A file grants
//! nothing by FILE_DELETE_CHILD, but an entry giving it there, such as one inherited from an
//! (OI)(DC) grant on the directory, is reported too. That is harmless: the directory is
//! reported as well, and removing the entry from the file is right.
constexpr uint32_t CHANGE = FILE_WRITEISH | FILE_DELETE_CHILD;
} // namespace rights

// The file attributes and the reparse tag bit ClassifyWinEntry reads, with their Win32 values.
constexpr uint32_t ATTRIBUTE_DIRECTORY = 0x10;
constexpr uint32_t ATTRIBUTE_REPARSE_POINT = 0x400;
//! The bit IsReparseTagNameSurrogate() tests: a reparse point with it set stands for another
//! named entity, so a path through it leads elsewhere.
constexpr uint32_t TAG_NAME_SURROGATE = 0x20000000;

// The Win32 errors AclRefusedBy and UnopenedPathFinding read, with their values.
constexpr uint32_t WIN32_FILE_NOT_FOUND = 2, WIN32_PATH_NOT_FOUND = 3;
constexpr uint32_t WIN32_ACCESS_DENIED = 5, WIN32_NOT_SUPPORTED = 50, WIN32_INVALID_OWNER = 1307,
                   WIN32_INVALID_PRIMARY_GROUP = 1308, WIN32_PRIVILEGE_NOT_HELD = 1314, WIN32_INVALID_ACL = 1336,
                   WIN32_INVALID_SECURITY_DESCR = 1338;

#ifdef WIN32
static_assert(ATTRIBUTE_DIRECTORY == FILE_ATTRIBUTE_DIRECTORY);
static_assert(ATTRIBUTE_REPARSE_POINT == FILE_ATTRIBUTE_REPARSE_POINT);
static_assert(IsReparseTagNameSurrogate(TAG_NAME_SURROGATE) && !IsReparseTagNameSurrogate(~TAG_NAME_SURROGATE));
static_assert(WIN32_FILE_NOT_FOUND == ERROR_FILE_NOT_FOUND && WIN32_PATH_NOT_FOUND == ERROR_PATH_NOT_FOUND);
static_assert(WIN32_ACCESS_DENIED == ERROR_ACCESS_DENIED && WIN32_NOT_SUPPORTED == ERROR_NOT_SUPPORTED);
static_assert(WIN32_INVALID_OWNER == ERROR_INVALID_OWNER && WIN32_INVALID_PRIMARY_GROUP == ERROR_INVALID_PRIMARY_GROUP);
static_assert(WIN32_PRIVILEGE_NOT_HELD == ERROR_PRIVILEGE_NOT_HELD && WIN32_INVALID_ACL == ERROR_INVALID_ACL);
static_assert(WIN32_INVALID_SECURITY_DESCR == ERROR_INVALID_SECURITY_DESCR);
#endif

//! The accounts the Windows rule allows: this one, SYSTEM and Administrators.
std::vector<Sid> AllowedAccounts(const Sid& self)
{
    return {self, util::winacl::wellknown::System(), util::winacl::wellknown::Administrators()};
}

bool IsAllowed(const std::vector<Sid>& allowed, const Sid& sid)
{
    return std::find(allowed.begin(), allowed.end(), sid) != allowed.end();
}

//! The name Windows shows for a group that the access lists of a volume commonly hold, or
//! nullptr.
const char* WellKnownName(const std::string& sid)
{
    static const std::pair<const char*, const char*> NAMES[] = {
        {"S-1-1-0", "Everyone"},
        {"S-1-5-7", "ANONYMOUS LOGON"},
        {"S-1-5-11", "Authenticated Users"},
        {"S-1-5-18", "SYSTEM"},
        {"S-1-5-32-544", "Administrators"},
        {"S-1-5-32-545", "Users"},
        {"S-1-5-32-546", "Guests"},
    };

    for (const auto& [text, name] : NAMES) {
        if (sid == text) return name;
    }

    return nullptr;
}

//! \p sid as the warning shows it: its string form, followed by the name of a well-known group.
std::string Describe(const Sid& sid)
{
    const std::string text = util::winacl::FormatSid(sid);
    if (text.empty()) return "an account with an unreadable SID";

    if (const char* name = WellKnownName(text)) return text + " (" + name + ")";

    return text;
}

//! An account outside the allowed ones that the access list lets read or change the path.
struct ForeignGrant
{
    Sid trustee;
    //! Every right its entries give it.
    uint32_t mask{0};
    //! Whether one of those entries is inherited.
    bool inherited{false};
};

//! The accounts outside \p allowed that an allow entry lets read or change the path, as
//! FindForeignReader and FindForeignWriter find them, each once, in the order of its first
//! entry.
std::vector<ForeignGrant> ForeignGrants(const ObservedAcl& acl, const std::vector<Sid>& allowed)
{
    constexpr uint32_t RIGHTS = util::winacl::mask::READISH | rights::CHANGE;

    std::vector<ForeignGrant> grants;
    if (!acl.dacl_present) return grants;

    for (const ObservedAce& ace : acl.aces) {
        const bool grant = ace.kind == AceKind::Allow || ace.kind == AceKind::OtherAllow;
        if (!grant || ace.inherit_only || (ace.mask & RIGHTS) == 0 || IsAllowed(allowed, ace.trustee)) continue;

        const auto it = std::find_if(grants.begin(), grants.end(),
                                     [&ace](const ForeignGrant& g) { return g.trustee == ace.trustee; });
        if (it == grants.end()) {
            grants.push_back({ace.trustee, ace.mask, ace.inherited});
        } else {
            it->mask |= ace.mask;
            it->inherited = it->inherited || ace.inherited;
        }
    }

    return grants;
}

} // anonymous namespace

std::optional<std::string> AclRefusedBy(uint32_t error, bool remote)
{
    const bool refused = error == WIN32_INVALID_OWNER || error == WIN32_INVALID_PRIMARY_GROUP ||
                         error == WIN32_INVALID_ACL || error == WIN32_INVALID_SECURITY_DESCR ||
                         error == WIN32_NOT_SUPPORTED || error == WIN32_PRIVILEGE_NOT_HELD;

    if (refused) return std::string{remote ? "the network share" : "the volume"};

    // A share may refuse an owner it does not take with a refusal of access. On a local volume
    // that refusal is about the directory, and the file is not created at all.
    if (remote && error == WIN32_ACCESS_DENIED) return std::string{"the network share"};

    return std::nullopt;
}

std::optional<ModeFinding> UnopenedPathFinding(const fs::path& path, uint32_t error, const std::string& error_text)
{
    if (error == WIN32_FILE_NOT_FOUND || error == WIN32_PATH_NOT_FOUND) return std::nullopt;

    const std::string why = error == util::winacl::WIN32_UNTRUSTED_MOUNT_POINT
                                ? std::string{util::winacl::UNTRUSTED_MOUNT_POINT_REASON}
                                : "it could not be opened (" + error_text + ")";

    return ModeFinding{path, ModeFinding::Kind::Unreadable, 0,
                       "WARNING: " + path.string() + " cannot be checked: " + why +
                           ". Its access list was not checked"};
}

std::optional<ModeFinding> UnlistedDirectoryFinding(const fs::path& dir, uint32_t error, const std::string& error_text)
{
    if (error == WIN32_FILE_NOT_FOUND || error == WIN32_PATH_NOT_FOUND) return std::nullopt;

    return ModeFinding{dir, ModeFinding::Kind::Unreadable, 0,
                       "WARNING: " + dir.string() + " cannot be checked: its entries could not be listed (" +
                           error_text + "). The access lists of the files in it were not checked"};
}

bool OnNetworkShare(const std::optional<std::wstring>& final_path, const std::function<bool()>& by_name)
{
    // Without a final path the directory is not taken to be local, where a refusal of access
    // would not be retried: the name still shows a share reached by a UNC path or a mapped
    // drive letter.
    if (!final_path) return by_name();

    const std::wstring unc = L"\\\\?\\UNC\\";
    return final_path->compare(0, unc.size(), unc) == 0;
}

WinEntry ClassifyWinEntry(uint32_t attributes, uint32_t reparse_tag)
{
    WinEntry entry;
    entry.directory = (attributes & ATTRIBUTE_DIRECTORY) != 0;
    entry.reparse = (attributes & ATTRIBUTE_REPARSE_POINT) != 0;
    // The tag means something only on a reparse point.
    entry.link = entry.reparse && (reparse_tag & TAG_NAME_SURROGATE) != 0;

    return entry;
}

WinAclVerdict ClassifyWinAcl(const ObservedAcl& acl, const Sid& self)
{
    const std::vector<Sid> allowed = AllowedAccounts(self);

    // Both also count a missing DACL and an owner outside the allowed accounts. Reading the
    // file needs a right of READISH; rights::CHANGE holds the rights that change it, and those
    // (WRITE_DAC, WRITE_OWNER) with which an account can give itself read.
    if (!util::winacl::FindForeignReader(acl, allowed) &&
        !util::winacl::FindForeignWriter(acl, allowed, rights::CHANGE)) {
        return WinAclVerdict::Clean;
    }

    return acl.owner == self ? WinAclVerdict::Wide : WinAclVerdict::WideNotOwned;
}

std::string WinAclWarning(const fs::path& path, const ObservedAcl& acl, const Sid& self, bool directory)
{
    const std::vector<Sid> allowed = AllowedAccounts(self);
    const std::vector<ForeignGrant> grants = ForeignGrants(acl, allowed);
    const bool foreign_owner = !acl.owner || !IsAllowed(allowed, *acl.owner);

    std::vector<std::string> problems;
    if (!acl.dacl_present) problems.emplace_back("it has no DACL, so every account has full access");
    if (!acl.owner) {
        problems.emplace_back("its owner could not be read");
    } else if (foreign_owner) {
        problems.push_back("it is owned by " + Describe(*acl.owner) + ", which can change its access list");
    }
    for (const ForeignGrant& grant : grants) {
        const bool reads = (grant.mask & util::winacl::mask::READISH) != 0;
        const bool changes = (grant.mask & rights::CHANGE) != 0;
        problems.push_back(strprintf("%s can %s it (access mask 0x%08x)", Describe(grant.trustee),
                                     reads && changes ? "read and change" : reads ? "read" : "change", grant.mask));
    }

    // The accounts to take off the list, as icacls reads a SID: marked with an asterisk.
    std::string removals;
    bool inherited = false;
    for (const ForeignGrant& grant : grants) {
        const std::string text = util::winacl::FormatSid(grant.trustee);
        if (!text.empty()) removals += " *" + text;
        inherited = inherited || grant.inherited;
    }

    const std::string quoted = "\"" + path.string() + "\"";
    std::string message = "WARNING: " + path.string() + " gives other accounts access to wallet data: " +
                          Join(problems, "; ") + ". ";

    if (acl.owner == self) {
        // The list the core gives what it creates: this account and SYSTEM, protected, and on a
        // directory inherited by what is inside. /inheritance:r drops every inherited entry,
        // and /remove:g the other accounts' own entries.
        const std::string full = directory ? ":(OI)(CI)F\"" : ":F\"";
        std::string command =
            "icacls " + quoted + " /inheritance:r /grant:r \"*" + util::winacl::FormatSid(self) + full;
        if (self != util::winacl::wellknown::System()) command += " \"*S-1-5-18" + full;
        if (!removals.empty()) command += " /remove:g" + removals;

        return message + "To make it owner-only, run in a command prompt: " + command;
    }

    // Another account's path is reported only. The fix takes the other accounts' access away
    // and gives this account no entry. Where the owner itself is the problem, an account
    // outside the allowed ones or one that could not be read, it also makes this account the
    // owner. Who can run it: an administrator always, elevated (SeTakeOwnershipPrivilege and
    // SeRestorePrivilege, which sets another account as owner); this account where its own
    // entry lets it take ownership (WRITE_OWNER) or change the permissions (WRITE_DAC); and,
    // where the owner stays, the owner, which may always change the permissions.
    std::vector<std::string> steps;
    if (foreign_owner) {
        // Named among the problems already.
        message += "To fix it, in a command prompt run as an administrator (elevated), or as this account where it "
                   "may take ownership, ";
        steps.push_back("make this account its owner: icacls " + quoted + " /setowner *" +
                        util::winacl::FormatSid(self));
    } else {
        message += "It is owned by " + Describe(*acl.owner) +
                   ". To fix it, in a command prompt run as its owner, as an administrator (elevated), or as this "
                   "account where it may change the permissions, ";
    }
    if (!removals.empty()) {
        // An inherited entry cannot be removed, so the inherited entries are first made the
        // path's own.
        steps.push_back("remove the other accounts' access: " +
                        (inherited ? "icacls " + quoted + " /inheritance:d && " : std::string{}) + "icacls " + quoted +
                        " /remove:g" + removals);
    }
    if (!acl.dacl_present) {
        steps.emplace_back("give it an access list that names only this account, SYSTEM and Administrators");
    }
    if (steps.empty()) steps.emplace_back("remove the access of the accounts named above");

    return message + Join(steps, ", then ");
}

std::string WinNoAclVolumeWarning(const fs::path& path)
{
    return "WARNING: " + path.string() + " is on a volume that keeps no access lists, such as FAT or exFAT, so "
           "other accounts on this computer can read and change it, and no access list can be set on it. Keep it "
           "on an NTFS volume instead.";
}

std::optional<std::string> BackupDirWarning(const fs::path& backupdir, const BackupDirCheck& check)
{
    if (check.not_writable) {
#ifdef WIN32
        return strprintf("-backupdir %s is not writable (%s): automatic wallet backups will fail. Make it writable "
                         "by the account the core runs as, or set -backupdir to a directory that account can write.",
                         backupdir.string(), *check.not_writable);
#else
        return strprintf("-backupdir %s is not writable (%s): automatic wallet backups will fail. Make it writable "
                         "by the account the core runs as (under a systemd unit with ProtectSystem=, also list it in "
                         "ReadWritePaths=), or set -backupdir to a directory that account can write.",
                         backupdir.string(), *check.not_writable);
#endif
    }
    if (check.not_owner_only) {
#ifdef WIN32
        return strprintf("Wallet backups in %s cannot be made owner-only (%s). This happens on a volume that keeps "
                         "no access lists, such as FAT or exFAT, and on a network share that does not accept the "
                         "owner-only access list. The backups are still written there, but other users of this "
                         "computer, or of the share, may be able to read them.",
                         backupdir.string(), *check.not_owner_only);
#else
        return strprintf("Wallet backups in %s cannot be made owner-only (%s). This happens on a filesystem without "
                         "POSIX permissions, such as FAT. The backups are still written there, but other users of "
                         "this computer may be able to read them.",
                         backupdir.string(), *check.not_owner_only);
#endif
    }
    return std::nullopt;
}

namespace {
//! Creates \p probe, a new file, as a backup copy is created, and removes it again. \p check
//! receives why it could not be created, or why it is not owner-only.
void ProbeBackupDir(const fs::path& probe, BackupDirCheck& check);
} // anonymous namespace

BackupDirCheck CheckBackupDir(const fs::path& backupdir)
{
    // A random name: under PrivatePIDs= every start has the same pid, so a name made
    // of the pid would meet the probe of an earlier start that was killed before it
    // removed it.
    return CheckBackupDir(backupdir, [] {
        unsigned char random[8];
        GetRandBytes(Span<unsigned char>(random, sizeof(random)));
        return ".gridcoin-write-test-" + HexStr(Span<const unsigned char>(random, sizeof(random)));
    });
}

BackupDirCheck CheckBackupDir(const fs::path& backupdir, const std::function<std::string()>& probe_name)
{
    BackupDirCheck check;

    // Restricted where the filesystem allows it. A directory that cannot be is kept:
    // backups that other users might read are better than no backups.
    std::string error;
    if (!util::CreateDirectoryRestrictedIfPossible(backupdir, error, check.not_owner_only)) {
        check.not_writable = error;
        return check;
    }

    ProbeBackupDir(backupdir / probe_name(), check);

    return check;
}

#ifndef WIN32

namespace {

//! The path as it is written into a command line a user can paste into a shell.
std::string ShellQuote(const std::string& text)
{
    const bool plain = !text.empty() && text.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./-+=@:,") == std::string::npos;

    if (plain) return text;

    std::string quoted = "'";
    for (const char c : text) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    return quoted + "'";
}

class ModeChecker
{
public:
    std::vector<ModeFinding> m_findings;

    //! follow_link: see CheckWalletFiles.
    void Check(const fs::path& path, bool follow_link = false)
    {
        if (path.empty() || !m_seen.insert(path).second) return;

        struct stat st;
        if ((follow_link ? ::stat(path.string().c_str(), &st) : ::lstat(path.string().c_str(), &st)) != 0) return;

        if (S_ISLNK(st.st_mode)) {
            m_findings.push_back({path, ModeFinding::Kind::Symlink, 0,
                                  "WARNING: " + path.string() + " is a symbolic link, so its mode was not checked"});
            return;
        }

        // The permission bits fit an unsigned.
        const unsigned mode = static_cast<unsigned>(st.st_mode & 07777);

        if (NotOwnedModeAcceptable(mode)) return;

        const bool ours = st.st_uid == ::geteuid();
        const std::string fix = std::string("chmod ") + (S_ISDIR(st.st_mode) ? "700 " : "600 ") + ShellQuote(path.string());

        std::string message = "WARNING: " + path.string() + " has mode " + strprintf("%04o", mode)
                            + ", which gives other accounts access to wallet data. ";
        if (ours) {
            message += "To make it owner-only run: " + fix;
        } else {
            message += "It is owned by another account, so only its owner can change it: " + fix;
        }

        m_findings.push_back({path, ours ? ModeFinding::Kind::WideMode : ModeFinding::Kind::WideModeNotOwned,
                              mode, message});
    }

    //! The entries of a directory whose names pass the filter, each checked as a path of its own.
    //! follow_link reaches the entries of a directory that is itself a link; the entries keep the
    //! rule that a link is reported.
    template <typename Filter>
    void CheckEntries(const fs::path& dir, Filter filter, bool follow_link = false)
    {
        struct stat st;
        if (dir.empty()) return;
        if ((follow_link ? ::stat(dir.string().c_str(), &st) : ::lstat(dir.string().c_str(), &st)) != 0) return;
        if (!S_ISDIR(st.st_mode)) return;

        boost::system::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            const fs::path entry = it->path();
            struct stat entry_st;
            if (::lstat(entry.string().c_str(), &entry_st) != 0) continue;

            // Subdirectories are not part of the check. A symbolic link is, so it is reported.
            if (S_ISDIR(entry_st.st_mode)) continue;

            if (filter(entry.filename().string())) Check(entry);
        }
    }

private:
    std::set<fs::path> m_seen;
};

} // anonymous namespace

std::vector<ModeFinding> CheckOwnerOnlyModes(const fs::path& datadir, const fs::path& conf,
                                             const fs::path& settings, const fs::path& wallet_file,
                                             const fs::path& backupdir, const fs::path& debuglog)
{
    ModeChecker checker;
    CheckWalletFiles(checker, datadir, conf, settings, wallet_file, backupdir, debuglog);

    return std::move(checker.m_findings);
}

bool OwnedByUs(const fs::path& path)
{
    struct stat st;
    if (::lstat(path.string().c_str(), &st) != 0) return false;

    return !S_ISLNK(st.st_mode) && st.st_uid == ::geteuid();
}

AppendResult AppendToOwnedFile(const fs::path& path, const std::string& text)
{
    // O_NOFOLLOW refuses a symbolic link (ELOOP; EMLINK on FreeBSD), so whatever it leads to
    // is never opened. O_NONBLOCK keeps a FIFO with no reader from holding the caller; it
    // changes nothing for the regular file the check below lets through. No O_CREAT: the file
    // must exist.
    const int fd = ::open(path.string().c_str(),
                          O_WRONLY | O_APPEND | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) {
        const int err = errno;
        if (err == ELOOP || err == EMLINK) return {AppendStatus::NotOwned, {}};
        return {AppendStatus::OpenFailed, SysErrorString(err)};
    }

    // Checked on what was opened, not on the name: the name may lead somewhere else by now.
    struct stat st;
    if (::fstat(fd, &st) != 0) {
        const int err = errno;
        ::close(fd);
        return {AppendStatus::OpenFailed, SysErrorString(err)};
    }
    if (!S_ISREG(st.st_mode) || st.st_uid != ::geteuid()) {
        ::close(fd);
        return {AppendStatus::NotOwned, {}};
    }

    int err = 0;
    size_t done = 0;
    while (done < text.size()) {
        const ssize_t n = ::write(fd, text.data() + done, text.size() - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            err = errno;
            break;
        }
        // write() returns at most what was asked, which is non-negative here.
        done += static_cast<size_t>(n);
    }

    if (::close(fd) != 0 && err == 0) err = errno;
    if (err != 0) return {AppendStatus::WriteFailed, SysErrorString(err)};

    return {AppendStatus::Appended, {}};
}

bool NotOwnedModeAcceptable(unsigned mode)
{
    // No access at all for "other" and no write for the group.
    return (mode & 007) == 0 && (mode & 020) == 0;
}

namespace {
//! Why a file this process created with mode 0600 through \p fd has another mode, or nullopt.
std::optional<std::string> NewFileModeProblem(int fd)
{
    struct stat st;
    if (::fstat(fd, &st) != 0) return std::nullopt;

    // The permission bits fit an unsigned.
    const unsigned mode = static_cast<unsigned>(st.st_mode & 07777);
    if ((mode & 077) == 0) return std::nullopt;

    return strprintf("a file created there with mode 0600 has mode %04o", mode);
}

void ProbeBackupDir(const fs::path& probe, BackupDirCheck& check)
{
    const auto create = [&probe] {
        return ::open(probe.string().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    };

    int fd = create();
    if (fd < 0 && errno == EEXIST) {
        // Taken: a probe that was never removed. It is removed, and the probe tried once more.
        ::unlink(probe.string().c_str());
        fd = create();
    }
    if (fd < 0) {
        check.not_writable = SysErrorString(errno);
        return;
    }

    // A directory that was there already is not restricted here, but the probe shows
    // whether the files written into it can be owner-only.
    if (!check.not_owner_only) check.not_owner_only = NewFileModeProblem(fd);

    ::close(fd);
    ::unlink(probe.string().c_str());
}
} // anonymous namespace

std::optional<std::string> CopyFileOwnerOnly(const fs::path& source, const fs::path& target)
{
    const auto fail = [](const char* what, const fs::path& path, int err) {
        throw fs::filesystem_error(what, path, boost::system::error_code(err, boost::system::generic_category()));
    };

    const int in = ::open(source.string().c_str(), O_RDONLY | O_CLOEXEC);
    if (in < 0) fail("CopyFileOwnerOnly: cannot open the source", source, errno);

    // A new target is created with mode 0600, so it is owner-only before any data is written, and
    // the mode is set again through the descriptor, so a umask that clears the owner's bits does not
    // leave a copy its owner cannot read. A target that exists is opened as it is, and keeps its mode.
    int out = ::open(target.string().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    // A filesystem without POSIX modes gives a new file a mode of its own, whatever fchmod says.
    std::optional<std::string> not_owner_only;
    if (out >= 0) {
        (void)::fchmod(out, 0600);
        not_owner_only = NewFileModeProblem(out);
    }
    if (out < 0 && errno == EEXIST) {
        out = ::open(target.string().c_str(), O_WRONLY | O_CLOEXEC);

        struct stat in_st, out_st;
        if (out >= 0 && ::fstat(in, &in_st) == 0 && ::fstat(out, &out_st) == 0
            && in_st.st_dev == out_st.st_dev && in_st.st_ino == out_st.st_ino) {
            ::close(in);
            ::close(out);
            fail("CopyFileOwnerOnly: the source and the target are the same file", target, EEXIST);
        }

        // Truncated only after the same-file check above.
        if (out >= 0 && ::ftruncate(out, 0) != 0) {
            const int err = errno;
            ::close(in);
            ::close(out);
            fail("CopyFileOwnerOnly: cannot truncate the target", target, err);
        }
    }
    if (out < 0) {
        const int err = errno;
        ::close(in);
        fail("CopyFileOwnerOnly: cannot open the target", target, err);
    }

    int err = 0;
    char buffer[65536];
    for (;;) {
        const ssize_t n = ::read(in, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR) continue;
            err = errno;
            break;
        }
        if (n == 0) break;

        // write() returns at most n, which is non-negative here.
        const size_t size = static_cast<size_t>(n);
        size_t done = 0;
        while (done < size) {
            const ssize_t w = ::write(out, buffer + done, size - done);
            if (w < 0) {
                if (errno == EINTR) continue;
                err = errno;
                break;
            }
            done += static_cast<size_t>(w);
        }
        if (err != 0) break;
    }

    ::close(in);
    if (::close(out) != 0 && err == 0) err = errno;

    if (err != 0) fail("CopyFileOwnerOnly: copy failed", target, err);
    return not_owner_only;
}

std::FILE* CreateNewOwnerOnlyFile(const fs::path& path, bool& taken, std::optional<std::string>& not_owner_only,
                                  std::string& error)
{
    taken = false;
    not_owner_only.reset();
    error.clear();

    // O_EXCL refuses any name that is taken, a symbolic link included whether or not it
    // leads anywhere; O_NOFOLLOW says so twice. Mode 0600 from creation, which a umask can
    // only narrow.
    const int fd = ::open(path.string().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        const int err = errno;
        taken = err == EEXIST;
        error = strprintf("cannot create %s: %s", path.string(), SysErrorString(err));
        return nullptr;
    }

    // A filesystem without POSIX modes gives a new file a mode of its own.
    not_owner_only = NewFileModeProblem(fd);

    std::FILE* const file = ::fdopen(fd, "w");
    if (!file) {
        error = strprintf("cannot open %s for writing: %s", path.string(), SysErrorString(errno));
        ::close(fd);
        ::unlink(path.string().c_str());
        return nullptr;
    }

    return file;
}

#else // WIN32

namespace {

using util::winacl::UniqueHandle;

//! "<message> (error <n>)", the message left out unless it is printable ASCII, as every log
//! line must be: Windows words it in its display language.
std::string Win32ErrorText(DWORD error)
{
    std::string message = std::system_category().message(static_cast<int>(error));

    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ' ||
                                message.back() == '.')) {
        message.pop_back();
    }

    const bool printable = std::all_of(message.begin(), message.end(), [](char c) { return c >= 0x20 && c < 0x7f; });
    if (message.empty() || !printable) return strprintf("error %u", error);

    return strprintf("%s (error %u)", message, error);
}

//! The reason a std::system_error from util::winacl gives, in ASCII. Its code is the Win32 error.
std::string ReasonOf(const std::system_error& e)
{
    return Win32ErrorText(static_cast<DWORD>(e.code().value()));
}

//! \p path opened to read its attributes and access list, a reparse point at \p path opened as
//! itself unless \p follow_link. A null handle when it cannot be opened, with the Win32 error in
//! \p error when it is given.
UniqueHandle OpenForCheck(const fs::path& path, bool follow_link, DWORD* error = nullptr)
{
    const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (follow_link ? 0 : FILE_FLAG_OPEN_REPARSE_POINT);

    UniqueHandle handle(::CreateFileW(path.wstring().c_str(), READ_CONTROL | FILE_READ_ATTRIBUTES,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      flags, nullptr));
    if (!handle && error) *error = ::GetLastError();

    return handle;
}

//! What \p handle refers to, read from its attributes and reparse tag, or nullopt when they
//! cannot be read.
std::optional<WinEntry> EntryOf(HANDLE handle)
{
    FILE_ATTRIBUTE_TAG_INFO info{};
    if (!::GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info))) return std::nullopt;

    return ClassifyWinEntry(info.FileAttributes, info.ReparseTag);
}

//! Whether the volume of the object \p handle refers to keeps access lists
//! (FILE_PERSISTENT_ACLS). A volume that cannot be asked is taken to keep them.
bool VolumeKeepsAcls(HANDLE handle)
{
    DWORD flags = 0;
    if (!::GetVolumeInformationByHandleW(handle, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0)) return true;

    return (flags & FILE_PERSISTENT_ACLS) != 0;
}

//! The final path of the directory \p dir as a handle on it shows it (GetFinalPathNameByHandleW,
//! VOLUME_NAME_DOS), or nullopt when the directory cannot be opened or the path cannot be read.
//! What a network redirector serves has a UNC final path, \\?\UNC\server\share\..., however it
//! was reached: through a mapped drive letter, a UNC path or a symbolic link to one.
std::optional<std::wstring> FinalPathOf(const fs::path& dir)
{
    // Opened following any link at it, as a backup directory moved behind a link is supported,
    // so the handle is on the directory the link leads to.
    const UniqueHandle handle(::CreateFileW(dir.wstring().c_str(), FILE_READ_ATTRIBUTES,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (!handle) return std::nullopt;

    // FILE_NAME_OPENED, the name as it was opened: only its volume part is read, which is the
    // same as in the normalized name (for a followed link, the volume of its target), and the
    // normalized query is reported to fail on some third-party redirectors and virtual volumes.
    const DWORD flags = FILE_NAME_OPENED | VOLUME_NAME_DOS;

    // The first call may be short; it then gives the size needed, the terminating null included.
    std::wstring name(MAX_PATH, L'\0');
    DWORD length = ::GetFinalPathNameByHandleW(handle.get(), name.data(), static_cast<DWORD>(name.size()), flags);
    if (length >= name.size()) {
        name.resize(length);
        length = ::GetFinalPathNameByHandleW(handle.get(), name.data(), static_cast<DWORD>(name.size()), flags);
    }
    // On success the length leaves out the terminating null, so it is below the buffer's size.
    if (length == 0 || length >= name.size()) return std::nullopt;
    name.resize(length);

    return name;
}

//! Whether the volume the name \p dir is on is a network share (GetVolumePathNameW, then
//! GetDriveTypeW). It sees a UNC path and a mapped drive letter; it reads the name, so it is
//! asked only where no handle shows where a link leads.
bool NameOnRemoteVolume(const fs::path& dir)
{
    wchar_t root[MAX_PATH + 1];
    if (!::GetVolumePathNameW(dir.wstring().c_str(), root, MAX_PATH + 1)) return false;

    return ::GetDriveTypeW(root) == DRIVE_REMOTE;
}

//! Whether the directory that \p path is created in is on a network share (OnNetworkShare). Its
//! final path decides, since that is read through a link to the directory it leads to. Where the
//! directory cannot be opened (on a share where this account may add files and not read the
//! directory's attributes, say) or the path cannot be read, its name decides.
bool ParentOnShare(const fs::path& path)
{
    const fs::path dir = path.has_parent_path() ? path.parent_path() : fs::path(".");

    return OnNetworkShare(FinalPathOf(dir), [&dir] { return NameOnRemoteVolume(dir); });
}

class AclChecker
{
public:
    explicit AclChecker(Sid self) : m_self(std::move(self)) {}

    std::vector<ModeFinding> m_findings;

    //! follow_link: see CheckWalletFiles. Without it a link is opened as itself, never as what
    //! it leads to.
    void Check(const fs::path& path, bool follow_link = false)
    {
        if (path.empty() || !m_seen.insert(path).second) return;

        DWORD open_error = 0;
        const UniqueHandle handle = OpenForCheck(path, follow_link, &open_error);
        if (!handle) {
            Unopened(path, open_error);
            return;
        }
        const std::optional<WinEntry> entry = EntryOf(handle.get());
        if (!entry) return;

        if (entry->link && !follow_link) {
            m_findings.push_back({path, ModeFinding::Kind::Symlink, 0,
                                  "WARNING: " + path.string() +
                                      " is a symbolic link or junction, so its access list was not checked"});
            return;
        }

        // On a volume that keeps no access lists every account has access, whatever is read
        // back, and only another volume helps.
        if (!VolumeKeepsAcls(handle.get())) {
            m_findings.push_back({path, ModeFinding::Kind::WideMode, 0, WinNoAclVolumeWarning(path)});
            return;
        }

        // Read through the handle: the path may lead somewhere else by now.
        ObservedAcl acl;
        try {
            acl = util::winacl::ReadAcl(handle.get(), /*kernel_object=*/false);
        } catch (const std::system_error& e) {
            Unreadable(path, ReasonOf(e));
            return;
        } catch (const std::exception& e) {
            Unreadable(path, e.what());
            return;
        }

        const WinAclVerdict verdict = ClassifyWinAcl(acl, m_self);
        if (verdict == WinAclVerdict::Clean) return;

        m_findings.push_back({path,
                              verdict == WinAclVerdict::Wide ? ModeFinding::Kind::WideMode
                                                             : ModeFinding::Kind::WideModeNotOwned,
                              0, WinAclWarning(path, acl, m_self, entry->directory)});
    }

    //! The entries of a directory whose names pass the filter, each checked as a path of its own.
    //! follow_link reaches the entries of a directory that is itself a link; the entries keep the
    //! rule that a link is reported.
    template <typename Filter>
    void CheckEntries(const fs::path& dir, Filter filter, bool follow_link = false)
    {
        if (dir.empty()) return;
        {
            const UniqueHandle handle = OpenForCheck(dir, follow_link);
            if (!handle) return;
            const std::optional<WinEntry> entry = EntryOf(handle.get());
            if (!entry || !entry->directory || (entry->link && !follow_link)) return;
        }

        boost::system::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            const fs::path path = it->path();
            DWORD open_error = 0;
            const UniqueHandle handle = OpenForCheck(path, /*follow_link=*/false, &open_error);
            if (!handle) {
                if (filter(path.filename().string()) && m_seen.insert(path).second) Unopened(path, open_error);
                continue;
            }
            const std::optional<WinEntry> entry = EntryOf(handle.get());
            if (!entry) continue;

            // Subdirectories are not part of the check. A link is, a junction included, so it
            // is reported.
            if (entry->directory && !entry->link) continue;

            if (filter(path.filename().string())) Check(path);
        }

        // A directory that opens but cannot be listed, or stops being listed partway, is
        // reported: what is in it was not checked.
        if (ec) {
            const uint32_t error = static_cast<uint32_t>(ec.value());
            const std::string error_text = ec.category() == boost::system::system_category()
                                               ? Win32ErrorText(static_cast<DWORD>(error))
                                               : strprintf("error %d", ec.value());
            if (std::optional<ModeFinding> finding = UnlistedDirectoryFinding(dir, error, error_text)) {
                m_findings.push_back(std::move(*finding));
            }
        }
    }

private:
    Sid m_self;
    std::set<fs::path> m_seen;
    //! The paths reported as reached through a junction Windows does not follow. A path below
    //! one of them fails to open for the same reason, and is not reported again.
    std::vector<fs::path> m_untrusted;

    //! Reports \p path, which exists but could not be opened with \p error (UnopenedPathFinding),
    //! once for each junction Windows does not follow.
    void Unopened(const fs::path& path, DWORD error)
    {
        if (error == util::winacl::WIN32_UNTRUSTED_MOUNT_POINT) {
            const auto below = [&path](const fs::path& reported) {
                return std::mismatch(reported.begin(), reported.end(), path.begin(), path.end()).first == reported.end();
            };
            if (std::any_of(m_untrusted.begin(), m_untrusted.end(), below)) return;
            m_untrusted.push_back(path);
        }

        if (std::optional<ModeFinding> finding = UnopenedPathFinding(path, error, Win32ErrorText(error))) {
            m_findings.push_back(std::move(*finding));
        }
    }

    void Unreadable(const fs::path& path, const std::string& reason)
    {
        m_findings.push_back({path, ModeFinding::Kind::Unreadable, 0,
                              "WARNING: the access list of " + path.string() + " could not be read (" + reason +
                                  "), so it was not checked"});
    }
};

//! Why the file this process created through \p handle with \p spec does not hold that list
//! now, or nullopt.
std::optional<std::string> NewFileAclProblem(HANDLE handle, const util::winacl::AclSpec& spec)
{
    try {
        if (std::optional<std::string> reason =
                util::winacl::CheckMatchesSpec(util::winacl::ReadAcl(handle, /*kernel_object=*/false), spec)) {
            return "a file created there with the owner-only access list does not keep it: " + *reason;
        }
    } catch (const std::system_error& e) {
        return "the access list of a file created there could not be read back (" + ReasonOf(e) + ")";
    } catch (const std::exception& e) {
        return std::string{"the access list of a file created there could not be read back ("} + e.what() + ")";
    }

    return std::nullopt;
}

//! The owner-only access list of a new file and a security descriptor that holds it, or why
//! they could not be made.
struct NewFileAcl
{
    util::winacl::AclSpec spec;
    util::winacl::UniqueSecurityDescriptor sd;
    std::optional<std::string> error;
};

NewFileAcl MakeNewFileAcl()
{
    NewFileAcl acl;
    try {
        acl.spec = util::OwnerOnlyAclSpec(/*inheritable=*/false);
        acl.sd.reset(util::winacl::MakeSecurityDescriptor(acl.spec));
    } catch (const std::system_error& e) {
        acl.error = ReasonOf(e);
    } catch (const std::exception& e) {
        acl.error = e.what();
    }

    return acl;
}

SECURITY_ATTRIBUTES AttributesFor(const NewFileAcl& acl)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = acl.sd.get();
    sa.bInheritHandle = FALSE;

    return sa;
}

//! Creates the new file \p path, with the owner-only list in \p acl from the start, opened with
//! \p access and \p flags. Where the volume refuses that list for what it is (a network share
//! that does not take this account as an owner, say), the file is created with the list it
//! inherits, and \p not_owner_only says why; it says so too when the new file does not hold the
//! list. Any other failure creates nothing. Returns INVALID_HANDLE_VALUE, with \p error, when no
//! file was created: ERROR_FILE_EXISTS or ERROR_ALREADY_EXISTS when the name is taken, by a
//! link too.
HANDLE CreateOwnerOnlyFile(const fs::path& path, DWORD access, DWORD flags, const NewFileAcl& acl,
                           std::optional<std::string>& not_owner_only, DWORD& error)
{
    // FILE_FLAG_OPEN_REPARSE_POINT: a link at the name is not followed, so the name counts as
    // taken, as O_EXCL treats one on POSIX, and nothing is created where the link leads.
    flags |= FILE_FLAG_OPEN_REPARSE_POINT;

    SECURITY_ATTRIBUTES sa = AttributesFor(acl);
    HANDLE handle = ::CreateFileW(path.wstring().c_str(), access, 0, &sa, CREATE_NEW, flags, nullptr);
    error = ::GetLastError();
    if (handle != INVALID_HANDLE_VALUE) {
        not_owner_only = NewFileAclProblem(handle, acl.spec);
        return handle;
    }

    // Only where the list itself was refused is the same file created without it. Whether the
    // parent is on a share is asked only where the answer can matter.
    const bool maybe_refused = error == ERROR_ACCESS_DENIED || AclRefusedBy(error, /*remote=*/false);
    const std::optional<std::string> refused_by = AclRefusedBy(error, maybe_refused && ParentOnShare(path));
    if (!refused_by) return handle;

    const DWORD first = error;
    handle = ::CreateFileW(path.wstring().c_str(), access, 0, nullptr, CREATE_NEW, flags, nullptr);
    error = ::GetLastError();
    if (handle != INVALID_HANDLE_VALUE) {
        not_owner_only = *refused_by + " did not accept a file with the owner-only access list (" +
                         Win32ErrorText(first) + ")";
    }

    return handle;
}

void ProbeBackupDir(const fs::path& probe, BackupDirCheck& check)
{
    // Created as a backup copy is, with the owner-only list, and removed when it is closed.
    // Without that list the probe still shows whether the directory can be written.
    const NewFileAcl acl = MakeNewFileAcl();
    if (acl.error && !check.not_owner_only) {
        check.not_owner_only = "the owner-only access list could not be made (" + *acl.error + ")";
    }

    const auto create = [&](std::optional<std::string>& not_owner_only, DWORD& error) {
        return CreateOwnerOnlyFile(probe, GENERIC_WRITE | READ_CONTROL | DELETE,
                                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, acl, not_owner_only, error);
    };

    std::optional<std::string> not_owner_only;
    DWORD error = 0;
    HANDLE handle = create(not_owner_only, error);
    if (handle == INVALID_HANDLE_VALUE && (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)) {
        // Taken: a probe that was never removed. It is removed, and the probe tried once more.
        ::DeleteFileW(probe.wstring().c_str());
        handle = create(not_owner_only, error);
    }
    if (handle == INVALID_HANDLE_VALUE) {
        check.not_writable = Win32ErrorText(error);
        return;
    }
    const UniqueHandle guard(handle);

    // A directory that was there already is not restricted here, but the probe shows
    // whether the files written into it can be owner-only.
    if (!check.not_owner_only) check.not_owner_only = not_owner_only;
}

//! The Win32 error codes, with the ASCII text Win32ErrorText gives them in place of the
//! system category's, which Windows words in its display language.
class AsciiWin32ErrorCategory final : public boost::system::error_category
{
public:
    const char* name() const noexcept override { return "win32"; }
    std::string message(int code) const override { return Win32ErrorText(static_cast<DWORD>(code)); }
};

const boost::system::error_category& AsciiWin32Category()
{
    static const AsciiWin32ErrorCategory category{};
    return category;
}

[[noreturn]] void CopyFailed(const char* what, const fs::path& path, DWORD error)
{
    throw fs::filesystem_error(what, path, boost::system::error_code(static_cast<int>(error), AsciiWin32Category()));
}

//! Whether \p a and \p b are one file: the same volume and file index. Throws as CopyFailed,
//! naming \p target, when that cannot be read.
bool SameFile(HANDLE a, HANDLE b, const fs::path& target)
{
    BY_HANDLE_FILE_INFORMATION ia{};
    BY_HANDLE_FILE_INFORMATION ib{};
    if (!::GetFileInformationByHandle(a, &ia) || !::GetFileInformationByHandle(b, &ib)) {
        CopyFailed("CopyFileOwnerOnly: cannot tell whether the source and the target are the same file", target,
                   ::GetLastError());
    }

    return ia.dwVolumeSerialNumber == ib.dwVolumeSerialNumber && ia.nFileIndexHigh == ib.nFileIndexHigh &&
           ia.nFileIndexLow == ib.nFileIndexLow;
}

} // anonymous namespace

std::vector<ModeFinding> CheckOwnerOnlyModes(const fs::path& datadir, const fs::path& conf,
                                             const fs::path& settings, const fs::path& wallet_file,
                                             const fs::path& backupdir, const fs::path& debuglog)
{
    Sid self;
    try {
        self = util::winacl::CurrentTokenUser();
    } catch (const std::system_error& e) {
        return {{datadir, ModeFinding::Kind::Unreadable, 0,
                 "WARNING: the access lists of the wallet files were not checked: this process's account could not "
                 "be read (" + ReasonOf(e) + ")"}};
    }

    AclChecker checker(std::move(self));
    CheckWalletFiles(checker, datadir, conf, settings, wallet_file, backupdir, debuglog);

    return std::move(checker.m_findings);
}

WinOwnership ReadWinOwnership(const fs::path& path)
{
    WinOwnership ownership;

    DWORD open_error = 0;
    const UniqueHandle handle = OpenForCheck(path, /*follow_link=*/false, &open_error);
    if (!handle) {
        ownership.kind = WinOwnership::Kind::Unopened;
        ownership.error = Win32ErrorText(open_error);
        return ownership;
    }

    const std::optional<WinEntry> entry = EntryOf(handle.get());
    if (!entry) {
        ownership.kind = WinOwnership::Kind::Unreadable;
        ownership.error = Win32ErrorText(::GetLastError());
        return ownership;
    }
    if (entry->reparse) {
        ownership.kind = WinOwnership::Kind::Link;
        return ownership;
    }

    // The owner of what was opened, not of the name.
    std::optional<util::winacl::Sid> owner;
    try {
        owner = util::winacl::ReadAcl(handle.get(), /*kernel_object=*/false).owner;
        if (owner && *owner == util::winacl::CurrentTokenUser()) {
            ownership.kind = WinOwnership::Kind::Owned;
            return ownership;
        }
    } catch (const std::system_error& e) {
        ownership.error = ReasonOf(e);
    } catch (const std::exception& e) {
        ownership.error = e.what();
    }

    // Not this account's. Why, for the message.
    if (!VolumeKeepsAcls(handle.get())) {
        ownership.kind = WinOwnership::Kind::NoAclVolume;
    } else if (owner) {
        ownership.kind = WinOwnership::Kind::Foreign;
        ownership.owner = *owner;
        ownership.owner_name = util::winacl::AccountNameOf(*owner);
    } else {
        ownership.kind = WinOwnership::Kind::Unreadable;
        if (ownership.error.empty()) ownership.error = "it has no owner";
    }

    return ownership;
}

std::string WinNotOwnedText(const fs::path& path)
{
    const WinOwnership ownership = ReadWinOwnership(path);
    if (ownership.kind == WinOwnership::Kind::Owned) return {};

    try {
        const util::winacl::Sid self = util::winacl::CurrentTokenUser();
        return WinNotOwnedText(path, ownership, self, util::winacl::AccountNameOf(self));
    } catch (const std::exception& e) {
        return std::string("it is not this account's, and this process's account could not be read (") + e.what() +
               ")";
    }
}

bool OwnedByUs(const fs::path& path)
{
    return ReadWinOwnership(path).kind == WinOwnership::Kind::Owned;
}

AppendResult AppendToOwnedFile(const fs::path& path, const std::string& text)
{
    // FILE_FLAG_OPEN_REPARSE_POINT opens a link as itself, so whatever it leads to is never
    // opened, and the check below refuses it. FILE_APPEND_DATA without FILE_WRITE_DATA writes
    // only at the end. OPEN_EXISTING: the file must exist.
    const HANDLE opened = ::CreateFileW(path.wstring().c_str(),
                                        FILE_APPEND_DATA | READ_CONTROL | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                                        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (opened == INVALID_HANDLE_VALUE) return {AppendStatus::OpenFailed, Win32ErrorText(::GetLastError())};
    const UniqueHandle handle(opened);

    // Checked on what was opened, not on the name: the name may lead somewhere else by now.
    const std::optional<WinEntry> entry = EntryOf(opened);
    if (!entry) return {AppendStatus::OpenFailed, Win32ErrorText(::GetLastError())};
    if (entry->reparse || entry->directory || ::GetFileType(opened) != FILE_TYPE_DISK) {
        return {AppendStatus::NotOwned, {}};
    }
    try {
        if (util::winacl::ReadAcl(opened, /*kernel_object=*/false).owner != util::winacl::CurrentTokenUser()) {
            return {AppendStatus::NotOwned, {}};
        }
    } catch (const std::system_error& e) {
        return {AppendStatus::OpenFailed, ReasonOf(e)};
    } catch (const std::exception& e) {
        return {AppendStatus::OpenFailed, e.what()};
    }

    size_t done = 0;
    while (done < text.size()) {
        // WriteFile takes a DWORD count, so a larger text goes in pieces.
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(text.size() - done, size_t{1} << 30));
        DWORD written = 0;
        if (!::WriteFile(opened, text.data() + done, chunk, &written, nullptr)) {
            return {AppendStatus::WriteFailed, Win32ErrorText(::GetLastError())};
        }
        if (written == 0) return {AppendStatus::WriteFailed, "nothing was written"};
        done += written;
    }

    return {AppendStatus::Appended, {}};
}

std::optional<std::string> CopyFileOwnerOnly(const fs::path& source, const fs::path& target)
{
    // Shared for writing too, so a source that is open elsewhere is still copied.
    const UniqueHandle in(::CreateFileW(source.wstring().c_str(), GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!in) CopyFailed("CopyFileOwnerOnly: cannot open the source", source, ::GetLastError());

    const NewFileAcl acl = MakeNewFileAcl();
    if (acl.error) {
        throw fs::filesystem_error("CopyFileOwnerOnly: the owner-only access list could not be made: " + *acl.error,
                                   target, boost::system::errc::make_error_code(boost::system::errc::io_error));
    }

    // A new target is created with the owner-only access list, so it is owner-only before any
    // data is written. A target that exists is opened as it is, a link at its name followed as
    // POSIX's O_WRONLY follows one, and keeps its list.
    std::optional<std::string> not_owner_only;
    DWORD error = 0;
    UniqueHandle out(CreateOwnerOnlyFile(target, GENERIC_WRITE | READ_CONTROL, FILE_ATTRIBUTE_NORMAL, acl,
                                         not_owner_only, error));
    if (!out && (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)) {
        out.reset(::CreateFileW(target.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr));
        error = ::GetLastError();

        if (out && SameFile(in.get(), out.get(), target)) {
            CopyFailed("CopyFileOwnerOnly: the source and the target are the same file", target, ERROR_FILE_EXISTS);
        }

        // Truncated only after the same-file check above. A file just opened is at its start.
        if (out && !::SetEndOfFile(out.get())) {
            CopyFailed("CopyFileOwnerOnly: cannot truncate the target", target, ::GetLastError());
        }
    }
    if (!out) CopyFailed("CopyFileOwnerOnly: cannot open the target", target, error);

    char buffer[65536];
    for (;;) {
        DWORD read = 0;
        if (!::ReadFile(in.get(), buffer, sizeof(buffer), &read, nullptr)) {
            CopyFailed("CopyFileOwnerOnly: copy failed", target, ::GetLastError());
        }
        if (read == 0) break;

        DWORD done = 0;
        while (done < read) {
            DWORD written = 0;
            if (!::WriteFile(out.get(), buffer + done, read - done, &written, nullptr)) {
                CopyFailed("CopyFileOwnerOnly: copy failed", target, ::GetLastError());
            }
            if (written == 0) CopyFailed("CopyFileOwnerOnly: copy failed", target, ERROR_WRITE_FAULT);
            done += written;
        }
    }

    return not_owner_only;
}

std::FILE* CreateNewOwnerOnlyFile(const fs::path& path, bool& taken, std::optional<std::string>& not_owner_only,
                                  std::string& error)
{
    taken = false;
    not_owner_only.reset();
    error.clear();

    const NewFileAcl acl = MakeNewFileAcl();
    if (acl.error) {
        error = "the owner-only access list could not be made: " + *acl.error;
        return nullptr;
    }

    // CREATE_NEW refuses a name that is taken, and a link at the name is not followed
    // (CreateOwnerOnlyFile), so nothing is created where a link leads.
    DWORD create_error = 0;
    HANDLE handle = CreateOwnerOnlyFile(path, GENERIC_WRITE | READ_CONTROL, FILE_ATTRIBUTE_NORMAL, acl,
                                        not_owner_only, create_error);
    if (handle == INVALID_HANDLE_VALUE) {
        taken = create_error == ERROR_FILE_EXISTS || create_error == ERROR_ALREADY_EXISTS;
        error = strprintf("cannot create %s: %s", path.string(), Win32ErrorText(create_error));
        return nullptr;
    }

    // In text mode, as the file was written before it was created this way. The descriptor
    // owns the handle once _open_osfhandle succeeds, and the FILE* owns the descriptor.
    const int fd = ::_open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_TEXT);
    if (fd == -1) {
        error = strprintf("cannot open %s for writing", path.string());
        ::CloseHandle(handle);
        ::DeleteFileW(path.wstring().c_str());
        return nullptr;
    }
    std::FILE* const file = ::_fdopen(fd, "w");
    if (!file) {
        error = strprintf("cannot open %s for writing", path.string());
        ::_close(fd);
        ::DeleteFileW(path.wstring().c_str());
        return nullptr;
    }

    return file;
}

#endif // WIN32

} // namespace fsmodes
