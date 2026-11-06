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

//! Every name \p conf_text sets, in the order of its lines.
std::vector<std::string> ConfNames(const std::string& conf_text)
{
    std::vector<std::string> names;
    size_t start = 0;
    while (start < conf_text.size()) {
        size_t end = conf_text.find('\n', start);
        if (end == std::string::npos) end = conf_text.size();
        if (std::optional<std::string> name = ConfLineName(conf_text.substr(start, end - start))) {
            names.push_back(std::move(*name));
        }
        start = end + 1;
    }
    return names;
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
    // the socket reads them, and each service may read the root's access list, as the check of
    // the directories above its autounlock file reads it. Neither is passed down.
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

    add(root / L"ipc", true,
        aces(tree, {{sids.grp_main, ipc::win::DIR_VIEW_MASK, here}, {sids.grp_test, ipc::win::DIR_VIEW_MASK, here}}));
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
    const std::vector<std::string> names = ConfNames(conf_text);
    const auto sets = [&names](const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };

    std::string missing;
    if (!sets("rpcuser")) missing += "rpcuser=gridcoinrpc\n";
    if (!sets("rpcpassword")) missing += "rpcpassword=" + rpcpassword + "\n";
    if (!sets("rpcserviceuser")) missing += "rpcserviceuser=" + Ascii(NamesFor(net).account) + "\n";

    if (!missing.empty() && !conf_text.empty() && conf_text.back() != '\n') missing.insert(0, "\n");

    return missing;
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

} // namespace winsvc
