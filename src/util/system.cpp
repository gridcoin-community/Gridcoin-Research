// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/system.h>
#include <util/strencodings.h>
#include <util/check.h>
#include <util/string.h>

#include <logging.h>

#include <chainparamsbase.h>

#include <boost/algorithm/string/replace.hpp>
#include <thread>
#include <typeinfo>
#include <univalue.h>

#ifdef WIN32
#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <codecvt>
#include <io.h> /* for _commit */
#include <shellapi.h>
#include "shlobj.h"
#elif defined(__linux__)
# include <sys/prctl.h>
#endif

#ifdef HAVE_MALLOPT_ARENA_MAX
#include <malloc.h>
#endif

const char * const GRIDCOIN_CONF_FILENAME = "gridcoinresearch.conf";
const char * const GRIDCOIN_SETTINGS_FILENAME = "gridcoinsettings.json";

std::string strMiscWarning;

ArgsManager gArgs;

/**
 * Interpret a string argument as a boolean.
 *
 * The definition of atoi() requires that non-numeric string values like "foo",
 * return 0. This means that if a user unintentionally supplies a non-integer
 * argument here, the return value is always false. This means that -foo=false
 * does what the user probably expects, but -foo=true is well defined but does
 * not do what they probably expected.
 *
 * The return value of atoi() is undefined when given input not representable as
 * an int. On most systems this means string value between "-2147483648" and
 * "2147483647" are well defined (this method will return true). Setting
 * -txindex=2147483648 on most systems, however, is probably undefined.
 *
 * For a more extensive discussion of this topic (and a wide range of opinions
 * on the Right Way to change this code), see PR12713.
 */
static bool InterpretBool(const std::string& strValue)
{
    if (strValue.empty())
        return true;

    // Maintaining the behavior as described above, but replacing the atoi with ParseInt.
    int value = 0;
    if (!ParseInt(strValue, &value)) {
        // Do nothing. The value will remain at zero if not parseable. This is to prevent
        // a warning on [[nodiscard]]
    }

    return (value);
}

static std::string SettingName(const std::string& arg)
{
    return arg.size() > 0 && arg[0] == '-' ? arg.substr(1) : arg;
}

/**
 * Interpret -nofoo as if the user supplied -foo=0.
 *
 * This method also tracks when the -no form was supplied, and if so,
 * checks whether there was a double-negative (-nofoo=0 -> -foo=1).
 *
 * If there was not a double negative, it removes the "no" from the key
 * and returns false.
 *
 * If there was a double negative, it removes "no" from the key, and
 * returns true.
 *
 * If there was no "no", it returns the string value untouched.
 *
 * Where an option was negated can be later checked using the
 * IsArgNegated() method. One use case for this is to have a way to disable
 * options that are not normally boolean (e.g. using -nodebuglogfile to request
 * that debug log output is not sent to any file at all).
 */

static util::SettingsValue InterpretOption(std::string& section, std::string& key, const std::string& value)
{
    // Split section name from key name for keys like "testnet.foo" or "regtest.bar"
    size_t option_index = key.find('.');
    if (option_index != std::string::npos) {
        section = key.substr(0, option_index);
        key.erase(0, option_index + 1);
    }
    if (key.substr(0, 2) == "no") {
        key.erase(0, 2);
        // Double negatives like -nofoo=0 are supported (but discouraged)
        if (!InterpretBool(value)) {
            LogPrintf("Warning: parsed potentially confusing double-negative -%s=%s\n", key, value);
            return true;
        }
        return false;
    }
    return value;
}

/**
 * Check settings value validity according to flags.
 *
 * TODO: Add more meaningful error checks here in the future
 * See "here's how the flags are meant to behave" in
 * https://github.com/bitcoin/bitcoin/pull/16097#issuecomment-514627823
 */
static bool CheckValid(const std::string& key, const util::SettingsValue& val, unsigned int flags, std::string& error)
{
    if (val.isBool() && !(flags & ArgsManager::ALLOW_BOOL)) {
        error = strprintf("Negating of -%s is meaningless and therefore forbidden", key);
        return false;
    }
    return true;
}

namespace {
fs::path StripRedundantLastElementsOfPath(const fs::path& path)
{
    auto result = path;
    while (result.filename().string() == ".") {
        result = result.parent_path();
    }

    assert(fs::equivalent(result, path));
    return result;
}
} // namespace

// Define default constructor and destructor that are not inline, so code instantiating this class doesn't need to
// #include class definitions for all members.
// For example, m_settings has an internal dependency on univalue.
ArgsManager::ArgsManager() {}
ArgsManager::~ArgsManager() {}

const std::set<std::string> ArgsManager::GetUnsuitableSectionOnlyArgs() const
{
    std::set<std::string> unsuitables;

    LOCK(cs_args);

    // if there's no section selected, don't worry
    if (m_network.empty()) return std::set<std::string> {};

    // if it's okay to use the default section for this network, don't worry
    if (m_network == CBaseChainParams::MAIN) return std::set<std::string> {};

    for (const auto& arg : m_network_only_args) {
        if (OnlyHasDefaultSectionSetting(m_settings, m_network, SettingName(arg))) {
            unsuitables.insert(arg);
        }
    }
    return unsuitables;
}

const std::list<SectionInfo> ArgsManager::GetUnrecognizedSections() const
{
    // Section names to be recognized in the config file.
    static const std::set<std::string> available_sections{
        CBaseChainParams::TESTNET,
        CBaseChainParams::MAIN
    };

    LOCK(cs_args);
    std::list<SectionInfo> unrecognized = m_config_sections;
    unrecognized.remove_if([](const SectionInfo& appeared){ return available_sections.find(appeared.m_name) != available_sections.end(); });
    return unrecognized;
}

void ArgsManager::SelectConfigNetwork(const std::string& network)
{
    LOCK(cs_args);
    m_network = network;
}

bool ArgsManager::ParseParameters(int argc, const char* const argv[], std::string& error)
{
    LOCK(cs_args);
    m_settings.command_line_options.clear();

    for (int i = 1; i < argc; i++) {
        std::string key(argv[i]);

#ifdef MAC_OSX
        // At the first time when a user gets the "App downloaded from the
        // internet" warning, and clicks the Open button, macOS passes
        // a unique process serial number (PSN) as -psn_... command-line
        // argument, which we filter out.
        if (key.substr(0, 5) == "-psn_") continue;
#endif

        if (key == "-") break; //bitcoin-tx using stdin
        std::string val;
        size_t is_index = key.find('=');
        if (is_index != std::string::npos) {
            val = key.substr(is_index + 1);
            key.erase(is_index);
        }
#ifdef WIN32
        key = ToLower(key);
        if (key[0] == '/')
            key[0] = '-';
#endif

        if (key[0] != '-') {
            if (!m_accept_any_command && m_command.empty()) {
                // The first non-dash arg is a registered command
                std::optional<unsigned int> flags = GetArgFlags(key);
                if (!flags || !(*flags & ArgsManager::COMMAND)) {
                    error = strprintf("Invalid command '%s'", argv[i]);
                    return false;
                }
            }
            m_command.push_back(key);
            while (++i < argc) {
                // The remaining args are command args
                m_command.push_back(argv[i]);
            }
            break;
        }

        // Transform --foo to -foo
        if (key.length() > 1 && key[1] == '-')
            key.erase(0, 1);

        // Transform -foo to foo
        key.erase(0, 1);
        std::string section;
        util::SettingsValue value = InterpretOption(section, key, val);
        std::optional<unsigned int> flags = GetArgFlags('-' + key);

        // Unknown command line options and command line options with dot
        // characters (which are returned from InterpretOption with nonempty
        // section strings) are not valid.
        if (!flags || !section.empty()) {
            error = strprintf("Invalid parameter %s", argv[i]);
            return false;
        }

        if (!CheckValid(key, value, *flags, error)) return false;

        m_settings.command_line_options[key].push_back(value);
    }

    // we do not allow -includeconf from command line, only -noincludeconf
    if (auto* includes = util::FindKey(m_settings.command_line_options, "includeconf")) {
        const util::SettingsSpan values{*includes};
        // Range may be empty if -noincludeconf was passed
        if (!values.empty()) {
            error = "-includeconf cannot be used from commandline; -includeconf=" + values.begin()->write();
            return false; // pick first value as example
        }
    }
    return true;
}

std::optional<unsigned int> ArgsManager::GetArgFlags(const std::string& name) const
{
    LOCK(cs_args);
    for (const auto& arg_map : m_available_args) {
        const auto search = arg_map.second.find(name);
        if (search != arg_map.second.end()) {
            return search->second.m_flags;
        }
    }
    return std::nullopt;
}

const fs::path& ArgsManager::GetBlocksDirPath()
{
    LOCK(cs_args);
    fs::path& path = m_cached_blocks_path;

    // Cache the path to avoid calling fs::create_directories on every call of
    // this function
    if (!path.empty()) return path;

    if (IsArgSet("-blocksdir")) {
        path = fs::system_complete(GetArg("-blocksdir", ""));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDataDirPath(false);
    }

    path /= BaseParams().DataDir();
    path /= "blocks";
    fs::create_directories(path);
    path = StripRedundantLastElementsOfPath(path);
    return path;
}

const fs::path& ArgsManager::GetDataDirPath(bool net_specific) const
{
    LOCK(cs_args);
    fs::path& path = net_specific ? m_cached_network_datadir_path : m_cached_datadir_path;

    // Cache the path to avoid calling fs::create_directories on every call of
    // this function
    if (!path.empty()) return path;

    std::string datadir = GetArg("-datadir", "");
    if (!datadir.empty()) {
        path = fs::system_complete(datadir);
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDefaultDataDir();
    }
    if (net_specific)
        path /= BaseParams().DataDir();

    if (fs::create_directories(path)) {
        // This is the first run, create wallets subdirectory too
        // Reserved for when we move wallets to a subdir like Bitcoin
        //fs::create_directories(path / "wallets");
    }

    path = StripRedundantLastElementsOfPath(path);
    return path;
}

void ArgsManager::ClearPathCache()
{
    LOCK(cs_args);

    m_cached_datadir_path = fs::path();
    m_cached_network_datadir_path = fs::path();
    m_cached_blocks_path = fs::path();
}

std::optional<const ArgsManager::Command> ArgsManager::GetCommand() const
{
    Command ret;
    LOCK(cs_args);
    auto it = m_command.begin();
    if (it == m_command.end()) {
        // No command was passed
        return std::nullopt;
    }
    if (!m_accept_any_command) {
        // The registered command
        ret.command = *(it++);
    }
    while (it != m_command.end()) {
        // The unregistered command and args (if any)
        ret.args.push_back(*(it++));
    }
    return ret;
}

std::vector<std::string> ArgsManager::GetArgs(const std::string& strArg) const
{
    std::vector<std::string> result;
    for (const util::SettingsValue& value : GetSettingsList(strArg)) {
        result.push_back(value.isFalse() ? "0" : value.isTrue() ? "1" : value.get_str());
    }
    return result;
}

bool ArgsManager::IsArgSet(const std::string& strArg) const
{
    return !GetSetting(strArg).isNull();
}

bool ArgsManager::InitSettings(std::string& error)
{
    if (!GetSettingsPath()) {
        return true; // Do nothing if settings file disabled.
    }

    std::vector<std::string> errors;
    if (!ReadSettingsFile(&errors)) {
        error = strprintf("Failed loading settings file:\n- %s\n", Join(errors, "\n- "));
        return false;
    }
    if (!WriteSettingsFile(&errors)) {
        error = strprintf("Failed saving settings file:\n- %s\n", Join(errors, "\n- "));
        return false;
    }
    return true;
}

bool ArgsManager::GetSettingsPath(fs::path* filepath, bool temp) const
{
    if (IsArgNegated("-settings")) {
        return false;
    }
    if (filepath) {
        std::string settings = GetArg("-settings", GRIDCOIN_SETTINGS_FILENAME);
        *filepath = fsbridge::AbsPathJoin(GetDataDirPath(/* net_specific= */ true), temp ? settings + ".tmp" : settings);
    }
    return true;
}

static void SaveErrors(const std::vector<std::string> errors, std::vector<std::string>* error_out)
{
    for (const auto& error : errors) {
        if (error_out) {
            error_out->emplace_back(error);
        } else {
            LogPrintf("%s\n", error);
        }
    }
}

bool ArgsManager::ReadSettingsFile(std::vector<std::string>* errors)
{
    fs::path path;
    if (!GetSettingsPath(&path, /* temp= */ false)) {
        return true; // Do nothing if settings file disabled.
    }

    LOCK(cs_args);
    m_settings.rw_settings.clear();
    std::vector<std::string> read_errors;
    if (!util::ReadSettings(path, m_settings.rw_settings, read_errors)) {
        SaveErrors(read_errors, errors);
        return false;
    }
    for (const auto& setting : m_settings.rw_settings) {
        std::string section;
        std::string key = setting.first;
        (void)InterpretOption(section, key, /* value */ {}); // Split setting key into section and argname
        if (!GetArgFlags('-' + key)) {
            LogPrintf("Ignoring unknown rw_settings value %s\n", setting.first);
        }
    }
    return true;
}

bool ArgsManager::WriteSettingsFile(std::vector<std::string>* errors) const
{
    fs::path path, path_tmp;
    if (!GetSettingsPath(&path, /* temp= */ false) || !GetSettingsPath(&path_tmp, /* temp= */ true)) {
        throw std::logic_error("Attempt to write settings file when dynamic settings are disabled.");
    }

    LOCK(cs_args);
    std::vector<std::string> write_errors;
    if (!util::WriteSettings(path_tmp, m_settings.rw_settings, write_errors)) {
        SaveErrors(write_errors, errors);
        return false;
    }
    if (!RenameOver(path_tmp, path)) {
        SaveErrors({strprintf("Failed renaming settings file %s to %s\n", path_tmp.string(), path.string())}, errors);
        return false;
    }
    return true;
}

bool ArgsManager::IsArgNegated(const std::string& strArg) const
{
    return GetSetting(strArg).isFalse();
}

std::string ArgsManager::GetArg(const std::string& strArg, const std::string& strDefault) const
{
    const util::SettingsValue value = GetSetting(strArg);
    return value.isNull() ? strDefault : value.isFalse() ? "0" : value.isTrue() ? "1" : value.get_str();
}

int64_t ArgsManager::GetIntArg(const std::string& strArg, int64_t nDefault) const
{
    return GetArg(strArg, nDefault);
}

int64_t ArgsManager::GetArg(const std::string& strArg, int64_t nDefault) const
{
    const util::SettingsValue value = GetSetting(strArg);

    int64_t arg_value = 0;

    if (value.isNull()) {
        arg_value = nDefault;
    } else if (value.isFalse()) {
        arg_value = 0;
    } else if (value.isTrue()) {
        arg_value = 1;
    } else if (value.isNum()) {
        arg_value = value.get_int64();
    } else {
        if (!ParseInt64(value.get_str(), &arg_value)) {
            // Do nothing. If we get here and the string cannot be parsed, it should return zero, just like atoi64.
        }
    }

    return arg_value;
}

bool ArgsManager::GetBoolArg(const std::string& strArg, bool fDefault) const
{
    const util::SettingsValue value = GetSetting(strArg);
    return value.isNull() ? fDefault : value.isBool() ? value.get_bool() : InterpretBool(value.get_str());
}

bool ArgsManager::SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    LOCK(cs_args);
    if (IsArgSet(strArg)) return false;
    ForceSetArg(strArg, strValue);
    return true;
}

bool ArgsManager::SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

void ArgsManager::ForceSetArg(const std::string& strArg, const std::string& strValue)
{
    LOCK(cs_args);
    m_settings.forced_settings[SettingName(strArg)] = strValue;
}

void ArgsManager::AddCommand(const std::string& cmd, const std::string& help, const OptionsCategory& cat)
{
    Assert(cmd.find('=') == std::string::npos);
    Assert(cmd.at(0) != '-');

    LOCK(cs_args);
    m_accept_any_command = false; // latch to false
    std::map<std::string, Arg>& arg_map = m_available_args[cat];
    auto ret = arg_map.emplace(cmd, Arg{"", help, ArgsManager::COMMAND});
    Assert(ret.second); // Fail on duplicate commands
}

void ArgsManager::AddArg(const std::string& name, const std::string& help, unsigned int flags, const OptionsCategory& cat)
{
    Assert((flags & ArgsManager::COMMAND) == 0); // use AddCommand

    // Split arg name from its help param
    size_t eq_index = name.find('=');
    if (eq_index == std::string::npos) {
        eq_index = name.size();
    }
    std::string arg_name = name.substr(0, eq_index);

    LOCK(cs_args);
    std::map<std::string, Arg>& arg_map = m_available_args[cat];
    auto ret = arg_map.emplace(arg_name, Arg{name.substr(eq_index, name.size() - eq_index), help, flags});
    assert(ret.second); // Make sure an insertion actually happened

    if (flags & ArgsManager::NETWORK_ONLY) {
        m_network_only_args.emplace(arg_name);
    }
}

void ArgsManager::AddHiddenArgs(const std::vector<std::string>& names)
{
    for (const std::string& name : names) {
        AddArg(name, "", ArgsManager::ALLOW_ANY, OptionsCategory::HIDDEN);
    }
}

std::string ArgsManager::GetHelpMessage() const
{
    const bool show_debug = GetBoolArg("-help-debug", false);

    std::string usage = "";
    LOCK(cs_args);
    for (const auto& arg_map : m_available_args) {
        switch(arg_map.first) {
        case OptionsCategory::OPTIONS:
            usage += HelpMessageGroup("Options:");
            break;
        case OptionsCategory::CONNECTION:
            usage += HelpMessageGroup("Connection options:");
            break;
        case OptionsCategory::ZMQ:
            usage += HelpMessageGroup("ZeroMQ notification options:");
            break;
        case OptionsCategory::DEBUG_TEST:
            usage += HelpMessageGroup("Debugging/Testing options:");
            break;
        case OptionsCategory::NODE_RELAY:
            usage += HelpMessageGroup("Node relay options:");
            break;
        case OptionsCategory::BLOCK_CREATION:
            usage += HelpMessageGroup("Block creation options:");
            break;
        case OptionsCategory::RPC:
            usage += HelpMessageGroup("RPC server options:");
            break;
        case OptionsCategory::WALLET:
            usage += HelpMessageGroup("Wallet options:");
            break;
        case OptionsCategory::WALLET_DEBUG_TEST:
            if (show_debug) usage += HelpMessageGroup("Wallet debugging/testing options:");
            break;
        case OptionsCategory::CHAINPARAMS:
            usage += HelpMessageGroup("Chain selection options:");
            break;
        case OptionsCategory::GUI:
            usage += HelpMessageGroup("UI Options:");
            break;
        case OptionsCategory::COMMANDS:
            usage += HelpMessageGroup("Commands:");
            break;
        case OptionsCategory::REGISTER_COMMANDS:
            usage += HelpMessageGroup("Register Commands:");
            break;
        case OptionsCategory::STAKING:
            usage += HelpMessageGroup("Staking options");
            break;
        case OptionsCategory::SCRAPER:
            usage += HelpMessageGroup("Scraper options:");
            break;
        case OptionsCategory::RESEARCHER:
            usage += HelpMessageGroup("Researcher options:");
            break;
        default:
            break;
        }

        // When we get to the hidden options, stop
        if (arg_map.first == OptionsCategory::HIDDEN) break;

        for (const auto& arg : arg_map.second) {
            if (show_debug || !(arg.second.m_flags & ArgsManager::DEBUG_ONLY)) {
                std::string name;
                if (arg.second.m_help_param.empty()) {
                    name = arg.first;
                } else {
                    name = arg.first + arg.second.m_help_param;
                }
                usage += HelpMessageOpt(name, arg.second.m_help_text);
            }
        }
    }
    return usage;
}

bool HelpRequested(const ArgsManager& args)
{
    return args.IsArgSet("-?") || args.IsArgSet("-h") || args.IsArgSet("-help") || args.IsArgSet("-help-debug");
}

void SetupHelpOptions(ArgsManager& args)
{
    args.AddArg("-?", "Print this help message and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    args.AddHiddenArgs({"-h", "-help"});
}

static const int screenWidth = 79;
static const int optIndent = 2;
static const int msgIndent = 7;

std::string HelpMessageGroup(const std::string &message) {
    return std::string(message) + std::string("\n\n");
}

std::string HelpMessageOpt(const std::string &option, const std::string &message) {
    return std::string(optIndent,' ') + std::string(option) +
           std::string("\n") + std::string(msgIndent,' ') +
           FormatParagraph(message, screenWidth - msgIndent, msgIndent) +
           std::string("\n\n");
}

static std::string FormatException(std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "gridcoin";
#endif
    if (pex)
        return strprintf(
            "EXCEPTION: %s       \n%s       \n%s in %s\n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        return strprintf(
            "UNKNOWN EXCEPTION       \n%s in %s\n", pszModule, pszThread);
}

void LogException(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("%s", message);
}

void PrintException(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s", message);
    tfm::format(std::cerr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
    throw;
}

void PrintExceptionContinue(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s", message);
    tfm::format(std::cerr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
}

fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific)
{
    if (path.is_absolute()) {
        return path;
    }

    fs::path data_dir = GetDataDir(net_specific);

    if (data_dir.empty()) {
        return fs::path {};
    } else {
    return fsbridge::AbsPathJoin(data_dir, path);
    }
}

fs::path GetDefaultDataDir()
{
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\GridcoinResearch
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\GridcoinResearch
    // Mac: ~/Library/Application Support/GridcoinResearch
    // Linux/Unix: ~/.GridcoinResearch
#ifdef WIN32
    // Windows

    // This is the user's base roaming AppData path with GridcoinResearch added.
    return GetSpecialFolderPath(CSIDL_APPDATA) / "GridcoinResearch";
#else
    fs::path pathRet;

    // For everything except for Windows, use the environment variable to get
    // the home path.
    char* pszHome = getenv("HOME");

    // There is no home path, so default to the root directory.
    if (pszHome == nullptr || strlen(pszHome) == 0) {
        pathRet = fs::path("/");
    } else {
        pathRet = fs::path(pszHome);
    }
#ifdef MAC_OSX
    // The pathRet here represents the HOME directory. Apple
    // applications are expected to store their files in
    // "~/Library/Application Support/[AppDir].
    return pathRet / "Library" / "Application Support" / "GridcoinResearch";
#else
    // Linux/Unix
    return pathRet / ".GridcoinResearch";
#endif // MAC_OSX
#endif // WIN32
}

const fs::path &GetDataDir(bool fNetSpecific)
{
    return gArgs.GetDataDirPath(fNetSpecific);
}

bool CheckDataDirOption()
{
    std::string datadir = gArgs.GetArg("-datadir", "");
    return datadir.empty() || fs::is_directory(fs::system_complete(datadir));
}

fs::path GetConfigFile(const std::string& confPath)
{
    // Unlike in Bitcoin, the net specific flag is TRUE, because we still use split config files.
    return AbsPathForConfigVal(fs::path(confPath), true);
}

fs::path GetConfigFile()
{
    return AbsPathForConfigVal(gArgs.GetArg("-conf", GRIDCOIN_CONF_FILENAME), true);
}

bool IsConfigFileEmpty()
{
    fsbridge::ifstream streamConfig(GetConfigFile());
    if (!streamConfig.good())
    {
        return true;
    }
    return false;

}

static bool GetConfigOptions(std::istream& stream, const std::string& filepath, std::string& error,
                             std::vector<std::pair<std::string, std::string>>& options, std::list<SectionInfo>& sections)
{
    std::string str, prefix;
    std::string::size_type pos;
    int linenr = 1;
    while (std::getline(stream, str)) {
        bool used_hash = false;
        if ((pos = str.find('#')) != std::string::npos) {
            str = str.substr(0, pos);
            used_hash = true;
        }
        const static std::string pattern = " \t\r\n";
        str = TrimString(str, pattern);
        if (!str.empty()) {
            if (*str.begin() == '[' && *str.rbegin() == ']') {
                const std::string section = str.substr(1, str.size() - 2);
                sections.emplace_back(SectionInfo{section, filepath, linenr});
                prefix = section + '.';
            } else if (*str.begin() == '-') {
                error = strprintf("parse error on line %i: %s, options in configuration file must be specified without leading -", linenr, str);
                return false;
            } else if ((pos = str.find('=')) != std::string::npos) {
                std::string name = prefix + TrimString(str.substr(0, pos), pattern);
                std::string value = TrimString(str.substr(pos + 1), pattern);
                if (used_hash && name.find("rpcpassword") != std::string::npos) {
                    error = strprintf("parse error on line %i, using # in rpcpassword can be ambiguous and should be avoided", linenr);
                    return false;
                }
                options.emplace_back(name, value);
                if ((pos = name.rfind('.')) != std::string::npos && prefix.length() <= pos) {
                    sections.emplace_back(SectionInfo{name.substr(0, pos), filepath, linenr});
                }
            } else {
                error = strprintf("parse error on line %i: %s", linenr, str);
                if (str.size() >= 2 && str.substr(0, 2) == "no") {
                    error += strprintf(", if you intended to specify a negated option, use %s=1 instead", str);
                }
                return false;
            }
        }
        ++linenr;
    }
    return true;
}

bool ArgsManager::ReadConfigStream(std::istream& stream, const std::string& filepath, std::string& error, bool ignore_invalid_keys)
{
    LOCK(cs_args);
    std::vector<std::pair<std::string, std::string>> options;
    if (!GetConfigOptions(stream, filepath, error, options, m_config_sections)) {
        return false;
    }
    for (const std::pair<std::string, std::string>& option : options) {
        std::string section;
        std::string key = option.first;
        util::SettingsValue value = InterpretOption(section, key, option.second);
        std::optional<unsigned int> flags = GetArgFlags('-' + key);
        if (flags) {
            if (!CheckValid(key, value, *flags, error)) {
                return false;
            }
            m_settings.ro_config[section][key].push_back(value);
        } else {
            if (ignore_invalid_keys) {
                LogPrintf("Ignoring unknown configuration value %s\n", option.first);
            } else {
                error = strprintf("Invalid configuration value %s", option.first);
                return false;
            }
        }
    }
    return true;
}

bool ArgsManager::ReadConfigFiles(std::string& error, bool ignore_invalid_keys)
{
    {
        LOCK(cs_args);
        m_settings.ro_config.clear();
        m_config_sections.clear();
    }

    const std::string confPath = GetArg("-conf", GRIDCOIN_CONF_FILENAME);

    fs::path config_file_spec = GetConfigFile(confPath);
    if (config_file_spec.empty()) return false;

    fsbridge::ifstream stream(config_file_spec);

    // not ok to have a config file specified that cannot be opened
    if (IsArgSet("-conf") && !stream.good()) {
        error = strprintf("specified config file \"%s\" could not be opened.", confPath);
        return false;
    }

    // ok to not have a config file
    if (stream.good()) {
        if (!ReadConfigStream(stream, confPath, error, ignore_invalid_keys)) {
            return false;
        }
        // `-includeconf` cannot be included in the command line arguments except
        // as `-noincludeconf` (which indicates that no included conf file should be used).
        bool use_conf_file{true};
        {
            LOCK(cs_args);
            if (auto* includes = util::FindKey(m_settings.command_line_options, "includeconf")) {
                // ParseParameters() fails if a non-negated -includeconf is passed on the command-line
                assert(util::SettingsSpan(*includes).last_negated());
                use_conf_file = false;
            }
        }
        if (use_conf_file) {
            std::string chain_id = GetChainName();
            std::vector<std::string> conf_file_names;

            auto add_includes = [&](const std::string& network, size_t skip = 0) {
                size_t num_values = 0;
                LOCK(cs_args);
                if (auto* section = util::FindKey(m_settings.ro_config, network)) {
                    if (auto* values = util::FindKey(*section, "includeconf")) {
                        for (size_t i = std::max(skip, util::SettingsSpan(*values).negated()); i < values->size(); ++i) {
                            conf_file_names.push_back((*values)[i].get_str());
                        }
                        num_values = values->size();
                    }
                }
                return num_values;
            };

            // We haven't set m_network yet (that happens in SelectParams()), so manually check
            // for network.includeconf args.
            const size_t chain_includes = add_includes(chain_id);
            const size_t default_includes = add_includes({});

            for (const std::string& conf_file_name : conf_file_names) {
                fsbridge::ifstream conf_file_stream(GetConfigFile(conf_file_name));
                if (conf_file_stream.good()) {
                    if (!ReadConfigStream(conf_file_stream, conf_file_name, error, ignore_invalid_keys)) {
                        return false;
                    }
                    LogPrintf("Included configuration file %s\n", conf_file_name);
                } else {
                    error = "Failed to include configuration file " + conf_file_name;
                    return false;
                }
            }

            // Warn about recursive -includeconf
            conf_file_names.clear();
            add_includes(chain_id, /* skip= */ chain_includes);
            add_includes({}, /* skip= */ default_includes);
            std::string chain_id_final = GetChainName();
            if (chain_id_final != chain_id) {
                // Also warn about recursive includeconf for the chain that was specified in one of the includeconfs
                add_includes(chain_id_final);
            }
            for (const std::string& conf_file_name : conf_file_names) {
                tfm::format(std::cerr, "warning: -includeconf cannot be used from included files; ignoring -includeconf=%s\n", conf_file_name);
            }
        }
    }

    // If datadir is changed in .conf file:
    gArgs.ClearPathCache();
    if (!CheckDataDirOption()) {
        error = strprintf("specified data directory \"%s\" does not exist.", GetArg("-datadir", ""));
        return false;
    }
    return true;
}

std::string ArgsManager::GetChainName() const
{
    auto get_net = [&](const std::string& arg) {
        LOCK(cs_args);
        util::SettingsValue value = util::GetSetting(m_settings, /* section= */ "", SettingName(arg),
            /* ignore_default_section_config= */ false,
            /* get_chain_name= */ true);
        return value.isNull() ? false : value.isBool() ? value.get_bool() : InterpretBool(value.get_str());
    };

    const bool fRegTest = get_net("-regtest");
    const bool fSigNet  = get_net("-signet");
    const bool fTestNet = get_net("-testnet");
    const bool is_chain_arg_set = IsArgSet("-chain");

    if ((int)is_chain_arg_set + (int)fRegTest + (int)fSigNet + (int)fTestNet > 1) {
        throw std::runtime_error("Invalid combination of -regtest, -signet, -testnet and -chain. Can use at most one.");
    }
    if (fTestNet) return CBaseChainParams::TESTNET;

    return GetArg("-chain", CBaseChainParams::MAIN);
}

bool ArgsManager::UseDefaultSection(const std::string& arg) const
{
    return m_network == CBaseChainParams::MAIN || m_network_only_args.count(arg) == 0;
}

util::SettingsValue ArgsManager::GetSetting(const std::string& arg) const
{
    LOCK(cs_args);
    return util::GetSetting(
        m_settings, m_network, SettingName(arg), !UseDefaultSection(arg), /* get_chain_name= */ false);
}

std::vector<util::SettingsValue> ArgsManager::GetSettingsList(const std::string& arg) const
{
    LOCK(cs_args);
    return util::GetSettingsList(m_settings, m_network, SettingName(arg), !UseDefaultSection(arg));
}

void ArgsManager::logArgsPrefix(
    const std::string& prefix,
    const std::string& section,
    const std::map<std::string, std::vector<util::SettingsValue>>& args) const
{
    std::string section_str = section.empty() ? "" : "[" + section + "] ";
    for (const auto& arg : args) {
        for (const auto& value : arg.second) {
            std::optional<unsigned int> flags = GetArgFlags('-' + arg.first);
            if (flags) {
                std::string value_str = (*flags & SENSITIVE) ? "****" : value.write();
                LogPrintf("%s %s%s=%s\n", prefix, section_str, arg.first, value_str);
            }
        }
    }
}

UniValue ArgsManager::OutputArgsSection(
        const std::string& section,
        const std::map<std::string, std::vector<util::SettingsValue>>& args) const
{
    UniValue result(UniValue::VOBJ);
    UniValue settings(UniValue::VARR);

    std::string section_str = section.empty() ? "" : "[" + section + "] ";
    for (const auto& arg : args) {

        UniValue setting(UniValue::VOBJ);

        for (const auto& value : arg.second) {
            std::optional<unsigned int> flags = GetArgFlags('-' + arg.first);
            if (flags) {
                if (*flags & SENSITIVE)
                {
                    setting.pushKV(arg.first, "****");
                }
                else
                {
                    setting.pushKV(arg.first, value);
                }

                setting.pushKV("changeable_without_restart", (bool)(*flags & IMMEDIATE_EFFECT));

                settings.push_back(setting);
            }
        }
    }

    result.pushKV("section", section_str);
    result.pushKV("settings", settings);

    return result;
}

void ArgsManager::LogArgs() const
{
    LOCK(cs_args);
    for (const auto& section : m_settings.ro_config) {
        logArgsPrefix("Config file arg:", section.first, section.second);
    }
    for (const auto& setting : m_settings.rw_settings) {
        LogPrintf("Setting file arg: %s = %s\n", setting.first, setting.second.write());
    }
    logArgsPrefix("Command-line arg:", "", m_settings.command_line_options);
}

UniValue ArgsManager::OutputArgs() const
{
    UniValue result(UniValue::VOBJ);
    UniValue sections(UniValue::VARR);
    UniValue args_section(UniValue::VOBJ);
    UniValue settings(UniValue::VARR);

    LOCK(cs_args);

    for (const auto& section : m_settings.ro_config) {

        args_section = OutputArgsSection(section.first, section.second);
        sections.push_back(args_section);
    }
    result.pushKV("ro_config_file_args", sections);

    // There are no sections for rw_settings and the command line.
    for (const auto& setting : m_settings.rw_settings) {
        UniValue arg(UniValue::VOBJ);
        std::optional<unsigned int> flags = GetArgFlags('-' + setting.first);

        arg.pushKV(setting.first, setting.second);

        arg.pushKV("changeable_without_restart", (bool)(*flags & IMMEDIATE_EFFECT));

        settings.push_back(arg);
    }

    result.pushKV("setting_file_args", settings);

    args_section.clear();
    args_section = OutputArgsSection("none", m_settings.command_line_options);
    result.pushKV("command_line_args", find_value(args_section, "settings"));

    return result;
}


// When we port the interfaces file over from Bitcoin, these two functions should be moved there.
util::SettingsValue getRwSetting(const std::string& name)
{
    util::SettingsValue result;
    gArgs.LockSettings([&](const util::Settings& settings) {
        if (const util::SettingsValue* value = util::FindKey(settings.rw_settings, name)) {
            result = *value;
        }
    });
    return result;
}

bool updateRwSetting(const std::string& name, const util::SettingsValue& value)
{
    gArgs.LockSettings([&](util::Settings& settings) {
        if (value.isNull()) {
            settings.rw_settings.erase(name);
        } else {
            settings.rw_settings[name] = value;
        }
    });
    return gArgs.WriteSettingsFile();
}

bool updateRwSettings(const std::vector<std::pair<std::string, util::SettingsValue>>& settings_in)
{
    gArgs.LockSettings([&](util::Settings& settings)  {
        for (const auto& iter : settings_in)
        {
            if (iter.second.isNull()) {
                settings.rw_settings.erase(iter.first);
            } else {
                settings.rw_settings[iter.first] = iter.second;
            }
        }
    });
    return gArgs.WriteSettingsFile();
}

bool RenameOver(fs::path src, fs::path dest)
{
#ifdef WIN32
    return MoveFileExW(src.wstring().c_str(), dest.wstring().c_str(),
                       MOVEFILE_REPLACE_EXISTING) != 0;
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

void SetupEnvironment()
{
#ifdef HAVE_MALLOPT_ARENA_MAX
    // glibc-specific: On 32-bit systems set the number of arenas to 1.
    // By default, since glibc 2.10, the C library will create up to two heap
    // arenas per core. This is known to cause excessive virtual address space
    // usage in our usage. Work around it by setting the maximum number of
    // arenas to 1.
    if (sizeof(void*) == 4) {
        mallopt(M_ARENA_MAX, 1);
    }
#endif
    // On most POSIX systems (e.g. Linux, but not BSD) the environment's locale
    // may be invalid, in which case the "C.UTF-8" locale is used as fallback.
#if !defined(WIN32) && !defined(MAC_OSX) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
    try {
        std::locale(""); // Raises a runtime error if current locale is invalid
    } catch (const std::runtime_error&) {
        setenv("LC_ALL", "C.UTF-8", 1);
    }
#elif defined(WIN32)
    // Set the default input/output charset is utf-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    // The path locale is lazy initialized and to avoid deinitialization errors
    // in multithreading environments, it is set explicitly by the main thread.
    // A dummy locale is used to extract the internal default locale, used by
    // fs::path, which is then used to explicitly imbue the path.
    std::locale loc = fs::path::imbue(std::locale::classic());
#ifndef WIN32
    fs::path::imbue(loc);
#else
    fs::path::imbue(std::locale(loc, new std::codecvt_utf8_utf16<wchar_t>()));
#endif
}

// Newer FileCommit overload from Bitcoin.
bool FileCommit(FILE *file)
{
    if (fflush(file) != 0) { // harmless if redundantly called
        LogPrintf("%s: fflush failed: %d\n", __func__, errno);
        return false;
    }
#ifdef WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    if (FlushFileBuffers(hFile) == 0) {
        LogPrintf("%s: FlushFileBuffers failed: %d\n", __func__, GetLastError());
        return false;
    }
#else
    #if defined(__linux__) || defined(__NetBSD__)
    if (fdatasync(fileno(file)) != 0 && errno != EINVAL) { // Ignore EINVAL for filesystems that don't support sync
        LogPrintf("%s: fdatasync failed: %d\n", __func__, errno);
        return false;
    }
    #elif defined(MAC_OSX) && defined(F_FULLFSYNC)
    if (fcntl(fileno(file), F_FULLFSYNC, 0) == -1) { // Manpage says "value other than -1" is returned on success
        LogPrintf("%s: fcntl F_FULLFSYNC failed: %d\n", __func__, errno);
        return false;
    }
    #else
    if (fsync(fileno(file)) != 0 && errno != EINVAL) {
        LogPrintf("%s: fsync failed: %d\n", __func__, errno);
        return false;
    }
    #endif
#endif
    return true;
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    wchar_t pszPath[MAX_PATH] = L"";

    if (SHGetSpecialFolderPathW(nullptr, pszPath, nFolder, fCreate))
    {
        return fs::path(pszPath);
    }

    LogPrintf("SHGetSpecialFolderPathW() failed, could not obtain requested path.");
    return fs::path("");
}
#endif

namespace util {
#ifdef WIN32
WinCmdLineArgs::WinCmdLineArgs()
{
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_cvt;
    argv = new char*[argc];
    args.resize(argc);
    for (int i = 0; i < argc; i++) {
        args[i] = utf8_cvt.to_bytes(wargv[i]);
        argv[i] = &*args[i].begin();
    }
    LocalFree(wargv);
}

WinCmdLineArgs::~WinCmdLineArgs()
{
    delete[] argv;
}

std::pair<int, char**> WinCmdLineArgs::get()
{
    return std::make_pair(argc, argv);
}
#endif
} // namespace util
