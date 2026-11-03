// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/listener_check.h>

#include <netbase.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>
#include <util/time.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string_view>

#if defined(__linux__)
#include <cerrno>
#include <pwd.h>
#include <unistd.h>
#endif

// The platforms with a backend, where CheckRpcListener identifies the listener's owner. Each one
// defines the four functions declared in the backend section below.
#if defined(__linux__)
#define LISTENER_CHECK_BACKEND 1
#endif

namespace {

//! Fields separated by spaces or tabs, empty ones dropped.
std::vector<std::string_view> SplitFields(std::string_view line)
{
    std::vector<std::string_view> fields;
    const auto is_separator = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    size_t pos = 0;
    while (pos < line.size()) {
        while (pos < line.size() && is_separator(line[pos])) ++pos;
        const size_t start = pos;
        while (pos < line.size() && !is_separator(line[pos])) ++pos;
        if (pos > start) fields.push_back(line.substr(start, pos - start));
    }
    return fields;
}

//! Exactly `digits` (at most 8) hex digits.
std::optional<uint32_t> ParseFixedHex(std::string_view text, size_t digits)
{
    if (text.size() != digits) return std::nullopt;
    uint32_t value{0};
    for (const char c : text) {
        const signed char digit = HexDigit(c);
        if (digit < 0) return std::nullopt;
        value = (value << 4) | static_cast<uint32_t>(digit);
    }
    return value;
}

bool IsDecimal(std::string_view text)
{
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

//! An address and port as the kernel prints them: "0100007F:3D63", or 32 hex digits for IPv6.
std::optional<CService> ParseEndpoint(std::string_view text, bool ipv6)
{
    const size_t words = ipv6 ? 4 : 1;
    const size_t address_digits = words * 8;
    if (text.size() != address_digits + 1 + 4 || text[address_digits] != ':') return std::nullopt;

    // Each word is four raw address bytes read as a host-order integer, so copying the parsed
    // integer back into memory restores the bytes in network order on either byte order.
    unsigned char raw[16];
    for (size_t i = 0; i < words; ++i) {
        const std::optional<uint32_t> word = ParseFixedHex(text.substr(i * 8, 8), 8);
        if (!word) return std::nullopt;
        std::memcpy(raw + i * 4, &*word, 4);
    }
    // The port is printed as a host-order number.
    const std::optional<uint32_t> port = ParseFixedHex(text.substr(address_digits + 1), 4);
    if (!port) return std::nullopt;

    if (ipv6) {
        in6_addr addr;
        std::memcpy(&addr, raw, sizeof(addr));
        return CService(addr, static_cast<unsigned short>(*port));
    }
    in_addr addr;
    std::memcpy(&addr, raw, sizeof(addr));
    return CService(addr, static_cast<unsigned short>(*port));
}

//! One line of the table: sl local_address rem_address st tx_queue:rx_queue tr:tm->when retrnsmt uid ...
std::optional<TcpConnEntry> ParseLine(std::string_view line, bool ipv6)
{
    const std::vector<std::string_view> fields = SplitFields(line);
    if (fields.size() < 8) return std::nullopt;

    const std::string_view slot = fields[0];
    if (slot.size() < 2 || slot.back() != ':' || !IsDecimal(slot.substr(0, slot.size() - 1))) return std::nullopt;

    const std::optional<CService> local = ParseEndpoint(fields[1], ipv6);
    const std::optional<CService> remote = ParseEndpoint(fields[2], ipv6);
    const std::optional<uint32_t> state = ParseFixedHex(fields[3], 2);
    uint64_t owner{0};
    if (!local || !remote || !state || !IsDecimal(fields[7]) || !ParseUInt64(std::string(fields[7]), &owner)) {
        return std::nullopt;
    }

    TcpConnEntry entry;
    entry.local = *local;
    entry.remote = *remote;
    // TCP_ESTABLISHED covers a connection still waiting in the listener's accept queue. A
    // connection whose last handshake packet the listener has not taken in yet (its accept queue
    // was full) is listed as TCP_SYN_RECV, with the listener's owner.
    if (*state == 0x01) {
        entry.state = TcpState::Established;
    } else if (*state == 0x03) {
        entry.state = TcpState::SynReceived;
    }
    entry.owner = owner;
    return entry;
}

} // namespace

std::vector<TcpConnEntry> ParseProcNetTcp(std::istream& in, bool ipv6)
{
    std::vector<TcpConnEntry> entries;
    std::string line;
    while (std::getline(in, line)) {
        // The header line does not parse and is skipped with anything else that does not.
        if (std::optional<TcpConnEntry> entry = ParseLine(line, ipv6)) {
            entries.push_back(std::move(*entry));
        }
    }
    return entries;
}

std::optional<TcpConnEntry> FindServerSide(const std::vector<TcpConnEntry>& entries,
                                           const CService& client_local, const CService& client_remote)
{
    std::optional<TcpConnEntry> found;
    for (const TcpConnEntry& entry : entries) {
        if (entry.state != TcpState::Established && entry.state != TcpState::SynReceived) continue;
        if (!(entry.local == client_remote) || !(entry.remote == client_local)) continue;
        if (found) return std::nullopt;
        found = entry;
    }
    return found;
}

std::optional<std::vector<TcpConnEntry>> ReadTcpConnectionTables()
{
#if defined(__linux__)
    std::ifstream tcp4("/proc/net/tcp");
    if (!tcp4.is_open()) return std::nullopt;
    std::vector<TcpConnEntry> entries = ParseProcNetTcp(tcp4, false);
    if (tcp4.bad()) return std::nullopt;

    // A kernel without IPv6 has no tcp6.
    std::ifstream tcp6("/proc/net/tcp6");
    if (tcp6.is_open()) {
        const std::vector<TcpConnEntry> entries6 = ParseProcNetTcp(tcp6, true);
        if (tcp6.bad()) return std::nullopt;
        entries.insert(entries.end(), entries6.begin(), entries6.end());
    }
    return entries;
#else
    return std::nullopt;
#endif
}

bool IsLocalPeerAddress(const CNetAddr& addr, const std::vector<CNetAddr>& host_addrs)
{
    // "::" is the all-zero address. Connecting to it, as to 0.0.0.0, reaches this host.
    return addr.IsLocal() || addr == CNetAddr() ||
           std::find(host_addrs.begin(), host_addrs.end(), addr) != host_addrs.end();
}

std::vector<CNetAddr> GetHostInterfaceAddresses()
{
    std::vector<CNetAddr> addrs;
#ifdef WIN32
    char host_name[1000] = "";
    if (gethostname(host_name, sizeof(host_name)) != SOCKET_ERROR) {
        std::vector<CNetAddr> resolved;
        if (LookupHost(host_name, resolved, 0, true)) {
            addrs = std::move(resolved);
        }
    }
#else
    struct ifaddrs* list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (const struct ifaddrs* ifa = list; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                addrs.emplace_back(reinterpret_cast<const struct sockaddr_in*>(ifa->ifa_addr)->sin_addr);
            } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                const auto* s6 = reinterpret_cast<const struct sockaddr_in6*>(ifa->ifa_addr);
                addrs.emplace_back(s6->sin6_addr, s6->sin6_scope_id);
            }
        }
        freeifaddrs(list);
    }
#endif
    return addrs;
}

LookupOutcome ClassifyLookup(bool server_side_found, const CNetAddr& remote, const std::vector<CNetAddr>& host_addrs)
{
    if (server_side_found) return LookupOutcome::CheckOwner;
    // Without this host's addresses, an address of this host cannot be told from another host's.
    if (host_addrs.empty() || IsLocalPeerAddress(remote, host_addrs)) return LookupOutcome::Unidentified;
    return LookupOutcome::Remote;
}

PathSecurityFacts PosixPathFacts(uint64_t uid, uint32_t mode)
{
    PathSecurityFacts facts;
    facts.owner = ToString(uid);
    // Others are every account but root: group or world write (022), or an owner other than root.
    facts.writable_by_others = (mode & 022) != 0 || uid != 0;
    return facts;
}

ListenerCheckResult DecideListenerOwner(const ListenerOwnerFacts& facts)
{
    if (!facts.image_is_installed_core) {
        return {ListenerVerdict::Refuse, "is not the installed Gridcoin core"};
    }
    if (facts.listener.empty() || facts.caller.empty()) {
        return {ListenerVerdict::Refuse, "has no identified owner"};
    }

    const bool conf_owner_rule = facts.caller_is_superuser && facts.conf_owner && !facts.conf_owner->empty();
    const bool service_rule = facts.service_account && !facts.service_account->empty();

    if (facts.listener == facts.caller) {
        return {ListenerVerdict::Accept, "is owned by this account"};
    }
    if (conf_owner_rule && facts.listener == *facts.conf_owner) {
        return {ListenerVerdict::Accept, "is owned by the owner of the configuration file"};
    }
    if (service_rule && facts.listener == *facts.service_account) {
        return {ListenerVerdict::Accept, "is owned by the service account"};
    }

    std::string also;
    if (conf_owner_rule && *facts.conf_owner != facts.caller) {
        also += " or by " + *facts.conf_owner + " (the configuration file's owner)";
    }
    if (service_rule) {
        also += " or by " + *facts.service_account + " (-rpcserviceuser)";
    }
    return {ListenerVerdict::Refuse, "is owned by " + facts.listener + ", not by " + facts.caller + also};
}

bool ConfTrustedForServiceAccount(const PathSecurityFacts& conf, const PathSecurityFacts& conf_dir, bool uses_includeconf)
{
    // root; SYSTEM and Administrators on Windows. Linux owners are decimal uids and Windows owners
    // SID strings, so the forms never meet.
    const auto trusted_owner = [](const std::string& owner) {
        return owner == "0" || owner == "S-1-5-18" || owner == "S-1-5-32-544";
    };
    return !uses_includeconf && trusted_owner(conf.owner) && trusted_owner(conf_dir.owner) &&
           !conf.writable_by_others && !conf_dir.writable_by_others;
}

std::string OwnerRefusalMessage(const std::string& listener_address, const std::string& reason)
{
    return strprintf("refusing to send RPC credentials: the listener at %s %s. Pass -rpcskiplistenercheck to send "
                     "anyway (not recommended: what the client sends then goes to whatever program holds that address).",
                     listener_address, reason);
}

std::string UnidentifiedListenerMessage(const std::string& listener_address)
{
    return strprintf("refusing to send RPC credentials: could not identify the owner of the listener at %s.",
                     listener_address);
}

#ifdef LISTENER_CHECK_BACKEND

namespace {

struct CallerIdentity {
    std::string id;
    bool superuser{false};
};

struct ListenerIdentity {
    std::string owner;
    bool image_is_installed_core{true};
};

// The backend: the caller's identity, the owner of the server side found in the table, the
// owner and write access of the configuration file this process read and of its directory, and
// an account name in the owner form.
CallerIdentity GetCallerIdentity();
std::optional<ListenerIdentity> IdentifyListener(const TcpConnEntry& server_side);
void GetConfFacts(std::optional<PathSecurityFacts>& conf, std::optional<PathSecurityFacts>& conf_dir);
std::optional<std::string> ResolveAccountName(const std::string& name);

#if defined(__linux__)

CallerIdentity GetCallerIdentity()
{
    // The effective uid, which the kernel records as the owner of the sockets a process creates.
    const uid_t uid = ::geteuid();
    return {ToString(uid), uid == 0};
}

std::optional<ListenerIdentity> IdentifyListener(const TcpConnEntry& server_side)
{
    // The table's uid for the server side of a connection: while it waits in the accept queue, the
    // account that created the listening socket; once accepted, the account of the process that
    // accepted it, which holds it.
    return ListenerIdentity{ToString(server_side.owner), true};
}

void GetConfFacts(std::optional<PathSecurityFacts>& conf, std::optional<PathSecurityFacts>& conf_dir)
{
    // What ReadConfigFiles recorded from the open file it read, not a path looked up now: by now the
    // path can name another file, and a datadir= in the file changes where it resolves.
    const std::optional<ConfigFileFacts> facts = gArgs.GetConfigFileFacts();
    if (!facts) return;
    conf = PosixPathFacts(facts->file.owner, facts->file.mode);
    if (facts->dir) conf_dir = PosixPathFacts(facts->dir->owner, facts->dir->mode);
}

std::optional<std::string> ResolveAccountName(const std::string& name)
{
    const long hint = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    std::vector<char> buffer(hint > 0 ? static_cast<size_t>(hint) : 16384);
    struct passwd pwd;
    struct passwd* found = nullptr;
    int rc;
    while ((rc = ::getpwnam_r(name.c_str(), &pwd, buffer.data(), buffer.size(), &found)) == ERANGE &&
           buffer.size() < 1024 * 1024) {
        buffer.resize(buffer.size() * 2);
    }
    if (rc != 0 || found == nullptr) return std::nullopt;
    return ToString(pwd.pw_uid);
}

#endif // __linux__

// The client runs one command per process, so once per process is once per run.
std::atomic<bool> g_service_user_ignored_warned{false};
std::atomic<bool> g_service_user_unknown_warned{false};

void WarnOnce(std::atomic<bool>& warned, const std::string& text)
{
    if (!warned.exchange(true)) {
        tfm::format(std::cerr, "warning: %s\n", text);
    }
}

//! The service account the listener may also belong to: -rpcserviceuser from the configuration
//! file alone, while that file can name one, resolved to the owner form.
std::optional<std::string> GetServiceAccount(const std::optional<PathSecurityFacts>& conf,
                                             const std::optional<PathSecurityFacts>& conf_dir)
{
    std::optional<std::string> from_conf = gArgs.GetConfigFileArg("-rpcserviceuser");
    if (from_conf && from_conf->empty()) from_conf.reset();
    const bool trusted = from_conf && conf && conf_dir &&
                         ConfTrustedForServiceAccount(*conf, *conf_dir, gArgs.ConfigFileUsedIncludes());
    const std::string honoured = trusted ? *from_conf : std::string{};

    // What was asked for, wherever it was set. A value from the command line, the settings file
    // or a configuration file that cannot name the account differs from the honoured one.
    const std::string requested = gArgs.IsArgNegated("-rpcserviceuser") ? "" : gArgs.GetArg("-rpcserviceuser", "");
    if (requested != honoured) {
        WarnOnce(g_service_user_ignored_warned, "ignoring -rpcserviceuser: it is honoured only from a configuration "
                                                "file that root owns and nobody else can write");
    }
    if (honoured.empty()) return std::nullopt;

    std::optional<std::string> account = ResolveAccountName(honoured);
    if (!account) {
        WarnOnce(g_service_user_unknown_warned,
                 strprintf("ignoring -rpcserviceuser=%s: no account has that name", SanitizeString(honoured)));
    }
    return account;
}

} // namespace

#endif // LISTENER_CHECK_BACKEND

ListenerCheckResult CheckRpcListener(const CService& local, const CService& remote)
{
#ifndef LISTENER_CHECK_BACKEND
    // No backend on this platform: the client sends as it always has.
    return {ListenerVerdict::Accept, "unsupported"};
#else
    const std::string where = remote.ToString();

    // The table first: a row for this connection shows the listener is on this host whatever the
    // address, including one that is routed locally without being on an interface. A read of the
    // table is not atomic, so a line can be skipped or repeated when the table changes during the
    // read; a miss or a duplicate is read again a few times.
    std::optional<TcpConnEntry> server_side;
    for (int attempt = 0; attempt < LISTENER_LOOKUP_ATTEMPTS && !server_side; ++attempt) {
        if (attempt > 0) UninterruptibleSleep(LISTENER_LOOKUP_RETRY_DELAY);
        if (const std::optional<std::vector<TcpConnEntry>> tables = ReadTcpConnectionTables()) {
            server_side = FindServerSide(*tables, local, remote);
        }
    }

    // Only a connection missing from the table can be another host's, and only when this host's
    // addresses are known and the address is none of them.
    const LookupOutcome outcome = ClassifyLookup(server_side.has_value(), remote,
                                                 server_side ? std::vector<CNetAddr>{} : GetHostInterfaceAddresses());
    if (outcome == LookupOutcome::Remote) return {ListenerVerdict::Accept, "remote"};
    if (outcome != LookupOutcome::CheckOwner || !server_side) {
        return {ListenerVerdict::Refuse, UnidentifiedListenerMessage(where)};
    }

    const std::optional<ListenerIdentity> listener = IdentifyListener(*server_side);
    if (!listener) return {ListenerVerdict::Refuse, UnidentifiedListenerMessage(where)};

    const CallerIdentity caller = GetCallerIdentity();
    std::optional<PathSecurityFacts> conf;
    std::optional<PathSecurityFacts> conf_dir;
    GetConfFacts(conf, conf_dir);

    ListenerOwnerFacts facts;
    facts.caller = caller.id;
    facts.caller_is_superuser = caller.superuser;
    facts.listener = listener->owner;
    if (conf) facts.conf_owner = conf->owner;
    facts.service_account = GetServiceAccount(conf, conf_dir);
    facts.image_is_installed_core = listener->image_is_installed_core;

    ListenerCheckResult result = DecideListenerOwner(facts);
    if (result.verdict == ListenerVerdict::Refuse) {
        result.reason = OwnerRefusalMessage(where, result.reason);
    }
    return result;
#endif
}
