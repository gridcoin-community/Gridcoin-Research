// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_WIN_ADMIN_DROP_H
#define GRIDCOIN_UTIL_WIN_ADMIN_DROP_H

#include "util/win_acl.h"

#include <cstdint>
#include <string>
#include <vector>

//! A same-account core started with administrator rights gives them up before it starts.
//!
//! Windows has no documented way to lower a running process's primary token, so the process
//! starts itself again under a restricted copy of its own token: the groups of User Account
//! Control's filtered token are deny-only in it, every privilege but SeChangeNotifyPrivilege is
//! gone, its integrity level is Medium, and its owner and default DACL name the token user. The
//! first process then only waits for the copy and exits with its exit code. It writes nothing:
//! this runs before the data directory or the configuration is touched.
//!
//! Which start needs it, and which groups the copy makes deny-only, are pure, so they build and
//! are tested on every platform. Building the token and starting the copy need Windows.
namespace util::winadmin {

using util::winacl::Sid;

//! The environment variable the copy is started with, set to "1". A process that finds it set
//! and still needs the drop refuses to start, so the drop never loops. It decides nothing else:
//! a process that does not need the drop runs whether it is set or not, and reads it only to
//! log STARTED_WITHOUT_ADMIN_RIGHTS.
inline constexpr char RIGHTS_DROPPED_ENV[] = "GRIDCOIN_ADMIN_RIGHTS_DROPPED";

//! What the copy logs once logging is up.
inline constexpr char STARTED_WITHOUT_ADMIN_RIGHTS[] =
    "Started without administrator rights: the process that started Gridcoin had them.";

//! Whether \p sid is one of the groups User Account Control makes deny-only in an
//! administrator's filtered token: BUILTIN\Administrators, Power Users, Account Operators,
//! Server Operators, Print Operators, Backup Operators, RAS Servers, Pre-Windows 2000
//! Compatible Access, Network Configuration Operators and Cryptographic Operators;
//! S-1-5-114 (a local account that is a member of Administrators); and, by RID on any domain
//! SID (S-1-5-21-x-y-z), Domain Admins, Domain Controllers, Cert Publishers, Schema Admins,
//! Enterprise Admins, Group Policy Creator Owners, Read-only Domain Controllers and
//! Enterprise Read-only Domain Controllers. Pure.
bool IsUacFilteredGroup(const Sid& sid);

//! The SIDs of \p groups that the copy makes deny-only: each one IsUacFilteredGroup names that
//! is enabled (GROUP_ENABLED, and not already GROUP_USE_FOR_DENY_ONLY), in their order. Pure.
std::vector<Sid> SelectDenyOnlyGroups(const std::vector<util::winacl::TokenGroup>& groups);

//! Whether a token needs the drop: it is \p token_elevated (TokenElevation), runs above Medium
//! integrity (util::winacl::AboveMediumOrElevated), or has \p filtered_group_enabled, an
//! enabled group of IsUacFilteredGroup. Pure.
bool DropNeeded(bool token_elevated, uint32_t integrity_rid, bool filtered_group_enabled);

//! What a process that may host a same-account core does first.
enum class StartAction : uint8_t
{
    //! Start as it is.
    Run,
    //! Start a restricted copy of itself, and exit with its exit code.
    Drop,
    //! Refuse the start: this is the copy, and its token still needs the drop.
    Refuse,
};

//! What DecideStart decides on.
struct StartFacts
{
    //! The command line asks for the Windows service (util::winsvc::HasServiceFlag).
    bool service{false};
    //! The command line asks for the RPC client (IsRpcClientCommandLine).
    bool rpc_client{false};
    //! The command line asks for the report-only permission check (-reportpermissions, read with
    //! util::winsvc::CommandLineFlag), which starts no core.
    bool report_permissions{false};
    //! The token is elevated (TokenElevation).
    bool token_elevated{false};
    //! The token's integrity level, a SECURITY_MANDATORY_*_RID.
    uint32_t integrity_rid{util::winacl::INTEGRITY_MEDIUM_RID};
    //! The token has an enabled group of IsUacFilteredGroup (SelectDenyOnlyGroups is not empty).
    bool filtered_group_enabled{false};
    //! RIGHTS_DROPPED_ENV is set to "1".
    bool rights_dropped_marker{false};
};

//! Run for the service, which runs as its own virtual account; for the RPC client, which must
//! still read the service's configuration file when an administrator runs it; and for the
//! report-only permission check, which starts no core and which the installer runs elevated;
//! whatever the token. Otherwise Run when the token does not need the drop (DropNeeded), the
//! marker notwithstanding; Refuse when it does and the marker is set; Drop when it does and the
//! marker is not set. Pure.
StartAction DecideStart(const StartFacts& facts);

//! The one line that refuses the start, for \p reason: the step that failed and the Windows
//! error. It ends "Start Gridcoin from a normal (not elevated) session.". ASCII apart from
//! \p reason. Pure.
std::string RefusalMessage(const std::string& reason);

#ifdef WIN32
//! What GiveUpAdminRights did.
struct StartOutcome
{
    enum class Kind : uint8_t
    {
        //! This process starts as it is.
        Run,
        //! The restricted copy ran and exited with exit_code, which this process exits with.
        Exited,
        //! The start is refused with the line in refusal.
        Refused,
    };

    Kind kind{Kind::Run};
    uint32_t exit_code{0};
    std::string refusal;
};

//! The start's drop of administrator rights, first thing in main(), for a process that may host
//! a same-account core: \p service, \p rpc_client and \p report_permissions as StartFacts states
//! them. Reads this process's token and RIGHTS_DROPPED_ENV, then follows DecideStart:
//! - Run: removes RIGHTS_DROPPED_ENV from the environment, so that a process this one starts
//!   does not inherit it, and returns Run;
//! - Drop: ignores Ctrl+C and Ctrl+Break, which reach the copy through the shared console, and
//!   waits for the copy before it lets a close, logoff or shutdown event end this process; asks
//!   to be shut down after the copy at logoff and shutdown; sets RIGHTS_DROPPED_ENV; starts the
//!   copy (CreateRestrictedProcessToken, StartRestrictedSuspended with this module's path and
//!   GetCommandLineW() unchanged, ResumeAndWait), and returns Exited with its exit code;
//! - Refuse, or any step that fails: returns Refused with RefusalMessage's line. Never runs on
//!   with the rights it was started with.
//! Nothing is written. Only this process's console handler, shutdown level and environment are
//! changed. The console handler, once the copy runs, does not return from a close, logoff or
//! shutdown event: it waits for the copy and leaves the exit to the main thread, which then
//! exits with the copy's exit code.
StartOutcome GiveUpAdminRights(bool service, bool rpc_client, bool report_permissions);

//! Whether this process is the copy a drop started: GiveUpAdminRights found RIGHTS_DROPPED_ENV
//! set and returned Run. For the STARTED_WITHOUT_ADMIN_RIGHTS line only.
bool StartedByAdminDrop();

//! The restricted copy of this process's token: CreateRestrictedToken with
//! DISABLE_MAX_PRIVILEGE, no restricting SIDs, and the groups SelectDenyOnlyGroups picks as
//! deny-only; then its integrity level set to Medium, its owner to the token user and its
//! default DACL to the token user and SYSTEM, each with GENERIC_ALL. The token object's own
//! DACL is set to the token user and SYSTEM with TOKEN_ALL_ACCESS, so that the copy can open its
//! own token: the list a new token gets from an elevated token grants Administrators, which is
//! deny-only in the copy. It is a primary token, with the access the process token was opened
//! for: TOKEN_DUPLICATE, TOKEN_QUERY, TOKEN_ASSIGN_PRIMARY, TOKEN_ADJUST_DEFAULT, READ_CONTROL
//! and WRITE_DAC. Throws std::system_error naming the step.
util::winacl::UniqueHandle CreateRestrictedProcessToken();

//! A process started by StartRestrictedSuspended: its process and thread handles, and the job
//! that holds it. Closing the job kills it.
struct RestrictedChild
{
    util::winacl::UniqueHandle process;
    util::winacl::UniqueHandle thread;
    util::winacl::UniqueHandle job;
};

//! Starts \p application with \p command_line under the primary token \p token
//! (CreateProcessAsUserW), suspended, in this process's console, environment and current
//! directory and with its startup information, and puts it in a new job that kills it when its
//! last handle closes (JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) and that the processes it starts are
//! not held in (JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK). It inherits only inheritable copies of
//! this process's standard handles, the ones that are valid (PROC_THREAD_ATTRIBUTE_HANDLE_LIST),
//! and none when the shell's hot key or monitor take their places in the startup information.
//! Throws std::system_error naming the step; a process already started is then killed.
RestrictedChild StartRestrictedSuspended(void* token, const std::wstring& application,
                                         const std::wstring& command_line);

//! Resumes \p child, waits for it to exit and returns its exit code. Throws std::system_error
//! naming the step.
uint32_t ResumeAndWait(const RestrictedChild& child);
#endif // WIN32

} // namespace util::winadmin

#endif // GRIDCOIN_UTIL_WIN_ADMIN_DROP_H
