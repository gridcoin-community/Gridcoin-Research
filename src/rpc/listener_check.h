// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_RPC_LISTENER_CHECK_H
#define GRIDCOIN_RPC_LISTENER_CHECK_H

#include <netaddress.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <vector>

// The RPC client identifies the owner of the listener it connected to before it sends anything,
// because its first request carries the RPC password. The decisions are the pure functions below;
// a platform backend in listener_check.cpp only gathers the facts they decide on.

enum class TcpState { Established, SynReceived, Other };

struct TcpConnEntry {
    CService local;
    CService remote;
    TcpState state{TcpState::Other};
    uint64_t owner{0}; //!< uid (Linux), pid (Windows)
};

//! /proc/net/tcp (ipv6=false) or /proc/net/tcp6. Skips the header and malformed lines. Each address
//! word is the kernel's host-order hex of the raw bytes: parse to uint32_t and memcpy into the address.
std::vector<TcpConnEntry> ParseProcNetTcp(std::istream& in, bool ipv6);

//! The entry with local == client_remote, remote == client_local and state Established or SynReceived.
//! nullopt when absent or not unique. CService equality makes ::ffff:a.b.c.d equal to a.b.c.d.
std::optional<TcpConnEntry> FindServerSide(const std::vector<TcpConnEntry>& entries,
                                           const CService& client_local, const CService& client_remote);

//! This host's TCP connection table, IPv4 and IPv6 together. Linux reads /proc/net/tcp and
//! /proc/net/tcp6 (a missing tcp6 is not an error). nullopt when the table cannot be read, and on
//! platforms without a backend.
std::optional<std::vector<TcpConnEntry>> ReadTcpConnectionTables();

//! Connecting to addr reaches this host: addr.IsLocal() (127/8, 0/8, ::1), "::", or in host_addrs.
bool IsLocalPeerAddress(const CNetAddr& addr, const std::vector<CNetAddr>& host_addrs);

//! getifaddrs (POSIX, loopback included); gethostname + LookupHost on Windows, as net.cpp Discover().
//! Empty when the addresses cannot be read: a host always has a loopback address.
std::vector<CNetAddr> GetHostInterfaceAddresses();

enum class LookupOutcome { CheckOwner, Remote, Unidentified };

//! What the search of this host's table for the connection decides. A row for it proves the
//! listener is on this host, whatever its address: CheckOwner. Without one the peer is Remote only
//! when this host's addresses are known (host_addrs not empty) and IsLocalPeerAddress is false;
//! otherwise the listener cannot be identified.
LookupOutcome ClassifyLookup(bool server_side_found, const CNetAddr& remote, const std::vector<CNetAddr>& host_addrs);

struct ListenerOwnerFacts {
    std::string caller;                          //!< decimal uid / SID string
    bool caller_is_superuser{false};             //!< uid 0 / S-1-5-18
    std::string listener;                        //!< owner of the server side of our connection
    std::optional<std::string> conf_owner;       //!< owner of the conf file this process read
    std::optional<std::string> service_account;  //!< -rpcserviceuser, resolved, only from a trusted conf
    bool image_is_installed_core{true};          //!< checked on Windows; always true elsewhere
};

enum class ListenerVerdict { Accept, Refuse };

struct ListenerCheckResult {
    ListenerVerdict verdict{ListenerVerdict::Refuse};
    std::string reason;
};

//! Refuse if !image_is_installed_core. Accept if listener == caller; or caller_is_superuser and
//! listener == *conf_owner; or listener == *service_account. Refuse otherwise. An empty listener or
//! caller is refused. A refusal's reason completes the sentence "the listener at <addr> ...".
ListenerCheckResult DecideListenerOwner(const ListenerOwnerFacts& facts);

struct PathSecurityFacts {
    std::string owner;
    bool writable_by_others{true};
};

//! A POSIX owner uid and st_mode as PathSecurityFacts: the owner as a decimal uid, writable by
//! others when the group or world write bit is set or the owner is not root.
PathSecurityFacts PosixPathFacts(uint64_t uid, uint32_t mode);

//! The conf may name a service account: conf file and its directory owned by "0" (Windows:
//! S-1-5-18 or S-1-5-32-544), neither writable by others, and no includeconf in use.
bool ConfTrustedForServiceAccount(const PathSecurityFacts& conf, const PathSecurityFacts& conf_dir, bool uses_includeconf);

//! The error for a refused owner: "refusing to send RPC credentials: the listener at <addr> <reason>.
//! Pass -rpcskiplistenercheck ...", where reason is DecideListenerOwner's.
std::string OwnerRefusalMessage(const std::string& listener_address, const std::string& reason);

//! The error when the owner of the listener cannot be identified.
std::string UnidentifiedListenerMessage(const std::string& listener_address);

using ListenerCheckFn = std::function<ListenerCheckResult(const CService& local, const CService& remote)>;

//! Production check. Find the server side of the exact 4-tuple (LISTENER_LOOKUP_ATTEMPTS reads,
//! LISTENER_LOOKUP_RETRY_DELAY apart). Found: gather the facts, the configuration file's from what
//! ReadConfigFiles recorded, and apply DecideListenerOwner, whatever the address. Not found:
//! Accept with reason "remote" when ClassifyLookup says Remote, else Refuse. Platforms without a
//! backend (macOS, the BSDs): Accept with reason "unsupported" (today's behaviour).
ListenerCheckResult CheckRpcListener(const CService& local, const CService& remote);

static constexpr int LISTENER_LOOKUP_ATTEMPTS{5};
static constexpr std::chrono::milliseconds LISTENER_LOOKUP_RETRY_DELAY{20};

#endif // GRIDCOIN_RPC_LISTENER_CHECK_H
