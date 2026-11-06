// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/winservice.h"

#include "tinyformat.h"
#include "util/strencodings.h"

#include <atomic>

#ifdef WIN32
#include "logging.h"
#include "node/shutdown.h"
#include "sync.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <system_error>
#include <thread>

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

    if (m_origin != StopOrigin::Scm) {
        m_status.win32_exit_code = ERROR_SERVICE_SPECIFIC;
        m_status.specific_exit_code = app_ok ? EXIT_SELF_STOP : EXIT_INIT_FAILED;
    }

    return m_status;
}

bool ServiceStatusMachine::ScmRequestedStop() const
{
    return m_origin == StopOrigin::Scm;
}

bool HasServiceFlag(int argc, const char* const argv[])
{
    bool service = false;

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

        // -noservice is -service=0, and -noservice=0 is -service=1.
        bool negated = false;
        if (key.compare(0, 2, "no") == 0) {
            negated = true;
            key.erase(0, 2);
        }

        if (key != "service") continue;

        const bool value_true = InterpretBool(value);
        service = negated ? !value_true : value_true;
    }

    return service;
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

std::string ImageDirectoryMessage(const fs::path& exe, const std::string& reason, bool allow_unsafe)
{
    if (allow_unsafe) {
        return strprintf("WARNING: -allowunsafeimagedir is set: %s, or a directory above it, can be changed by "
                         "another account (%s); starting anyway",
                         exe.string(), reason);
    }

    return strprintf("This program, %s, or a directory above it, can be changed by another account: %s. A core run "
                     "with -service or -multiprocess must be installed where only administrators can change it, "
                     "such as the default location under Program Files. -allowunsafeimagedir starts it anyway.",
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

//! The service's run: what it reports and when app_main has returned.
class ServiceRuntime
{
public:
    void SetMain(std::function<bool()> app_main) { m_app_main = std::move(app_main); }

    //! The service's entry point, on the thread the dispatcher gives it.
    void Main(DWORD argc, LPWSTR* argv) LOCKS_EXCLUDED(m_mutex)
    {
        const wchar_t* const name = argc > 0 && argv != nullptr && argv[0] != nullptr ? argv[0] : L"";

        const SERVICE_STATUS_HANDLE handle = ::RegisterServiceCtrlHandlerExW(name, &ServiceRuntime::Handler, this);
        if (handle == nullptr) {
            // Nothing can be reported without the handle; the service manager sees the
            // service end without a stopped report.
            m_exit_code = EXIT_FAILURE;
            return;
        }

        try {
            g_service_name = WideToUtf8(name);
        } catch (const std::range_error&) {
            g_service_name.clear();
        }
        g_running_as_service.store(true);

        {
            LOCK(m_mutex);
            m_handle = handle;
            Report(m_machine.Start());
            Report(m_machine.InitBegun());
        }

        std::thread watcher;
        try {
            watcher = std::thread([this] { Watch(); });
        } catch (const std::system_error& e) {
            // The stop is still honoured and the exit reported; only the progress of a stop
            // the core began on its own is not.
            LogPrintf("Service: the stop progress reporter could not be started: %s", e.what());
        }

        bool app_ok = false;
        try {
            app_ok = m_app_main();
        } catch (...) {
            app_ok = false;
        }

        {
            LOCK(m_mutex);
            m_app_done = true;
        }
        m_cv.notify_all();
        if (watcher.joinable()) watcher.join();

        m_exit_code = app_ok ? EXIT_SUCCESS : EXIT_FAILURE;

        LOCK(m_mutex);
        Report(m_machine.Exit(app_ok));
        // The service manager may end the process once it has the stopped report, so nothing
        // follows it.
    }

    int ExitCode() const { return m_exit_code.load(); }

    static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);

private:
    static DWORD WINAPI Handler(DWORD control, DWORD /*event_type*/, LPVOID /*event_data*/, LPVOID context)
    {
        return static_cast<ServiceRuntime*>(context)->OnControl(control);
    }

    DWORD OnControl(DWORD control) LOCKS_EXCLUDED(m_mutex)
    {
        {
            LOCK(m_mutex);
            const std::optional<SvcStatus> status = m_machine.Control(control);
            if (!status) return ERROR_CALL_NOT_IMPLEMENTED;

            Report(*status);

            if (control == CONTROL_STOP || control == CONTROL_PRESHUTDOWN) {
                // Recorded as the service manager's stop above, before the request below, so
                // the watcher never takes it for the core's own.
                LogPrintf("Service: the service manager asked the core to stop (%s)",
                          control == CONTROL_STOP ? "stop" : "system shutdown");
                SetShutdownRequested();
            }
        }
        m_cv.notify_all();

        return NO_ERROR;
    }

    //! Reports the core's own stop once a shutdown is requested, then the stop's progress
    //! every STOP_TICK_MS, until app_main has returned.
    void Watch() LOCKS_EXCLUDED(m_mutex)
    {
        WAIT_LOCK(m_mutex, lock);
        bool stopping = false;
        auto next_tick = std::chrono::steady_clock::now();

        while (!m_app_done) {
            const auto now = std::chrono::steady_clock::now();

            if (!stopping && ShutdownRequested()) {
                Report(m_machine.ShutdownObserved());
                stopping = true;
                next_tick = now + std::chrono::milliseconds{STOP_TICK_MS};
            } else if (stopping && now >= next_tick) {
                Report(m_machine.Tick());
                next_tick = now + std::chrono::milliseconds{STOP_TICK_MS};
            }

            const auto wake = stopping ? next_tick : now + SHUTDOWN_POLL;
            m_cv.wait_until(lock, wake, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return m_app_done; });
        }
    }

    void Report(const SvcStatus& status) EXCLUSIVE_LOCKS_REQUIRED(m_mutex)
    {
        SERVICE_STATUS report{};
        report.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        report.dwCurrentState = static_cast<DWORD>(status.state);
        report.dwControlsAccepted = status.controls_accepted;
        report.dwWin32ExitCode = status.win32_exit_code;
        report.dwServiceSpecificExitCode = status.specific_exit_code;
        report.dwCheckPoint = status.checkpoint;
        report.dwWaitHint = status.wait_hint_ms;

        // A control that came before the handle was stored has nothing to report through.
        if (m_handle == nullptr) return;

        if (!::SetServiceStatus(m_handle, &report)) {
            LogPrintf("Service: SetServiceStatus failed with error %u", static_cast<unsigned>(::GetLastError()));
        }
    }

    std::function<bool()> m_app_main;
    //! Written on the service's thread, read on the dispatcher's once the service has stopped.
    std::atomic<int> m_exit_code{EXIT_FAILURE};

    Mutex m_mutex;
    std::condition_variable m_cv;
    //! Stored by Main before the first report.
    SERVICE_STATUS_HANDLE m_handle GUARDED_BY(m_mutex){nullptr};
    ServiceStatusMachine m_machine GUARDED_BY(m_mutex);
    bool m_app_done GUARDED_BY(m_mutex){false};
};

ServiceRuntime g_runtime;

void WINAPI ServiceRuntime::ServiceMain(DWORD argc, LPWSTR* argv)
{
    g_runtime.Main(argc, argv);
}
} // namespace

int RunAsService(const std::function<bool()>& app_main)
{
    g_runtime.SetMain(app_main);

    // The name is ignored for a service of its own process, and may be empty.
    wchar_t empty_name[] = L"";
    SERVICE_TABLE_ENTRYW table[] = {{empty_name, &ServiceRuntime::ServiceMain}, {nullptr, nullptr}};

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

    return g_runtime.ExitCode();
}

std::string ServiceName()
{
    if (!IsRunningAsWindowsService()) return {};

    return g_service_name;
}

std::string AccountName(const winacl::Sid& sid)
{
    if (winacl::FormatSid(sid).empty()) return {};

    PSID psid = const_cast<unsigned char*>(sid.data());
    DWORD name_size = 0;
    DWORD domain_size = 0;
    SID_NAME_USE use;
    ::LookupAccountSidW(nullptr, psid, nullptr, &name_size, nullptr, &domain_size, &use);
    if (name_size == 0) return {};

    std::wstring name(name_size, L'\0');
    std::wstring domain(domain_size, L'\0');
    if (!::LookupAccountSidW(nullptr, psid, name.data(), &name_size, domain.data(), &domain_size, &use)) return {};
    // On success each size is its length, without the terminating NUL.
    name.resize(name_size);
    domain.resize(domain_size);

    try {
        return domain.empty() ? WideToUtf8(name) : WideToUtf8(domain) + "\\" + WideToUtf8(name);
    } catch (const std::range_error&) {
        return {};
    }
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
        const std::string name = AccountName(user);
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
