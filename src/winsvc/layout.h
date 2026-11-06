// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WINSVC_LAYOUT_H
#define GRIDCOIN_WINSVC_LAYOUT_H

#include "fs.h"
#include "ipc/win_endpoint.h"
#include "util/win_acl.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

//! The Windows service's names, the tree gridcoin-service.exe keeps for it under
//! %ProgramData%\Gridcoin with each entry's owner and access list, and the configuration file it
//! writes there.
//!
//! Both networks share one tree. Gridcoin\conf holds each network's configuration file,
//! Gridcoin\mainnet and Gridcoin\testnet are the services' data directories, and Gridcoin\ipc holds
//! each service's IPC directory. Every entry is owned by Administrators and has a protected access
//! list stated here in full, so nothing above it decides who has access. The service reads its
//! configuration file and cannot change it; it has full access to its own data directory alone.
//!
//! Everything here is pure, so it builds and is tested on every platform. Paths are joined with
//! the platform's separator, and BuildImagePath (winsvc/scm.h) writes them with backslashes.
namespace winsvc {

using util::winacl::Sid;

enum class Network : uint8_t
{
    Main,
    Test,
};

//! What a network's service is called and where its files are: the service name, its display
//! name, its virtual account, its attach group, its configuration file's name, the names of its
//! data directory and IPC directory under Gridcoin\ and Gridcoin\ipc\, and whether it is the
//! test network.
struct NetworkNames
{
    std::wstring service, display, account, group, conf_file, datadir_leaf, ipc_leaf;
    bool testnet{false};
};

//! Main: gridcoinresearchd, "Gridcoin Research core (mainnet)", NT SERVICE\gridcoinresearchd,
//! "Gridcoin GUI Users", mainnet.conf, mainnet, mainnet. Test: gridcointestnetd,
//! "Gridcoin Research core (testnet)", NT SERVICE\gridcointestnetd, "Gridcoin Testnet GUI Users",
//! testnet.conf, testnet, testnet.
NetworkNames NamesFor(Network net);

//! The accounts the layout names: Administrators, SYSTEM, each network's service account and
//! each network's attach group.
struct LayoutSids
{
    Sid admins, system, svc_main, svc_test, grp_main, grp_test;
};

//! One directory or file of the layout, and the owner and protected access list it must have.
struct LayoutEntry
{
    fs::path path;
    bool directory{true};
    util::winacl::AclSpec acl;
};

namespace detail {
// The single rights by their bare names, which are <windows.h>'s macros of the same values in a
// file that included it first (see util/win_acl.h).
using namespace util::winacl::mask;
constexpr uint32_t FULL = FILE_ALL_ACCESS;
constexpr uint32_t CONF_READ = READ_CONTROL | FILE_READ_DATA | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE;
constexpr uint32_t STAT_AND_ACL = READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE;
constexpr uint32_t ROOT_SERVICE = STAT_AND_ACL;
constexpr uint32_t IPC_PARENT_SERVICE = STAT_AND_ACL;
constexpr uint32_t IPC_SERVICE = ipc::win::DIR_VIEW_MASK | FILE_READ_EA | FILE_ADD_FILE | FILE_DELETE_CHILD;
} // namespace detail

//! A service's rights on its configuration file: FILE_GENERIC_READ (checked against the SDK in
//! winsvc/scm.cpp). A GENERIC_READ open, as the core's, needs every one of them, FILE_READ_EA
//! included; none of them changes the file.
constexpr uint32_t CONF_READ_MASK = detail::CONF_READ;

//! Each service's rights on Gridcoin\ itself: read its attributes, as the boost::filesystem calls
//! that make sure each data directory exists do (a status query opens it with
//! FILE_READ_ATTRIBUTES | FILE_READ_EA), and its access list, as the core's check of the
//! directories above its autounlock file does. Not inherited, and nothing that lists or changes
//! it.
constexpr uint32_t ROOT_SERVICE_MASK = detail::ROOT_SERVICE;

//! Each service's rights on Gridcoin\ipc: read its attributes, as the core's preparation of its
//! IPC directory makes sure the directory above exists, and its access list. Not inherited, and
//! nothing that lists or changes it.
constexpr uint32_t IPC_PARENT_SERVICE_MASK = detail::IPC_PARENT_SERVICE;

//! A service's rights on its own IPC directory: view it as the attach group does, read its
//! extended attributes, which boost::filesystem::canonical opens it for, and add and remove the
//! socket and cookie it serves.
constexpr uint32_t IPC_SERVICE_MASK = detail::IPC_SERVICE;

//! <programdata>\Gridcoin
fs::path GridcoinRoot(const fs::path& programdata);

//! <programdata>\Gridcoin\mainnet or ...\Gridcoin\testnet: the service's -datadir.
fs::path DataDir(const fs::path& programdata, Network net);

//! What the service's GetDataDir() returns: DataDir, and for Test its testnet\ below.
fs::path NetDataDir(const fs::path& programdata, Network net);

//! <programdata>\Gridcoin\ipc\mainnet or ...\ipc\testnet: the service's -ipcdir.
fs::path IpcDir(const fs::path& programdata, Network net);

//! <programdata>\Gridcoin\conf\mainnet.conf or ...\conf\testnet.conf: the service's -conf.
fs::path ConfPath(const fs::path& programdata, Network net);

//! <programdata>\Gridcoin\conf\quarantine
fs::path QuarantineDir(const fs::path& programdata);

//! <programdata>\Gridcoin\conf\mainnet.state or ...\conf\testnet.state: whose wallet the service
//! holds, written when one is moved in.
fs::path StatePath(const fs::path& programdata, Network net);

//! The whole tree, parents before what they hold: Gridcoin, conf, both configuration files,
//! quarantine, both data directories, ipc and both IPC directories. Every entry is owned by
//! \p sids.admins, and Administrators and SYSTEM have full access to it.
//!
//! - Gridcoin: also each attach group with ipc::win::DIR_VIEW_MASK and each service with
//!   ROOT_SERVICE_MASK, none inherited.
//! - conf, conf\quarantine: nobody else, inherited.
//! - conf\<net>.conf: also that network's service with CONF_READ_MASK.
//! - <net> (the data directory): also that network's service with full access, inherited.
//! - ipc: also each attach group with DIR_VIEW_MASK and each service with
//!   IPC_PARENT_SERVICE_MASK, not inherited.
//! - ipc\<net>: also that network's service with IPC_SERVICE_MASK and that network's group with
//!   DIR_VIEW_MASK. Nothing there is inherited, so the socket and cookie carry only the lists the
//!   core gives them.
//!
//! The full access of Administrators and SYSTEM is inherited everywhere except on a configuration
//! file and an IPC directory.
std::vector<LayoutEntry> ServiceLayout(const fs::path& programdata, const LayoutSids& sids);

//! The SID of the virtual account NT SERVICE\<service>: S-1-5-80 and five sub-authorities, the
//! SHA-1 of the upper-case name in UTF-16LE read as five little-endian 32-bit numbers (as `sc
//! showsid` gives it, whether or not the service exists). nullopt for an empty name or one that is
//! not all ASCII, which this tool never uses: Windows upper-cases other letters by its own table.
std::optional<Sid> VirtualServiceSid(const std::wstring& service);

//! The configuration file install writes, in lines ending in LF: a comment that says who owns it,
//! rpcuser=gridcoinrpc, rpcpassword=\p rpcpassword, server=1, rpcserviceuser=<the network's
//! account> and, for Test, testnet=1. Never rpcallowip, rpcbind or includeconf. \p rpcpassword is
//! the base58 text of 32 random bytes.
std::string BuildServiceConf(Network net, const std::string& rpcpassword);

//! False when a line of \p conf_text is an includeconf directive: a line whose name, with a
//! section prefix such as "main." taken off, is includeconf or noincludeconf, in any case and with
//! any spaces or leading '-' around it. A directive under a [section] header counts too; a comment
//! does not. The CLI honours rpcserviceuser only from a file without one.
bool ServiceConfAcceptable(const std::string& conf_text);

//! What install adds to an existing configuration file \p conf_text that it accepts: the lines of
//! BuildServiceConf that set rpcuser, rpcpassword (\p rpcpassword) and rpcserviceuser, each only
//! when the file does not set that name for \p net. As ArgsManager reads the file, a name set at
//! the top, before any [section] header, counts, and so does one in the network's own section
//! (main or test): under its [main] or [test] header, or as "main.<name>" or "test.<name>". A
//! negated name ("no<name>") counts as set. When the file has a [section] header, the lines come
//! under the network's own header, as a line at the end of the file would otherwise land in the
//! last section. They come after a line break when the text does not end in one. Empty when the
//! file sets all three. Nothing already there is changed, so a file the administrators edited
//! keeps their values.
std::string MissingServiceConfLines(Network net, const std::string& conf_text, const std::string& rpcpassword);

//! The value of the first "<key>=<value>" line of the state file \p text, or nullopt when no line
//! sets \p key. A line may end in CRLF.
std::optional<std::string> StateFileValue(const std::string& text, const std::string& key);

//! What enable writes to the state file: user_sid=\p user_sid, from=\p from (the root the wallet
//! came from, in UTF-8), enabled_at=\p enabled_at and task_disabled=0 or 1, one line each, ending
//! in LF.
std::string StateFileText(const std::string& user_sid, const std::string& from, const std::string& enabled_at,
                          bool task_disabled);

//! The owner and access list enable gives the state file from its creation: owned by
//! \p sids' Administrators, with Administrators and SYSTEM given full access and nothing passed
//! down. StateFileRefusal accepts it.
util::winacl::AclSpec StateFileSpec(const LayoutSids& sids);

//! Whether \p image, a process's image name, is the core's: gridcoinresearch.exe (the GUI, which
//! hosts a core) or gridcoinresearchd.exe, in any case.
bool IsCoreImageName(const std::wstring& image);

//! Why status does not show the state file whose owner and access list are \p acl, or nullopt. It
//! must have a DACL and be owned by Administrators or SYSTEM, and its list may name only those two,
//! which is what conf\ passes down to a file created there (util::winacl::FindForeignTrustee). A
//! file another account owns, or can read or change, may have been put there by that account.
//! Pure.
std::optional<std::string> StateFileRefusal(const util::winacl::ObservedAcl& acl);

//! The accounts that may own an entry install finds directly inside \p level, a layout directory
//! that was there before install: Administrators, SYSTEM and TrustedInstaller, and, inside a
//! network's data directory or IPC directory, that network's service, whose core makes the entries
//! there. Pure.
std::vector<Sid> LevelEntryOwners(const fs::path& programdata, const fs::path& level, const LayoutSids& sids);

//! Whether install checks the entries directly inside \p level when it finds the level in place:
//! every directory level, whether or not its list already matches the layout's
//! (\p list_matches_layout), as a list says who can add an entry now and not who added one before.
//! Never a file. Pure.
bool ChecksLevelEntries(const LayoutEntry& level, bool list_matches_layout);

//! One entry found directly inside a layout directory, read without following it.
struct LevelEntryFacts
{
    fs::path path;
    //! A reparse point that names another path: a junction, a symbolic link or a mount point.
    bool link{false};
    //! nullopt when the owner could not be read.
    std::optional<Sid> owner;
    //! The name Windows gives the owner, empty when it gives none.
    std::string owner_name;
};

//! Why install refuses \p entry, found directly inside the layout directory \p level, or nullopt.
//! A link (a junction, a symbolic link or a mount point) is refused whatever its name, a path of
//! \p layout included: no layout path is ever a link, and the level's list must not be passed down
//! through one. Any other entry at a path of \p layout is nullopt: a directory of the layout is
//! checked as a level of its own (by its owner and kind, and its own entries), the network's
//! configuration file by CheckConf, and the other network's configuration file is left to that
//! network's install, whose CheckConf refuses one the administrators do not own with the layout's
//! list. Any other entry is refused when its owner is not one of LevelEntryOwners or cannot be
//! read. The text names the entry, and its owner by name and SID. Pure.
std::optional<std::string> LevelEntryRefusal(const std::vector<LayoutEntry>& layout, const fs::path& programdata,
                                             const fs::path& level, const LevelEntryFacts& entry,
                                             const LayoutSids& sids);

} // namespace winsvc

#endif // GRIDCOIN_WINSVC_LAYOUT_H
