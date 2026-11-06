// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "winsvc/scm.h"

#include <algorithm>

#ifdef WIN32
#include <system_error>

#include <windows.h>
#endif

#ifdef WIN32
static_assert(winsvc::CONF_READ_MASK == static_cast<uint32_t>(FILE_GENERIC_READ));
static_assert(winsvc::SID_TYPE_UNRESTRICTED == static_cast<uint32_t>(SERVICE_SID_TYPE_UNRESTRICTED));
static_assert(winsvc::SID_TYPE_RESTRICTED == static_cast<uint32_t>(SERVICE_SID_TYPE_RESTRICTED));
#endif

namespace winsvc {

namespace {

// The Win32 errors ClassifyServiceOpen reads, checked against the SDK below.
constexpr uint32_t WIN32_SUCCESS = 0, WIN32_ACCESS_DENIED = 5, WIN32_SERVICE_DOES_NOT_EXIST = 1060;
#ifdef WIN32
static_assert(WIN32_SUCCESS == static_cast<uint32_t>(ERROR_SUCCESS));
static_assert(WIN32_ACCESS_DENIED == static_cast<uint32_t>(ERROR_ACCESS_DENIED));
static_assert(WIN32_SERVICE_DOES_NOT_EXIST == static_cast<uint32_t>(ERROR_SERVICE_DOES_NOT_EXIST));
#endif

//! \p arg in double quotes as QuoteArg quotes it, whatever it holds.
std::wstring Quoted(const std::wstring& arg)
{
    std::wstring quoted{L'"'};
    size_t backslashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        // Backslashes are literal unless a double quote follows them: before one inside, each is
        // doubled and one more escapes the quote.
        quoted.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        quoted += c;
    }
    // Before the closing quote, so that it still closes.
    quoted.append(backslashes * 2, L'\\');
    quoted += L'"';

    return quoted;
}

//! \p path as Windows writes it, with backslashes. On Windows that is how it is joined already;
//! elsewhere, where tests build these paths, the platform's separator is replaced.
std::wstring WindowsText(const fs::path& path)
{
    std::wstring text = path.wstring();
    std::replace(text.begin(), text.end(), L'/', L'\\');
    return text;
}

std::optional<Network> ParseNetwork(const std::string& value)
{
    if (value == "main") return Network::Main;
    if (value == "test") return Network::Test;
    return std::nullopt;
}

std::optional<Verb> ParseVerb(const std::string& value)
{
    if (value == "install") return Verb::Install;
    if (value == "uninstall") return Verb::Uninstall;
    if (value == "status") return Verb::Status;
    if (value == "enable") return Verb::Enable;
    if (value == "disable") return Verb::Disable;
    if (value == "help") return Verb::Help;
    return std::nullopt;
}

//! The value of the option \p arg when it is "<name>=<value>", or nullopt.
std::optional<std::string> OptionValue(const std::string& arg, const std::string& name)
{
    const std::string prefix = name + "=";
    if (arg.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
    return arg.substr(prefix.size());
}

} // namespace

std::wstring QuoteArg(const std::wstring& arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;
    return Quoted(arg);
}

std::wstring BuildImagePath(const fs::path& exe, Network net, const fs::path& programdata)
{
    const NetworkNames names = NamesFor(net);

    std::wstring image_path = Quoted(WindowsText(exe)) + L" -service -multiprocess";
    if (names.testnet) image_path += L" -testnet";
    image_path += L" -conf=" + Quoted(WindowsText(ConfPath(programdata, net)));
    image_path += L" -datadir=" + Quoted(WindowsText(DataDir(programdata, net)));
    image_path += L" -ipcdir=" + Quoted(WindowsText(IpcDir(programdata, net)));
    image_path += L" -ipcattachgroup=" + Quoted(names.group);
    // Without a value: the default file in the data directory.
    image_path += L" -autounlockblob";

    return image_path;
}

std::optional<std::wstring> ImagePathExecutable(const std::wstring& image_path)
{
    if (image_path.empty()) return std::nullopt;

    // As CreateProcess reads the program's name: up to the next double quote, with no escapes.
    if (image_path.front() == L'"') {
        const size_t close = image_path.find(L'"', 1);
        if (close == std::wstring::npos) return std::nullopt;
        return image_path.substr(1, close - 1);
    }

    return image_path.substr(0, image_path.find_first_of(L" \t"));
}

std::wstring ServiceDescription(Network net)
{
    const NetworkNames names = NamesFor(net);
    return L"The Gridcoin Research core for " + names.datadir_leaf + L", running as its own account, " +
           names.account + L". Set up, enabled and disabled with gridcoin-service.exe.";
}

std::wstring FirewallRuleName(Network net)
{
    return L"Gridcoin service (" + NamesFor(net).service + L")";
}

std::wstring FirewallStagingRuleName(Network net)
{
    return FirewallRuleName(net) + L" (new)";
}

std::wstring FirewallAddRuleArgs(Network net, const fs::path& exe, const std::wstring& name)
{
    return L"advfirewall firewall add rule name=" + Quoted(name) + L" dir=in action=allow program=" +
           Quoted(WindowsText(exe)) + L" service=" + NamesFor(net).service + L" protocol=TCP profile=domain,private";
}

std::wstring FirewallShowRuleArgs(const std::wstring& name)
{
    return L"advfirewall firewall show rule name=" + Quoted(name);
}

std::wstring FirewallDeleteRuleArgs(const std::wstring& name)
{
    return L"advfirewall firewall delete rule name=" + Quoted(name);
}

std::wstring FirewallRenameRuleArgs(const std::wstring& from, const std::wstring& to)
{
    return L"advfirewall firewall set rule name=" + Quoted(from) + L" new name=" + Quoted(to);
}

std::vector<FirewallStep> FirewallSteps(bool rule_exists, bool staging_exists)
{
    std::vector<FirewallStep> steps;
    if (staging_exists) {
        // A staged rule an earlier install left: deleted while the rule is there, and otherwise
        // made the rule, as it is the one that allows the traffic.
        if (rule_exists) {
            steps.push_back(FirewallStep::DeleteStaging);
        } else {
            steps.push_back(FirewallStep::RenameStagingToRule);
            rule_exists = true;
        }
    }

    if (!rule_exists) {
        steps.push_back(FirewallStep::AddRule);
        return steps;
    }

    // The new rule is added under the staging name before the old one goes.
    steps.insert(steps.end(), {FirewallStep::AddStaging, FirewallStep::DeleteRule, FirewallStep::RenameStagingToRule});
    return steps;
}

ServiceOpen ClassifyServiceOpen(uint32_t win32_error)
{
    switch (win32_error) {
    case WIN32_SUCCESS: return ServiceOpen::Opened;
    case WIN32_SERVICE_DOES_NOT_EXIST: return ServiceOpen::NotInstalled;
    case WIN32_ACCESS_DENIED: return ServiceOpen::Unreadable;
    }
    return ServiceOpen::Failed;
}

std::string ServiceUnreadableText(Network net)
{
    std::string service;
    for (const wchar_t c : NamesFor(net).service) service += static_cast<char>(c);
    return "Service " + service + ": this account cannot read the service's settings, as the service's access "
           "list does not let it; run status from an elevated prompt to see them";
}

std::optional<ToolOptions> ParseToolArgs(const std::vector<std::string>& args, std::string& error)
{
    ToolOptions options;
    std::optional<Verb> verb;
    bool install_only = false;
    // A value an option needs, which may not be empty.
    const auto take = [&error](const std::string& name, const std::string& value, std::optional<std::string>& to) {
        if (value.empty()) {
            error = name + " needs a value";
            return false;
        }
        to = value;
        return true;
    };

    for (const std::string& arg : args) {
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            options.verb = Verb::Help;
            return options;
        }

        if (arg.compare(0, 2, "--") == 0) {
            if (const std::optional<std::string> value = OptionValue(arg, "--network")) {
                const std::optional<Network> net = ParseNetwork(*value);
                if (!net) {
                    error = "--network takes main or test, not '" + *value + "'";
                    return std::nullopt;
                }
                options.net = *net;
            } else if (const std::optional<std::string> sid_type = OptionValue(arg, "--sid-type")) {
                if (*sid_type != "restricted" && *sid_type != "unrestricted") {
                    error = "--sid-type takes restricted or unrestricted, not '" + *sid_type + "'";
                    return std::nullopt;
                }
                options.restricted_sid = *sid_type == "restricted";
                install_only = true;
            } else if (arg == "--reconfigure") {
                options.reconfigure = true;
                install_only = true;
            } else if (const std::optional<std::string> user = OptionValue(arg, "--user")) {
                if (!take("--user", *user, options.user)) return std::nullopt;
            } else if (const std::optional<std::string> from = OptionValue(arg, "--from")) {
                if (!take("--from", *from, options.from)) return std::nullopt;
            } else if (const std::optional<std::string> to = OptionValue(arg, "--to")) {
                if (!take("--to", *to, options.to)) return std::nullopt;
            } else {
                error = "unknown option '" + arg + "'";
                return std::nullopt;
            }
            continue;
        }

        const std::optional<Verb> named = ParseVerb(arg);
        if (!named) {
            error = "unknown command '" + arg + "'";
            return std::nullopt;
        }
        if (*named == Verb::Help) {
            options.verb = Verb::Help;
            return options;
        }
        if (verb) {
            error = "more than one command";
            return std::nullopt;
        }
        verb = named;
    }

    if (!verb) {
        error = "no command given";
        return std::nullopt;
    }
    if (install_only && *verb != Verb::Install) {
        error = "--sid-type and --reconfigure are options of install";
        return std::nullopt;
    }
    if ((options.user || options.from) && *verb != Verb::Enable) {
        error = "--user and --from are options of enable";
        return std::nullopt;
    }
    if (options.to && *verb != Verb::Disable) {
        error = "--to is an option of disable";
        return std::nullopt;
    }
    if (*verb == Verb::Enable && !options.user) {
        error = "enable needs --user=<account>, the account whose wallet is moved into the service";
        return std::nullopt;
    }

    options.verb = *verb;
    return options;
}

std::string ToolUsage()
{
    return "Usage: gridcoin-service.exe <command> [options]\n"
           "\n"
           "Sets up the Gridcoin core as a Windows service that runs under its own account,\n"
           "NT SERVICE\\gridcoinresearchd (testnet: NT SERVICE\\gridcointestnetd). Run it from an\n"
           "elevated (administrator) prompt; status also works without one, showing less.\n"
           "\n"
           "Commands:\n"
           "  install    Create the service, disabled; the attach groups \"Gridcoin GUI Users\" and\n"
           "             \"Gridcoin Testnet GUI Users\"; the directories under %ProgramData%\\Gridcoin\n"
           "             with their access lists; the service's configuration file when there is\n"
           "             none; and the inbound firewall rule for the daemon.\n"
           "  uninstall  Remove the service, its firewall rule and the empty directories. Refused while\n"
           "             the service is enabled or its data directory holds a wallet.\n"
           "  status     Show the service's settings and check every directory and file of the\n"
           "             layout against its access list.\n"
           "  enable     Move an account's wallet into the service and start it. The account's\n"
           "             cores must be stopped. Its data directory is copied, every file checked,\n"
           "             and then removed; the account joins the attach group.\n"
           "  disable    Stop the service and move the wallet back to the account it came from,\n"
           "             owner-only; the account leaves the attach group.\n"
           "  help       Show this text.\n"
           "\n"
           "Options:\n"
           "  --network=main|test                 The network (default: main).\n"
           "  --sid-type=restricted|unrestricted  install: the service's SID type (default:\n"
           "                                      restricted).\n"
           "  --reconfigure                       install: apply these settings to a service that is\n"
           "                                      already installed.\n"
           "  --user=<account>                    enable: the account whose wallet is moved in, by name\n"
           "                                      or SID (required).\n"
           "  --from=<directory>                  enable: its data directory (default: its\n"
           "                                      %APPDATA%\\GridcoinResearch).\n"
           "  --to=<directory>                    disable: where the wallet goes (default: where it\n"
           "                                      came from).\n";
}

#ifdef WIN32
void ConfigureService(void* service, Network net, bool restricted_sid)
{
    const SC_HANDLE handle = static_cast<SC_HANDLE>(service);
    const auto change = [handle](DWORD level, void* info, const char* what) {
        if (!::ChangeServiceConfig2W(handle, level, info)) {
            throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                    std::string("ChangeServiceConfig2W ") + what);
        }
    };

    std::wstring description = ServiceDescription(net);
    SERVICE_DESCRIPTIONW description_info{description.data()};
    change(SERVICE_CONFIG_DESCRIPTION, &description_info, "description");

    SERVICE_SID_INFO sid_info{restricted_sid ? SID_TYPE_RESTRICTED : SID_TYPE_UNRESTRICTED};
    change(SERVICE_CONFIG_SERVICE_SID_INFO, &sid_info, "SID type");

    // A list of strings, each ending in a null, with one more null after the last.
    std::wstring privileges = std::wstring(REQUIRED_PRIVILEGE) + L'\0' + L'\0';
    SERVICE_REQUIRED_PRIVILEGES_INFOW privileges_info{privileges.data()};
    change(SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &privileges_info, "required privileges");

    SERVICE_PRESHUTDOWN_INFO preshutdown{PRESHUTDOWN_TIMEOUT_MS};
    change(SERVICE_CONFIG_PRESHUTDOWN_INFO, &preshutdown, "preshutdown timeout");

    std::array<SC_ACTION, FAILURE_RESTART_DELAYS_MS.size()> actions{};
    for (size_t i = 0; i < actions.size(); ++i) actions[i] = SC_ACTION{SC_ACTION_RESTART, FAILURE_RESTART_DELAYS_MS[i]};
    SERVICE_FAILURE_ACTIONSW failure{FAILURE_RESET_SECONDS, nullptr, nullptr, static_cast<DWORD>(actions.size()),
                                     actions.data()};
    change(SERVICE_CONFIG_FAILURE_ACTIONS, &failure, "failure actions");

    // The core's own stop exits with a service-specific error, which these actions restart too.
    SERVICE_FAILURE_ACTIONS_FLAG failure_flag{TRUE};
    change(SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &failure_flag, "failure actions flag");

    SERVICE_DELAYED_AUTO_START_INFO delayed{TRUE};
    change(SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed, "delayed automatic start");
}
#endif // WIN32

} // namespace winsvc
