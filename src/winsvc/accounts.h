// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WINSVC_ACCOUNTS_H
#define GRIDCOIN_WINSVC_ACCOUNTS_H

#include "fs.h"
#include "util/win_acl.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

//! What gridcoin-service.exe's enable and disable read and change about the account whose wallet
//! moves: the account itself, its profile directory, the cores running as it, and its Core-Start
//! task. Windows only; the pure rules they use are in winsvc/layout.h and winsvc/transfer.h.
namespace winsvc {

#ifdef WIN32
using util::winacl::Sid;

//! A user account: its SID and the name Windows gives it ("DOMAIN\name", or the SID when it gives
//! none).
struct UserAccount
{
    Sid sid;
    std::string name;
};

//! The user account \p user names: a name LookupAccountNameW maps to a user account ("name",
//! "COMPUTER\name", "DOMAIN\name", "name@domain"), or the string form of a user account's SID,
//! which also names an account whose name does not resolve here. nullopt, with \p why, when it
//! names no account, or one that is not a user account (a group, a service account or a
//! computer). Throws std::runtime_error when the lookup itself fails.
std::optional<UserAccount> FindUserAccount(const std::string& user, std::string& why);

//! The profile directory Windows records for \p user: the ProfileImagePath value of
//! HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\<SID>, with its environment
//! strings expanded. nullopt when there is none, as for an account that never signed in. Throws
//! std::runtime_error when the value cannot be read.
std::optional<fs::path> ProfileDirectory(const Sid& user);

//! A core running as the account asked about: a GUI or daemon process.
struct CoreProcess
{
    uint32_t pid{0};
    uint32_t session{0};
    std::string image;
    //! The process's account could not be read, so it may be the account asked about.
    bool account_unknown{false};
};

//! Every process in every session whose image is the core's (IsCoreImageName) and whose token's
//! user is \p user, or whose user cannot be read (WTSEnumerateProcessesExW, level 1). A
//! same-account core started elevated runs as two processes, the elevated parent and the restricted
//! copy it starts, and both are listed: each token keeps the account's SID as its user. Throws
//! std::runtime_error when the processes cannot be listed.
std::vector<CoreProcess> CoresRunningAs(const Sid& user);

//! \Gridcoin\Core-Start as Task Scheduler has it.
struct CoreStartTask
{
    bool exists{false};
    bool enabled{false};
    //! The principal's user as the task names it, a name or a SID, in UTF-8.
    std::string principal;
    //! Its SID, or nullopt when it does not resolve to one.
    std::optional<Sid> principal_sid;
};

//! \Gridcoin\Core-Start, read through the Task Scheduler (COM). Throws std::runtime_error when the
//! Task Scheduler cannot be reached.
CoreStartTask ReadCoreStartTask();

//! Enables or disables \Gridcoin\Core-Start, and reads it back. Throws std::runtime_error when the
//! task is not there or cannot be changed.
void SetCoreStartTaskEnabled(bool enabled);
#endif // WIN32

} // namespace winsvc

#endif // GRIDCOIN_WINSVC_ACCOUNTS_H
