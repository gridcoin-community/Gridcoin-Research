// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/bind_policy.h>
#include <rpc/client.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <alert.h>
#include <test/state_guard.h>
#include <util.h>
#include <util/sock.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

#ifndef WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using boost::system::error_code;
using boost::system::system_category;

namespace {
RpcBindAttempt A(const std::string& addr, bool disables, RpcBindError result)
{
    RpcBindAttempt a;
    a.address = addr;
    a.conflict_disables_rpc = disables;
    a.result = result;
    return a;
}
} // namespace

BOOST_AUTO_TEST_SUITE(rpc_bind_policy_tests, *boost::unit_test::fixture<grc_test::StateGuard>())

BOOST_AUTO_TEST_CASE(classify_conflicts)
{
    BOOST_CHECK(ClassifyRpcBindError(boost::asio::error::address_in_use) == RpcBindError::Conflict);
#ifdef WIN32
    BOOST_CHECK(ClassifyRpcBindError(error_code(WSAEACCES, system_category())) == RpcBindError::Conflict);
#else
    BOOST_CHECK(ClassifyRpcBindError(error_code(EACCES, system_category())) == RpcBindError::Other);
#endif
    BOOST_CHECK(ClassifyRpcBindError(boost::asio::error::invalid_argument) == RpcBindError::Other);
}

BOOST_AUTO_TEST_CASE(classify_skips)
{
#ifdef WIN32
    BOOST_CHECK(ClassifyRpcBindError(error_code(WSAEADDRNOTAVAIL, system_category())) == RpcBindError::Skip);
    BOOST_CHECK(ClassifyRpcBindError(error_code(WSAEAFNOSUPPORT, system_category())) == RpcBindError::Skip);
#else
    BOOST_CHECK(ClassifyRpcBindError(error_code(EADDRNOTAVAIL, system_category())) == RpcBindError::Skip);
    BOOST_CHECK(ClassifyRpcBindError(error_code(EAFNOSUPPORT, system_category())) == RpcBindError::Skip);
#endif
}

BOOST_AUTO_TEST_CASE(outcome_loopback_conflict_disables)
{
    BOOST_CHECK(DecideRpcListenOutcome({A("127.0.0.1:15715", true, RpcBindError::Conflict),
                                        A("[::1]:15715", true, RpcBindError::None)})
                == RpcListenOutcome::DisabledByConflict);
}

BOOST_AUTO_TEST_CASE(outcome_ipv6_absent_serves)
{
    BOOST_CHECK(DecideRpcListenOutcome({A("[::1]:15715", true, RpcBindError::Skip),
                                        A("127.0.0.1:15715", true, RpcBindError::None)})
                == RpcListenOutcome::Serve);
}

BOOST_AUTO_TEST_CASE(outcome_nonloopback_conflict_skipped)
{
    BOOST_CHECK(DecideRpcListenOutcome({A("10.0.0.5:15715", false, RpcBindError::Conflict),
                                        A("127.0.0.1:15715", true, RpcBindError::None)})
                == RpcListenOutcome::Serve);
}

BOOST_AUTO_TEST_CASE(outcome_conflict_never_nothing_bound)
{
    BOOST_CHECK(DecideRpcListenOutcome({A("10.0.0.5:15715", false, RpcBindError::Conflict)})
                == RpcListenOutcome::DisabledByConflict);
}

BOOST_AUTO_TEST_CASE(outcome_config_error_nothing_bound)
{
    BOOST_CHECK(DecideRpcListenOutcome({A("[::1]:15715", true, RpcBindError::Skip)})
                == RpcListenOutcome::NothingBound);
    BOOST_CHECK(DecideRpcListenOutcome({A("10.0.0.5:15715", false, RpcBindError::Other)})
                == RpcListenOutcome::NothingBound);
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(live_conflict_classified)
{
    SOCKET holder = CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    BOOST_REQUIRE(holder != INVALID_SOCKET);
    int one = 1;
    BOOST_REQUIRE(::setsockopt(holder, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0);
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = 0;
    BOOST_REQUIRE(::bind(holder, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0);
    BOOST_REQUIRE(::listen(holder, 1) == 0);
    socklen_t len = sizeof(sin);
    BOOST_REQUIRE(::getsockname(holder, reinterpret_cast<sockaddr*>(&sin), &len) == 0);

    boost::asio::io_context io;
    boost::asio::ip::tcp::acceptor acceptor(io);
    const boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4::loopback(), ntohs(sin.sin_port));
    bool threw = false;
    try {
        OpenRpcAcceptorForTest(acceptor, endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();
    } catch (const boost::system::system_error& e) {
        threw = true;
        BOOST_CHECK(ClassifyRpcBindError(e.code()) == RpcBindError::Conflict);
    }
    BOOST_CHECK(threw);
    ::close(holder);
}
#else
BOOST_AUTO_TEST_CASE(windows_exclusive_blocks_cobind)
{
    boost::asio::io_context io;
    boost::asio::ip::tcp::acceptor acceptor(io);
    OpenRpcAcceptorForTest(acceptor, boost::asio::ip::tcp::v4());
    acceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
    acceptor.listen();
    const unsigned short port = acceptor.local_endpoint().port();

    SOCKET s = CreateCloexecSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    BOOST_REQUIRE(s != INVALID_SOCKET);
    BOOL one = TRUE;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(port);
    BOOST_CHECK(::bind(s, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) != 0);
    ::closesocket(s);
}
#endif

BOOST_AUTO_TEST_CASE(warning_reaches_statusbar)
{
    const std::string saved = strMiscWarning;
    SetRPCBindWarning("RPC server disabled: test");
    BOOST_CHECK(GetWarnings("statusbar").find("RPC server disabled: test") != std::string::npos);
    strMiscWarning = "misc";
    const std::string both = GetWarnings("statusbar");
    BOOST_CHECK(both.find("misc") != std::string::npos);
    BOOST_CHECK(both.find("RPC server disabled: test") != std::string::npos);
    SetRPCBindWarning("");
    strMiscWarning = saved;
    BOOST_CHECK(GetRPCBindWarning().empty());
}

BOOST_AUTO_TEST_CASE(warmup_refuses_all_but_stop)
{
    // Warmup is process state outside StateGuard, so put it back whatever happens.
    struct RestoreWarmup {
        ~RestoreWarmup() { SetRPCWarmupForTest(false); }
    } restore;

    SetRPCWarmupForTest(true);
    BOOST_CHECK(RPCIsInWarmup());
    BOOST_CHECK(RPCRefusedInWarmup("getblockcount"));
    BOOST_CHECK(RPCRefusedInWarmup("walletpassphrase"));
    BOOST_CHECK(!RPCRefusedInWarmup("stop"));

    SetRPCWarmupFinished();
    BOOST_CHECK(!RPCIsInWarmup());
    BOOST_CHECK(!RPCRefusedInWarmup("getblockcount"));
    BOOST_CHECK(!RPCRefusedInWarmup("stop"));
}

BOOST_AUTO_TEST_CASE(cli_retries_warmup_reply)
{
    UniValue reply;
    BOOST_REQUIRE(reply.read(R"({"result":null,"error":{"code":-28,"message":"x"},"id":1})"));
    BOOST_CHECK(ShouldRetryWarmupReply(true, reply));
    BOOST_CHECK(!ShouldRetryWarmupReply(false, reply));

    UniValue other;
    BOOST_REQUIRE(other.read(R"({"result":null,"error":{"code":-1,"message":"x"},"id":1})"));
    BOOST_CHECK(!ShouldRetryWarmupReply(true, other));

    UniValue ok;
    BOOST_REQUIRE(ok.read(R"({"result":5,"error":null,"id":1})"));
    BOOST_CHECK(!ShouldRetryWarmupReply(true, ok));
}

BOOST_AUTO_TEST_SUITE_END()
