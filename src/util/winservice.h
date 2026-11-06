// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_WINSERVICE_H
#define GRIDCOIN_UTIL_WINSERVICE_H

#include "fs.h"
#include "util/win_acl.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

//! gridcoinresearchd under the Windows service control manager.
//!
//! What the service reports, how its flag is read, and the checks of its account, its
//! token and its executable's directory are pure, so they build and are tested on every
//! platform. Only RunAsService and the reads of this process's token need Windows.
namespace util::winsvc {

//! A service's state, with the SERVICE_* values of winsvc.h.
enum class SvcState : uint32_t
{
    Stopped = 1,
    StartPending = 2,
    StopPending = 3,
    Running = 4,
};

//! SERVICE_ACCEPT_STOP and SERVICE_ACCEPT_PRESHUTDOWN.
constexpr uint32_t ACCEPT_STOP = 0x1, ACCEPT_PRESHUTDOWN = 0x100;
//! SERVICE_CONTROL_STOP, SERVICE_CONTROL_INTERROGATE and SERVICE_CONTROL_PRESHUTDOWN.
constexpr uint32_t CONTROL_STOP = 0x1, CONTROL_INTERROGATE = 0x4, CONTROL_PRESHUTDOWN = 0xF;
//! ERROR_SERVICE_SPECIFIC_ERROR, and the codes reported with it: the core stopped on its own
//! (the RPC stop, the GUI's stop over IPC), or its start failed. Either makes the service
//! manager run the service's failure actions, which restart it.
constexpr uint32_t ERROR_SERVICE_SPECIFIC = 1066, EXIT_SELF_STOP = 1, EXIT_INIT_FAILED = 2;
//! How long the service manager is told to wait for the next report while the service
//! starts and while it stops, and how often a stop reports its progress.
constexpr uint32_t START_WAIT_HINT_MS = 30000, STOP_WAIT_HINT_MS = 10000, STOP_TICK_MS = 2000;

//! One SERVICE_STATUS report, without the service type.
struct SvcStatus
{
    SvcState state{SvcState::Stopped};
    uint32_t controls_accepted{0};
    uint32_t checkpoint{0};
    uint32_t wait_hint_ms{0};
    uint32_t win32_exit_code{0};
    uint32_t specific_exit_code{0};
};

//! What the service reports to the service manager, from the start to the exit. Not
//! thread-safe: RunAsService calls it under one lock.
class ServiceStatusMachine
{
public:
    //! StartPending, accepting no control, checkpoint 1, START_WAIT_HINT_MS.
    SvcStatus Start();
    //! Running, accepting STOP and PRESHUTDOWN, checkpoint 0, no hint. Reported before the
    //! core's long start, so that a stop during it is honoured. From StartPending only;
    //! otherwise the status is left as it is.
    SvcStatus InitBegun();
    //! The service manager's \p control. STOP and PRESHUTDOWN record that the service
    //! manager asked for the stop, and move Running to StopPending (accepting no control,
    //! checkpoint 1, STOP_WAIT_HINT_MS); in any other state the status is left as it is.
    //! INTERROGATE gives the status as it is. nullopt for any other control, which the
    //! service does not implement.
    std::optional<SvcStatus> Control(uint32_t control);
    //! The core began to stop. Running moves to StopPending as for STOP, and a stop the
    //! service manager did not ask for counts as the core's own. In any other state the
    //! status is left as it is.
    SvcStatus ShutdownObserved();
    //! While StopPending: the next checkpoint, with STOP_WAIT_HINT_MS. In any other state
    //! the status is left as it is.
    SvcStatus Tick();
    //! Stopped. A stop the service manager asked for exits 0; any other exit reports
    //! ERROR_SERVICE_SPECIFIC with EXIT_SELF_STOP when \p app_ok (the core ran and stopped
    //! on its own) or EXIT_INIT_FAILED when not (its start failed).
    SvcStatus Exit(bool app_ok);
    //! Whether the service manager asked for the stop.
    bool ScmRequestedStop() const;

private:
    //! Who asked for the stop.
    enum class StopOrigin : uint8_t
    {
        None,
        Scm,
        Self,
    };

    //! The StopPending status a first stop reports.
    void BeginStop();

    SvcStatus m_status{};
    StopOrigin m_origin{StopOrigin::None};
};

//! Whether the command line \p argv asks for the service mode, read as
//! ArgsManager::ParseParameters reads the -service option on Windows: "-service",
//! "--service" or "/service" in any case, "-service=<v>" with v empty or a non-zero
//! integer, "-noservice=0"; the last of them counts, and nothing after the first argument
//! that is not an option, or after "-", is read. Not "-service=0", "-noservice" or
//! "-servicex". Pure.
bool HasServiceFlag(int argc, const char* const argv[]);

//! Whether this process runs as a Windows service. False unless the service entry point
//! has said so, and always false on other platforms. Safe to call from any thread.
bool IsRunningAsWindowsService();

//! Make IsRunningAsWindowsService() answer \p value, for tests.
void SetRunningAsWindowsServiceForTesting(bool value);

//! What a service that cannot read its configuration file is told: who writes it.
inline constexpr const char* SERVICE_CONF_SETUP =
    "The Windows service's configuration file is written by gridcoin-service.exe install, run as an administrator "
    "(with --reconfigure for a service that is already installed); the service does not write one itself.";

//! The kinds of account a service can run as.
enum class ServiceAccountKind : uint8_t
{
    //! NT SERVICE\<name>: S-1-5-80 and the five sub-authorities hashed from the name.
    VirtualServiceAccount,
    //! S-1-5-18, NT AUTHORITY\SYSTEM.
    LocalSystem,
    //! S-1-5-19, NT AUTHORITY\LOCAL SERVICE.
    LocalService,
    //! S-1-5-20, NT AUTHORITY\NETWORK SERVICE.
    NetworkService,
    //! Any other account, a user account included.
    Other,
};

//! What \p user, a token user SID, is as a service account. Pure.
ServiceAccountKind ClassifyServiceAccount(const util::winacl::Sid& user);

//! Why the service may not run as \p user, or nullopt when \p user is a virtual service
//! account. \p account_name is the name Windows gives \p user, empty when it gives none, and
//! \p service_name the name the service manager started the service under. The text names
//! the account found and the virtual account the service must run as. ASCII apart from the
//! names. Pure.
std::optional<std::string> ServiceAccountRefusal(const util::winacl::Sid& user, const std::string& account_name,
                                                 const std::string& service_name);

//! What DescribeProcessToken reads from this process's token.
struct TokenFacts
{
    //! The token user as "<name> (<SID>)", or the SID alone when it has no name.
    std::string user;
    std::vector<std::string> enabled_privileges;
    //! Privileges the token holds but has not enabled, which the process could enable.
    std::vector<std::string> disabled_privileges;
    //! The number of restricting SIDs: a restricted SID type gives the service's token one.
    //! Each of these three is nullopt when it could not be read, and shown as unknown.
    std::optional<uint32_t> restricting_sids;
    std::optional<bool> write_restricted;
    //! The last sub-authority of the token's mandatory label, a SECURITY_MANDATORY_*_RID.
    std::optional<uint32_t> integrity_rid;
};

//! The name of the integrity level \p rid ("Low", "Medium", "High", "System" and so on), or
//! its value in hex when it is none of those. Pure.
std::string IntegrityLevelName(uint32_t rid);

//! \p facts as one log line. Pure.
std::string DescribeTokenFacts(const TokenFacts& facts);

//! CTRL_LOGOFF_EVENT, which Windows sends to every service whenever any user signs out.
constexpr uint32_t CONSOLE_LOGOFF_EVENT = 5;

//! Whether the console control event \p event stops the node. A service ignores
//! CONSOLE_LOGOFF_EVENT, as a user signing out says nothing about the service. Every other
//! event stops the node, as it does in a console. Pure.
bool ConsoleEventStopsNode(uint32_t event, bool service);

//! The accounts that may change the core's executable and the directories above it: SYSTEM,
//! Administrators and TrustedInstaller, and \p token_user too when the core is not a service
//! (-multiprocess), as the GUI that account runs is started from the same place. Pure.
std::vector<util::winacl::Sid> ImageDirectoryAllowed(bool service, const util::winacl::Sid& token_user);

//! The startup message for the executable \p exe, which \p reason says another account can
//! change: an error naming -allowunsafeimagedir, or with \p allow_unsafe set the warning
//! logged instead. ASCII apart from the path. Pure.
std::string ImageDirectoryMessage(const fs::path& exe, const std::string& reason, bool allow_unsafe);

#ifdef WIN32
//! Runs \p app_main as the service the service manager started this process for
//! (StartServiceCtrlDispatcherW). The service's handler (RegisterServiceCtrlHandlerExW)
//! answers STOP and PRESHUTDOWN by requesting a shutdown. The service reports Start(), then
//! InitBegun() before app_main runs; a watcher reports ShutdownObserved() once a shutdown is
//! requested and Tick() every STOP_TICK_MS until app_main returns, then Exit(result).
//! IsRunningAsWindowsService() is true from before app_main runs.
//!
//! Returns the process exit code: EXIT_SUCCESS when app_main returned true, EXIT_FAILURE
//! otherwise, and EXIT_FAILURE with a message on stderr when this process was not started by
//! the service manager.
int RunAsService(const std::function<bool()>& app_main);

//! The name the service manager started this service under, as UTF-8; empty when the
//! process does not run as a service.
std::string ServiceName();

//! The name Windows gives the account \p sid (LookupAccountSidW), as "DOMAIN\name" in
//! UTF-8, or empty when it gives none.
std::string AccountName(const util::winacl::Sid& sid);

//! This process's token (DescribeTokenFacts): its user, its enabled privileges and those it
//! holds disabled, whether it is write-restricted, and its integrity level. A part that
//! could not be read is reported as such.
std::string DescribeProcessToken();
#endif // WIN32

} // namespace util::winsvc

#endif // GRIDCOIN_UTIL_WINSERVICE_H
