// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WINSVC_TRANSFER_H
#define GRIDCOIN_WINSVC_TRANSFER_H

#include "fs.h"
#include "util/win_acl.h"
#include "winsvc/layout.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

//! How gridcoin-service.exe moves a wallet's tree between a user's data directory and the
//! service's: enable copies it in, disable copies it back out.
//!
//! Every file is copied to a new file, read back and compared with its source by SHA-256. The
//! source is never changed; the caller removes it only after the copy is complete. Nothing is
//! followed: a symbolic link, a junction or any other reparse point, a file with more than one
//! hard link and anything that is neither a file nor a directory are refused before anything is
//! written. Every directory on the way is held open while what it holds is read or changed, and
//! everything below it is opened as itself.
//!
//! The rules and checks are pure and tested on every platform. The walk, copy and removal run on
//! POSIX, where the tests use them, and on Windows.
namespace winsvc {

//! One entry of a tree as read without following it. \p relative is its path below the tree's
//! root.
struct EntryFacts
{
    fs::path relative;
    enum class Type : uint8_t
    {
        Directory,
        Regular,
        Symlink,
        //! A Windows reparse point other than a symbolic link: a junction, a mount point, a socket
        //! file or a cloud file placeholder.
        Reparse,
        //! Neither a file, a directory nor a link: a FIFO, a socket or a device.
        Other,
    } type{Type::Other};
    uint64_t link_count{1};
    uint64_t size{0};
};

//! What a transfer does with an entry: copies it (a directory is created and its entries
//! decided in turn), leaves it out, copies it to the quarantine directory instead, copies it with
//! FilterSettingsJson applied, or refuses the whole transfer.
enum class EntryDecision : uint8_t
{
    Copy,
    Skip,
    Quarantine,
    FilterSettings,
    Refuse,
};

//! Which entries a transfer takes and how. Names are compared without regard to ASCII case, as
//! Windows opens them, and match at any depth.
struct TransferRules
{
    //! Left out: runtime files the core makes again at each start, and the autounlock directory.
    std::vector<std::string> skip_names;
    //! Copied to the quarantine directory instead of the destination.
    std::vector<std::string> quarantine_names;
    //! Copied with FilterSettingsJson applied.
    std::vector<std::string> filter_settings_names;
    //! Top-level directories of the source that are not part of the transfer.
    std::vector<std::string> exclude_top_dirs;
    //! When set, the only top-level entry of the source that is part of the transfer.
    std::optional<std::string> only_top_dir;
    //! A file the caller holds open until the source is gone and removes itself: the data
    //! directory's lock file, as a path below the source's root. The transfer skips it, and
    //! leaves it and the directories it is in out of TransferResult::source_remove.
    std::optional<fs::path> hold;
};

//! The most a settings file may hold for FilterSettings to read it.
constexpr uint64_t SETTINGS_SIZE_LIMIT = 1 << 20;

//! Moving a user's tree into the service: skip node.sock, ipc.cookie, .lock,
//! gridcoinresearchd.pid and autounlock (the user's autounlock directory); quarantine
//! gridcoinresearch.conf and DB_CONFIG, which the service must not read; filter
//! gridcoinsettings.json. Main leaves out the top-level testnet directory; Test takes only it.
TransferRules RulesForEnable(Network net);

//! Moving the service's tree back to a user: skip node.sock, ipc.cookie, .lock,
//! gridcoinresearchd.pid and autounlock (the service's autounlock directory), and nothing else.
//! Main leaves out a top-level testnet directory; Test takes only it.
TransferRules RulesForDisable(Network net);

//! Whether the entry at \p relative is part of the transfer by \p rules' top-level scope.
bool InScope(const fs::path& relative, const TransferRules& rules);

//! What a transfer does with \p e, and why in \p reason for Skip and Refuse. In order: an entry
//! outside InScope is skipped; one with a skip name is skipped whatever it is, as a socket file
//! left behind would be refused otherwise; a symbolic link, a reparse point, anything else that is
//! not a file or a directory, and a file with more than one link are refused; a file with a
//! quarantine name is quarantined; a file with a filter name is filtered, and refused when it is
//! larger than SETTINGS_SIZE_LIMIT; a directory with either name is refused; everything else is
//! copied. Pure.
EntryDecision Decide(const EntryFacts& e, const TransferRules& rules, std::string& reason);

//! Drops Refuse-class keys; returns the filtered JSON and the dropped key names.
//!
//! A key is dropped when IsRefusedSettingKey takes it, the rule the core applies when it reads
//! the settings file. The kept keys keep their order and values, written as the core writes the
//! file. When nothing is dropped, \p json is returned unchanged. Throws std::runtime_error when
//! \p json is not a JSON object or repeats a key, as the core would refuse it.
std::pair<std::string, std::vector<std::string>> FilterSettingsJson(const std::string& json);

struct TransferResult
{
    bool ok{false};
    std::string error;
    //! The files written to the destination, and their bytes.
    uint64_t files{0};
    uint64_t bytes{0};
    //! The top-level entries created under the destination, as names relative to it.
    std::vector<fs::path> created_top;
    //! The files written under the quarantine directory, as full paths.
    std::vector<fs::path> quarantined;
    //! The keys FilterSettingsJson dropped.
    std::vector<std::string> dropped_settings;
    //! The entries of the source that the transfer took and the caller removes once the copy is
    //! complete, as paths relative to it: copied, filtered or skipped, but not quarantined, not
    //! outside the rules' scope and not the held file. A directory is named whole when nothing in
    //! it stays; otherwise its other entries are named one by one and it stays with them.
    std::vector<fs::path> source_remove;
    //! The quarantined entries, which stay in the source because the account's own programs read
    //! them there, as paths relative to it.
    std::vector<fs::path> source_kept;
    //! Whether the destination directory itself was created.
    bool created_dst{false};
};

//! Why \p dst cannot take the transfer of \p src under \p rules, or nullopt: it is there and is
//! not a directory (a link to one included), or it holds an entry at a top-level name the
//! transfer would create there. Entries of other names are kept as they are. Also a reason when
//! \p src cannot be read.
std::optional<std::string> DestinationRefusal(const fs::path& src, const fs::path& dst, const TransferRules& rules);

//! dst must be absent, an empty directory, or a directory that holds none of the top-level names
//! the transfer creates (DestinationRefusal); entries of other names are never touched, as when
//! disable copies into a user's directory that kept its configuration file. Every copied file is
//! re-read and its SHA-256 compared with the source. On any refusal or error, removes everything
//! it created under dst and in \p quarantine_dir; src is never modified.
//!
//! Every entry is decided (Decide) before anything is written, so a refusal writes nothing.
//! Directories are created exclusively, as are files (POSIX: O_CREAT|O_EXCL|O_NOFOLLOW, mode 0600,
//! directories 0700; Windows: CREATE_NEW, with the access list the destination's parent passes
//! down). Quarantined files go to \p quarantine_dir, below their relative path; it is created when
//! the first one needs it. A settings file whose keys were dropped is quarantined unchanged too, so
//! that nothing of it is lost.
TransferResult TransferTree(const fs::path& src, const fs::path& dst, const TransferRules& rules,
                            const fs::path& quarantine_dir);

//! Removes entries without following links (a link is removed as an entry).
//!
//! Removes \p root / p for each relative path p of \p only_top, with everything below it. The
//! directories on the way are opened as themselves and held while their entries are removed. A
//! path that is not there is passed over, and \p root itself is never removed. False, with
//! \p error naming the path, at the first entry that cannot be removed, or for a path in
//! \p only_top that is not relative or has a "." or ".." component.
bool RemoveTreeNoFollow(const fs::path& root, const std::vector<fs::path>& only_top, std::string& error);

//! Removes \p root / \p relative when it is a directory with nothing in it, without following
//! anything; \p removed says whether it did. A directory that holds something, or is not there, is
//! left as it is and is not an error. False, with \p error, for a path that is not plain
//! (PlainRelative), or that cannot be read or removed.
bool RemoveDirIfEmpty(const fs::path& root, const fs::path& relative, bool& removed, std::string& error);

//! One step to do after the point of no return of enable or disable.
struct FollowUp
{
    //! What the step does, for the warning.
    std::string name;
    //! What to do by hand to finish it.
    std::string finish;
    //! Throws when it fails.
    std::function<void()> run;
    //! Index of an earlier step this one needs to have succeeded, or -1.
    int needs{-1};
};

//! Runs \p steps in order, each on its own: one that throws is recorded and the ones after it still
//! run, except one whose \p needs step failed, which is not run. Returns a warning for each step
//! that failed or was not attempted, naming it and what to do by hand. Pure apart from the steps.
std::vector<std::string> RunFollowUps(const std::vector<FollowUp>& steps);

namespace testing {
//! Called with the path of each file written, after it is flushed and before it is read back.
//! Empty outside tests, which use it to corrupt what was written.
extern std::function<void(const fs::path&)> after_write;
} // namespace testing

//! Why \p path cannot be the root of a wallet tree the tool moves, or nullopt: it must be an
//! absolute path on a local drive ("X:\..."), with no "." or ".." component. A network path is
//! refused. '/' counts as '\'. Pure.
std::optional<std::string> LocalTreePathProblem(const std::wstring& path);

//! The owner and root access list a tree moved into the service gets (ResetTreeSecurity): the
//! service account owns everything below the network data directory, and the network data
//! directory gets the layout's list for the service's data directory, owner included.
struct MovedTreeSecurity
{
    Sid owner;
    util::winacl::AclSpec root_spec;
};

//! MovedTreeSecurity for \p net: owner \p sids' service account, root_spec the ServiceLayout entry
//! of DataDir(\p programdata, \p net). Pure.
MovedTreeSecurity EnableSecurity(const fs::path& programdata, Network net, const LayoutSids& sids);

//! The owner-only list a tree moved back to \p user gets: owned by \p user, with \p user and
//! SYSTEM given FILE_ALL_ACCESS, inherited by what a directory holds. Pure.
util::winacl::AclSpec OwnerOnlySpec(const Sid& user);

//! \p spec for a file: the same owner and entries, none of them passed down. Pure.
util::winacl::AclSpec SpecForFile(const util::winacl::AclSpec& spec);

//! Why \p acl, read back from an entry below the root ResetTreeSecurity gave \p root_spec, is not
//! what it set, or nullopt: \p owner must own it, and its DACL must be present, not protected, and
//! hold exactly the inherited copies of \p root_spec's entries that are passed down, with no entry
//! of its own. Pure.
std::optional<std::string> InheritedOnlyRefusal(const util::winacl::ObservedAcl& acl, const Sid& owner,
                                                const util::winacl::AclSpec& root_spec);

#ifdef WIN32
//! Root gets `root_spec`; every descendant gets owner `owner` and an empty, unprotected DACL (inherits only).
//!
//! Each entry is opened as itself through the directories held above it, and set through its
//! handle (SetKernelObjectSecurity), which passes nothing down. A descendant's list is written as
//! the inherited copies of \p root_spec's inheritable entries, which is what Windows would give it,
//! and read back (InheritedOnlyRefusal). A root that is a file gets SpecForFile(\p root_spec). A
//! link, a reparse point or a file with more than one link anywhere in the tree is refused. Needs
//! SeRestorePrivilege, for an owner other than this account. Throws std::runtime_error naming the
//! path.
void ResetTreeSecurity(const fs::path& root, const Sid& owner, const util::winacl::AclSpec& root_spec);
#endif

} // namespace winsvc

#endif // GRIDCOIN_WINSVC_TRANSFER_H
