// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/winservice.h"

#include "logging.h"
#include "tinyformat.h"
#include "util/strencodings.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <system_error>
#include <thread>

#ifdef WIN32
#include "node/shutdown.h"

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>

#include <windows.h>
#endif

#ifdef WIN32
static_assert(static_cast<DWORD>(util::winsvc::SvcState::Stopped) == SERVICE_STOPPED);
static_assert(static_cast<DWORD>(util::winsvc::SvcState::StartPending) == SERVICE_START_PENDING);
static_assert(static_cast<DWORD>(util::winsvc::SvcState::StopPending) == SERVICE_STOP_PENDING);
static_assert(static_cast<DWORD>(util::winsvc::SvcState::Running) == SERVICE_RUNNING);
static_assert(util::winsvc::ACCEPT_STOP == SERVICE_ACCEPT_STOP);
static_assert(util::winsvc::ACCEPT_PRESHUTDOWN == SERVICE_ACCEPT_PRESHUTDOWN);
static_assert(util::winsvc::CONTROL_STOP == SERVICE_CONTROL_STOP);
static_assert(util::winsvc::CONTROL_INTERROGATE == SERVICE_CONTROL_INTERROGATE);
static_assert(util::winsvc::CONTROL_PRESHUTDOWN == SERVICE_CONTROL_PRESHUTDOWN);
static_assert(util::winsvc::ERROR_SERVICE_SPECIFIC == ERROR_SERVICE_SPECIFIC_ERROR);
static_assert(util::winsvc::CONSOLE_LOGOFF_EVENT == CTRL_LOGOFF_EVENT);
static_assert(util::winsvc::HANDLER_OK == NO_ERROR && util::winsvc::HANDLER_NOT_IMPLEMENTED == ERROR_CALL_NOT_IMPLEMENTED);
static_assert(SECURITY_MANDATORY_UNTRUSTED_RID == 0x0000 && SECURITY_MANDATORY_LOW_RID == 0x1000);
static_assert(SECURITY_MANDATORY_MEDIUM_RID == 0x2000 && SECURITY_MANDATORY_MEDIUM_PLUS_RID == 0x2100);
static_assert(SECURITY_MANDATORY_HIGH_RID == 0x3000 && SECURITY_MANDATORY_SYSTEM_RID == 0x4000);
static_assert(SECURITY_MANDATORY_PROTECTED_PROCESS_RID == 0x5000);
static_assert(SECURITY_SERVICE_ID_BASE_RID == 80 && SECURITY_SERVICE_ID_RID_COUNT == 6);
static_assert(SECURITY_LOCAL_SYSTEM_RID == 18 && SECURITY_LOCAL_SERVICE_RID == 19);
static_assert(SECURITY_NETWORK_SERVICE_RID == 20);
#endif

namespace util::winsvc {
namespace {
namespace winacl = util::winacl;

//! Whether this process runs as a Windows service. Atomic, because it is read from
//! whichever thread needs to know.
std::atomic<bool> g_running_as_service{false};

//! ArgsManager's reading of a boolean value: empty is true, otherwise the integer it parses
//! as, and 0 when it does not parse.
bool InterpretBool(const std::string& value)
{
    if (value.empty()) return true;

    int32_t number = 0;
    if (!ParseInt32(value, &number)) return false;

    return number != 0;
}

//! The sub-authority at \p index of the well-formed \p sid.
uint32_t SubAuthority(const winacl::Sid& sid, size_t index)
{
    const size_t offset = 8 + 4 * index;

    return uint32_t{sid[offset]} | uint32_t{sid[offset + 1]} << 8 | uint32_t{sid[offset + 2]} << 16
        | uint32_t{sid[offset + 3]} << 24;
}

//! How the refusal names the kind of account it found.
const char* AccountKindText(ServiceAccountKind kind)
{
    switch (kind) {
    case ServiceAccountKind::LocalSystem: return "the LocalSystem account";
    case ServiceAccountKind::LocalService: return "the LocalService account";
    case ServiceAccountKind::NetworkService: return "the NetworkService account";
    case ServiceAccountKind::VirtualServiceAccount:
    case ServiceAccountKind::Other: break;
    }

    return "an account that is not a virtual service account";
}

std::string JoinOrNone(const std::vector<std::string>& items)
{
    if (items.empty()) return "none";

    std::string text;
    for (const std::string& item : items) {
        if (!text.empty()) text += ", ";
        text += item;
    }

    return text;
}
} // namespace

SvcStatus ServiceStatusMachine::Start()
{
    m_status = SvcStatus{};
    m_status.state = SvcState::StartPending;
    m_status.checkpoint = 1;
    m_status.wait_hint_ms = START_WAIT_HINT_MS;
    m_origin = StopOrigin::None;

    return m_status;
}

SvcStatus ServiceStatusMachine::InitBegun()
{
    if (m_status.state != SvcState::StartPending) return m_status;

    m_status.state = SvcState::Running;
    m_status.controls_accepted = ACCEPT_STOP | ACCEPT_PRESHUTDOWN;
    m_status.checkpoint = 0;
    m_status.wait_hint_ms = 0;

    return m_status;
}

void ServiceStatusMachine::BeginStop()
{
    m_status.state = SvcState::StopPending;
    m_status.controls_accepted = 0;
    m_status.checkpoint = 1;
    m_status.wait_hint_ms = STOP_WAIT_HINT_MS;
}

std::optional<SvcStatus> ServiceStatusMachine::Control(uint32_t control)
{
    if (control == CONTROL_INTERROGATE) return m_status;
    if (control != CONTROL_STOP && control != CONTROL_PRESHUTDOWN) return std::nullopt;

    // The service manager sends no control to a service that has stopped. A stop it asks for
    // while the core already stops on its own still decides the exit: it wants the service
    // to stay stopped.
    if (m_status.state == SvcState::Stopped) return m_status;

    m_origin = StopOrigin::Scm;
    if (m_status.state == SvcState::Running) BeginStop();

    return m_status;
}

SvcStatus ServiceStatusMachine::ShutdownObserved()
{
    if (m_status.state != SvcState::Running) return m_status;

    if (m_origin == StopOrigin::None) m_origin = StopOrigin::Self;
    BeginStop();

    return m_status;
}

SvcStatus ServiceStatusMachine::Tick()
{
    if (m_status.state != SvcState::StopPending) return m_status;

    ++m_status.checkpoint;
    m_status.wait_hint_ms = STOP_WAIT_HINT_MS;

    return m_status;
}

SvcStatus ServiceStatusMachine::Exit(bool app_ok)
{
    m_status = SvcStatus{};
    m_status.state = SvcState::Stopped;

    if (m_refused) {
        if (*m_refused != 0) {
            m_status.win32_exit_code = ERROR_SERVICE_SPECIFIC;
            m_status.specific_exit_code = *m_refused;
        }
    } else if (m_origin != StopOrigin::Scm) {
        m_status.win32_exit_code = ERROR_SERVICE_SPECIFIC;
        m_status.specific_exit_code = app_ok ? EXIT_SELF_STOP : EXIT_INIT_FAILED;
    }

    return m_status;
}

bool ServiceStatusMachine::ScmRequestedStop() const
{
    return m_origin == StopOrigin::Scm;
}

void ServiceStatusMachine::RefuseStart(uint32_t specific_exit_code)
{
    m_refused = specific_exit_code;
}

ServiceRunner::ServiceRunner(ReportFunction report, std::function<bool()> shutdown_requested,
                             std::function<void()> request_shutdown, std::chrono::milliseconds poll,
                             std::chrono::milliseconds tick)
    : m_report(std::move(report)), m_shutdown_requested(std::move(shutdown_requested)),
      m_request_shutdown(std::move(request_shutdown)), m_poll(poll), m_tick(tick)
{
}

bool ServiceRunner::Run(const std::function<bool()>& app_main, const std::function<void(bool)>& before_stopped_report)
{
    {
        LOCK(m_mutex);
        m_report(m_machine.Start());
        m_report(m_machine.InitBegun());
        m_phase = Phase::Reporting;
    }

    std::thread watcher;
    try {
        watcher = std::thread([this] { Watch(); });
    } catch (const std::system_error& e) {
        // The stop is still honoured and the exit reported; only the progress of a stop the
        // core began on its own is not.
        LogPrintf("Service: the stop progress reporter could not be started: %s", e.what());
    }

    bool app_ok = false;
    try {
        app_ok = app_main();
    } catch (...) {
        app_ok = false;
    }

    {
        LOCK(m_mutex);
        m_app_done = true;
    }
    m_cv.notify_all();
    if (watcher.joinable()) watcher.join();

    // The stopped report, and in the same hold of the lock the end of every other report: the
    // service manager may end the process once it has this one, so a control that is waiting
    // for the lock finds the run stopped and does nothing.
    LOCK(m_mutex);
    if (before_stopped_report) before_stopped_report(app_ok);
    m_report(m_machine.Exit(app_ok));
    m_phase = Phase::Stopped;

    return app_ok;
}

uint32_t ServiceRunner::OnControl(uint32_t control)
{
    bool stop = false;
    {
        LOCK(m_mutex);
        if (m_phase != Phase::Reporting) return HANDLER_OK;

        const std::optional<SvcStatus> status = m_machine.Control(control);
        if (!status) return HANDLER_NOT_IMPLEMENTED;

        m_report(*status);

        if (control == CONTROL_STOP || control == CONTROL_PRESHUTDOWN) {
            // Recorded as the service manager's stop above, before the request below, so the
            // watcher never takes it for the core's own.
            LogPrintf("Service: the service manager asked the core to stop (%s)",
                      control == CONTROL_STOP ? "stop" : "system shutdown");
            m_request_shutdown();
            stop = true;
        }
    }
    if (stop) m_cv.notify_all();

    return HANDLER_OK;
}

void ServiceRunner::RefuseStart(uint32_t specific_exit_code)
{
    LOCK(m_mutex);
    m_machine.RefuseStart(specific_exit_code);
}

void ServiceRunner::Watch()
{
    WAIT_LOCK(m_mutex, lock);
    bool stopping = false;
    auto next_tick = std::chrono::steady_clock::now();

    while (!m_app_done) {
        const auto now = std::chrono::steady_clock::now();

        if (!stopping && m_shutdown_requested()) {
            m_report(m_machine.ShutdownObserved());
            stopping = true;
            next_tick = now + m_tick;
        } else if (stopping && now >= next_tick) {
            m_report(m_machine.Tick());
            next_tick = now + m_tick;
        }

        const auto wake = stopping ? next_tick : now + m_poll;
        m_cv.wait_until(lock, wake, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return m_app_done; });
    }
}

bool HasServiceFlag(int argc, const char* const argv[])
{
    return CommandLineFlag(argc, argv, "service");
}

bool CommandLineFlag(int argc, const char* const argv[], const std::string& name)
{
    bool flag = false;

    for (int i = 1; i < argc && argv[i] != nullptr; ++i) {
        std::string key(argv[i]);

        if (key == "-") break;

        std::string value;
        const size_t is_index = key.find('=');
        if (is_index != std::string::npos) {
            value = key.substr(is_index + 1);
            key.erase(is_index);
        }

        key = ToLower(key);
        if (!key.empty() && key[0] == '/') key[0] = '-';

        // The first argument that is not an option is a command, and the rest are its own.
        if (key.empty() || key[0] != '-') break;

        if (key.size() > 1 && key[1] == '-') key.erase(0, 1);
        key.erase(0, 1);

        // -no<name> is -<name>=0, and -no<name>=0 is -<name>=1.
        bool negated = false;
        if (key.compare(0, 2, "no") == 0) {
            negated = true;
            key.erase(0, 2);
        }

        if (key != name) continue;

        const bool value_true = InterpretBool(value);
        flag = negated ? !value_true : value_true;
    }

    return flag;
}

bool IsRunningAsWindowsService()
{
    return g_running_as_service.load();
}

void SetRunningAsWindowsServiceForTesting(bool value)
{
    g_running_as_service.store(value);
}

ServiceAccountKind ClassifyServiceAccount(const winacl::Sid& user)
{
    // FormatSid writes nothing for a SID that is not well formed.
    if (winacl::FormatSid(user).empty()) return ServiceAccountKind::Other;

    // NT AUTHORITY, the 48-bit authority 5.
    const bool nt_authority = user[2] == 0 && user[3] == 0 && user[4] == 0 && user[5] == 0 && user[6] == 0 && user[7] == 5;
    if (!nt_authority) return ServiceAccountKind::Other;

    const size_t count = user[1];
    const uint32_t first = SubAuthority(user, 0);

    if (count == 1 && first == 18) return ServiceAccountKind::LocalSystem;
    if (count == 1 && first == 19) return ServiceAccountKind::LocalService;
    if (count == 1 && first == 20) return ServiceAccountKind::NetworkService;
    // SECURITY_SERVICE_ID_BASE_RID with SECURITY_SERVICE_ID_RID_COUNT sub-authorities in all.
    if (count == 6 && first == 80) return ServiceAccountKind::VirtualServiceAccount;

    return ServiceAccountKind::Other;
}

std::optional<std::string> ServiceAccountRefusal(const winacl::Sid& user, const std::string& account_name,
                                                 const std::string& service_name)
{
    const ServiceAccountKind kind = ClassifyServiceAccount(user);
    if (kind == ServiceAccountKind::VirtualServiceAccount) return std::nullopt;

    std::string sid = winacl::FormatSid(user);
    if (sid.empty()) sid = "an unreadable SID";
    const std::string shown = account_name.empty() ? sid : account_name + " (" + sid + ")";
    const std::string virtual_account = "NT SERVICE\\" + (service_name.empty() ? "<service name>" : service_name);

    return strprintf("The Gridcoin service is running as %s, %s. It must run as its own virtual account, %s, "
                     "which gridcoin-service.exe install sets up. It does not start as LocalSystem, LocalService, "
                     "NetworkService or any other account.",
                     shown, AccountKindText(kind), virtual_account);
}

std::string IntegrityLevelName(uint32_t rid)
{
    switch (rid) {
    case 0x0000: return "Untrusted";
    case 0x1000: return "Low";
    case 0x2000: return "Medium";
    case 0x2100: return "Medium Plus";
    case 0x3000: return "High";
    case 0x4000: return "System";
    case 0x5000: return "Protected Process";
    }

    return strprintf("0x%04x", rid);
}

std::string DescribeTokenFacts(const TokenFacts& facts)
{
    return strprintf("user %s; enabled privileges: %s; disabled privileges: %s; restricting SIDs: %s; "
                     "write-restricted: %s; integrity: %s",
                     facts.user.empty() ? "unknown" : facts.user, JoinOrNone(facts.enabled_privileges),
                     JoinOrNone(facts.disabled_privileges),
                     facts.restricting_sids ? strprintf("%u", *facts.restricting_sids) : "unknown",
                     !facts.write_restricted ? "unknown" : *facts.write_restricted ? "yes" : "no",
                     facts.integrity_rid ? IntegrityLevelName(*facts.integrity_rid) : "unknown");
}

bool ConsoleEventStopsNode(uint32_t event, bool service)
{
    return !(service && event == CONSOLE_LOGOFF_EVENT);
}

std::vector<winacl::Sid> ImageDirectoryAllowed(bool service, const winacl::Sid& token_user)
{
    std::vector<winacl::Sid> allowed{winacl::wellknown::System(), winacl::wellknown::Administrators(),
                                     winacl::wellknown::TrustedInstaller()};

    // The service's own account never: it would let the service change the program it runs.
    if (!service) allowed.push_back(token_user);

    return allowed;
}

std::string ImageDirectoryMessage(const fs::path& exe, const std::string& reason, bool service, bool allow_unsafe)
{
    if (allow_unsafe) {
        return strprintf("WARNING: -allowunsafeimagedir is set: %s, or a directory above it, can be changed by "
                         "another account (%s); starting anyway",
                         exe.string(), reason);
    }

    if (service) {
        return strprintf("This program, %s, or a directory above it, can be changed by an account other than "
                         "SYSTEM, Administrators and TrustedInstaller: %s. A core run with -service may be changed only "
                         "by those, not even the service's own account: install it where only administrators can "
                         "change it, such as the default location under Program Files. -allowunsafeimagedir starts it "
                         "anyway.",
                         exe.string(), reason);
    }

    return strprintf("This program, %s, or a directory above it, can be changed by an account other than this one, "
                     "SYSTEM, Administrators and TrustedInstaller: %s. A core run with -multiprocess may be changed only "
                     "by those: install it where only administrators and this account can change it, such as the "
                     "default location under Program Files. -allowunsafeimagedir starts it anyway.",
                     exe.string(), reason);
}

#ifdef WIN32
namespace {
//! The name the service manager started this service under. Written once, before
//! g_running_as_service is set, and read only after it is.
std::string g_service_name;

//! TOKEN_WRITE_RESTRICTED in the token's flags (TOKEN_ACCESS_INFORMATION::Flags). The WDK's
//! ntifs.h names it; the SDK does not.
constexpr DWORD TOKEN_FLAG_WRITE_RESTRICTED = 0x8;

//! How often the watcher looks for a shutdown request before the core begins to stop.
constexpr std::chrono::milliseconds SHUTDOWN_POLL{200};

//! The handle the service reports through, stored once the handler is registered and before
//! the first report.
std::atomic<SERVICE_STATUS_HANDLE> g_status_handle{nullptr};

//! What RunAsService runs, and the run, both made before the dispatcher starts and kept for
//! the life of the process, as the handler may be called until it ends.
std::function<bool()> g_app_main;
std::optional<ServiceRunner> g_runner;

//! Written on the service's thread before its stopped report, read on the dispatcher's once the
//! service has stopped.
std::atomic<int> g_exit_code{EXIT_FAILURE};

//! Set when ServiceMain is about to return, so that RunAsService does not return, and the
//! runtime's statics are not destroyed, while the service's thread is still inside the runner.
std::mutex g_main_mutex;
std::condition_variable g_main_cv;
bool g_main_done{false};

//! How long RunAsService waits for ServiceMain to return once the dispatcher has.
constexpr std::chrono::seconds SERVICE_MAIN_WAIT{10};

void MarkServiceMainDone()
{
    {
        std::lock_guard<std::mutex> lock(g_main_mutex);
        g_main_done = true;
    }
    g_main_cv.notify_all();
}

//! One report to the service manager. ServiceRunner makes every report under one lock.
void ReportToServiceManager(const SvcStatus& status)
{
    const SERVICE_STATUS_HANDLE handle = g_status_handle.load();
    if (handle == nullptr) return;

    SERVICE_STATUS report{};
    report.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    report.dwCurrentState = static_cast<DWORD>(status.state);
    report.dwControlsAccepted = status.controls_accepted;
    report.dwWin32ExitCode = status.win32_exit_code;
    report.dwServiceSpecificExitCode = status.specific_exit_code;
    report.dwCheckPoint = status.checkpoint;
    report.dwWaitHint = status.wait_hint_ms;

    if (!::SetServiceStatus(handle, &report)) {
        LogPrintf("Service: SetServiceStatus failed with error %u", static_cast<unsigned>(::GetLastError()));
    }
}

DWORD WINAPI Handler(DWORD control, DWORD /*event_type*/, LPVOID /*event_data*/, LPVOID /*context*/)
{
    return g_runner->OnControl(control);
}

//! The service's entry point, on the thread the dispatcher gives it.
void WINAPI ServiceMain(DWORD argc, LPWSTR* argv)
{
    const wchar_t* const name = argc > 0 && argv != nullptr && argv[0] != nullptr ? argv[0] : L"";

    const SERVICE_STATUS_HANDLE handle = ::RegisterServiceCtrlHandlerExW(name, &Handler, nullptr);
    if (handle == nullptr) {
        // Nothing can be reported without the handle; the service manager sees the service
        // end without a stopped report.
        g_exit_code = EXIT_FAILURE;
        MarkServiceMainDone();
        return;
    }
    g_status_handle.store(handle);

    try {
        g_service_name = WideToUtf8(name);
    } catch (const std::range_error&) {
        g_service_name.clear();
    }
    g_running_as_service.store(true);

    // The exit code is stored before the stopped report: once the service manager has that, the
    // dispatcher may return, and RunAsService reads the code.
    g_runner->Run(g_app_main, [](bool app_ok) { g_exit_code = app_ok ? EXIT_SUCCESS : EXIT_FAILURE; });
    MarkServiceMainDone();
}
} // namespace

int RunAsService(const std::function<bool()>& app_main)
{
    g_app_main = app_main;
    g_runner.emplace(&ReportToServiceManager, [] { return ShutdownRequested(); }, [] { SetShutdownRequested(); },
                     SHUTDOWN_POLL, std::chrono::milliseconds{STOP_TICK_MS});

    // The name is ignored for a service of its own process, and may be empty.
    wchar_t empty_name[] = L"";
    SERVICE_TABLE_ENTRYW table[] = {{empty_name, &ServiceMain}, {nullptr, nullptr}};

    if (!::StartServiceCtrlDispatcherW(table)) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            tfm::format(std::cerr, "Error: -service is given, but this process was not started by the service control "
                                   "manager. The Gridcoin service is set up with gridcoin-service.exe install and "
                                   "started as a service; to run the core in a console, leave -service out.\n");
        } else {
            tfm::format(std::cerr, "Error: -service: StartServiceCtrlDispatcherW failed with error %u\n",
                        static_cast<unsigned>(error));
        }
        return EXIT_FAILURE;
    }

    // The dispatcher returns once the stopped report is processed, which may be before the
    // service's thread has left the runner. Waited for, with a bound, so that what it uses is not
    // destroyed under it as the process ends.
    {
        std::unique_lock<std::mutex> lock(g_main_mutex);
        if (!g_main_cv.wait_for(lock, SERVICE_MAIN_WAIT, [] { return g_main_done; })) {
            LogPrintf("Service: the service's thread had not finished %d s after it stopped",
                      static_cast<int>(SERVICE_MAIN_WAIT.count()));
        }
    }

    return g_exit_code.load();
}

void RefuseServiceStart(uint32_t specific_exit_code)
{
    if (g_runner) g_runner->RefuseStart(specific_exit_code);
}

std::string ServiceName()
{
    if (!IsRunningAsWindowsService()) return {};

    return g_service_name;
}

bool AppendToLogNoFollow(const fs::path& path, const std::string& text)
{
    // FILE_FLAG_OPEN_REPARSE_POINT opens a link as itself, which the check below refuses, so
    // nothing is written where it leads. OPEN_ALWAYS creates the log in a directory that
    // exists; nothing creates the directory.
    const winacl::UniqueHandle file(::CreateFileW(path.wstring().c_str(),
                                                  FILE_APPEND_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!file) return false;

    // Checked on what was opened: a regular file, not a reparse point, with no second name,
    // so that no other file is reached through a hard link.
    BY_HANDLE_FILE_INFORMATION info{};
    if (!::GetFileInformationByHandle(file.get(), &info)) return false;
    if (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) return false;
    if (info.nNumberOfLinks != 1 || ::GetFileType(file.get()) != FILE_TYPE_DISK) return false;

    DWORD written = 0;
    return ::WriteFile(file.get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
           written == text.size();
}

std::string DescribeProcessToken()
{
    HANDLE opened = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &opened)) {
        return strprintf("the token could not be opened (OpenProcessToken failed with error %u)",
                         static_cast<unsigned>(::GetLastError()));
    }
    const winacl::UniqueHandle token(opened);

    TokenFacts facts;
    std::vector<std::string> unread;

    try {
        const winacl::Sid user = winacl::CurrentTokenUser();
        const std::string name = winacl::AccountNameOf(user);
        facts.user = name.empty() ? winacl::FormatSid(user) : name + " (" + winacl::FormatSid(user) + ")";
    } catch (const std::exception& e) {
        unread.push_back(e.what());
    }

    try {
        const std::vector<unsigned char> buffer =
            winacl::QueryTokenInformation(token.get(), TokenPrivileges, "TokenPrivileges");
        const auto* privileges = reinterpret_cast<const TOKEN_PRIVILEGES*>(buffer.data());

        for (DWORD i = 0; i < privileges->PrivilegeCount; ++i) {
            LUID luid = privileges->Privileges[i].Luid;
            wchar_t text[128];
            DWORD length = sizeof(text) / sizeof(text[0]);
            std::string privilege = "an unnamed privilege";
            if (::LookupPrivilegeNameW(nullptr, &luid, text, &length)) {
                // Privilege names are ASCII, such as SeChangeNotifyPrivilege.
                privilege = WideToUtf8(std::wstring_view(text, length));
            }

            if (privileges->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) {
                facts.enabled_privileges.push_back(privilege);
            } else {
                facts.disabled_privileges.push_back(privilege);
            }
        }
    } catch (const std::exception& e) {
        unread.push_back(e.what());
    }

    try {
        const std::vector<unsigned char> buffer =
            winacl::QueryTokenInformation(token.get(), TokenRestrictedSids, "TokenRestrictedSids");
        facts.restricting_sids = reinterpret_cast<const TOKEN_GROUPS*>(buffer.data())->GroupCount;
    } catch (const std::exception& e) {
        unread.push_back(e.what());
    }

    try {
        const std::vector<unsigned char> buffer =
            winacl::QueryTokenInformation(token.get(), TokenAccessInformation, "TokenAccessInformation");
        const DWORD flags = reinterpret_cast<const TOKEN_ACCESS_INFORMATION*>(buffer.data())->Flags;
        facts.write_restricted = (flags & TOKEN_FLAG_WRITE_RESTRICTED) != 0;
    } catch (const std::exception& e) {
        unread.push_back(e.what());
    }

    try {
        facts.integrity_rid = winacl::CurrentTokenIntegrityRid();
    } catch (const std::exception& e) {
        unread.push_back(e.what());
    }

    std::string text = DescribeTokenFacts(facts);
    if (!unread.empty()) text += "; not read: " + JoinOrNone(unread);

    return text;
}
#endif // WIN32

} // namespace util::winsvc
