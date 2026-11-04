// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/client.h>
#include <rpc/listener_check.h>
#include <netbase.h>
#include <test/state_guard.h>
#include <util/sock.h>
#include <util/string.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

// Port 15715 is 3D63; client ports 41394, 41395 and 41396 are A1B2, A1B3 and A1B4.
const std::string TCP4_HEADER =
    "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode\n";
const std::string TCP4_LINE0 =
    "   0: 0100007F:3D63 00000000:0000 0A 00000000:00000000 00:00000000 00000000  1000        0 40001 1 0000000000000000 100 0 0 10 0\n";
const std::string TCP4_LINE1 =
    "   1: 0200007F:3D63 00000000:0000 0A 00000000:00000000 00:00000000 00000000  1002        0 40002 1 0000000000000000 100 0 0 10 0\n";
const std::string TCP4_LINE2 =
    "   2: 0100007F:A1B2 0100007F:3D63 01 00000000:00000000 00:00000000 00000000  1001        0 40003 1 0000000000000000 20 4 30 10 -1\n";
const std::string TCP4_LINE3 =
    "   3: 0100007F:3D63 0100007F:A1B2 01 00000000:00000000 00:00000000 00000000  1000        0 40004 1 0000000000000000 20 4 30 10 -1\n";
const std::string TCP4 = TCP4_HEADER + TCP4_LINE0 + TCP4_LINE1 + TCP4_LINE2 + TCP4_LINE3;

const std::string TCP6_HEADER =
    "  sl  local_address                         remote_address                        st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode\n";
const std::string TCP6_LOOPBACK =
    "   0: 00000000000000000000000001000000:3D63 00000000000000000000000001000000:A1B3 01 00000000:00000000 00:00000000 00000000  1000        0 50001 1 0000000000000000 20 4 30 10 -1\n";
const std::string TCP6_MAPPED =
    "   1: 0000000000000000FFFF00000100007F:3D63 0000000000000000FFFF00000100007F:A1B4 01 00000000:00000000 00:00000000 00000000  1000        0 50002 1 0000000000000000 20 4 30 10 -1\n";
const std::string TCP6 = TCP6_HEADER + TCP6_LOOPBACK + TCP6_MAPPED;

std::vector<TcpConnEntry> Parse(const std::string& text, bool ipv6)
{
    std::istringstream in(text);
    return ParseProcNetTcp(in, ipv6);
}

//! TCP4_LINE3 with its state field replaced.
std::string Line3WithState(const std::string& state)
{
    std::string line = TCP4_LINE3;
    const size_t pos = line.find(" 01 ");
    line.replace(pos + 1, 2, state);
    return line;
}

CService Svc(const std::string& ip, unsigned short port)
{
    return LookupNumeric(ip.c_str(), port);
}

CNetAddr Addr(const std::string& ip)
{
    return LookupNumeric(ip.c_str(), 0);
}

ListenerOwnerFacts Facts(const std::string& caller, bool superuser, const std::string& listener,
                         std::optional<std::string> conf_owner, std::optional<std::string> service, bool image)
{
    ListenerOwnerFacts facts;
    facts.caller = caller;
    facts.caller_is_superuser = superuser;
    facts.listener = listener;
    facts.conf_owner = std::move(conf_owner);
    facts.service_account = std::move(service);
    facts.image_is_installed_core = image;
    return facts;
}

sockaddr_in LoopbackAddr(unsigned short port)
{
    sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(port);
    return sin;
}

//! A listener on 127.0.0.1 with a kernel-chosen port.
std::shared_ptr<Sock> MakeLoopbackListener(unsigned short& port)
{
    auto listener = std::make_shared<Sock>(CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    BOOST_REQUIRE(listener->Get() != INVALID_SOCKET);
    sockaddr_in sin = LoopbackAddr(0);
    BOOST_REQUIRE(::bind(listener->Get(), reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0);
    BOOST_REQUIRE(::listen(listener->Get(), 1) == 0);
    socklen_t len = sizeof(sin);
    BOOST_REQUIRE(::getsockname(listener->Get(), reinterpret_cast<sockaddr*>(&sin), &len) == 0);
    port = ntohs(sin.sin_port);
    return listener;
}

bool WaitReadable(const std::shared_ptr<const Sock>& sock, std::chrono::steady_clock::time_point deadline)
{
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return false;
        Sock::EventsPerSock events{{sock, Sock::Events{Sock::RECV}}};
        if (!Sock::WaitMany(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now), events)) {
            return false;
        }
        if (events[sock].occurred != 0) return true;
    }
}

//! A loopback server for one connection on its own thread. It records every byte it receives until
//! the peer closes or ten seconds pass; with reply set it answers the first request's headers with a
//! fixed JSON-RPC reply. No Boost.Test macro runs on the thread.
class OneShotServer
{
public:
    explicit OneShotServer(bool reply) : m_reply{reply}
    {
        m_listener = MakeLoopbackListener(m_port);
        m_thread = std::thread([this] { Run(); });
    }

    ~OneShotServer()
    {
        if (m_thread.joinable()) m_thread.join();
    }

    OneShotServer(const OneShotServer&) = delete;
    OneShotServer& operator=(const OneShotServer&) = delete;

    unsigned short Port() const { return m_port; }

    //! Waits for the connection to end, then returns what arrived on it.
    std::string Received()
    {
        if (m_thread.joinable()) m_thread.join();
        return m_received;
    }

private:
    void Run()
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        if (!WaitReadable(m_listener, deadline)) return;
        auto conn = std::make_shared<Sock>(AcceptCloexec(m_listener->Get(), nullptr, nullptr));
        if (conn->Get() == INVALID_SOCKET) return;

        bool replied = false;
        char buf[4096];
        while (WaitReadable(conn, deadline)) {
            const int n = conn->Recv(buf, sizeof(buf), 0);
            if (n <= 0) break;
            m_received.append(buf, n);
            if (m_reply && !replied && m_received.find("\r\n\r\n") != std::string::npos) {
                const std::string body = "{\"result\":1,\"error\":null,\"id\":1}";
                const std::string reply = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                                          ToString(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                conn->Send(reply.data(), reply.size(), MSG_NOSIGNAL);
                replied = true;
            }
        }
    }

    bool m_reply{false};
    unsigned short m_port{0};
    std::shared_ptr<Sock> m_listener;
    std::thread m_thread;
    std::string m_received;
};

//! The connection settings CallRPC reads, pointed at a test server, restored on scope exit.
struct ClientArgs {
    explicit ClientArgs(unsigned short port)
        : user("rpcuser", "listener_check_user"),
          password("rpcpassword", "listener_check_password"),
          connect("rpcconnect", "127.0.0.1"),
          rpcport("rpcport", ToString(port))
    {
    }
    grc_test::ForcedArgGuard user;
    grc_test::ForcedArgGuard password;
    grc_test::ForcedArgGuard connect;
    grc_test::ForcedArgGuard rpcport;
};

} // namespace

BOOST_AUTO_TEST_SUITE(rpc_listener_check_tests, *boost::unit_test::fixture<grc_test::StateGuard>())

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

BOOST_AUTO_TEST_CASE(parse_tcp4)
{
    const std::vector<TcpConnEntry> entries = Parse(TCP4, false);
    BOOST_REQUIRE_EQUAL(entries.size(), 4U);

    BOOST_CHECK(entries[0].state == TcpState::Other);
    BOOST_CHECK(entries[0].local == Svc("127.0.0.1", 15715));
    BOOST_CHECK_EQUAL(entries[0].owner, 1000U);

    BOOST_CHECK(entries[1].local == Svc("127.0.0.2", 15715));
    BOOST_CHECK_EQUAL(entries[1].owner, 1002U);

    BOOST_CHECK(entries[2].state == TcpState::Established);
    BOOST_CHECK(entries[2].local == Svc("127.0.0.1", 41394));
    BOOST_CHECK_EQUAL(entries[2].owner, 1001U);

    BOOST_CHECK_EQUAL(entries[3].owner, 1000U);
    BOOST_CHECK(entries[3].state == TcpState::Established);
    BOOST_CHECK(entries[3].local == Svc("127.0.0.1", 15715));
    BOOST_CHECK(entries[3].remote == Svc("127.0.0.1", 41394));
}

BOOST_AUTO_TEST_CASE(parse_skips_malformed)
{
    const std::string truncated = "   4: 0100007F:3D63 0100007F:A1B5 01 00000000:00000000\n";
    const std::string bad_hex =
        "   5: ZZZZZZZZ:3D63 0100007F:A1B6 01 00000000:00000000 00:00000000 00000000  1000        0 40005 1 0000000000000000 20 4 30 10 -1\n";
    const std::vector<TcpConnEntry> entries = Parse(TCP4_HEADER + TCP4_LINE0 + truncated + bad_hex + TCP4_LINE3, false);
    BOOST_REQUIRE_EQUAL(entries.size(), 2U);
    BOOST_CHECK(entries[0].state == TcpState::Other);
    BOOST_CHECK(entries[1].remote == Svc("127.0.0.1", 41394));
}

BOOST_AUTO_TEST_CASE(parse_tcp6_loopback_and_mapped)
{
    const std::vector<TcpConnEntry> entries = Parse(TCP6, true);
    BOOST_REQUIRE_EQUAL(entries.size(), 2U);

    BOOST_CHECK(entries[0].local == Svc("::1", 15715));
    BOOST_CHECK(entries[0].remote == Svc("::1", 41395));
    BOOST_CHECK(entries[0].state == TcpState::Established);
    BOOST_CHECK_EQUAL(entries[0].owner, 1000U);

    BOOST_CHECK(entries[1].local == CService(LookupNumeric("127.0.0.1", 15715)));
    BOOST_CHECK(entries[1].remote == Svc("127.0.0.1", 41396));
    BOOST_CHECK(entries[1].state == TcpState::Established);
    BOOST_CHECK_EQUAL(entries[1].owner, 1000U);

    // An IPv4 table is not read as IPv6, or the reverse.
    BOOST_CHECK(Parse(TCP6, false).empty());
    BOOST_CHECK(Parse(TCP4, true).empty());
}

BOOST_AUTO_TEST_CASE(find_exact_tuple)
{
    // Not 1001 (our own direction of the connection), not 1002 (the 127.0.0.2 listener).
    const std::optional<TcpConnEntry> found =
        FindServerSide(Parse(TCP4, false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715));
    BOOST_REQUIRE(found);
    BOOST_CHECK_EQUAL(found->owner, 1000U);
}

BOOST_AUTO_TEST_CASE(find_absent)
{
    const std::string without_line3 = TCP4_HEADER + TCP4_LINE0 + TCP4_LINE1 + TCP4_LINE2;
    BOOST_CHECK(!FindServerSide(Parse(without_line3, false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715)));
}

BOOST_AUTO_TEST_CASE(find_syn_recv)
{
    const std::string text = TCP4_HEADER + TCP4_LINE0 + TCP4_LINE1 + TCP4_LINE2 + Line3WithState("03");
    const std::optional<TcpConnEntry> found =
        FindServerSide(Parse(text, false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715));
    BOOST_REQUIRE(found);
    BOOST_CHECK(found->state == TcpState::SynReceived);
    BOOST_CHECK_EQUAL(found->owner, 1000U);
}

BOOST_AUTO_TEST_CASE(find_ignores_time_wait)
{
    const std::string text = TCP4_HEADER + TCP4_LINE0 + TCP4_LINE1 + TCP4_LINE2 + Line3WithState("06");
    BOOST_CHECK(!FindServerSide(Parse(text, false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715)));
}

BOOST_AUTO_TEST_CASE(find_not_unique)
{
    BOOST_CHECK(!FindServerSide(Parse(TCP4 + TCP4_LINE3, false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715)));
}

BOOST_AUTO_TEST_CASE(find_v4_client_mapped_server)
{
    std::vector<TcpConnEntry> entries = Parse(TCP4_HEADER + TCP4_LINE0 + TCP4_LINE1 + TCP4_LINE2, false);
    const std::vector<TcpConnEntry> mapped = Parse(TCP6_HEADER + TCP6_MAPPED, true);
    entries.insert(entries.end(), mapped.begin(), mapped.end());

    const std::optional<TcpConnEntry> found =
        FindServerSide(entries, Svc("127.0.0.1", 41396), Svc("127.0.0.1", 15715));
    BOOST_REQUIRE(found);
    BOOST_CHECK_EQUAL(found->owner, 1000U);
}

#endif // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

BOOST_AUTO_TEST_CASE(local_address_classification)
{
    const std::vector<CNetAddr> none;
    for (const char* ip : {"127.0.0.1", "127.0.1.1", "0.0.0.0", "::1", "::", "::ffff:127.0.0.1"}) {
        BOOST_CHECK_MESSAGE(IsLocalPeerAddress(Addr(ip), none), ip);
    }

    BOOST_CHECK(IsLocalPeerAddress(Addr("192.168.1.10"), {Addr("192.168.1.10")}));
    BOOST_CHECK(IsLocalPeerAddress(Addr("fe80::1"), {Addr("fe80::1")}));

    BOOST_CHECK(!IsLocalPeerAddress(Addr("192.168.1.11"), {Addr("192.168.1.10")}));
    BOOST_CHECK(!IsLocalPeerAddress(Addr("8.8.8.8"), {Addr("192.168.1.10")}));
    BOOST_CHECK(!IsLocalPeerAddress(Addr("8.8.8.8"), none));

#ifdef __linux__
    // Other spellings of 127.0.0.1 that the resolver accepts. The classification is made on the
    // address the client resolved and connected to, so each counts as local.
    for (const char* spelling : {"127.1", "2130706433", "0x7f000001"}) {
        CNetAddr resolved;
        BOOST_REQUIRE_MESSAGE(LookupHost(spelling, resolved, false), spelling);
        BOOST_CHECK_MESSAGE(IsLocalPeerAddress(resolved, none), spelling);
    }
#endif
}

BOOST_AUTO_TEST_CASE(host_interface_addresses_include_loopback)
{
#ifndef WIN32
    const std::vector<CNetAddr> addrs = GetHostInterfaceAddresses();
    BOOST_CHECK(std::find(addrs.begin(), addrs.end(), Addr("127.0.0.1")) != addrs.end());
#endif
}

BOOST_AUTO_TEST_CASE(lookup_outcome)
{
    const std::vector<CNetAddr> host{Addr("192.168.1.10")};
    // What GetHostInterfaceAddresses returns when getifaddrs fails.
    const std::vector<CNetAddr> unknown;

    // A row for the connection proves the listener is on this host, whatever the address: a locally
    // routed address that is on no interface, or interfaces that could not be read.
    BOOST_CHECK(ClassifyLookup(true, Addr("203.0.113.5"), host) == LookupOutcome::CheckOwner);
    BOOST_CHECK(ClassifyLookup(true, Addr("192.168.1.10"), unknown) == LookupOutcome::CheckOwner);
    BOOST_CHECK(ClassifyLookup(true, Addr("127.0.0.1"), host) == LookupOutcome::CheckOwner);

    // No row.
    BOOST_CHECK(ClassifyLookup(false, Addr("8.8.8.8"), host) == LookupOutcome::Remote);
    BOOST_CHECK(ClassifyLookup(false, Addr("127.0.0.1"), host) == LookupOutcome::Unidentified);
    BOOST_CHECK(ClassifyLookup(false, Addr("192.168.1.10"), host) == LookupOutcome::Unidentified);

    // No row and getifaddrs failed: this host's own address must not pass as remote, and with the
    // interfaces unknown no address can be told to be another host's.
    BOOST_CHECK(ClassifyLookup(false, Addr("192.168.1.10"), unknown) == LookupOutcome::Unidentified);
    BOOST_CHECK(ClassifyLookup(false, Addr("8.8.8.8"), unknown) == LookupOutcome::Unidentified);
    BOOST_CHECK(ClassifyLookup(false, Addr("127.0.0.1"), unknown) == LookupOutcome::Unidentified);
}

BOOST_AUTO_TEST_CASE(posix_path_facts)
{
    const PathSecurityFacts root_file = PosixPathFacts(0, 0100644);
    BOOST_CHECK_EQUAL(root_file.owner, "0");
    BOOST_CHECK(!root_file.writable_by_others);
    BOOST_CHECK(!PosixPathFacts(0, 040755).writable_by_others);
    BOOST_CHECK(!PosixPathFacts(0, 0100640).writable_by_others);

    // Group or world write.
    BOOST_CHECK(PosixPathFacts(0, 0100664).writable_by_others);
    BOOST_CHECK(PosixPathFacts(0, 0100646).writable_by_others);
    BOOST_CHECK(PosixPathFacts(0, 041777).writable_by_others);

    // An owner other than root can write it.
    const PathSecurityFacts user_file = PosixPathFacts(1000, 0100600);
    BOOST_CHECK_EQUAL(user_file.owner, "1000");
    BOOST_CHECK(user_file.writable_by_others);
}

BOOST_AUTO_TEST_CASE(owner_policy_table)
{
    const std::optional<std::string> none;
    struct Row {
        ListenerOwnerFacts facts;
        ListenerVerdict verdict;
        std::string reason_contains;
    };
    const std::vector<Row> rows{
        {Facts("1000", false, "1000", "1000", none, true), ListenerVerdict::Accept, ""},
        {Facts("1000", false, "1002", "1000", none, true), ListenerVerdict::Refuse, "1002"},
        {Facts("0", true, "1000", "1000", none, true), ListenerVerdict::Accept, ""},
        {Facts("0", true, "1002", "1000", none, true), ListenerVerdict::Refuse, "1002"},
        // The conf-owner rule is for root and SYSTEM only.
        {Facts("1000", false, "1002", "1002", none, true), ListenerVerdict::Refuse, "1002"},
        {Facts("0", true, "998", "0", "998", true), ListenerVerdict::Accept, ""},
        {Facts("1000", false, "998", "0", "998", true), ListenerVerdict::Accept, ""},
        // The SYSTEM shutdown script against the user's core.
        {Facts("S-1-5-18", true, "S-1-5-21-1-2-3-1001", "S-1-5-21-1-2-3-1001", none, true), ListenerVerdict::Accept, ""},
        {Facts("S-1-5-18", true, "S-1-5-21-1-2-3-1001", "S-1-5-21-1-2-3-1001", none, false), ListenerVerdict::Refuse,
         "not the installed Gridcoin core"},
        // A forwarder run by the same user.
        {Facts("S-1-5-21-1-2-3-1001", false, "S-1-5-21-1-2-3-1001", "S-1-5-21-1-2-3-1001", none, false),
         ListenerVerdict::Refuse, "not the installed Gridcoin core"},
        // An owner that could not be named is never a match, even for an equally empty caller.
        {Facts("", false, "", none, none, true), ListenerVerdict::Refuse, ""},
        {Facts("1000", false, "", none, "", true), ListenerVerdict::Refuse, ""},
    };

    for (size_t i = 0; i < rows.size(); ++i) {
        const ListenerCheckResult result = DecideListenerOwner(rows[i].facts);
        BOOST_CHECK_MESSAGE(result.verdict == rows[i].verdict, "row " << i + 1);
        BOOST_CHECK_MESSAGE(result.reason.find(rows[i].reason_contains) != std::string::npos,
                            "row " << i + 1 << ": " << result.reason);
    }
}

BOOST_AUTO_TEST_CASE(refusal_messages)
{
    const ListenerCheckResult refused = DecideListenerOwner(Facts("1000", false, "1002", "1000", std::nullopt, true));
    BOOST_CHECK_EQUAL(OwnerRefusalMessage("127.0.0.1:15715", refused.reason),
                      "refusing to send RPC credentials: the listener at 127.0.0.1:15715 is owned by 1002, not by 1000. "
                      "Pass -rpcskiplistenercheck to send anyway (not recommended: what the client sends then goes to "
                      "whatever program holds that address).");

    // Root names the other owners it would have accepted.
    const ListenerCheckResult root_refused = DecideListenerOwner(Facts("0", true, "1002", "1000", "998", true));
    BOOST_CHECK_EQUAL(root_refused.reason,
                      "is owned by 1002, not by 0 or by 1000 (the configuration file's owner) or by 998 (-rpcserviceuser)");

    BOOST_CHECK_EQUAL(UnidentifiedListenerMessage("[::1]:15715"),
                      "refusing to send RPC credentials: could not identify the owner of the listener at [::1]:15715.");
}

BOOST_AUTO_TEST_CASE(conf_trust_predicate)
{
    const PathSecurityFacts root{"0", false};
    BOOST_CHECK(ConfTrustedForServiceAccount(root, root, false));
    BOOST_CHECK(!ConfTrustedForServiceAccount({"1000", false}, root, false));
    BOOST_CHECK(!ConfTrustedForServiceAccount({"0", true}, root, false));
    BOOST_CHECK(!ConfTrustedForServiceAccount(root, {"1000", false}, false));
    BOOST_CHECK(!ConfTrustedForServiceAccount(root, {"0", true}, false));
    BOOST_CHECK(!ConfTrustedForServiceAccount(root, root, true));
    BOOST_CHECK(ConfTrustedForServiceAccount({"S-1-5-32-544", false}, {"S-1-5-18", false}, false));
}

#ifdef __linux__

BOOST_AUTO_TEST_CASE(live_loopback_lookup_own_uid)
{
    unsigned short port{0};
    const std::shared_ptr<Sock> listener = MakeLoopbackListener(port);

    // Connected and left in the accept queue: the server side exists before anyone accepts it.
    Sock client(CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    BOOST_REQUIRE(client.Get() != INVALID_SOCKET);
    sockaddr_in server_addr = LoopbackAddr(port);
    BOOST_REQUIRE(::connect(client.Get(), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == 0);
    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    BOOST_REQUIRE(::getsockname(client.Get(), reinterpret_cast<sockaddr*>(&client_addr), &len) == 0);

    CService client_local;
    CService client_remote;
    BOOST_REQUIRE(client_local.SetSockAddr(reinterpret_cast<sockaddr*>(&client_addr)));
    BOOST_REQUIRE(client_remote.SetSockAddr(reinterpret_cast<sockaddr*>(&server_addr)));

    const std::optional<std::vector<TcpConnEntry>> tables = ReadTcpConnectionTables();
    BOOST_REQUIRE(tables);
    const std::optional<TcpConnEntry> found = FindServerSide(*tables, client_local, client_remote);
    BOOST_REQUIRE(found);
    BOOST_CHECK_EQUAL(found->owner, static_cast<uint64_t>(::geteuid()));

    // The production check end to end: our own listener is accepted.
    const ListenerCheckResult result = CheckRpcListener(client_local, client_remote);
    BOOST_CHECK_MESSAGE(result.verdict == ListenerVerdict::Accept, result.reason);
}

BOOST_AUTO_TEST_CASE(live_absent_tuple_refused)
{
    // No connection has this tuple, so there is no server side to identify.
    const ListenerCheckResult result = CheckRpcListener(Svc("127.0.0.1", 1), Svc("127.0.0.1", 2));
    BOOST_CHECK(result.verdict == ListenerVerdict::Refuse);
    BOOST_CHECK_EQUAL(result.reason, UnidentifiedListenerMessage("127.0.0.1:2"));
}

BOOST_AUTO_TEST_CASE(remote_peer_is_not_checked)
{
    // 192.0.2.1 is a documentation address (RFC 5737) that no host interface carries.
    const ListenerCheckResult result = CheckRpcListener(Svc("192.0.2.2", 41394), Svc("192.0.2.1", 15715));
    BOOST_CHECK(result.verdict == ListenerVerdict::Accept);
    BOOST_CHECK_EQUAL(result.reason, "remote");
}

#endif // __linux__

BOOST_AUTO_TEST_CASE(callrpc_sends_nothing_when_refused)
{
    OneShotServer server(/*reply=*/false);
    ClientArgs args(server.Port());

    // The request would carry a passphrase in the elevation header; a refused listener gets none of it.
    const SecureString elevation{"test passphrase"};

    std::string error;
    try {
        CallRPC("getblockcount", UniValue(UniValue::VARR), {[](auto&, auto&) {
                    return ListenerCheckResult{ListenerVerdict::Refuse, "test refusal"};
                }, &elevation});
    } catch (const std::runtime_error& e) {
        error = e.what();
    }
    BOOST_CHECK_MESSAGE(error.find("test refusal") != std::string::npos, error);
    BOOST_CHECK_EQUAL(server.Received().size(), 0U);
}

BOOST_AUTO_TEST_CASE(callrpc_sends_after_accept)
{
    OneShotServer server(/*reply=*/true);
    ClientArgs args(server.Port());

    CService seen_local;
    CService seen_remote;
    const auto accept = [&](const CService& local, const CService& remote) {
        seen_local = local;
        seen_remote = remote;
        return ListenerCheckResult{ListenerVerdict::Accept, "test accept"};
    };
    const UniValue reply = CallRPC("getblockcount", UniValue(UniValue::VARR), {accept});
    BOOST_CHECK_EQUAL(find_value(reply, "result").get_int(), 1);
    BOOST_CHECK_EQUAL(server.Received().rfind("POST / HTTP/1.1", 0), 0U);

    // The check saw this connection: the server's address, and a loopback address of our own.
    BOOST_CHECK(seen_remote == Svc("127.0.0.1", server.Port()));
    BOOST_CHECK(seen_local.IsLocal());
    BOOST_CHECK(seen_local.GetPort() != 0);
}

BOOST_AUTO_TEST_CASE(callrpc_skip_flag_bypasses_check)
{
    OneShotServer server(/*reply=*/true);
    ClientArgs args(server.Port());
    grc_test::ForcedArgGuard skip("rpcskiplistenercheck", "1");

    bool called = false;
    const auto refuse = [&](auto&, auto&) {
        called = true;
        return ListenerCheckResult{ListenerVerdict::Refuse, "test refusal"};
    };
    const UniValue reply = CallRPC("getblockcount", UniValue(UniValue::VARR), {refuse});
    BOOST_CHECK(!called);
    BOOST_CHECK_EQUAL(find_value(reply, "result").get_int(), 1);
    BOOST_CHECK_EQUAL(server.Received().rfind("POST / HTTP/1.1", 0), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
