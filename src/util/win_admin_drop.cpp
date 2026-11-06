// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/win_admin_drop.h"

#include "tinyformat.h"

#include <algorithm>
#include <array>
#include <optional>

#ifdef WIN32
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <system_error>
#include <type_traits>
#include <utility>

#include <windows.h>
#include <aclapi.h>
#endif

namespace util::winadmin {
namespace {

using util::winacl::TokenGroup;

// The groups User Account Control makes deny-only, from Microsoft's "How User Account Control
// works" (Windows security documentation, learn.microsoft.com): the groups whose membership makes
// Windows give a user a filtered token at logon, in which they are deny-only. They are named
// there as Built-in Administrators, Power Users, Account Operators, System (Server) Operators,
// Print Operators, Backup Operators, RAS Servers, Pre-Windows 2000 Compatible Access, Network
// Configuration Operators, Cryptographic Operators, Domain Administrators, Domain Controllers,
// Certificate Administrators (Cert Publishers), Schema Administrators, Enterprise
// Administrators, Policy Administrators (Group Policy Creator Owners), Read-only Domain
// Controllers and Enterprise Read-only Domain Controllers. The relative IDs are winnt.h's
// DOMAIN_ALIAS_RID_* and DOMAIN_GROUP_RID_*, checked against the SDK below. S-1-5-114 is
// SECURITY_LOCAL_ACCOUNT_AND_ADMIN_RID in Microsoft's list of well-known SIDs, which the filtered
// token of a local administrator also holds deny-only.
constexpr uint32_t RID_BUILTIN_ADMINS = 544, RID_BUILTIN_POWER_USERS = 547, RID_BUILTIN_ACCOUNT_OPS = 548,
    RID_BUILTIN_SYSTEM_OPS = 549, RID_BUILTIN_PRINT_OPS = 550, RID_BUILTIN_BACKUP_OPS = 551,
    RID_BUILTIN_RAS_SERVERS = 553, RID_BUILTIN_PREW2KCOMPACCESS = 554, RID_BUILTIN_NETWORK_CONFIGURATION_OPS = 556,
    RID_BUILTIN_CRYPTO_OPERATORS = 569;
constexpr uint32_t RID_DOMAIN_ENTERPRISE_READONLY_CONTROLLERS = 498, RID_DOMAIN_ADMINS = 512,
    RID_DOMAIN_CONTROLLERS = 516, RID_DOMAIN_CERT_ADMINS = 517, RID_DOMAIN_SCHEMA_ADMINS = 518,
    RID_DOMAIN_ENTERPRISE_ADMINS = 519, RID_DOMAIN_POLICY_ADMINS = 520, RID_DOMAIN_READONLY_CONTROLLERS = 521;

//! S-1-5-32-<rid>.
constexpr std::array<uint32_t, 10> BUILTIN_RIDS{
    RID_BUILTIN_ADMINS,       RID_BUILTIN_POWER_USERS,      RID_BUILTIN_ACCOUNT_OPS,
    RID_BUILTIN_SYSTEM_OPS,   RID_BUILTIN_PRINT_OPS,        RID_BUILTIN_BACKUP_OPS,
    RID_BUILTIN_RAS_SERVERS,  RID_BUILTIN_PREW2KCOMPACCESS, RID_BUILTIN_NETWORK_CONFIGURATION_OPS,
    RID_BUILTIN_CRYPTO_OPERATORS,
};

//! S-1-5-21-<x>-<y>-<z>-<rid>, of any domain. RAS Servers is a domain local group in a domain
//! ("RAS and IAS Servers"), so its RID counts there as well.
constexpr std::array<uint32_t, 9> DOMAIN_RIDS{
    RID_DOMAIN_ENTERPRISE_READONLY_CONTROLLERS,
    RID_DOMAIN_ADMINS,
    RID_DOMAIN_CONTROLLERS,
    RID_DOMAIN_CERT_ADMINS,
    RID_DOMAIN_SCHEMA_ADMINS,
    RID_DOMAIN_ENTERPRISE_ADMINS,
    RID_DOMAIN_POLICY_ADMINS,
    RID_DOMAIN_READONLY_CONTROLLERS,
    RID_BUILTIN_RAS_SERVERS,
};

//! The first sub-authority of BUILTIN (S-1-5-32), of a domain (S-1-5-21), and S-1-5-114.
constexpr uint32_t BUILTIN_DOMAIN = 32, NT_NON_UNIQUE = 21, LOCAL_ACCOUNT_AND_ADMIN = 114;

//! A domain SID's sub-authorities: 21, three for the domain, and the relative ID.
constexpr size_t DOMAIN_SUB_AUTHORITIES = 5;

//! The sub-authorities of \p sid when it is a well-formed SID of the NT authority (5).
std::optional<std::vector<uint32_t>> NtSubAuthorities(const Sid& sid)
{
    // FormatSid gives nothing for a SID that is not well formed.
    if (util::winacl::FormatSid(sid).empty()) return std::nullopt;

    // The 48-bit authority, big-endian, after the revision and the count.
    constexpr std::array<unsigned char, 6> NT_AUTHORITY{0, 0, 0, 0, 0, 5};
    if (!std::equal(NT_AUTHORITY.begin(), NT_AUTHORITY.end(), sid.begin() + 2)) return std::nullopt;

    std::vector<uint32_t> sub_authorities;
    for (size_t offset = 8; offset + 4 <= sid.size(); offset += 4) {
        sub_authorities.push_back(uint32_t{sid[offset]} | uint32_t{sid[offset + 1]} << 8 |
                                  uint32_t{sid[offset + 2]} << 16 | uint32_t{sid[offset + 3]} << 24);
    }

    return sub_authorities;
}

template <size_t N>
bool Contains(const std::array<uint32_t, N>& rids, uint32_t rid)
{
    return std::find(rids.begin(), rids.end(), rid) != rids.end();
}

} // namespace

bool IsUacFilteredGroup(const Sid& sid)
{
    const std::optional<std::vector<uint32_t>> sub = NtSubAuthorities(sid);
    if (!sub) return false;

    if (sub->size() == 1) return (*sub)[0] == LOCAL_ACCOUNT_AND_ADMIN;
    if (sub->size() == 2) return (*sub)[0] == BUILTIN_DOMAIN && Contains(BUILTIN_RIDS, (*sub)[1]);
    if (sub->size() == DOMAIN_SUB_AUTHORITIES) {
        return (*sub)[0] == NT_NON_UNIQUE && Contains(DOMAIN_RIDS, sub->back());
    }

    return false;
}

std::vector<Sid> SelectDenyOnlyGroups(const std::vector<TokenGroup>& groups)
{
    std::vector<Sid> selected;
    for (const TokenGroup& group : groups) {
        const bool enabled = (group.attributes & util::winacl::GROUP_ENABLED) != 0 &&
                             (group.attributes & util::winacl::GROUP_USE_FOR_DENY_ONLY) == 0;
        if (enabled && IsUacFilteredGroup(group.sid)) selected.push_back(group.sid);
    }

    return selected;
}

bool DropNeeded(bool token_elevated, uint32_t integrity_rid, bool filtered_group_enabled)
{
    return util::winacl::AboveMediumOrElevated(token_elevated, integrity_rid) || filtered_group_enabled;
}

StartAction DecideStart(const StartFacts& facts)
{
    if (facts.service || facts.rpc_client || facts.report_permissions) return StartAction::Run;
    if (!DropNeeded(facts.token_elevated, facts.integrity_rid, facts.filtered_group_enabled)) return StartAction::Run;

    return facts.rights_dropped_marker ? StartAction::Refuse : StartAction::Drop;
}

std::string RefusalMessage(const std::string& reason)
{
    return strprintf("Gridcoin could not give up the administrator rights it was started with: %s. Start Gridcoin "
                     "from a normal (not elevated) session.",
                     reason);
}

#ifdef WIN32
namespace {
//! \p value, a constant of the SDK, as the unsigned value the code compares it with.
constexpr uint32_t Sdk(long value)
{
    return static_cast<uint32_t>(value);
}
} // namespace

static_assert(RID_BUILTIN_ADMINS == Sdk(DOMAIN_ALIAS_RID_ADMINS));
static_assert(RID_BUILTIN_POWER_USERS == Sdk(DOMAIN_ALIAS_RID_POWER_USERS));
static_assert(RID_BUILTIN_ACCOUNT_OPS == Sdk(DOMAIN_ALIAS_RID_ACCOUNT_OPS));
static_assert(RID_BUILTIN_SYSTEM_OPS == Sdk(DOMAIN_ALIAS_RID_SYSTEM_OPS));
static_assert(RID_BUILTIN_PRINT_OPS == Sdk(DOMAIN_ALIAS_RID_PRINT_OPS));
static_assert(RID_BUILTIN_BACKUP_OPS == Sdk(DOMAIN_ALIAS_RID_BACKUP_OPS));
static_assert(RID_BUILTIN_RAS_SERVERS == Sdk(DOMAIN_ALIAS_RID_RAS_SERVERS));
static_assert(RID_BUILTIN_PREW2KCOMPACCESS == Sdk(DOMAIN_ALIAS_RID_PREW2KCOMPACCESS));
static_assert(RID_BUILTIN_NETWORK_CONFIGURATION_OPS == Sdk(DOMAIN_ALIAS_RID_NETWORK_CONFIGURATION_OPS));
static_assert(RID_BUILTIN_CRYPTO_OPERATORS == Sdk(DOMAIN_ALIAS_RID_CRYPTO_OPERATORS));
static_assert(RID_DOMAIN_ENTERPRISE_READONLY_CONTROLLERS ==
              Sdk(DOMAIN_GROUP_RID_ENTERPRISE_READONLY_DOMAIN_CONTROLLERS));
static_assert(RID_DOMAIN_ADMINS == Sdk(DOMAIN_GROUP_RID_ADMINS));
static_assert(RID_DOMAIN_CONTROLLERS == Sdk(DOMAIN_GROUP_RID_CONTROLLERS));
static_assert(RID_DOMAIN_CERT_ADMINS == Sdk(DOMAIN_GROUP_RID_CERT_ADMINS));
static_assert(RID_DOMAIN_SCHEMA_ADMINS == Sdk(DOMAIN_GROUP_RID_SCHEMA_ADMINS));
static_assert(RID_DOMAIN_ENTERPRISE_ADMINS == Sdk(DOMAIN_GROUP_RID_ENTERPRISE_ADMINS));
static_assert(RID_DOMAIN_POLICY_ADMINS == Sdk(DOMAIN_GROUP_RID_POLICY_ADMINS));
static_assert(RID_DOMAIN_READONLY_CONTROLLERS == Sdk(DOMAIN_GROUP_RID_READONLY_CONTROLLERS));
static_assert(BUILTIN_DOMAIN == Sdk(SECURITY_BUILTIN_DOMAIN_RID));
static_assert(NT_NON_UNIQUE == Sdk(SECURITY_NT_NON_UNIQUE));
static_assert(LOCAL_ACCOUNT_AND_ADMIN == Sdk(SECURITY_LOCAL_ACCOUNT_AND_ADMIN_RID));
static_assert(DOMAIN_SUB_AUTHORITIES == 2 + Sdk(SECURITY_NT_NON_UNIQUE_SUB_AUTH_COUNT));

namespace {

using util::winacl::UniqueHandle;

//! A step of the drop that failed, with the Windows error it gave as its code.
class StepError : public std::system_error
{
public:
    StepError(std::string step, DWORD error)
        : std::system_error(static_cast<int>(error), std::system_category(), step), m_step(std::move(step))
    {
    }

    const std::string& step() const { return m_step; }

private:
    std::string m_step;
};

[[noreturn]] void Fail(const std::string& step, DWORD error)
{
    throw StepError(step, error);
}

//! What \p read returns, with a std::system_error it throws, as util::winacl's readers do, named
//! as the step \p step.
template <typename Read>
auto Named(const char* step, Read read)
{
    try {
        return read();
    } catch (const StepError&) {
        throw;
    } catch (const std::system_error& e) {
        throw StepError(step, static_cast<DWORD>(e.code().value()));
    }
}

//! Frees memory from LocalAlloc, or from an API that allocates with it, on every exit.
struct LocalFreeDeleter
{
    void operator()(void* p) const
    {
        if (p != nullptr) ::LocalFree(p);
    }
};

struct AttributeListDeleter
{
    void operator()(LPPROC_THREAD_ATTRIBUTE_LIST list) const noexcept { ::DeleteProcThreadAttributeList(list); }
};

//! Whether GiveUpAdminRights found RIGHTS_DROPPED_ENV set and ran on.
std::atomic<bool> g_started_by_drop{false};

//! The copy's process, once it runs, for the console handler.
std::atomic<HANDLE> g_copy{nullptr};

//! RIGHTS_DROPPED_ENV in UTF-16. It is ASCII.
std::wstring MarkerName()
{
    return std::wstring(RIGHTS_DROPPED_ENV, RIGHTS_DROPPED_ENV + std::strlen(RIGHTS_DROPPED_ENV));
}

//! Whether RIGHTS_DROPPED_ENV is set to "1".
bool MarkerSet()
{
    wchar_t value[4]{};
    const DWORD length = ::GetEnvironmentVariableW(MarkerName().c_str(), value, 4);

    return length == 1 && value[0] == L'1';
}

//! The path of this process's executable (GetModuleFileNameW).
std::wstring ModulePath()
{
    std::wstring module(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = ::GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (length == 0) Fail("GetModuleFileNameW", ::GetLastError());
        if (length < module.size()) {
            module.resize(length);
            return module;
        }
        // Cut short: try again with room for the whole path.
        module.resize(module.size() * 2);
    }
}

//! S-1-16-8192, the Medium mandatory label.
Sid MediumIntegritySid()
{
    return Sid{1, 1, 0, 0, 0, 0, 0, 16, 0x00, 0x20, 0x00, 0x00};
}

//! The console handler of the process that started the copy.
BOOL WINAPI RelayConsoleEvent(DWORD event)
{
    // Ctrl+C and Ctrl+Break reach the copy through the shared console, and the copy decides what
    // they do.
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) return TRUE;
    if (event != CTRL_CLOSE_EVENT && event != CTRL_LOGOFF_EVENT && event != CTRL_SHUTDOWN_EVENT) return FALSE;

    // This process would end when the handler returns, which closes the job and kills the copy
    // while it flushes. Wait for the copy instead; the main thread then exits with its exit code.
    const HANDLE copy = g_copy.load();
    if (copy == nullptr) return FALSE;
    ::WaitForSingleObject(copy, INFINITE);
    ::Sleep(INFINITE);

    return TRUE;
}

} // namespace

UniqueHandle CreateRestrictedProcessToken()
{
    // The new token has the access this handle has: duplicate, query, assign as a primary token,
    // adjust its defaults, and read and set its own DACL.
    HANDLE opened = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(),
                            TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT | READ_CONTROL |
                                WRITE_DAC,
                            &opened)) {
        Fail("OpenProcessToken", ::GetLastError());
    }
    const UniqueHandle token(opened);

    Sid user = Named("GetTokenInformation(TokenUser)", [&token] { return util::winacl::ReadTokenUser(token.get()); });
    std::vector<Sid> deny_only = SelectDenyOnlyGroups(
        Named("GetTokenInformation(TokenGroups)", [&token] { return util::winacl::ReadTokenGroups(token.get()); }));

    // CreateRestrictedToken takes non-const PSIDs and only reads them.
    std::vector<SID_AND_ATTRIBUTES> disable;
    for (Sid& sid : deny_only) {
        disable.push_back({sid.data(), 0});
    }

    HANDLE created = nullptr;
    if (!::CreateRestrictedToken(token.get(), DISABLE_MAX_PRIVILEGE, static_cast<DWORD>(disable.size()),
                                 disable.empty() ? nullptr : disable.data(), 0, nullptr, 0, nullptr, &created)) {
        Fail("CreateRestrictedToken", ::GetLastError());
    }
    UniqueHandle restricted(created);

    Sid medium = MediumIntegritySid();
    TOKEN_MANDATORY_LABEL label{};
    label.Label.Sid = medium.data();
    label.Label.Attributes = SE_GROUP_INTEGRITY;
    if (!::SetTokenInformation(restricted.get(), TokenIntegrityLevel, &label,
                               static_cast<DWORD>(sizeof(label) + medium.size()))) {
        Fail("SetTokenInformation(TokenIntegrityLevel)", ::GetLastError());
    }

    TOKEN_OWNER owner{user.data()};
    if (!::SetTokenInformation(restricted.get(), TokenOwner, &owner, sizeof(owner))) {
        Fail("SetTokenInformation(TokenOwner)", ::GetLastError());
    }

    // The elevated token's default DACL grants Administrators, which is deny-only in the copy, so
    // an object the copy creates without an explicit or inherited list would be shut to it.
    const Sid system = util::winacl::wellknown::System();
    const std::unique_ptr<void, LocalFreeDeleter> default_dacl(
        Named("BuildDacl", [&] { return util::winacl::BuildDacl({{user, GENERIC_ALL}, {system, GENERIC_ALL}}); }));
    TOKEN_DEFAULT_DACL dacl_info{static_cast<PACL>(default_dacl.get())};
    if (!::SetTokenInformation(restricted.get(), TokenDefaultDacl, &dacl_info, sizeof(dacl_info))) {
        Fail("SetTokenInformation(TokenDefaultDacl)", ::GetLastError());
    }

    // The token object's own list is not the default DACL set above: it comes from this process's
    // token, which for an elevated token grants Administrators and SYSTEM. With Administrators
    // deny-only, the copy could not open its own token, as the core does at its start.
    const std::unique_ptr<void, LocalFreeDeleter> token_dacl(Named(
        "BuildDacl", [&] { return util::winacl::BuildDacl({{user, TOKEN_ALL_ACCESS}, {system, TOKEN_ALL_ACCESS}}); }));
    const DWORD rc = ::SetSecurityInfo(restricted.get(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                       static_cast<PACL>(token_dacl.get()), nullptr);
    if (rc != ERROR_SUCCESS) Fail("SetSecurityInfo(token)", rc);

    return restricted;
}

RestrictedChild StartRestrictedSuspended(void* token, const std::wstring& application,
                                         const std::wstring& command_line)
{
    RestrictedChild child;

    // Closing the job ends the copy. The processes the copy starts, such as the editor the GUI
    // opens a file in, are not held in it, so they outlive the copy as they would without the job.
    child.job.reset(::CreateJobObjectW(nullptr, nullptr));
    if (!child.job) Fail("CreateJobObjectW", ::GetLastError());
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    if (!::SetInformationJobObject(child.job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        Fail("SetInformationJobObject", ::GetLastError());
    }

    // This process's startup information, so the copy opens as this process would have: its show
    // state, title and feedback. Not what the C runtime passes a child it starts (lpReserved2),
    // which names handles that are not inherited, nor the desktop, which the copy inherits.
    STARTUPINFOEXW startup{};
    ::GetStartupInfoW(&startup.StartupInfo);
    startup.StartupInfo.cb = sizeof(startup.StartupInfo);
    startup.StartupInfo.lpReserved = nullptr;
    startup.StartupInfo.lpDesktop = nullptr;
    startup.StartupInfo.cbReserved2 = 0;
    startup.StartupInfo.lpReserved2 = nullptr;
    startup.StartupInfo.dwFlags &= ~static_cast<DWORD>(STARTF_USESTDHANDLES);

    // An inheritable copy of each standard handle that is valid, one copy for a handle that serves
    // as two. A value that is not a handle of this process, such as the monitor the shell names
    // in the place of the standard output, does not duplicate and is not passed on. With
    // STARTF_USEHOTKEY the place of the standard input holds the hot key, and the shell started
    // this process without standard handles.
    std::vector<UniqueHandle> copies;
    std::vector<HANDLE> inherited;
    if ((startup.StartupInfo.dwFlags & STARTF_USEHOTKEY) == 0) {
        const std::array<DWORD, 3> which{STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
        std::array<HANDLE, 3> originals{};
        std::array<HANDLE, 3> passed{};
        for (size_t i = 0; i < which.size(); ++i) {
            originals[i] = ::GetStdHandle(which[i]);
            if (originals[i] == nullptr || originals[i] == INVALID_HANDLE_VALUE) continue;

            for (size_t j = 0; j < i; ++j) {
                if (originals[j] == originals[i]) passed[i] = passed[j];
            }
            if (passed[i] != nullptr) continue;

            HANDLE copy = nullptr;
            if (!::DuplicateHandle(::GetCurrentProcess(), originals[i], ::GetCurrentProcess(), &copy, 0, TRUE,
                                   DUPLICATE_SAME_ACCESS)) {
                continue;
            }
            copies.emplace_back(copy);
            inherited.push_back(copy);
            passed[i] = copy;
        }

        if (!inherited.empty()) {
            startup.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
            startup.StartupInfo.hStdInput = passed[0];
            startup.StartupInfo.hStdOutput = passed[1];
            startup.StartupInfo.hStdError = passed[2];
        }
    }

    // Only the handles in the list are inherited. The list points at inherited, which outlives it.
    std::vector<unsigned char> list_buffer;
    std::unique_ptr<std::remove_pointer_t<LPPROC_THREAD_ATTRIBUTE_LIST>, AttributeListDeleter> list_guard;
    DWORD flags = CREATE_SUSPENDED;
    if (!inherited.empty()) {
        SIZE_T list_size = 0;
        // The first call sizes the list; it fails with ERROR_INSUFFICIENT_BUFFER by design.
        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &list_size);
        list_buffer.resize(list_size);
        const auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(list_buffer.data());
        if (!::InitializeProcThreadAttributeList(list, 1, 0, &list_size)) {
            Fail("InitializeProcThreadAttributeList", ::GetLastError());
        }
        list_guard.reset(list);
        if (!::UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(),
                                         inherited.size() * sizeof(HANDLE), nullptr, nullptr)) {
            Fail("UpdateProcThreadAttribute", ::GetLastError());
        }
        startup.lpAttributeList = list;
        startup.StartupInfo.cb = sizeof(startup);
        flags |= EXTENDED_STARTUPINFO_PRESENT;
    }

    // CreateProcessAsUserW may write to the command line it is given. No environment block and no
    // directory: the copy has this process's, and its console.
    std::wstring command = command_line;
    PROCESS_INFORMATION started{};
    if (!::CreateProcessAsUserW(static_cast<HANDLE>(token), application.c_str(), command.data(), nullptr, nullptr,
                                inherited.empty() ? FALSE : TRUE, flags, nullptr, nullptr, &startup.StartupInfo,
                                &started)) {
        Fail("CreateProcessAsUserW", ::GetLastError());
    }
    child.process.reset(started.hProcess);
    child.thread.reset(started.hThread);

    if (!::AssignProcessToJobObject(child.job.get(), child.process.get())) {
        const DWORD error = ::GetLastError();
        ::TerminateProcess(child.process.get(), EXIT_FAILURE);
        Fail("AssignProcessToJobObject", error);
    }

    return child;
}

uint32_t ResumeAndWait(const RestrictedChild& child)
{
    if (::ResumeThread(child.thread.get()) == static_cast<DWORD>(-1)) Fail("ResumeThread", ::GetLastError());
    if (::WaitForSingleObject(child.process.get(), INFINITE) != WAIT_OBJECT_0) {
        Fail("WaitForSingleObject", ::GetLastError());
    }

    DWORD code = 0;
    if (!::GetExitCodeProcess(child.process.get(), &code)) Fail("GetExitCodeProcess", ::GetLastError());

    return code;
}

StartOutcome GiveUpAdminRights(bool service, bool rpc_client, bool report_permissions)
{
    StartOutcome outcome;

    // The service, the RPC client and the permission check keep their token, so it is not read
    // for them.
    if (service || rpc_client || report_permissions) return outcome;

    try {
        StartFacts facts;
        facts.token_elevated =
            Named("GetTokenInformation(TokenElevation)", [] { return util::winacl::CurrentTokenIsElevated(); });
        facts.integrity_rid =
            Named("GetTokenInformation(TokenIntegrityLevel)", [] { return util::winacl::CurrentTokenIntegrityRid(); });
        facts.filtered_group_enabled = !SelectDenyOnlyGroups(Named("GetTokenInformation(TokenGroups)", [] {
                                            return util::winacl::CurrentTokenGroups();
                                        })).empty();
        facts.rights_dropped_marker = MarkerSet();

        switch (DecideStart(facts)) {
        case StartAction::Run:
            // A process this one starts does not inherit the marker.
            if (facts.rights_dropped_marker) {
                if (!::SetEnvironmentVariableW(MarkerName().c_str(), nullptr)) {
                    Fail("SetEnvironmentVariableW", ::GetLastError());
                }
                g_started_by_drop = true;
            }
            return outcome;
        case StartAction::Refuse:
            outcome.kind = StartOutcome::Kind::Refused;
            outcome.refusal = RefusalMessage(strprintf(
                "%s is set, so this process should have been started without them, but its token is still elevated, "
                "above Medium integrity, or has an administrators' group enabled",
                RIGHTS_DROPPED_ENV));
            return outcome;
        case StartAction::Drop:
            break;
        }

        if (!::SetConsoleCtrlHandler(RelayConsoleEvent, TRUE)) Fail("SetConsoleCtrlHandler", ::GetLastError());
        // At logoff and shutdown Windows ends processes from the highest shutdown level down. The
        // lowest level an application may take puts this one after the copy, which keeps the
        // default, so the copy is not killed through the job while it flushes.
        if (!::SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY)) {
            Fail("SetProcessShutdownParameters", ::GetLastError());
        }
        if (!::SetEnvironmentVariableW(MarkerName().c_str(), L"1")) Fail("SetEnvironmentVariableW", ::GetLastError());

        const UniqueHandle token = CreateRestrictedProcessToken();
        const RestrictedChild copy = StartRestrictedSuspended(token.get(), ModulePath(), ::GetCommandLineW());
        g_copy.store(copy.process.get());
        outcome.exit_code = ResumeAndWait(copy);
        outcome.kind = StartOutcome::Kind::Exited;
    } catch (const StepError& e) {
        outcome.kind = StartOutcome::Kind::Refused;
        outcome.refusal = RefusalMessage(strprintf("%s failed with Windows error %d", e.step(), e.code().value()));
    } catch (const std::exception& e) {
        outcome.kind = StartOutcome::Kind::Refused;
        outcome.refusal = RefusalMessage(e.what());
    }

    return outcome;
}

bool StartedByAdminDrop()
{
    return g_started_by_drop;
}
#endif // WIN32

} // namespace util::winadmin
