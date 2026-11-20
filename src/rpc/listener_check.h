// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_RPC_LISTENER_CHECK_H
#define GRIDCOIN_RPC_LISTENER_CHECK_H

#include <netaddress.h>
#include <util/win_acl.h>

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
    uint64_t inode{0}; //!< Linux: the socket's inode, 0 until a process accepts the connection
};

//! /proc/net/tcp (ipv6=false) or /proc/net/tcp6. Skips the header and malformed lines. Each address
//! word is the kernel's host-order hex of the raw bytes: parse to uint32_t and memcpy into the address.
std::vector<TcpConnEntry> ParseProcNetTcp(std::istream& in, bool ipv6);

//! The entry with local == client_remote, remote == client_local and state Established or SynReceived.
//! nullopt when absent or not unique. CService equality makes ::ffff:a.b.c.d equal to a.b.c.d.
std::optional<TcpConnEntry> FindServerSide(const std::vector<TcpConnEntry>& entries,
                                           const CService& client_local, const CService& client_remote);

//! Linux: a process has accepted the server side of the connection, which the table shows as
//! Established with a nonzero inode. Only then does the row's uid name the account of a process
//! that holds the connection. Before that, kernels up to 6.16 show 0, root, and later ones the
//! account that created the listening socket.
bool ServerSideAccepted(const TcpConnEntry& server_side);

//! Linux: the row of server_side once ServerSideAccepted holds for it. The table is read again until
//! it does, the first time after 1 ms and each pause twice the last, at most
//! LISTENER_ACCEPT_POLL_MAX; nullopt once \p wait has passed. The Windows table has no inode, so
//! this is for Linux only.
std::optional<TcpConnEntry> WaitForAcceptedServerSide(const TcpConnEntry& server_side, std::chrono::milliseconds wait);

//! Windows: how much later than a listening socket the process the TCP table names for it may have
//! been created and still be the one that made it: the two times are read from clocks that can be
//! a little apart, and may be rounded. See ProcessPredatesListener.
constexpr std::chrono::seconds LISTENER_CREATION_SLACK{2};

//! Windows: whether a process created at \p process_created can be the one the TCP table names
//! for a listening socket created at \p listener_created, both counts of 100 ns as a FILETIME
//! holds them. The table names the process that made the socket, for the socket and for every
//! connection to it, and goes on naming it after that process has ended while another one holds the
//! socket: a pid a later process may have taken. One created more than LISTENER_CREATION_SLACK
//! after the socket did not make it.
bool ProcessPredatesListener(uint64_t process_created, uint64_t listener_created);

#ifdef WIN32
//! Windows: when the listening socket that \p server_side (a row FindServerSide found) is a
//! connection of was created: the LISTEN row of GetExtendedTcpTable's
//! TCP_TABLE_OWNER_MODULE_LISTENER on its local port, at its local address or the unspecified
//! one, made by the process \p server_side names. nullopt when there is no such row or the table
//! cannot be read.
std::optional<uint64_t> ListenerCreationTime(const TcpConnEntry& server_side);
#endif

//! This host's TCP connection table, IPv4 and IPv6 together. Linux reads /proc/net/tcp and
//! /proc/net/tcp6 (a missing tcp6 is not an error); Windows GetExtendedTcpTable's
//! TCP_TABLE_OWNER_PID_CONNECTIONS for AF_INET and AF_INET6. nullopt when the table cannot be read,
//! and on platforms without a backend.
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

//! One entry of a Windows DACL: the trustee as a SID string, the access mask, whether it allows
//! (not denies), and whether it is inherit-only (it applies to what the object's children inherit,
//! not to the object itself).
struct AceFacts {
    std::string sid;
    uint32_t mask{0};
    bool allow{true};
    bool inherit_only{false};
};

//! FILE_WRITE_DATA (FILE_ADD_FILE), FILE_APPEND_DATA (FILE_ADD_SUBDIRECTORY), FILE_DELETE_CHILD,
//! DELETE, WRITE_DAC, WRITE_OWNER, GENERIC_ALL and GENERIC_WRITE: the rights that change what a file
//! or directory holds, replace an entry in a directory, or take over the access list or the owner.
constexpr uint32_t WINDOWS_WRITE_RIGHTS = 0x00000002 | 0x00000004 | 0x00000040 | 0x00010000 |
                                          0x00040000 | 0x00080000 | 0x10000000 | 0x40000000;

//! An allow ACE, not inherit-only, grants any WINDOWS_WRITE_RIGHTS bit to a SID other than
//! S-1-5-18, S-1-5-32-544 or S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464.
bool AclGrantsWriteToOthers(const std::vector<AceFacts>& aces);

//! An owner and DACL as util::winacl reads them, as PathSecurityFacts: the owner as a SID string
//! (empty when it was not read), writable by others when there is no DACL or AclGrantsWriteToOthers
//! holds for its entries, an allow entry of any kind (callback and object ones included) counted
//! as an allow.
PathSecurityFacts WindowsPathFacts(const util::winacl::ObservedAcl& acl);

//! Both strings are Windows paths ('\\' or '/' separators, ASCII case-insensitive). True when
//! listener_image equals self_image, or is gridcoinresearch.exe in self_image's directory, or in
//! its parent when that directory is named "daemon" (the NSIS layout).
bool IsInstalledCoreImage(const std::string& listener_image, const std::string& self_image);

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
//! LISTENER_LOOKUP_RETRY_DELAY apart). Found: on Linux wait up to LISTENER_ACCEPT_WAIT for the
//! listener to accept the connection (WaitForAcceptedServerSide), and refuse when it does not; gather
//! the facts, the configuration file's from what ReadConfigFiles recorded, and apply
//! DecideListenerOwner, whatever the address. Not found:
//! Accept with reason "remote" when ClassifyLookup says Remote, else Refuse. Platforms without a
//! backend (macOS, the BSDs): Accept with reason "unsupported" (today's behaviour).
ListenerCheckResult CheckRpcListener(const CService& local, const CService& remote);

static constexpr int LISTENER_LOOKUP_ATTEMPTS{5};
static constexpr std::chrono::milliseconds LISTENER_LOOKUP_RETRY_DELAY{20};
//! The core accepts a connection when one of its -rpcthreads is free, and a thread that an idle
//! client holds is freed within the server's idle timeout, 30 seconds by default.
static constexpr std::chrono::seconds LISTENER_ACCEPT_WAIT{30};
static constexpr std::chrono::milliseconds LISTENER_ACCEPT_POLL_MAX{250};

#endif // GRIDCOIN_RPC_LISTENER_CHECK_H
