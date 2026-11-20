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
#elif defined(WIN32)
#include <cstddef>
#include <limits>

#include <iphlpapi.h>
#endif

// The platforms with a backend, where CheckRpcListener identifies the listener's owner. Each one
// defines the four functions declared in the backend section below.
#if defined(__linux__) || defined(WIN32)
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

//! One line of the table: sl local_address rem_address st tx_queue:rx_queue tr:tm->when retrnsmt uid timeout
//! inode ...
std::optional<TcpConnEntry> ParseLine(std::string_view line, bool ipv6)
{
    const std::vector<std::string_view> fields = SplitFields(line);
    if (fields.size() < 10) return std::nullopt;

    const std::string_view slot = fields[0];
    if (slot.size() < 2 || slot.back() != ':' || !IsDecimal(slot.substr(0, slot.size() - 1))) return std::nullopt;

    const std::optional<CService> local = ParseEndpoint(fields[1], ipv6);
    const std::optional<CService> remote = ParseEndpoint(fields[2], ipv6);
    const std::optional<uint32_t> state = ParseFixedHex(fields[3], 2);
    uint64_t owner{0};
    uint64_t inode{0};
    if (!local || !remote || !state || !IsDecimal(fields[7]) || !ParseUInt64(std::string(fields[7]), &owner) ||
        !IsDecimal(fields[9]) || !ParseUInt64(std::string(fields[9]), &inode)) {
        return std::nullopt;
    }

    TcpConnEntry entry;
    entry.local = *local;
    entry.remote = *remote;
    // TCP_ESTABLISHED covers a connection still waiting in the listener's accept queue, with inode 0
    // until a process accepts it. A connection whose last handshake packet the listener has not
    // taken in yet (its accept queue was full) is listed as TCP_SYN_RECV, with the listener's owner.
    if (*state == 0x01) {
        entry.state = TcpState::Established;
    } else if (*state == 0x03) {
        entry.state = TcpState::SynReceived;
    }
    entry.owner = owner;
    entry.inode = inode;
    return entry;
}

#ifdef WIN32

//! What GetExtendedTcpTable gives for \p family (AF_INET or AF_INET6) and \p table, in a buffer
//! sized to fit. nullopt when it fails.
std::optional<std::vector<unsigned char>> GetTcpTable(ULONG family, TCP_TABLE_CLASS table)
{
    std::vector<unsigned char> buffer;
    // The table can grow between the call that sizes the buffer and the call that fills it.
    for (int attempt = 0; attempt < 4; ++attempt) {
        DWORD size = static_cast<DWORD>(buffer.size());
        const DWORD rc =
            ::GetExtendedTcpTable(buffer.empty() ? nullptr : buffer.data(), &size, FALSE, family, table, 0);
        if (rc == NO_ERROR) return buffer;
        if (rc != ERROR_INSUFFICIENT_BUFFER) return std::nullopt;
        // Room for a few rows more than the size asked for, of the largest row type read.
        buffer.assign(static_cast<size_t>(size) + 16 * sizeof(MIB_TCP6ROW_OWNER_MODULE), 0);
    }
    return std::nullopt;
}

//! TCP_TABLE_OWNER_PID_CONNECTIONS: every connection but the listening sockets.
std::optional<std::vector<unsigned char>> GetConnectionTable(ULONG family)
{
    return GetTcpTable(family, TCP_TABLE_OWNER_PID_CONNECTIONS);
}

//! The rows of a table GetConnectionTable read: Table is MIB_TCPTABLE_OWNER_PID or
//! MIB_TCP6TABLE_OWNER_PID, and Row its row type. nullopt when the count does not fit the buffer.
template <typename Table, typename Row>
std::optional<std::vector<Row>> TableRows(const std::vector<unsigned char>& buffer)
{
    DWORD count{0};
    if (buffer.size() < sizeof(count)) return std::nullopt;
    std::memcpy(&count, buffer.data(), sizeof(count));

    const size_t first = offsetof(Table, table);
    if (first > buffer.size() || count > (buffer.size() - first) / sizeof(Row)) return std::nullopt;

    std::vector<Row> rows(count);
    if (count > 0) std::memcpy(rows.data(), buffer.data() + first, rows.size() * sizeof(Row));
    return rows;
}

TcpState StateOfRow(DWORD state)
{
    // MIB_TCP_STATE_ESTAB covers a connection still waiting in the listener's backlog as well as
    // an accepted one; MIB_TCP_STATE_SYN_RCVD is one whose handshake has not completed.
    switch (state) {
    case MIB_TCP_STATE_ESTAB:
        return TcpState::Established;
    case MIB_TCP_STATE_SYN_RCVD:
        return TcpState::SynReceived;
    default:
        return TcpState::Other;
    }
}

//! A port as a table row gives it: the low 16 bits of the DWORD, in network order.
unsigned short PortOfRow(DWORD port)
{
    return ntohs(static_cast<u_short>(port));
}

std::optional<std::vector<TcpConnEntry>> ReadWindowsTcpTable(ULONG family)
{
    const std::optional<std::vector<unsigned char>> buffer = GetConnectionTable(family);
    if (!buffer) return std::nullopt;

    std::vector<TcpConnEntry> entries;
    if (family == AF_INET) {
        const std::optional<std::vector<MIB_TCPROW_OWNER_PID>> rows =
            TableRows<MIB_TCPTABLE_OWNER_PID, MIB_TCPROW_OWNER_PID>(*buffer);
        if (!rows) return std::nullopt;
        for (const MIB_TCPROW_OWNER_PID& row : *rows) {
            // The addresses are in network order, as in_addr holds them.
            in_addr local{};
            in_addr remote{};
            local.s_addr = row.dwLocalAddr;
            remote.s_addr = row.dwRemoteAddr;

            TcpConnEntry entry;
            entry.local = CService(local, PortOfRow(row.dwLocalPort));
            entry.remote = CService(remote, PortOfRow(row.dwRemotePort));
            entry.state = StateOfRow(row.dwState);
            entry.owner = row.dwOwningPid;
            entries.push_back(std::move(entry));
        }
        return entries;
    }

    const std::optional<std::vector<MIB_TCP6ROW_OWNER_PID>> rows =
        TableRows<MIB_TCP6TABLE_OWNER_PID, MIB_TCP6ROW_OWNER_PID>(*buffer);
    if (!rows) return std::nullopt;
    for (const MIB_TCP6ROW_OWNER_PID& row : *rows) {
        in6_addr local{};
        in6_addr remote{};
        static_assert(sizeof(local) == sizeof(row.ucLocalAddr) && sizeof(remote) == sizeof(row.ucRemoteAddr));
        std::memcpy(&local, row.ucLocalAddr, sizeof(local));
        std::memcpy(&remote, row.ucRemoteAddr, sizeof(remote));

        TcpConnEntry entry;
        entry.local = CService(CNetAddr(local, row.dwLocalScopeId), PortOfRow(row.dwLocalPort));
        entry.remote = CService(CNetAddr(remote, row.dwRemoteScopeId), PortOfRow(row.dwRemotePort));
        entry.state = StateOfRow(row.dwState);
        entry.owner = row.dwOwningPid;
        entries.push_back(std::move(entry));
    }
    return entries;
}

//! A row's creation time as a count of 100 ns.
uint64_t CountOf(const LARGE_INTEGER& stamp)
{
    return static_cast<uint64_t>(stamp.QuadPart);
}

#endif // WIN32

} // namespace

#ifdef WIN32
std::optional<uint64_t> ListenerCreationTime(const TcpConnEntry& server_side)
{
    const bool ipv6 = !server_side.local.IsIPv4();
    const std::optional<std::vector<unsigned char>> buffer =
        GetTcpTable(ipv6 ? AF_INET6 : AF_INET, TCP_TABLE_OWNER_MODULE_LISTENER);
    if (!buffer) return std::nullopt;

    // The socket at the connection's own address, else the one on every address of the port:
    // 0.0.0.0, or "::", the all-zero address.
    const CNetAddr any_ipv4{in_addr{}};
    const CNetAddr any_ipv6;
    std::optional<uint64_t> exact;
    std::optional<uint64_t> unspecified;
    const auto consider = [&](const CNetAddr& addr, DWORD port, DWORD pid, const LARGE_INTEGER& created) {
        if (PortOfRow(port) != server_side.local.GetPort() || pid != server_side.owner) return;
        if (addr == static_cast<const CNetAddr&>(server_side.local)) {
            exact = CountOf(created);
        } else if (addr == any_ipv4 || addr == any_ipv6) {
            unspecified = CountOf(created);
        }
    };
    if (!ipv6) {
        const std::optional<std::vector<MIB_TCPROW_OWNER_MODULE>> rows =
            TableRows<MIB_TCPTABLE_OWNER_MODULE, MIB_TCPROW_OWNER_MODULE>(*buffer);
        if (!rows) return std::nullopt;
        for (const MIB_TCPROW_OWNER_MODULE& row : *rows) {
            in_addr local{};
            local.s_addr = row.dwLocalAddr;
            consider(CNetAddr(local), row.dwLocalPort, row.dwOwningPid, row.liCreateTimestamp);
        }
    } else {
        const std::optional<std::vector<MIB_TCP6ROW_OWNER_MODULE>> rows =
            TableRows<MIB_TCP6TABLE_OWNER_MODULE, MIB_TCP6ROW_OWNER_MODULE>(*buffer);
        if (!rows) return std::nullopt;
        for (const MIB_TCP6ROW_OWNER_MODULE& row : *rows) {
            in6_addr local{};
            static_assert(sizeof(local) == sizeof(row.ucLocalAddr));
            std::memcpy(&local, row.ucLocalAddr, sizeof(local));
            consider(CNetAddr(local, row.dwLocalScopeId), row.dwLocalPort, row.dwOwningPid, row.liCreateTimestamp);
        }
    }
    return exact ? exact : unspecified;
}
#endif // WIN32

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

bool ServerSideAccepted(const TcpConnEntry& server_side)
{
    return server_side.state == TcpState::Established && server_side.inode != 0;
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
#elif defined(WIN32)
    std::optional<std::vector<TcpConnEntry>> entries = ReadWindowsTcpTable(AF_INET);
    if (!entries) return std::nullopt;
    const std::optional<std::vector<TcpConnEntry>> entries6 = ReadWindowsTcpTable(AF_INET6);
    if (!entries6) return std::nullopt;
    entries->insert(entries->end(), entries6->begin(), entries6->end());
    return entries;
#else
    return std::nullopt;
#endif
}

std::optional<TcpConnEntry> WaitForAcceptedServerSide(const TcpConnEntry& server_side, std::chrono::milliseconds wait)
{
    const auto deadline = std::chrono::steady_clock::now() + wait;
    std::chrono::milliseconds pause{1};
    std::optional<TcpConnEntry> entry = server_side;
    while (!entry || !ServerSideAccepted(*entry)) {
        if (std::chrono::steady_clock::now() >= deadline) return std::nullopt;
        UninterruptibleSleep(pause);
        pause = std::min(pause * 2, LISTENER_ACCEPT_POLL_MAX);
        // A read of the table can miss the row while the table changes, so a miss is read again too.
        entry.reset();
        if (const std::optional<std::vector<TcpConnEntry>> tables = ReadTcpConnectionTables()) {
            entry = FindServerSide(*tables, server_side.remote, server_side.local);
        }
    }
    return entry;
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

bool AclGrantsWriteToOthers(const std::vector<AceFacts>& aces)
{
    // SYSTEM, Administrators and TrustedInstaller, which own and service what Windows installs.
    const auto trusted = [](const std::string& sid) {
        return sid == "S-1-5-18" || sid == "S-1-5-32-544" ||
               sid == "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";
    };
    return std::any_of(aces.begin(), aces.end(), [&](const AceFacts& ace) {
        return ace.allow && !ace.inherit_only && (ace.mask & WINDOWS_WRITE_RIGHTS) != 0 && !trusted(ace.sid);
    });
}

PathSecurityFacts WindowsPathFacts(const util::winacl::ObservedAcl& acl)
{
    std::vector<AceFacts> aces;
    aces.reserve(acl.aces.size());
    for (const util::winacl::ObservedAce& ace : acl.aces) {
        AceFacts facts;
        // An entry whose SID was not well formed has an empty trustee, which formats as an empty
        // string and so is nobody trusted.
        facts.sid = util::winacl::FormatSid(ace.trustee);
        facts.mask = ace.mask;
        facts.allow = ace.kind == util::winacl::AceKind::Allow || ace.kind == util::winacl::AceKind::OtherAllow;
        facts.inherit_only = ace.inherit_only;
        aces.push_back(std::move(facts));
    }

    PathSecurityFacts facts;
    if (acl.owner) facts.owner = util::winacl::FormatSid(*acl.owner);
    // No DACL, or a NULL one, gives everyone full access.
    facts.writable_by_others = !acl.dacl_present || AclGrantsWriteToOthers(aces);
    return facts;
}

namespace {

//! A Windows path to compare: every '/' as '\', and ASCII letters in lower case.
std::string FoldWindowsPath(const std::string& path)
{
    std::string folded = ToLower(path);
    std::replace(folded.begin(), folded.end(), '/', '\\');
    return folded;
}

} // namespace

bool ProcessPredatesListener(uint64_t process_created, uint64_t listener_created)
{
    const uint64_t slack = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                    LISTENER_CREATION_SLACK)
                                                    .count() /
                                                100);
    return process_created <= listener_created || process_created - listener_created <= slack;
}

bool IsInstalledCoreImage(const std::string& listener_image, const std::string& self_image)
{
    const std::string listener = FoldWindowsPath(listener_image);
    const std::string self = FoldWindowsPath(self_image);
    if (listener.empty() || self.empty()) return false;
    if (listener == self) return true;

    const size_t name_start = self.rfind('\\');
    if (name_start == std::string::npos) return false;
    const std::string dir = self.substr(0, name_start);
    if (listener == dir + "\\gridcoinresearch.exe") return true;

    // The installer puts the daemon in daemon\ and the GUI in the directory above it.
    const size_t dir_name_start = dir.rfind('\\');
    if (dir_name_start == std::string::npos || dir.compare(dir_name_start + 1, std::string::npos, "daemon") != 0) {
        return false;
    }
    return listener == dir.substr(0, dir_name_start) + "\\gridcoinresearch.exe";
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
    // The table's uid for the server side of a connection is the account of the process that accepted
    // it, which holds it. Until a process accepts it the uid names no such process: kernels up to 6.16
    // show 0, root, for every connection in the accept queue. So the check waits for the accept,
    // which the core makes as soon as one of its RPC threads is free.
    const std::optional<TcpConnEntry> accepted = WaitForAcceptedServerSide(server_side, LISTENER_ACCEPT_WAIT);
    if (!accepted) return std::nullopt;
    return ListenerIdentity{ToString(accepted->owner), true};
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

#elif defined(WIN32)

using util::winacl::UniqueHandle;

CallerIdentity GetCallerIdentity()
{
    // The user of this process's token: S-1-5-18 for SYSTEM.
    try {
        const std::string sid = util::winacl::FormatSid(util::winacl::CurrentTokenUser());
        return {sid, sid == "S-1-5-18"};
    } catch (const std::exception&) {
        // An empty caller is never accepted.
        return {};
    }
}

//! SeDebugPrivilege, enabled on this process's token by Enable() when the token holds it (an
//! elevated administrator's holds it, disabled), and put back as it was when this goes out of scope.
class DebugPrivilegeScope
{
public:
    DebugPrivilegeScope() = default;
    ~DebugPrivilegeScope()
    {
        // m_previous lists the privilege only when Enable() changed it.
        if (m_token && m_previous.PrivilegeCount > 0) {
            ::AdjustTokenPrivileges(m_token.get(), FALSE, &m_previous, 0, nullptr, nullptr);
        }
    }

    DebugPrivilegeScope(const DebugPrivilegeScope&) = delete;
    DebugPrivilegeScope& operator=(const DebugPrivilegeScope&) = delete;

    //! True when the privilege is enabled now.
    bool Enable()
    {
        if (m_enabled) return true;
        if (m_token) return false;

        HANDLE token = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return false;
        m_token.reset(token);

        TOKEN_PRIVILEGES wanted{};
        wanted.PrivilegeCount = 1;
        wanted.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (!::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &wanted.Privileges[0].Luid)) return false;

        DWORD length{0};
        if (!::AdjustTokenPrivileges(m_token.get(), FALSE, &wanted, sizeof(m_previous), &m_previous, &length)) {
            return false;
        }
        // The call succeeds without enabling a privilege the token does not hold, and then reports
        // ERROR_NOT_ALL_ASSIGNED.
        m_enabled = ::GetLastError() == ERROR_SUCCESS;
        return m_enabled;
    }

private:
    UniqueHandle m_token;
    TOKEN_PRIVILEGES m_previous{};
    bool m_enabled{false};
};

//! What \p open returns: a handle, or null with the error in GetLastError. When it is refused with
//! ERROR_ACCESS_DENIED, SeDebugPrivilege is enabled through \p debug and \p open is called once more.
template <typename Open>
UniqueHandle OpenAllowingDebug(DebugPrivilegeScope& debug, const Open& open)
{
    UniqueHandle handle(open());
    if (!handle && ::GetLastError() == ERROR_ACCESS_DENIED && debug.Enable()) {
        handle.reset(open());
    }
    return handle;
}

//! Room for the longest path Windows has, 32767 characters, and the terminating null.
constexpr DWORD LONGEST_PATH{32768};

//! The path of the image \p process runs, in the Win32 form (QueryFullProcessImageNameW with
//! flags 0), as UTF-8.
std::optional<std::string> ProcessImagePath(HANDLE process)
{
    std::wstring name(LONGEST_PATH, L'\0');
    DWORD size = LONGEST_PATH;
    if (!::QueryFullProcessImageNameW(process, 0, name.data(), &size)) return std::nullopt;
    // On success the size leaves out the terminating null.
    name.resize(size);
    return WideToUtf8(name);
}

//! The path of this process's own image (GetModuleFileNameW), as UTF-8.
std::optional<std::string> OwnImagePath()
{
    std::wstring module(LONGEST_PATH, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, module.data(), LONGEST_PATH);
    // A path cut short fills the whole buffer.
    if (length == 0 || length >= LONGEST_PATH) return std::nullopt;
    module.resize(length);
    return WideToUtf8(module);
}

//! The user of the access token \p token as a SID string.
std::optional<std::string> TokenUserSid(HANDLE token)
{
    const std::vector<unsigned char> buffer = util::winacl::QueryTokenInformation(token, TokenUser, "TokenUser");
    if (buffer.size() < sizeof(TOKEN_USER)) return std::nullopt;
    const std::optional<util::winacl::Sid> sid =
        util::winacl::SidAt(reinterpret_cast<const TOKEN_USER*>(buffer.data())->User.Sid);
    if (!sid) return std::nullopt;
    return util::winacl::FormatSid(*sid);
}

std::optional<ListenerIdentity> IdentifyListener(const TcpConnEntry& server_side)
{
    // The table's pid for the server side of a connection: the process that made the listening
    // socket, while the connection waits in its backlog and once it is accepted. 0, the System Idle
    // Process, is no process that can hold one.
    if (server_side.owner == 0 || server_side.owner > std::numeric_limits<DWORD>::max()) return std::nullopt;
    const DWORD pid = static_cast<DWORD>(server_side.owner);

    try {
        DebugPrivilegeScope debug;
        const UniqueHandle process = OpenAllowingDebug(debug, [pid]() -> void* {
            return ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        });
        if (!process) return std::nullopt;

        // A pid is given to another process only once its process has ended and every handle to it is
        // closed. With the process held open, the same row giving the same pid again shows that the
        // process opened holds the connection, and is not a later one that took over the pid.
        const std::optional<std::vector<TcpConnEntry>> tables = ReadTcpConnectionTables();
        if (!tables) return std::nullopt;
        const std::optional<TcpConnEntry> again = FindServerSide(*tables, server_side.remote, server_side.local);
        if (!again || again->owner != server_side.owner) return std::nullopt;

        // The table goes on naming the process that made the listening socket after it has ended,
        // while another one holds the socket on, so the pid may have been given to a later
        // process before it was opened. The process opened must be as old as the socket.
        const std::optional<uint64_t> listener_created = ListenerCreationTime(server_side);
        FILETIME created{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        if (!listener_created || !::GetProcessTimes(process.get(), &created, &exited, &kernel, &user)) {
            return std::nullopt;
        }
        const uint64_t process_created =
            (static_cast<uint64_t>(created.dwHighDateTime) << 32) | static_cast<uint64_t>(created.dwLowDateTime);
        if (!ProcessPredatesListener(process_created, *listener_created)) return std::nullopt;

        const std::optional<std::string> image = ProcessImagePath(process.get());
        const std::optional<std::string> self = OwnImagePath();
        if (!image || !self) return std::nullopt;

        const UniqueHandle token = OpenAllowingDebug(debug, [&process]() -> void* {
            HANDLE opened = nullptr;
            return ::OpenProcessToken(process.get(), TOKEN_QUERY, &opened) ? opened : nullptr;
        });
        if (!token) return std::nullopt;

        const std::optional<std::string> owner = TokenUserSid(token.get());
        if (!owner) return std::nullopt;
        return ListenerIdentity{*owner, IsInstalledCoreImage(*image, *self)};
    } catch (const std::exception&) {
        // A token query that failed, or a path that is not valid UTF-16.
        return std::nullopt;
    }
}

void GetConfFacts(std::optional<PathSecurityFacts>& conf, std::optional<PathSecurityFacts>& conf_dir)
{
    // What ReadConfigFiles recorded from the open file it read, as on Linux.
    const std::optional<ConfigFileFacts> facts = gArgs.GetConfigFileFacts();
    if (!facts) return;
    if (facts->file_acl) conf = WindowsPathFacts(*facts->file_acl);
    if (facts->dir_acl) conf_dir = WindowsPathFacts(*facts->dir_acl);
}

std::optional<std::string> ResolveAccountName(const std::string& name)
{
    try {
        const std::optional<util::winacl::Sid> sid =
            util::winacl::LookupAccountSidByName(Utf8ToWide(name), /*require_alias=*/false);
        if (!sid) return std::nullopt;
        return util::winacl::FormatSid(*sid);
    } catch (const std::exception&) {
        // A name that cannot be looked up names no account the listener can be checked against.
        return std::nullopt;
    }
}

#endif // __linux__, WIN32

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
#ifdef WIN32
        WarnOnce(g_service_user_ignored_warned, "ignoring -rpcserviceuser: it is honoured only from a configuration "
                                                "file that SYSTEM or Administrators own and nobody else can write");
#else
        WarnOnce(g_service_user_ignored_warned, "ignoring -rpcserviceuser: it is honoured only from a configuration "
                                                "file that root owns and nobody else can write");
#endif
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
