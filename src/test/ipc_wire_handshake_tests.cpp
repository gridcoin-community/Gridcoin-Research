// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// WIRE-level handshake tests: drive ipc::ClientHandshake against a served
// interfaces::Init over a REAL Cap'n Proto connection on a real AF_UNIX socket,
// using the SAME production transport the daemon and GUI use -- CapnpProtocol's
// listen()/connect() -- rather than the in-process FakeInit coverage in
// ipc_handshake_tests. This exercises, end to end over the wire: the served
// ServeInit's auth gate, the per-connection serve factory, and the client's
// schema/network compatibility checks (BuildInfo/NodeIdentity marshalled across
// the socket). Multiprocess-only (needs gridcoin_ipc and the generated capnp
// proxies); the whole file is gated on ENABLE_MULTIPROCESS in CMake.
//
// Using CapnpProtocol (not a hand-rolled EventLoop) matters: it runs each
// ConnectStream/ListenConnections on its own background loop thread and marshals
// calls cross-thread, exactly as production does. A test that constructs the loop
// and calls ConnectStream inline on the same thread (before loop()) deadlocks.

#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "ipc/capnp/protocol.h"
#include "ipc/handshake.h"
#include "ipc/protocol.h"
#include "ipc/serve_init.h"

#include <boost/test/unit_test.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <string>

namespace {

//! Inner Init the served ServeInit delegates to; untouched by the handshake
//! (ServeInit answers authenticate()/getBuildInfo()/getIdentity() itself).
struct StubInner : interfaces::Init {};

//! Server double reporting a schema_major one greater than this build's, so a
//! genuine server-reported mismatch marshals over the wire. authenticate()
//! accepts any cookie so the handshake reaches the build-info compare.
struct SchemaMismatchServer : interfaces::Init {
    bool authenticate(const std::string&) override { return true; }
    interfaces::BuildInfo getBuildInfo() override
    {
        interfaces::BuildInfo b = ipc::GetLocalBuildInfo();
        b.schema_major += 1;
        return b;
    }
    interfaces::NodeIdentity getIdentity() override
    {
        interfaces::NodeIdentity id;
        id.network = "main";
        return id;
    }
};

//! A temporary AF_UNIX socket bound in a mkdtemp directory. CapnpProtocol::listen()
//! performs the ::listen(); connectClient() dials a fresh client fd.
struct UnixSocket {
    std::string dir;
    std::string path;
    int bound_fd = -1;

    UnixSocket()
    {
        char tmpl[] = "/tmp/gcipcwire.XXXXXX";
        BOOST_REQUIRE(::mkdtemp(tmpl) != nullptr);
        dir = tmpl;
        path = dir + "/wire.sock";

        bound_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        BOOST_REQUIRE(bound_fd >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        BOOST_REQUIRE(path.size() < sizeof(addr.sun_path));
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        BOOST_REQUIRE_EQUAL(::bind(bound_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    }

    ~UnixSocket()
    {
        if (bound_fd >= 0) ::close(bound_fd);
        if (!path.empty()) ::unlink(path.c_str());
        if (!dir.empty()) ::rmdir(dir.c_str());
    }

    //! Hand the bound fd to CapnpProtocol::listen (which ::listen()s + owns it).
    int releaseBoundFd()
    {
        int fd = bound_fd;
        bound_fd = -1;
        return fd;
    }

    int connectClient() const
    {
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        BOOST_REQUIRE(fd >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        BOOST_REQUIRE_EQUAL(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
        return fd;
    }
};

//! Factory serving a fresh ServeInit (StubInner + the given cookie/identity) per
//! connection, matching how the daemon serves.
interfaces::MakeServeInitFn CookieServeFactory(std::string cookie, interfaces::NodeIdentity id)
{
    return [cookie = std::move(cookie), id = std::move(id)](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<StubInner>(), cookie, id);
    };
}

//! Server + client CapnpProtocols wired over a real socket. connect() returns the
//! remote Init proxy for the test to hand to ClientHandshake. Teardown drops the
//! client proxy and both protocols (each joins its own loop thread) in order.
struct WireLink {
    UnixSocket sock;
    std::unique_ptr<ipc::Protocol> server;
    std::unique_ptr<ipc::Protocol> client_proto;
    std::unique_ptr<interfaces::Init> client;

    WireLink(interfaces::MakeServeInitFn factory)
    {
        server = ipc::capnp::MakeCapnpProtocol();
        server->listen(sock.releaseBoundFd(), "ipc-wire-server", std::move(factory));

        int fd = sock.connectClient();
        client_proto = ipc::capnp::MakeCapnpProtocol();
        client = client_proto->connect(fd, "ipc-wire-client", /*on_disconnect=*/{});
        BOOST_REQUIRE(client != nullptr);
    }

    ~WireLink()
    {
        client.reset();          // drop the remote proxy
        server->disconnectIncoming();
        client_proto.reset();    // joins the client loop thread
        server.reset();          // joins the server loop thread
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(ipc_wire_handshake_tests)

// Happy path over the real wire: correct cookie + this build's info => ok, and the
// served identity/build round-trip back to the client.
BOOST_AUTO_TEST_CASE(wire_handshake_succeeds_with_correct_cookie)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    id.identity_token = "tok";
    WireLink link(CookieServeFactory("the-cookie", id));

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "the-cookie", "main");
    BOOST_CHECK(r.ok);
    BOOST_CHECK(r.error.empty());
    BOOST_CHECK_EQUAL(r.remote_ident.identity_token, "tok");
    BOOST_CHECK_EQUAL(r.remote_build.schema_major, ipc::IPC_SCHEMA_MAJOR);
}

// Wrong cookie: the served ServeInit's authenticate() returns false over the
// wire, so the client rejects with a non-empty error.
BOOST_AUTO_TEST_CASE(wire_handshake_rejects_wrong_cookie)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link(CookieServeFactory("right-cookie", id));

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "wrong-cookie", "main");
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
}

// Server reports a higher schema_major than this build => hard fail (the compare
// runs against a real value marshalled from the server).
BOOST_AUTO_TEST_CASE(wire_handshake_rejects_schema_major_mismatch)
{
    WireLink link([](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<SchemaMismatchServer>();
    });

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "any-cookie", "main");
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
}

// Network mismatch: server serves "main", client expects "test" => hard fail.
BOOST_AUTO_TEST_CASE(wire_handshake_rejects_network_mismatch)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link(CookieServeFactory("cookie", id));

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "cookie", "test");
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
}

BOOST_AUTO_TEST_SUITE_END()
