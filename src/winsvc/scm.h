// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WINSVC_SCM_H
#define GRIDCOIN_WINSVC_SCM_H

#include "fs.h"
#include "winsvc/layout.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

//! How gridcoin-service.exe registers the service with the service control manager: the command
//! line the service runs (its ImagePath), its settings, its firewall rule, and the tool's own
//! command line.
//!
//! The command lines are pure, so they build and are tested on every platform. Applying the
//! settings needs Windows.
namespace winsvc {

//! \p arg as one argument of a Windows command line, as CommandLineToArgvW reads it back: as it
//! is when it is not empty and holds no space, tab, line feed, vertical tab or double quote;
//! otherwise in double quotes, each double quote inside after a backslash, and the backslashes
//! before a double quote, and before the closing one, doubled. Other characters are kept as
//! they are.
std::wstring QuoteArg(const std::wstring& arg);

//! The command line the service runs, its ImagePath, for the daemon \p exe:
//! "<exe>" -service -multiprocess -conf="<conf>" -datadir="<datadir>" -ipcdir="<ipcdir>"
//! -ipcattachgroup="<group>" -autounlockblob, with -testnet after -multiprocess for Test, the
//! paths from \p programdata (ConfPath, DataDir, IpcDir) and the group from NamesFor. The
//! executable and every value are quoted whatever they hold, and every path is written with
//! backslashes.
std::wstring BuildImagePath(const fs::path& exe, Network net, const fs::path& programdata);

//! The executable at the start of the ImagePath \p image_path: the text inside its opening double
//! quote, or, when it does not start with one, the text up to the first space or tab. nullopt
//! when \p image_path is empty or its opening quote is not closed.
std::optional<std::wstring> ImagePathExecutable(const std::wstring& image_path);

//! The service's description, shown in the Services console.
std::wstring ServiceDescription(Network net);

//! "Gridcoin service (<service>)", the name of the network's inbound firewall rule.
std::wstring FirewallRuleName(Network net);

//! "Gridcoin service (<service>) (new)": the name of the rule install adds while a rule named
//! FirewallRuleName is there, until that one is deleted and this one is renamed to it. So a
//! failed reinstall never leaves the machine without the rule it had.
std::wstring FirewallStagingRuleName(Network net);

//! The arguments of netsh.exe that add the inbound rule \p name for the daemon \p exe:
//! advfirewall firewall add rule name="<name>" dir=in action=allow program="<exe>"
//! service=<service> protocol=TCP profile=domain,private
std::wstring FirewallAddRuleArgs(Network net, const fs::path& exe, const std::wstring& name);

//! The arguments of netsh.exe that show the rules named \p name, which exits 0 only when there is
//! one: advfirewall firewall show rule name="<name>"
std::wstring FirewallShowRuleArgs(const std::wstring& name);

//! The arguments of netsh.exe that delete every rule named \p name.
std::wstring FirewallDeleteRuleArgs(const std::wstring& name);

//! The arguments of netsh.exe that rename the rules named \p from to \p to:
//! advfirewall firewall set rule name="<from>" new name="<to>"
std::wstring FirewallRenameRuleArgs(const std::wstring& from, const std::wstring& to);

//! One netsh.exe step of install's firewall rule.
enum class FirewallStep : uint8_t
{
    //! Add FirewallRuleName.
    AddRule,
    //! Add FirewallStagingRuleName.
    AddStaging,
    //! Delete every rule named FirewallRuleName.
    DeleteRule,
    //! Delete every rule named FirewallStagingRuleName.
    DeleteStaging,
    //! Rename the staging rules to FirewallRuleName.
    RenameStagingToRule,
};

//! The steps install takes for the network's rule, given whether a rule named FirewallRuleName
//! (\p rule_exists) and one named FirewallStagingRuleName (\p staging_exists) are there. Run in
//! order, they end with the one rule, for the daemon install names, under FirewallRuleName. A
//! failed step changes nothing and stops the run, and no step deletes the last rule there: a staging
//! rule is deleted only while the rule is there, renamed to the rule when the rule is not, and the
//! rule is deleted only once a new staging rule is there.
std::vector<FirewallStep> FirewallSteps(bool rule_exists, bool staging_exists);

//! How long the service manager waits for the core to stop at a system shutdown.
constexpr uint32_t PRESHUTDOWN_TIMEOUT_MS = 300000;
//! The failure actions: restart after each of these delays, and count the failures from 0 again
//! after FAILURE_RESET_SECONDS without one.
constexpr std::array<uint32_t, 3> FAILURE_RESTART_DELAYS_MS{10000, 60000, 300000};
constexpr uint32_t FAILURE_RESET_SECONDS = 86400;
//! SERVICE_SID_TYPE_UNRESTRICTED and SERVICE_SID_TYPE_RESTRICTED.
constexpr uint32_t SID_TYPE_UNRESTRICTED = 1, SID_TYPE_RESTRICTED = 3;
//! The one privilege the service's token keeps.
inline constexpr const wchar_t* REQUIRED_PRIVILEGE = L"SeChangeNotifyPrivilege";

//! What gridcoin-service.exe was asked to do.
enum class Verb : uint8_t
{
    Help,
    Install,
    Uninstall,
    Status,
    Enable,
    Disable,
};

struct ToolOptions
{
    Verb verb{Verb::Help};
    Network net{Network::Main};
    //! install: SID_TYPE_RESTRICTED, or SID_TYPE_UNRESTRICTED with --sid-type=unrestricted.
    bool restricted_sid{true};
    //! install: apply the settings to a service that is already installed.
    bool reconfigure{false};
    //! enable: the account whose wallet is moved in (--user, required).
    std::optional<std::string> user;
    //! enable: the data directory the wallet is moved from (--from), instead of the account's
    //! %APPDATA%\GridcoinResearch.
    std::optional<std::string> from;
    //! disable: the data directory the wallet is moved back to (--to), instead of the one it came
    //! from.
    std::optional<std::string> to;
};

//! What opening the network's service for status came to, from OpenServiceW's Win32 error:
//! Opened (0), NotInstalled (ERROR_SERVICE_DOES_NOT_EXIST), Unreadable (ERROR_ACCESS_DENIED: the
//! service's security descriptor does not let this account read its settings, as the default one
//! does not for a user who is not signed in interactively) or Failed (anything else).
enum class ServiceOpen : uint8_t
{
    Opened,
    NotInstalled,
    Unreadable,
    Failed,
};

ServiceOpen ClassifyServiceOpen(uint32_t win32_error);

//! What status says when the network's service is Unreadable. ASCII.
std::string ServiceUnreadableText(Network net);

//! The tool's exit codes: done, refused or failed, and a command line it does not take.
constexpr int EXIT_OK = 0, EXIT_REFUSED = 1, EXIT_USAGE = 2;

//! \p args, the tool's arguments after its own name, as ToolOptions: one command (install,
//! uninstall, status, enable, disable or help), --network=main|test (main when absent), for
//! install alone --sid-type=restricted|unrestricted and --reconfigure, for enable alone
//! --user=<account> (required) and --from=<directory>, and for disable alone --to=<directory>.
//! A value may not be empty. "--help", "-h" and "/?" anywhere are help. nullopt, with \p error
//! saying why, for anything else.
std::optional<ToolOptions> ParseToolArgs(const std::vector<std::string>& args, std::string& error);

//! The tool's usage text, in lines ending in LF.
std::string ToolUsage();

#ifdef WIN32
//! Give the service \p service (an SC_HANDLE opened with SERVICE_CHANGE_CONFIG and SERVICE_START)
//! the rest of its settings (ChangeServiceConfig2W): ServiceDescription(\p net), the SID type
//! (SID_TYPE_RESTRICTED, or SID_TYPE_UNRESTRICTED unless \p restricted_sid), REQUIRED_PRIVILEGE
//! alone, PRESHUTDOWN_TIMEOUT_MS, a restart after each of FAILURE_RESTART_DELAYS_MS with
//! FAILURE_RESET_SECONDS, failure actions on a stop with an error too, and a delayed automatic
//! start once it is set to start automatically. Throws std::system_error naming the setting.
void ConfigureService(void* service, Network net, bool restricted_sid);
#endif

} // namespace winsvc

#endif // GRIDCOIN_WINSVC_SCM_H
