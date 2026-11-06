// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "winsvc/layout.h"

#include "crypto/sha1.h"
#include "util/strencodings.h"
#include "util/string.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

namespace winsvc {

using util::winacl::AceSpec;
using util::winacl::AclSpec;
using util::winacl::Inherit;

namespace {

//! \p text, which is ASCII, as a narrow string.
std::string Ascii(const std::wstring& text)
{
    std::string narrow;
    narrow.reserve(text.size());
    for (const wchar_t c : text) narrow += static_cast<char>(c);
    return narrow;
}

//! The name one line of a configuration file sets, as ArgsManager reads the line: the text before
//! '=' (the whole line when there is none), with any comment, the spaces around it and a section
//! prefix such as "main." taken off. nullopt for a [section] header and for a line that is empty
//! or only a comment.
std::optional<std::string> ConfLineName(std::string line)
{
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line.erase(hash);
    line = TrimString(line, " \t\r\n");
    if (line.empty() || (line.front() == '[' && line.back() == ']')) return std::nullopt;

    std::string name = TrimString(line.substr(0, line.find('=')), " \t\r\n");
    const size_t dot = name.rfind('.');
    if (dot != std::string::npos) name.erase(0, dot + 1);

    return name;
}

//! Each line of \p text, without its line break.
std::vector<std::string> Lines(const std::string& text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

//! Every name \p conf_text sets, in the order of its lines.
std::vector<std::string> ConfNames(const std::string& conf_text)
{
    std::vector<std::string> names;
    for (const std::string& line : Lines(conf_text)) {
        if (std::optional<std::string> name = ConfLineName(line)) names.push_back(std::move(*name));
    }
    return names;
}

//! One name a configuration file sets, and the section it is set in, empty at the top.
struct ConfSetting
{
    std::string section;
    std::string name;
};

//! Every name \p conf_text sets, with its section, as ArgsManager reads the file
//! (GetConfigOptions, then InterpretOption): a [section] header prefixes the names after it with
//! "<section>.", the text up to the first '.' of a name is its section, and a leading "no" is a
//! negation of the name after it. Also whether the text has a [section] header.
std::vector<ConfSetting> ConfSettings(const std::string& conf_text, bool& has_section_header)
{
    has_section_header = false;
    std::vector<ConfSetting> settings;
    std::string prefix;
    for (std::string line : Lines(conf_text)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        line = TrimString(line, " \t\r\n");
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            prefix = line.substr(1, line.size() - 2) + ".";
            has_section_header = true;
            continue;
        }
        // A line the core does not read as a setting, which it refuses to start with.
        const size_t equals = line.find('=');
        if (line.front() == '-' || equals == std::string::npos) continue;

        std::string name = prefix + TrimString(line.substr(0, equals), " \t\r\n");
        ConfSetting setting;
        const size_t dot = name.find('.');
        if (dot != std::string::npos) {
            setting.section = name.substr(0, dot);
            name.erase(0, dot + 1);
        }
        if (name.compare(0, 2, "no") == 0) name.erase(0, 2);
        setting.name = std::move(name);
        settings.push_back(std::move(setting));
    }
    return settings;
}

} // namespace

NetworkNames NamesFor(Network net)
{
    if (net == Network::Test) {
        return {L"gridcointestnetd", L"Gridcoin Research core (testnet)", L"NT SERVICE\\gridcointestnetd",
                L"Gridcoin Testnet GUI Users", L"testnet.conf", L"testnet", L"testnet", true};
    }
    return {L"gridcoinresearchd", L"Gridcoin Research core (mainnet)", L"NT SERVICE\\gridcoinresearchd",
            L"Gridcoin GUI Users", L"mainnet.conf", L"mainnet", L"mainnet", false};
}

fs::path GridcoinRoot(const fs::path& programdata)
{
    return programdata / L"Gridcoin";
}

fs::path DataDir(const fs::path& programdata, Network net)
{
    return GridcoinRoot(programdata) / NamesFor(net).datadir_leaf;
}

fs::path NetDataDir(const fs::path& programdata, Network net)
{
    // GetDataDir() keeps the test network's files in a testnet directory below -datadir.
    const fs::path datadir = DataDir(programdata, net);
    return net == Network::Test ? datadir / L"testnet" : datadir;
}

fs::path IpcDir(const fs::path& programdata, Network net)
{
    return GridcoinRoot(programdata) / L"ipc" / NamesFor(net).ipc_leaf;
}

fs::path ConfPath(const fs::path& programdata, Network net)
{
    return GridcoinRoot(programdata) / L"conf" / NamesFor(net).conf_file;
}

fs::path QuarantineDir(const fs::path& programdata)
{
    return GridcoinRoot(programdata) / L"conf" / L"quarantine";
}

fs::path StatePath(const fs::path& programdata, Network net)
{
    return GridcoinRoot(programdata) / L"conf" / (NamesFor(net).datadir_leaf + L".state");
}

std::vector<LayoutEntry> ServiceLayout(const fs::path& programdata, const LayoutSids& sids)
{
    const fs::path root = GridcoinRoot(programdata);
    const Inherit tree = Inherit::ContainerAndObject;
    const Inherit here = Inherit::None;
    const auto service = [&sids](Network net) { return net == Network::Main ? sids.svc_main : sids.svc_test; };
    const auto group = [&sids](Network net) { return net == Network::Main ? sids.grp_main : sids.grp_test; };

    // Administrators and SYSTEM with full access, inherited by what is created inside or not,
    // followed by the entry's other trustees.
    const auto aces = [&sids](Inherit inherit, std::initializer_list<AceSpec> more) {
        std::vector<AceSpec> list{{sids.admins, detail::FULL, inherit}, {sids.system, detail::FULL, inherit}};
        list.insert(list.end(), more);
        return list;
    };

    std::vector<LayoutEntry> layout;
    const auto add = [&layout, &sids](const fs::path& path, bool directory, std::vector<AceSpec> list) {
        layout.push_back({path, directory, AclSpec{sids.admins, std::move(list)}});
    };

    // Each attach group may view the root and ipc, as the GUI's check of the directories above
    // the socket reads them. Each service may read the root's attributes and access list
    // (ROOT_SERVICE_MASK: READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE): the
    // creation path of its data directory reads the root's status at every start, and the check of
    // the directories above its autounlock file reads the root's list. Neither is passed down.
    add(root, true,
        aces(tree, {{sids.grp_main, ipc::win::DIR_VIEW_MASK, here},
                    {sids.grp_test, ipc::win::DIR_VIEW_MASK, here},
                    {sids.svc_main, ROOT_SERVICE_MASK, here},
                    {sids.svc_test, ROOT_SERVICE_MASK, here}}));

    add(root / L"conf", true, aces(tree, {}));
    for (const Network net : {Network::Main, Network::Test}) {
        add(ConfPath(programdata, net), false, aces(here, {{service(net), CONF_READ_MASK, here}}));
    }
    add(QuarantineDir(programdata), true, aces(tree, {}));

    for (const Network net : {Network::Main, Network::Test}) {
        add(DataDir(programdata, net), true, aces(tree, {{service(net), detail::FULL, tree}}));
    }

    // Each service may also read the status and access list of ipc, as the core's preparation of
    // its IPC directory makes sure the directory above it exists.
    add(root / L"ipc", true,
        aces(tree, {{sids.grp_main, ipc::win::DIR_VIEW_MASK, here},
                    {sids.grp_test, ipc::win::DIR_VIEW_MASK, here},
                    {sids.svc_main, IPC_PARENT_SERVICE_MASK, here},
                    {sids.svc_test, IPC_PARENT_SERVICE_MASK, here}}));
    for (const Network net : {Network::Main, Network::Test}) {
        add(IpcDir(programdata, net), true,
            aces(here, {{service(net), IPC_SERVICE_MASK, here}, {group(net), ipc::win::DIR_VIEW_MASK, here}}));
    }

    return layout;
}

std::optional<Sid> VirtualServiceSid(const std::wstring& service)
{
    if (service.empty()) return std::nullopt;

    // The upper-case name in UTF-16LE. Only ASCII is upper-cased here.
    std::vector<unsigned char> utf16;
    utf16.reserve(service.size() * 2);
    for (const wchar_t c : service) {
        if (c < 0x20 || c > 0x7e) return std::nullopt;
        utf16.push_back(static_cast<unsigned char>(ToUpper(static_cast<char>(c))));
        utf16.push_back(0);
    }

    std::array<unsigned char, CSHA1::OUTPUT_SIZE> digest{};
    CSHA1().Write(utf16.data(), utf16.size()).Finalize(digest.data());

    // Revision 1, six sub-authorities, authority 5 (NT AUTHORITY), then 80 and the digest. A SID
    // keeps its sub-authorities little-endian, the order the digest's five words are read in, so
    // the digest's bytes go in as they are.
    Sid sid{1, 6, 0, 0, 0, 0, 0, 5, 80, 0, 0, 0};
    sid.insert(sid.end(), digest.begin(), digest.end());

    return sid;
}

std::string BuildServiceConf(Network net, const std::string& rpcpassword)
{
    const NetworkNames names = NamesFor(net);

    std::string conf = "# Generated by gridcoin-service.exe install. Owned by Administrators; the service can only "
                       "read it.\n";
    conf += "rpcuser=gridcoinrpc\n";
    conf += "rpcpassword=" + rpcpassword + "\n";
    conf += "server=1\n";
    conf += "rpcserviceuser=" + Ascii(names.account) + "\n";
    if (names.testnet) conf += "testnet=1\n";

    return conf;
}

bool ServiceConfAcceptable(const std::string& conf_text)
{
    for (std::string name : ConfNames(conf_text)) {
        // A leading '-', which the core refuses in a file, and the negated form count too.
        name.erase(0, name.find_first_not_of('-'));
        name = ToLower(name);
        if (name == "includeconf" || name == "noincludeconf") return false;
    }
    return true;
}

std::string MissingServiceConfLines(Network net, const std::string& conf_text, const std::string& rpcpassword)
{
    // CBaseChainParams::MAIN and TESTNET, the sections ArgsManager reads for each network.
    const std::string own = net == Network::Main ? "main" : "test";

    bool has_section_header = false;
    const std::vector<ConfSetting> settings = ConfSettings(conf_text, has_section_header);
    const auto sets = [&settings, &own](const std::string& name) {
        return std::any_of(settings.begin(), settings.end(), [&](const ConfSetting& setting) {
            return setting.name == name && (setting.section.empty() || setting.section == own);
        });
    };

    std::string missing;
    if (!sets("rpcuser")) missing += "rpcuser=gridcoinrpc\n";
    if (!sets("rpcpassword")) missing += "rpcpassword=" + rpcpassword + "\n";
    if (!sets("rpcserviceuser")) missing += "rpcserviceuser=" + Ascii(NamesFor(net).account) + "\n";
    if (missing.empty()) return missing;

    // A line at the end of a file with sections lands in its last one.
    if (has_section_header) missing.insert(0, "[" + own + "]\n");
    if (!conf_text.empty() && conf_text.back() != '\n') missing.insert(0, "\n");

    return missing;
}

namespace {

//! \p text without the spaces and tabs at its ends.
std::string Trimmed(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return std::string();
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

//! The lines of \p text, without their line ends (LF or CRLF).
std::vector<std::string> LinesOf(const std::string& text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        start = end + 1;
    }
    return lines;
}

//! \p text with each doubled percent sign of a launcher line made one.
std::string UndoublePercents(const std::string& text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        out += text[i];
        if (text[i] == '%' && i + 1 < text.size() && text[i + 1] == '%') ++i;
    }
    return out;
}

} // namespace

std::optional<LauncherCore> ParseLauncherCoreLine(const std::string& text)
{
    bool inside = false;
    std::optional<LauncherCore> found;
    for (const std::string& line : LinesOf(text)) {
        if (!inside) {
            inside = line == "rem BEGIN GENERATED";
            continue;
        }
        if (line == "rem END GENERATED") return found;
        if (found || line.empty() || line[0] != '"') continue;

        // The core's quoted path, then its options. An option ends at a space outside quotes, and a
        // redirection ends the options.
        const size_t path_end = line.find('"', 1);
        if (path_end == std::string::npos) return std::nullopt;
        LauncherCore core;
        std::vector<std::string> options;
        std::string option;
        bool quoted = false;
        for (size_t i = path_end + 1; i < line.size(); ++i) {
            const char c = line[i];
            if (c == '"') quoted = !quoted;
            if (c == ' ' && !quoted) {
                if (!option.empty()) options.push_back(std::move(option));
                option.clear();
            } else {
                option += c;
            }
        }
        if (!option.empty()) options.push_back(std::move(option));
        for (const std::string& item : options) {
            if (item.compare(0, 2, "2>") == 0) break;
            if (item == "-testnet" || item == "-testnet=1") core.testnet = true;
            const std::string prefix = "-datadir=";
            if (item.compare(0, prefix.size(), prefix) == 0) {
                std::string value = item.substr(prefix.size());
                value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
                core.datadir = UndoublePercents(value);
            }
        }
        found = core;
    }
    return std::nullopt;
}

std::optional<std::string> LauncherPathOfArguments(const std::string& arguments)
{
    const std::string head = "/s /c ";
    if (arguments.compare(0, head.size(), head) != 0) return std::nullopt;
    std::string path = Trimmed(arguments.substr(head.size()));
    while (!path.empty() && path.front() == '"') path.erase(path.begin());
    while (!path.empty() && path.back() == '"') path.pop_back();
    if (path.empty()) return std::nullopt;
    return path;
}

Network NetworkOfConf(const std::string& conf_text)
{
    bool test = false;
    for (std::string line : LinesOf(conf_text)) {
        const size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        const size_t equals = line.find('=');
        if (equals == std::string::npos || Trimmed(line.substr(0, equals)) != "testnet") continue;
        // As the core reads a boolean: empty is true, otherwise a number other than 0.
        const std::string value = Trimmed(line.substr(equals + 1));
        test = value.empty() || LocaleIndependentAtoi<int>(value) != 0;
    }
    return test ? Network::Test : Network::Main;
}

std::optional<std::string> StateFileValue(const std::string& text, const std::string& key)
{
    const std::string prefix = key + "=";
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, prefix.size(), prefix) == 0) return line.substr(prefix.size());
        start = end + 1;
    }
    return std::nullopt;
}

std::string StateFileText(const std::string& user_sid, const std::string& from, const std::string& enabled_at,
                          bool task_disabled)
{
    return "user_sid=" + user_sid + "\nfrom=" + from + "\nenabled_at=" + enabled_at +
           "\ntask_disabled=" + (task_disabled ? "1" : "0") + "\n";
}

AclSpec StateFileSpec(const LayoutSids& sids)
{
    return AclSpec{sids.admins, {{sids.admins, detail::FULL, Inherit::None}, {sids.system, detail::FULL, Inherit::None}}};
}

bool IsCoreImageName(const std::wstring& image)
{
    // Without regard to case, as Windows compares the names. Only ASCII is folded, as both names
    // are ASCII.
    const auto folded = [](std::wstring text) {
        for (wchar_t& c : text) {
            if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        }
        return text;
    };
    const std::wstring name = folded(image);
    return name == L"gridcoinresearch.exe" || name == L"gridcoinresearchd.exe";
}

std::optional<std::string> StateFileRefusal(const util::winacl::ObservedAcl& acl)
{
    return util::winacl::FindForeignTrustee(acl, {util::winacl::wellknown::Administrators(),
                                                  util::winacl::wellknown::System()});
}

std::vector<Sid> LevelEntryOwners(const fs::path& programdata, const fs::path& level, const LayoutSids& sids)
{
    std::vector<Sid> owners{sids.admins, sids.system, util::winacl::wellknown::TrustedInstaller()};
    for (const Network net : {Network::Main, Network::Test}) {
        if (level == DataDir(programdata, net) || level == IpcDir(programdata, net)) {
            owners.push_back(net == Network::Main ? sids.svc_main : sids.svc_test);
        }
    }
    return owners;
}

bool ChecksLevelEntries(const LayoutEntry& level, [[maybe_unused]] bool list_matches_layout)
{
    // A list that matches now says nothing of who added an entry before: an earlier install
    // refused after giving the level its list, or an administrator corrected the list by hand.
    return level.directory;
}

std::optional<std::string> LevelEntryRefusal(const std::vector<LayoutEntry>& layout, const fs::path& programdata,
                                             const fs::path& level, const LevelEntryFacts& entry,
                                             const LayoutSids& sids)
{
    // No layout path is ever a link, and the propagation of the level's list must not reach through
    // one, so a link is refused here whatever its name.
    if (entry.link) {
        return entry.path.string() +
               " is a link (a junction, a symbolic link or a mount point), which install does not follow";
    }

    const bool in_layout = std::any_of(layout.begin(), layout.end(), [&entry](const LayoutEntry& layout_entry) {
        return layout_entry.path == entry.path;
    });
    if (in_layout) return std::nullopt;
    if (!entry.owner) return entry.path.string() + ": its owner cannot be read";

    const std::vector<Sid> owners = LevelEntryOwners(programdata, level, sids);
    if (std::find(owners.begin(), owners.end(), *entry.owner) != owners.end()) return std::nullopt;

    const std::string sid = util::winacl::FormatSid(*entry.owner);
    return entry.path.string() + " is owned by " +
           (entry.owner_name.empty() ? sid : entry.owner_name + " (" + sid + ")");
}

bool DisableTargetOwnerAllowed(const std::optional<Sid>& owner, const Sid& user)
{
    return owner && (*owner == user || *owner == util::winacl::wellknown::System() ||
                     *owner == util::winacl::wellknown::Administrators());
}

} // namespace winsvc
