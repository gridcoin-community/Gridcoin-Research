// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_WINSERVICE_H
#define GRIDCOIN_UTIL_WINSERVICE_H

#include "fs.h"
#include "sync.h"
#include "util/win_acl.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

//! gridcoinresearchd under the Windows service control manager.
//!
//! What the service reports and when (ServiceRunner, with the service manager given as
//! functions), how its flag is read, and the checks of its account, its token and its
//! executable's directory are platform-neutral, so they build and are tested on every
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
//! (the RPC stop, the GUI's stop over IPC), its start failed, or it was started under an
//! account other than its own virtual one (ServiceAccountRefusal). Each makes the service
//! manager run the service's failure actions, which restart it, and event 7024 shows the code.
constexpr uint32_t ERROR_SERVICE_SPECIFIC = 1066, EXIT_SELF_STOP = 1, EXIT_INIT_FAILED = 2, EXIT_WRONG_ACCOUNT = 3;
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
//! thread-safe: ServiceRunner calls it under its lock.
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
    //! Stopped. A start refused with RefuseStart() exits as it says. Otherwise a stop the
    //! service manager asked for exits 0, and any other exit reports ERROR_SERVICE_SPECIFIC
    //! with EXIT_SELF_STOP when \p app_ok (the core ran and stopped on its own) or
    //! EXIT_INIT_FAILED when not (its start failed).
    SvcStatus Exit(bool app_ok);
    //! The start is refused before the core runs. Exit() then reports ERROR_SERVICE_SPECIFIC
    //! with \p specific_exit_code, or exits 0, and the service stays stopped, when it is 0,
    //! whatever app_ok and whoever asked for a stop.
    void RefuseStart(uint32_t specific_exit_code);
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
    std::optional<uint32_t> m_refused;
};

//! What a service's control handler returns: NO_ERROR and ERROR_CALL_NOT_IMPLEMENTED.
constexpr uint32_t HANDLER_OK = 0, HANDLER_NOT_IMPLEMENTED = 120;

//! The service's run, from its first report to its last, with the service manager and the
//! node's shutdown flag given as functions: RunAsService gives it SetServiceStatus and the
//! node's flag, and tests give it fakes. Run() and OnControl() may be called from different
//! threads, as the service's thread and the dispatcher's call them. Every report is made
//! under one lock, so reports never interleave.
class ServiceRunner
{
public:
    using ReportFunction = std::function<void(const SvcStatus&)>;

    //! \p report sends one status to the service manager. \p shutdown_requested and
    //! \p request_shutdown read and set the node's shutdown flag. The watcher looks for a
    //! shutdown every \p poll and reports a stop's progress every \p tick.
    ServiceRunner(ReportFunction report, std::function<bool()> shutdown_requested,
                  std::function<void()> request_shutdown, std::chrono::milliseconds poll,
                  std::chrono::milliseconds tick);

    //! Reports Start(), then InitBegun(), runs \p app_main with a watcher that reports
    //! ShutdownObserved() once a shutdown is requested and Tick() every tick until app_main
    //! returns, then calls \p before_stopped_report with what app_main returned, and reports
    //! Exit(). Nothing is reported after the stopped report, and OnControl() does nothing from
    //! then on. Returns what app_main returned, false when it threw. Call once.
    bool Run(const std::function<bool()>& app_main,
             const std::function<void(bool)>& before_stopped_report = {}) LOCKS_EXCLUDED(m_mutex);

    //! The service manager's \p control. Before the first report and after the stopped one it
    //! does nothing and returns HANDLER_OK. Otherwise ServiceStatusMachine::Control():
    //! HANDLER_NOT_IMPLEMENTED for a control it does not take; else its status is reported,
    //! for STOP and PRESHUTDOWN a shutdown is requested after it, and HANDLER_OK is returned.
    uint32_t OnControl(uint32_t control) LOCKS_EXCLUDED(m_mutex);

    //! ServiceStatusMachine::RefuseStart() for this run.
    void RefuseStart(uint32_t specific_exit_code) LOCKS_EXCLUDED(m_mutex);

private:
    //! Where the run is: OnControl() acts only while Reporting.
    enum class Phase : uint8_t
    {
        NotStarted,
        Reporting,
        Stopped,
    };

    void Watch() LOCKS_EXCLUDED(m_mutex);

    const ReportFunction m_report;
    const std::function<bool()> m_shutdown_requested;
    const std::function<void()> m_request_shutdown;
    const std::chrono::milliseconds m_poll;
    const std::chrono::milliseconds m_tick;

    Mutex m_mutex;
    std::condition_variable m_cv;
    ServiceStatusMachine m_machine GUARDED_BY(m_mutex);
    Phase m_phase GUARDED_BY(m_mutex){Phase::NotStarted};
    bool m_app_done GUARDED_BY(m_mutex){false};
};

//! Whether the command line \p argv asks for the service mode, read as
//! ArgsManager::ParseParameters reads the -service option on Windows: "-service",
//! "--service" or "/service" in any case, "-service=<v>" with v empty or a non-zero
//! integer, "-noservice=0"; the last of them counts, and nothing after the first argument
//! that is not an option, or after "-", is read. Not "-service=0", "-noservice" or
//! "-servicex". Pure.
bool HasServiceFlag(int argc, const char* const argv[]);

//! Whether the command line \p argv sets the boolean option \p name (written without its dash, in
//! lower case) true, read as HasServiceFlag reads -service: "-<name>", "--<name>" or "/<name>" in
//! any case, "-<name>=<v>" with v empty or a non-zero integer, "-no<name>=0"; the last of them
//! counts, and nothing after the first argument that is not an option, or after "-", is read.
//! HasServiceFlag is CommandLineFlag(argc, argv, "service"). Pure.
bool CommandLineFlag(int argc, const char* const argv[], const std::string& name);

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
//! change: an error that states the rule of the mode (\p service: only the administrators'
//! accounts; -multiprocess: those and this account) and names -allowunsafeimagedir, or with
//! \p allow_unsafe set the warning logged instead. ASCII apart from the path. Pure.
std::string ImageDirectoryMessage(const fs::path& exe, const std::string& reason, bool service, bool allow_unsafe);

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

//! ServiceRunner::RefuseStart() for the service RunAsService runs; nothing otherwise.
void RefuseServiceStart(uint32_t specific_exit_code);

//! The name the service manager started this service under, as UTF-8; empty when the
//! process does not run as a service.
std::string ServiceName();

//! Appends \p text to the log file \p path, creating it if absent, for a message from before
//! the log is open. Nothing is written to a link or another reparse point at the name, nor to
//! a file with a second hard link: what is opened is checked through its handle. False when
//! nothing was written.
bool AppendToLogNoFollow(const fs::path& path, const std::string& text);

//! This process's token (DescribeTokenFacts): its user, its enabled privileges and those it
//! holds disabled, whether it is write-restricted, and its integrity level. A part that
//! could not be read is reported as such.
std::string DescribeProcessToken();
#endif // WIN32

} // namespace util::winsvc

#endif // GRIDCOIN_UTIL_WINSERVICE_H
