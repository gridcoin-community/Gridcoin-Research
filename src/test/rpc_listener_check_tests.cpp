// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/client.h>
#include <rpc/listener_check.h>
#include <netbase.h>
#include <test/state_guard.h>
#include <test/wine.h>
#include <util/sock.h>
#include <util/string.h>
#include <util/time.h>

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
// The server side of TCP4_LINE3's connection before a process accepts it, as kernels up to 6.16 list
// it: uid 0 and no inode.
const std::string TCP4_LINE3_UNACCEPTED =
    "   3: 0100007F:3D63 0100007F:A1B2 01 00000000:00000000 00:00000000 00000000     0        0 0 1 0000000000000000 20 4 30 10 -1\n";

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
    BOOST_CHECK_EQUAL(entries[3].inode, 40004U);
    BOOST_CHECK(entries[3].state == TcpState::Established);
    BOOST_CHECK(entries[3].local == Svc("127.0.0.1", 15715));
    BOOST_CHECK(entries[3].remote == Svc("127.0.0.1", 41394));
}

BOOST_AUTO_TEST_CASE(parse_skips_malformed)
{
    const std::string truncated = "   4: 0100007F:3D63 0100007F:A1B5 01 00000000:00000000\n";
    const std::string bad_hex =
        "   5: ZZZZZZZZ:3D63 0100007F:A1B6 01 00000000:00000000 00:00000000 00000000  1000        0 40005 1 0000000000000000 20 4 30 10 -1\n";
    const std::string no_inode = "   6: 0100007F:3D63 0100007F:A1B7 01 00000000:00000000 00:00000000 00000000  1000        0\n";
    const std::vector<TcpConnEntry> entries =
        Parse(TCP4_HEADER + TCP4_LINE0 + truncated + bad_hex + no_inode + TCP4_LINE3, false);
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

BOOST_AUTO_TEST_CASE(server_side_accepted_needs_an_inode)
{
    // Accepted: established, with the inode of the socket the accepting process holds.
    const std::optional<TcpConnEntry> accepted =
        FindServerSide(Parse(TCP4, false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715));
    BOOST_REQUIRE(accepted);
    BOOST_CHECK(ServerSideAccepted(*accepted));

    // Still in the accept queue: no inode, and a uid that names no process holding the connection.
    const std::string waiting = TCP4_HEADER + TCP4_LINE0 + TCP4_LINE1 + TCP4_LINE2 + TCP4_LINE3_UNACCEPTED;
    const std::optional<TcpConnEntry> queued =
        FindServerSide(Parse(waiting, false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715));
    BOOST_REQUIRE(queued);
    BOOST_CHECK_EQUAL(queued->owner, 0U);
    BOOST_CHECK_EQUAL(queued->inode, 0U);
    BOOST_CHECK(!ServerSideAccepted(*queued));

    // A handshake the listener has not completed is not accepted, whatever the row's inode.
    const std::optional<TcpConnEntry> half_open =
        FindServerSide(Parse(TCP4_HEADER + Line3WithState("03"), false), Svc("127.0.0.1", 41394), Svc("127.0.0.1", 15715));
    BOOST_REQUIRE(half_open);
    BOOST_CHECK(!ServerSideAccepted(*half_open));
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

BOOST_AUTO_TEST_CASE(windows_acl_write_detection)
{
    const std::string trusted_installer{"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464"};
    const std::string users{"S-1-5-32-545"};

    BOOST_CHECK(!AclGrantsWriteToOthers({{"S-1-5-18", 0x10000000}}));
    BOOST_CHECK(AclGrantsWriteToOthers({{users, 0x2}}));
    BOOST_CHECK(!AclGrantsWriteToOthers({{"S-1-5-11", 0x001200A9}}));
    // An inherit-only entry is what the children receive, not a right on the object.
    BOOST_CHECK(!AclGrantsWriteToOthers({{users, 0x2, true, true}}));
    // A deny entry grants nothing.
    BOOST_CHECK(!AclGrantsWriteToOthers({{users, 0x2, false}}));
    BOOST_CHECK(!AclGrantsWriteToOthers({{trusted_installer, 0x10000000}}));

    // Each write right alone, and none of the others.
    for (const uint32_t right : {0x2U, 0x4U, 0x40U, 0x10000U, 0x40000U, 0x80000U, 0x10000000U, 0x40000000U}) {
        BOOST_CHECK_MESSAGE(AclGrantsWriteToOthers({{users, right}}), right);
    }
    for (const uint32_t right : {0x1U, 0x8U, 0x10U, 0x20U, 0x80U, 0x100U, 0x20000U, 0x100000U, 0x20000000U, 0x80000000U}) {
        BOOST_CHECK_MESSAGE(!AclGrantsWriteToOthers({{users, right}}), right);
    }

    // The service's configuration file grants Administrators and SYSTEM full control and the service read.
    const std::vector<AceFacts> service_conf{{"S-1-5-32-544", 0x001F01FF},
                                             {"S-1-5-18", 0x001F01FF},
                                             {"S-1-5-80-1-2-3-4-5", 0x00120089}};
    BOOST_CHECK(!AclGrantsWriteToOthers(service_conf));
    std::vector<AceFacts> with_user = service_conf;
    with_user.push_back({users, 0x00120116});
    BOOST_CHECK(AclGrantsWriteToOthers(with_user));
    BOOST_CHECK(!AclGrantsWriteToOthers({}));
}

BOOST_AUTO_TEST_CASE(windows_path_facts)
{
    using namespace util::winacl;
    const Sid admins = wellknown::Administrators();
    const Sid system = wellknown::System();
    const Sid users = wellknown::Users();
    const std::optional<Sid> service = ParseSidString("S-1-5-80-1-2-3-4-5");
    BOOST_REQUIRE(service);
    const auto entry = [](AceKind kind, const Sid& trustee, uint32_t mask) {
        ObservedAce ace;
        ace.kind = kind;
        ace.trustee = trustee;
        ace.mask = mask;
        return ace;
    };

    // The service's configuration file, as the Windows service setup makes it.
    ObservedAcl conf;
    conf.owner = admins;
    conf.dacl_present = true;
    conf.is_protected = true;
    conf.aces = {entry(AceKind::Allow, admins, 0x001F01FF), entry(AceKind::Allow, system, 0x001F01FF),
                 entry(AceKind::Allow, *service, 0x00120089)};
    const PathSecurityFacts conf_facts = WindowsPathFacts(conf);
    BOOST_CHECK_EQUAL(conf_facts.owner, "S-1-5-32-544");
    BOOST_CHECK(!conf_facts.writable_by_others);
    BOOST_CHECK(ConfTrustedForServiceAccount(conf_facts, conf_facts, false));

    // No DACL, or a NULL one, gives everyone full access.
    ObservedAcl no_dacl = conf;
    no_dacl.dacl_present = false;
    no_dacl.aces.clear();
    BOOST_CHECK(WindowsPathFacts(no_dacl).writable_by_others);

    ObservedAcl user_write = conf;
    user_write.aces.push_back(entry(AceKind::Allow, users, 0x2));
    BOOST_CHECK(WindowsPathFacts(user_write).writable_by_others);

    // A callback or object allow entry grants what it names as well.
    ObservedAcl other_allow = conf;
    other_allow.aces.push_back(entry(AceKind::OtherAllow, users, 0x2));
    BOOST_CHECK(WindowsPathFacts(other_allow).writable_by_others);

    // A deny entry, and an entry that is neither an allow nor a deny, grant nothing.
    ObservedAcl deny = conf;
    deny.aces.push_back(entry(AceKind::Deny, users, 0x2));
    deny.aces.push_back(entry(AceKind::Other, users, 0x2));
    BOOST_CHECK(!WindowsPathFacts(deny).writable_by_others);

    // CREATOR OWNER's inherit-only entry on a directory is for what is created in it.
    ObservedAcl inherit_only = conf;
    inherit_only.aces.push_back(entry(AceKind::Allow, *ParseSidString("S-1-3-0"), 0x10000000));
    inherit_only.aces.back().inherit_only = true;
    BOOST_CHECK(!WindowsPathFacts(inherit_only).writable_by_others);

    // An entry whose SID could not be read is nobody trusted.
    ObservedAcl no_sid = conf;
    no_sid.aces.push_back(entry(AceKind::Allow, Sid{}, 0x2));
    BOOST_CHECK(WindowsPathFacts(no_sid).writable_by_others);

    // An owner that could not be read is no owner the configuration file may have.
    ObservedAcl no_owner = conf;
    no_owner.owner.reset();
    const PathSecurityFacts no_owner_facts = WindowsPathFacts(no_owner);
    BOOST_CHECK_EQUAL(no_owner_facts.owner, "");
    BOOST_CHECK(!ConfTrustedForServiceAccount(no_owner_facts, conf_facts, false));

    // A file a user owns.
    ObservedAcl user_owned = conf;
    user_owned.owner = *ParseSidString("S-1-5-21-1-2-3-1001");
    const PathSecurityFacts user_facts = WindowsPathFacts(user_owned);
    BOOST_CHECK_EQUAL(user_facts.owner, "S-1-5-21-1-2-3-1001");
    BOOST_CHECK(!ConfTrustedForServiceAccount(user_facts, conf_facts, false));
}

BOOST_AUTO_TEST_CASE(installed_core_image_match)
{
    const std::string installed{R"(C:\Program Files\GridcoinResearch\daemon\gridcoinresearchd.exe)"};
    BOOST_CHECK(IsInstalledCoreImage(R"(c:\program files\gridcoinresearch\DAEMON\GridcoinResearchD.exe)", installed));
    BOOST_CHECK(IsInstalledCoreImage(R"(C:\Program Files\GridcoinResearch\gridcoinresearch.exe)", installed));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\Users\a\AppData\Local\Temp\gridcoinresearchd.exe)", installed));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\Windows\System32\OpenSSH\ssh.exe)", installed));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\Program Files\WSL\wslrelay.exe)", installed));

    const std::string build{R"(C:\src\build\bin\gridcoinresearchd.exe)"};
    BOOST_CHECK(IsInstalledCoreImage(R"(C:\src\build\bin\gridcoinresearch.exe)", build));

    // Either separator, on either side.
    BOOST_CHECK(IsInstalledCoreImage("C:/Program Files/GridcoinResearch/daemon/gridcoinresearchd.exe", installed));
    BOOST_CHECK(IsInstalledCoreImage(R"(C:\src\build\bin\gridcoinresearch.exe)", "C:/src/build/bin/gridcoinresearchd.exe"));

    // The GUI beside the daemon in daemon\ too.
    BOOST_CHECK(IsInstalledCoreImage(R"(C:\Program Files\GridcoinResearch\daemon\gridcoinresearch.exe)", installed));

    // Only the GUI one level up, and only from a directory named daemon.
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\Program Files\GridcoinResearch\gridcoinresearchd.exe)", installed));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\src\build\gridcoinresearch.exe)", build));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\a\gridcoinresearch.exe)", R"(C:\a\xdaemon\gridcoinresearchd.exe)"));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\gridcoinresearch.exe)", R"(C:\Program Files\GridcoinResearch\daemon\gridcoinresearchd.exe)"));

    // Whole names only.
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\Program Files\GridcoinResearch\daemon\gridcoinresearchd.exe.bak)", installed));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\Program Files\GridcoinResearch\daemon\xgridcoinresearch.exe)", installed));
    BOOST_CHECK(!IsInstalledCoreImage(R"(C:\Program Files\GridcoinResearch\daemon\gridcoinresearch.exe\x)", installed));

    // Case folds for ASCII letters only.
    BOOST_CHECK(!IsInstalledCoreImage("C:\\\xC3\x84\\gridcoinresearchd.exe", "C:\\\xC3\xA4\\gridcoinresearchd.exe"));

    // An image that could not be read is never the core.
    BOOST_CHECK(!IsInstalledCoreImage("", installed));
    BOOST_CHECK(!IsInstalledCoreImage("", ""));
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

// A process created after a listening socket, by more than the slack, did not make it, whatever pid
// the table names. Production change that would fail this: a decision that takes the table's pid
// as the maker of the socket whatever the age of the process that has it now.
BOOST_AUTO_TEST_CASE(process_younger_than_the_listener_did_not_make_it)
{
    constexpr uint64_t SECOND{10'000'000};
    const uint64_t listener = 134'000'000'000'000'000ULL;
    const uint64_t slack = static_cast<uint64_t>(LISTENER_CREATION_SLACK.count()) * SECOND;
    BOOST_CHECK(ProcessPredatesListener(listener - 3600 * SECOND, listener));
    BOOST_CHECK(ProcessPredatesListener(listener, listener));
    BOOST_CHECK(ProcessPredatesListener(listener + slack, listener));
    BOOST_CHECK(!ProcessPredatesListener(listener + slack + 1, listener));
    BOOST_CHECK(!ProcessPredatesListener(listener + 3600 * SECOND, listener));
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

// The platforms with a backend.
#if defined(__linux__) || defined(WIN32)

namespace {

//! Connects client to the loopback listener on port, and gives the two ends of the connection as the
//! client sees them.
void ConnectLoopback(const Sock& client, unsigned short port, CService& client_local, CService& client_remote)
{
    BOOST_REQUIRE(client.Get() != INVALID_SOCKET);
    sockaddr_in server_addr = LoopbackAddr(port);
    BOOST_REQUIRE(::connect(client.Get(), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == 0);
    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    BOOST_REQUIRE(::getsockname(client.Get(), reinterpret_cast<sockaddr*>(&client_addr), &len) == 0);
    BOOST_REQUIRE(client_local.SetSockAddr(reinterpret_cast<sockaddr*>(&client_addr)));
    BOOST_REQUIRE(client_remote.SetSockAddr(reinterpret_cast<sockaddr*>(&server_addr)));
}

} // namespace

BOOST_AUTO_TEST_CASE(live_loopback_lookup_own_uid)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine's TCP table names no owning process; skipping");
        return;
    }
    unsigned short port{0};
    const std::shared_ptr<Sock> listener = MakeLoopbackListener(port);

    // Connected and accepted, as the core accepts each connection.
    Sock client(CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    CService client_local;
    CService client_remote;
    ConnectLoopback(client, port, client_local, client_remote);
    Sock accepted(AcceptCloexec(listener->Get(), nullptr, nullptr));
    BOOST_REQUIRE(accepted.Get() != INVALID_SOCKET);

    const std::optional<std::vector<TcpConnEntry>> tables = ReadTcpConnectionTables();
    BOOST_REQUIRE(tables);
    const std::optional<TcpConnEntry> found = FindServerSide(*tables, client_local, client_remote);
    BOOST_REQUIRE(found);
#ifdef WIN32
    // The Windows table gives the process that holds the connection.
    BOOST_CHECK_EQUAL(found->owner, static_cast<uint64_t>(::GetCurrentProcessId()));
#else
    BOOST_CHECK(ServerSideAccepted(*found));
    BOOST_CHECK_EQUAL(found->owner, static_cast<uint64_t>(::geteuid()));
#endif

    // The production check end to end: our own listener is accepted.
    const ListenerCheckResult result = CheckRpcListener(client_local, client_remote);
    BOOST_CHECK_MESSAGE(result.verdict == ListenerVerdict::Accept, result.reason);
}

#ifdef WIN32

// The table's creation time of a listening socket is when this process made it, and this process
// is older: the age check accepts the process that made the socket, as the end-to-end case above
// shows for the whole check.
BOOST_AUTO_TEST_CASE(live_listener_creation_time_is_when_it_was_made)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine's TCP table names no owning process; skipping");
        return;
    }
    const auto now = [] {
        FILETIME time{};
        ::GetSystemTimeAsFileTime(&time);
        return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    const uint64_t before = now();
    unsigned short port{0};
    const std::shared_ptr<Sock> listener = MakeLoopbackListener(port);
    const uint64_t after = now();

    Sock client(CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    CService client_local;
    CService client_remote;
    ConnectLoopback(client, port, client_local, client_remote);
    const std::optional<std::vector<TcpConnEntry>> tables = ReadTcpConnectionTables();
    BOOST_REQUIRE(tables);
    const std::optional<TcpConnEntry> found = FindServerSide(*tables, client_local, client_remote);
    BOOST_REQUIRE(found);

    const std::optional<uint64_t> created = ListenerCreationTime(*found);
    BOOST_REQUIRE(created);
    BOOST_TEST_MESSAGE("the table's creation time is " << static_cast<int64_t>(*created - before)
                       << " x 100 ns after the listener was begun; " << (*created % 10'000'000)
                       << " x 100 ns past its second");
    const uint64_t slack = static_cast<uint64_t>(LISTENER_CREATION_SLACK.count()) * 10'000'000;
    BOOST_CHECK(*created + slack >= before);
    BOOST_CHECK(*created <= after + slack);

    FILETIME process_created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    BOOST_REQUIRE(::GetProcessTimes(::GetCurrentProcess(), &process_created, &exited, &kernel, &user));
    const uint64_t self =
        (static_cast<uint64_t>(process_created.dwHighDateTime) << 32) | process_created.dwLowDateTime;
    BOOST_CHECK(ProcessPredatesListener(self, *created));

    // An IPv6 socket on every address that takes IPv4 too: for its IPv4 connection, too, the
    // listening socket is found.
    Sock dual(CreateCloexecSocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP));
    BOOST_REQUIRE(dual.Get() != INVALID_SOCKET);
    DWORD v6only = 0;
    BOOST_REQUIRE(::setsockopt(dual.Get(), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&v6only),
                               sizeof(v6only)) == 0);
    sockaddr_in6 any6{};
    any6.sin6_family = AF_INET6;
    BOOST_REQUIRE(::bind(dual.Get(), reinterpret_cast<sockaddr*>(&any6), sizeof(any6)) == 0);
    BOOST_REQUIRE(::listen(dual.Get(), 1) == 0);
    socklen_t len = sizeof(any6);
    BOOST_REQUIRE(::getsockname(dual.Get(), reinterpret_cast<sockaddr*>(&any6), &len) == 0);
    Sock client4(CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    CService client4_local;
    CService client4_remote;
    ConnectLoopback(client4, ntohs(any6.sin6_port), client4_local, client4_remote);
    const std::optional<std::vector<TcpConnEntry>> tables4 = ReadTcpConnectionTables();
    BOOST_REQUIRE(tables4);
    const std::optional<TcpConnEntry> found4 = FindServerSide(*tables4, client4_local, client4_remote);
    BOOST_REQUIRE(found4);
    BOOST_CHECK(ListenerCreationTime(*found4));
}

#endif // WIN32

#ifdef __linux__

BOOST_AUTO_TEST_CASE(live_owner_read_once_the_connection_is_accepted)
{
    unsigned short port{0};
    const std::shared_ptr<Sock> listener = MakeLoopbackListener(port);
    Sock client(CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    CService client_local;
    CService client_remote;
    ConnectLoopback(client, port, client_local, client_remote);

    const std::optional<std::vector<TcpConnEntry>> tables = ReadTcpConnectionTables();
    BOOST_REQUIRE(tables);
    const std::optional<TcpConnEntry> queued = FindServerSide(*tables, client_local, client_remote);
    BOOST_REQUIRE(queued);

    // In the accept queue the row names no process that holds the connection, so no owner is read.
    BOOST_CHECK(!ServerSideAccepted(*queued));
    BOOST_CHECK(!WaitForAcceptedServerSide(*queued, std::chrono::milliseconds{200}));

    // Accepted while the check waits: the row then names this process's account.
    SOCKET accepted_socket{INVALID_SOCKET};
    std::thread acceptor([&listener, &accepted_socket] {
        UninterruptibleSleep(std::chrono::milliseconds{100});
        accepted_socket = AcceptCloexec(listener->Get(), nullptr, nullptr);
    });
    const std::optional<TcpConnEntry> accepted = WaitForAcceptedServerSide(*queued, std::chrono::seconds{10});
    acceptor.join();
    const Sock accepted_sock(accepted_socket);
    BOOST_REQUIRE(accepted_socket != INVALID_SOCKET);
    BOOST_REQUIRE(accepted);
    BOOST_CHECK(accepted->inode != 0);
    BOOST_CHECK_EQUAL(accepted->owner, static_cast<uint64_t>(::geteuid()));
}

#endif // __linux__

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

#endif // __linux__ || WIN32

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
