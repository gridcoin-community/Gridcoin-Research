// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "winsvc/accounts.h"

#ifdef WIN32
#include "tinyformat.h"
#include "util/strencodings.h"
#include "winsvc/layout.h"

#include <cwchar>
#include <memory>
#include <new>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

#include <windows.h>
#include <oleauto.h>
#include <taskschd.h>
#include <wtsapi32.h>

// WTSEnumerateProcessesExW and its level-1 record are documented in the Windows SDK's
// wtsapi32.h and exported by wtsapi32.dll since Windows 7, but mingw-w64's header declares
// neither. The import library has the function.
#ifdef WTS_PROCESS_INFO_EX
using WtsProcessInfoEx = WTS_PROCESS_INFO_EXW;
#else
struct WtsProcessInfoEx
{
    DWORD SessionId;
    DWORD ProcessId;
    LPWSTR pProcessName;
    PSID pUserSid;
    DWORD NumberOfThreads;
    DWORD HandleCount;
    DWORD PagefileUsage;
    DWORD PeakPagefileUsage;
    DWORD WorkingSetSize;
    DWORD PeakWorkingSetSize;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
};
extern "C" BOOL WINAPI WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId,
                                                LPWSTR* ppProcessInfo, DWORD* pCount);
#endif

namespace winsvc {

namespace {

namespace winacl = util::winacl;

std::string Win32Text(DWORD error)
{
    std::string message = std::system_category().message(static_cast<int>(error));
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == '.')) {
        message.pop_back();
    }
    return strprintf("%s (error %u)", message, static_cast<unsigned>(error));
}

[[noreturn]] void ThrowWin32(DWORD error, const std::string& what)
{
    throw std::runtime_error(what + ": " + Win32Text(error));
}

[[noreturn]] void ThrowHresult(HRESULT result, const std::string& what)
{
    throw std::runtime_error(strprintf("%s: HRESULT 0x%08x", what, static_cast<unsigned>(result)));
}

std::string Narrow(const std::wstring& text)
{
    try {
        return WideToUtf8(text);
    } catch (const std::range_error&) {
        return "(a name that is not valid UTF-16)";
    }
}

// The Task Scheduler, through COM.

//! COM for this thread, for as long as it lives. A thread where COM is set up already (in
//! another apartment model) keeps it.
class ComThread
{
public:
    ComThread()
    {
        const HRESULT result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(result)) {
            m_uninitialize = true;
        } else if (result != RPC_E_CHANGED_MODE) {
            ThrowHresult(result, "CoInitializeEx");
        }
        // As Microsoft's Task Scheduler examples set it, so that the service can act for this
        // caller. A process sets this once; a second call is refused, which is fine.
        const HRESULT security = ::CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                                                        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);
        if (FAILED(security) && security != RPC_E_TOO_LATE) {
            if (m_uninitialize) ::CoUninitialize();
            ThrowHresult(security, "CoInitializeSecurity");
        }
    }
    ~ComThread()
    {
        if (m_uninitialize) ::CoUninitialize();
    }
    ComThread(const ComThread&) = delete;
    ComThread& operator=(const ComThread&) = delete;

private:
    bool m_uninitialize{false};
};

//! A COM interface pointer, released when it goes out of scope.
template <typename T>
class ComPtr
{
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* operator->() const { return m_p; }
    //! For a call that hands out an interface: what was held is released first.
    T** Out()
    {
        Reset();
        return &m_p;
    }

private:
    void Reset()
    {
        if (m_p != nullptr) m_p->Release();
        m_p = nullptr;
    }

    T* m_p{nullptr};
};

//! A BSTR, freed when it goes out of scope.
class Bstr
{
public:
    Bstr() = default;
    explicit Bstr(const wchar_t* text) : m_text(::SysAllocString(text))
    {
        if (m_text == nullptr) throw std::bad_alloc();
    }
    ~Bstr() { ::SysFreeString(m_text); }
    Bstr(const Bstr&) = delete;
    Bstr& operator=(const Bstr&) = delete;

    BSTR Get() const { return m_text; }
    BSTR* Out()
    {
        ::SysFreeString(m_text);
        m_text = nullptr;
        return &m_text;
    }
    std::wstring Text() const { return m_text ? std::wstring(m_text, ::SysStringLen(m_text)) : std::wstring(); }

private:
    BSTR m_text{nullptr};
};

bool IsMissing(HRESULT result)
{
    return result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
}

//! \Gridcoin\Core-Start in \p task. False when the folder or the task is not there.
bool OpenCoreStartTask(ComPtr<ITaskService>& service, ComPtr<ITaskFolder>& folder, ComPtr<IRegisteredTask>& task)
{
    HRESULT result = ::CoCreateInstance(__uuidof(TaskScheduler), nullptr, CLSCTX_INPROC_SERVER, __uuidof(ITaskService),
                                        reinterpret_cast<void**>(service.Out()));
    if (FAILED(result)) ThrowHresult(result, "CoCreateInstance TaskScheduler");

    // This computer, as this account.
    VARIANT empty;
    ::VariantInit(&empty);
    result = service->Connect(empty, empty, empty, empty);
    if (FAILED(result)) ThrowHresult(result, "ITaskService::Connect");

    const Bstr folder_name(L"\\Gridcoin");
    result = service->GetFolder(folder_name.Get(), folder.Out());
    if (IsMissing(result)) return false;
    if (FAILED(result)) ThrowHresult(result, "ITaskService::GetFolder \\Gridcoin");

    const Bstr task_name(L"Core-Start");
    result = folder->GetTask(task_name.Get(), task.Out());
    if (IsMissing(result)) return false;
    if (FAILED(result)) ThrowHresult(result, "ITaskFolder::GetTask Core-Start");
    return true;
}

bool TaskEnabled(ComPtr<IRegisteredTask>& task)
{
    VARIANT_BOOL enabled = VARIANT_FALSE;
    const HRESULT result = task->get_Enabled(&enabled);
    if (FAILED(result)) ThrowHresult(result, "IRegisteredTask::get_Enabled");
    return enabled != VARIANT_FALSE;
}

} // namespace

std::optional<UserAccount> FindUserAccount(const std::string& user, std::string& why)
{
    Sid sid;
    if (const std::optional<Sid> parsed = winacl::ParseSidString(user)) {
        sid = *parsed;
    } else {
        std::wstring name;
        try {
            name = Utf8ToWide(user);
        } catch (const std::range_error&) {
            why = "it is not valid UTF-8";
            return std::nullopt;
        }
        try {
            const std::optional<winacl::AccountLookup> account = winacl::LookupAccount(name);
            if (!account) {
                why = "Windows knows no account of that name";
                return std::nullopt;
            }
            sid = account->sid;
        } catch (const std::system_error& e) {
            throw std::runtime_error(std::string("look the account up: ") + e.what());
        }
    }

    // The kind and name of the account the SID is, which is what a SID string is checked by too.
    PSID psid = sid.data();
    DWORD name_size = 0;
    DWORD domain_size = 0;
    SID_NAME_USE use{};
    ::LookupAccountSidW(nullptr, psid, nullptr, &name_size, nullptr, &domain_size, &use);
    DWORD error = ::GetLastError();
    if (error == ERROR_NONE_MAPPED) {
        why = "Windows knows no account with that SID";
        return std::nullopt;
    }
    if (error != ERROR_INSUFFICIENT_BUFFER || name_size == 0) ThrowWin32(error, "LookupAccountSidW");
    std::wstring name(name_size, L'\0');
    std::wstring domain(domain_size, L'\0');
    if (!::LookupAccountSidW(nullptr, psid, name.data(), &name_size, domain.data(), &domain_size, &use)) {
        ThrowWin32(::GetLastError(), "LookupAccountSidW");
    }
    // On success each size is its length, without the terminating NUL.
    name.resize(name_size);
    domain.resize(domain_size);
    if (use != SidTypeUser) {
        why = "it is not a user account";
        return std::nullopt;
    }

    UserAccount account;
    account.sid = std::move(sid);
    account.name = domain.empty() ? Narrow(name) : Narrow(domain) + "\\" + Narrow(name);
    return account;
}

std::optional<fs::path> ProfileDirectory(const Sid& user)
{
    const std::wstring key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\" +
                             Utf8ToWide(winacl::FormatSid(user));
    HKEY raw = nullptr;
    LSTATUS status = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &raw);
    if (status == ERROR_FILE_NOT_FOUND) return std::nullopt;
    if (status != ERROR_SUCCESS) ThrowWin32(static_cast<DWORD>(status), "RegOpenKeyExW HKLM\\" + Narrow(key));
    const std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&::RegCloseKey)> guard(raw, &::RegCloseKey);

    // Read twice at most: the value may change size between the calls.
    std::vector<wchar_t> data;
    DWORD type = 0;
    for (int attempt = 0;; ++attempt) {
        DWORD size = 0;
        status = ::RegQueryValueExW(raw, L"ProfileImagePath", nullptr, &type, nullptr, &size);
        if (status == ERROR_FILE_NOT_FOUND) return std::nullopt;
        if (status != ERROR_SUCCESS) ThrowWin32(static_cast<DWORD>(status), "RegQueryValueExW ProfileImagePath");
        data.assign(size / sizeof(wchar_t) + 1, L'\0');
        size = static_cast<DWORD>((data.size() - 1) * sizeof(wchar_t));
        status = ::RegQueryValueExW(raw, L"ProfileImagePath", nullptr, &type, reinterpret_cast<LPBYTE>(data.data()),
                                    &size);
        if (status == ERROR_MORE_DATA && attempt == 0) continue;
        if (status != ERROR_SUCCESS) ThrowWin32(static_cast<DWORD>(status), "RegQueryValueExW ProfileImagePath");
        data.resize(size / sizeof(wchar_t) + 1, L'\0');
        break;
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        throw std::runtime_error("the profile path of " + winacl::FormatSid(user) + " in the registry is not a string");
    }
    // A value need not end in a null.
    std::wstring value(data.data(), ::wcsnlen(data.data(), data.size()));

    if (type == REG_EXPAND_SZ) {
        const DWORD needed = ::ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (needed == 0) ThrowWin32(::GetLastError(), "ExpandEnvironmentStringsW");
        std::wstring expanded(needed, L'\0');
        const DWORD written = ::ExpandEnvironmentStringsW(value.c_str(), expanded.data(), needed);
        if (written == 0 || written > needed) ThrowWin32(::GetLastError(), "ExpandEnvironmentStringsW");
        // The count includes the terminating null.
        expanded.resize(written - 1);
        value = std::move(expanded);
    }
    if (value.empty()) return std::nullopt;
    return fs::path(value);
}

std::vector<CoreProcess> CoresRunningAs(const Sid& user)
{
    DWORD level = 1;
    LPWSTR raw = nullptr;
    DWORD count = 0;
    if (!::WTSEnumerateProcessesExW(WTS_CURRENT_SERVER_HANDLE, &level, WTS_ANY_SESSION, &raw, &count)) {
        ThrowWin32(::GetLastError(), "WTSEnumerateProcessesExW");
    }
    const auto free_list = [count](LPWSTR list) { ::WTSFreeMemoryExW(WTSTypeProcessInfoLevel1, list, count); };
    const std::unique_ptr<wchar_t, decltype(free_list)> guard(raw, free_list);
    const auto* processes = reinterpret_cast<const WtsProcessInfoEx*>(raw);

    std::vector<CoreProcess> cores;
    for (DWORD i = 0; i < count; ++i) {
        const WtsProcessInfoEx& process = processes[i];
        const std::wstring image = process.pProcessName ? process.pProcessName : L"";
        if (!IsCoreImageName(image)) continue;

        const std::optional<Sid> owner = winacl::SidAt(process.pUserSid);
        if (owner && *owner != user) continue;

        CoreProcess core;
        core.pid = process.ProcessId;
        core.session = process.SessionId;
        core.image = Narrow(image);
        core.account_unknown = !owner;
        cores.push_back(std::move(core));
    }
    return cores;
}

CoreStartTask ReadCoreStartTask()
{
    const ComThread com;
    ComPtr<ITaskService> service;
    ComPtr<ITaskFolder> folder;
    ComPtr<IRegisteredTask> task;
    CoreStartTask result;
    if (!OpenCoreStartTask(service, folder, task)) return result;

    result.exists = true;
    result.enabled = TaskEnabled(task);

    ComPtr<ITaskDefinition> definition;
    HRESULT status = task->get_Definition(definition.Out());
    if (FAILED(status)) ThrowHresult(status, "IRegisteredTask::get_Definition");
    ComPtr<IPrincipal> principal;
    status = definition->get_Principal(principal.Out());
    if (FAILED(status)) ThrowHresult(status, "ITaskDefinition::get_Principal");
    Bstr user_id;
    status = principal->get_UserId(user_id.Out());
    if (FAILED(status)) ThrowHresult(status, "IPrincipal::get_UserId");

    // Its action: one, and one that starts a program.
    ComPtr<IActionCollection> actions;
    status = definition->get_Actions(actions.Out());
    if (FAILED(status)) ThrowHresult(status, "ITaskDefinition::get_Actions");
    long action_count = 0;
    status = actions->get_Count(&action_count);
    if (FAILED(status)) ThrowHresult(status, "IActionCollection::get_Count");
    if (action_count == 1) {
        ComPtr<IAction> action;
        status = actions->get_Item(1, action.Out());
        if (FAILED(status)) ThrowHresult(status, "IActionCollection::get_Item");
        ComPtr<IExecAction> exec;
        if (SUCCEEDED(action->QueryInterface(__uuidof(IExecAction), reinterpret_cast<void**>(exec.Out())))) {
            Bstr arguments;
            status = exec->get_Arguments(arguments.Out());
            if (FAILED(status)) ThrowHresult(status, "IExecAction::get_Arguments");
            result.arguments = Narrow(arguments.Text());
        }
    }

    const std::wstring id = user_id.Text();
    result.principal = Narrow(id);
    // A principal registered by SID, or by name.
    if (const std::optional<Sid> sid = winacl::ParseSidString(result.principal)) {
        result.principal_sid = sid;
    } else if (!id.empty()) {
        try {
            if (const std::optional<winacl::AccountLookup> account = winacl::LookupAccount(id)) {
                result.principal_sid = account->sid;
            }
        } catch (const std::exception&) {
            // A name that does not resolve is no account's, and is reported as the text it is.
        }
    }
    return result;
}

void SetCoreStartTaskEnabled(bool enabled)
{
    const ComThread com;
    ComPtr<ITaskService> service;
    ComPtr<ITaskFolder> folder;
    ComPtr<IRegisteredTask> task;
    if (!OpenCoreStartTask(service, folder, task)) {
        throw std::runtime_error("the task \\Gridcoin\\Core-Start is not there");
    }
    const HRESULT result = task->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
    if (FAILED(result)) ThrowHresult(result, "IRegisteredTask::put_Enabled");
    if (TaskEnabled(task) != enabled) {
        throw std::runtime_error(strprintf("the task \\Gridcoin\\Core-Start is still %s after it was %s",
                                           enabled ? "disabled" : "enabled", enabled ? "enabled" : "disabled"));
    }
}

} // namespace winsvc

#endif // WIN32
