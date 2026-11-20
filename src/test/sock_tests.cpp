// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/sock.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#ifndef WIN32
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

bool IsInheritable(SOCKET s)
{
#ifdef WIN32
    DWORD flags = 0;
    BOOST_REQUIRE(::GetHandleInformation(reinterpret_cast<HANDLE>(s), &flags));
    return (flags & HANDLE_FLAG_INHERIT) != 0;
#else
    const int flags = ::fcntl(s, F_GETFD);
    BOOST_REQUIRE(flags != -1);
    return (flags & FD_CLOEXEC) == 0;
#endif
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

//! A connected loopback pair: the accepted server side and the client side.
struct Pair {
    std::unique_ptr<Sock> listener;
    std::unique_ptr<Sock> server;
    std::unique_ptr<Sock> client;
};

Pair MakePair()
{
    Pair p;
    p.listener = std::make_unique<Sock>(CreateCloexecSocket(AF_INET, SOCK_STREAM, 0));
    BOOST_REQUIRE(p.listener->Get() != INVALID_SOCKET);

    sockaddr_in sin = LoopbackAddr(0);
    BOOST_REQUIRE(::bind(p.listener->Get(), reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0);
    BOOST_REQUIRE(::listen(p.listener->Get(), 1) == 0);
    socklen_t len = sizeof(sin);
    BOOST_REQUIRE(::getsockname(p.listener->Get(), reinterpret_cast<sockaddr*>(&sin), &len) == 0);

    p.client = std::make_unique<Sock>(CreateCloexecSocket(AF_INET, SOCK_STREAM, 0));
    BOOST_REQUIRE(p.client->Get() != INVALID_SOCKET);
    BOOST_REQUIRE(::connect(p.client->Get(), reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0);

    sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    p.server = std::make_unique<Sock>(
        AcceptCloexec(p.listener->Get(), reinterpret_cast<sockaddr*>(&peer), &peer_len));
    BOOST_REQUIRE(p.server->Get() != INVALID_SOCKET);
    BOOST_CHECK(peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    return p;
}

bool GetLinger(SOCKET s, linger& lg)
{
    socklen_t len = sizeof(lg);
#ifdef WIN32
    return ::getsockopt(s, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lg), &len) == 0;
#else
    return ::getsockopt(s, SOL_SOCKET, SO_LINGER, &lg, &len) == 0;
#endif
}

void SetNonBlocking(SOCKET s)
{
#ifdef WIN32
    u_long on = 1;
    BOOST_REQUIRE(::ioctlsocket(s, FIONBIO, &on) == 0);
#else
    const int flags = ::fcntl(s, F_GETFL);
    BOOST_REQUIRE(flags != -1);
    BOOST_REQUIRE(::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0);
#endif
}

} // namespace

BOOST_AUTO_TEST_SUITE(sock_tests)

BOOST_AUTO_TEST_CASE(cloexec_socket_not_inherited)
{
    Sock s(CreateCloexecSocket(AF_INET, SOCK_STREAM, 0));
    BOOST_REQUIRE(s.Get() != INVALID_SOCKET);
    BOOST_CHECK(!IsInheritable(s.Get()));
}

BOOST_AUTO_TEST_CASE(accept_cloexec_not_inherited)
{
    // MakePair checks the accepted address is 127.0.0.1.
    Pair p = MakePair();
    BOOST_CHECK(!IsInheritable(p.listener->Get()));
    BOOST_CHECK(!IsInheritable(p.client->Get()));
    BOOST_CHECK(!IsInheritable(p.server->Get()));
}

BOOST_AUTO_TEST_CASE(drain_graceful_when_peer_closes)
{
    Pair p = MakePair();
    const std::string payload(1024, 'x');
    BOOST_REQUIRE(p.server->Send(payload.data(), payload.size(), 0) == static_cast<int>(payload.size()));

    // The client reads everything, then closes.
    size_t got = 0;
    char buf[2048];
    while (got < payload.size()) {
        const int n = p.client->Recv(buf, sizeof(buf), 0);
        BOOST_REQUIRE(n > 0);
        got += static_cast<size_t>(n);
    }
    BOOST_CHECK_EQUAL(got, payload.size());
    p.client->Reset();

    const auto start = std::chrono::steady_clock::now();
    DrainBeforeClose(p.server->Get(), std::chrono::milliseconds{5000});
    const auto elapsed = std::chrono::steady_clock::now() - start;
    BOOST_CHECK(elapsed < std::chrono::seconds{1});

#ifndef WIN32
    linger lg;
    BOOST_REQUIRE(GetLinger(p.server->Get(), lg));
    BOOST_CHECK_EQUAL(lg.l_onoff, 0);
#endif
}

BOOST_AUTO_TEST_CASE(drain_bounded_when_peer_idle)
{
    Pair p = MakePair();

    const auto start = std::chrono::steady_clock::now();
    DrainBeforeClose(p.server->Get(), std::chrono::milliseconds{200});
    const auto elapsed = std::chrono::steady_clock::now() - start;
    // A small tolerance below the bound covers millisecond truncation of the remaining time.
    BOOST_CHECK(elapsed >= std::chrono::milliseconds{190});
    BOOST_CHECK(elapsed <= std::chrono::seconds{2});

    linger lg;
    BOOST_REQUIRE(GetLinger(p.server->Get(), lg));
    BOOST_CHECK_EQUAL(lg.l_onoff, 1);
    BOOST_CHECK_EQUAL(lg.l_linger, 0);
}

// A non-blocking socket must be drained the same way as a blocking one, waiting up to the bound
// while the peer is idle.
BOOST_AUTO_TEST_CASE(drain_nonblocking_waits_when_peer_idle)
{
    Pair p = MakePair();
    SetNonBlocking(p.server->Get());

    const auto start = std::chrono::steady_clock::now();
    DrainBeforeClose(p.server->Get(), std::chrono::milliseconds{200});
    const auto elapsed = std::chrono::steady_clock::now() - start;
    BOOST_CHECK(elapsed >= std::chrono::milliseconds{190});
    BOOST_CHECK(elapsed <= std::chrono::seconds{2});

    linger lg;
    BOOST_REQUIRE(GetLinger(p.server->Get(), lg));
    BOOST_CHECK_EQUAL(lg.l_onoff, 1);
    BOOST_CHECK_EQUAL(lg.l_linger, 0);
}

// A peer that closes part way through the bound ends the drain promptly with no reset.
BOOST_AUTO_TEST_CASE(drain_nonblocking_returns_when_peer_closes)
{
    Pair p = MakePair();
    SetNonBlocking(p.server->Get());

    std::thread closer([&p] {
        std::this_thread::sleep_for(std::chrono::milliseconds{150});
        p.client->Reset();
    });

    const auto start = std::chrono::steady_clock::now();
    DrainBeforeClose(p.server->Get(), std::chrono::milliseconds{5000});
    const auto elapsed = std::chrono::steady_clock::now() - start;
    closer.join();

    BOOST_CHECK(elapsed >= std::chrono::milliseconds{100});
    BOOST_CHECK(elapsed < std::chrono::seconds{2});

    linger lg;
    BOOST_REQUIRE(GetLinger(p.server->Get(), lg));
    BOOST_CHECK_EQUAL(lg.l_onoff, 0);
}

BOOST_AUTO_TEST_SUITE_END()
