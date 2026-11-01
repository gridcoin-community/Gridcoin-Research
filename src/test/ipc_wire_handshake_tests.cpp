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
// Complete types for the unique_ptrs the gated factory methods return: the
// pre-auth test destroys those unique_ptrs (they never get a value, but the
// destructor is still instantiated).
#include "interfaces/node.h"
#include "interfaces/mrc.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/staking.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_coin_source.h"
#include "interfaces/wallet_tx_source.h"
#include "ipc/capnp/protocol.h"
#include "ipc/handshake.h"
#include "ipc/protocol.h"
#include "ipc/serve_init.h"

#include <boost/test/unit_test.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib> // mkdtemp
#include <cstring>
#include <memory>
#include <string>
#include <thread>

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

//! Records which Init factory methods actually ARRIVED on the server. Every
//! factory returns nullptr -- what is under test is whether the call crossed the
//! wire at all, not what it produced, so no concrete interface implementations
//! are needed.
//!
//! This is the runtime half of the guard whose static half is
//! test/lint/lint-serve-init-complete.py. A virtual on interfaces::Init with no
//! ordinal in init.capnp gets no override on ProxyClient<Init>, so the client
//! call resolves to interfaces::Init's own default and returns nullptr WITHOUT
//! dispatching. Nothing fails to compile, nothing logs, and the capability
//! simply does not exist over IPC -- which is how the coin channel shipped
//! unmarshalled and left the multiprocess GUI unable to start. Flags are atomic
//! because the server answers on its own loop thread.
struct RecordingInner : interfaces::Init {
    struct Seen {
        std::atomic<bool> node{false};
        std::atomic<bool> staking{false};
        std::atomic<bool> wallet{false};
        std::atomic<bool> wallet_tx_source{false};
        std::atomic<bool> wallet_coin_source{false};
        std::atomic<bool> mrc{false};
        std::atomic<bool> voting{false};
        std::atomic<bool> researcher{false};
        std::atomic<bool> psgt{false};
        std::atomic<bool> sidestake{false};
        std::atomic<bool> core_ready{false};
    };

    explicit RecordingInner(Seen& seen) : m_seen(seen) {}

    bool isCoreReady() override { m_seen.core_ready = true; return true; }
    std::unique_ptr<interfaces::Node> makeNode() override { m_seen.node = true; return nullptr; }
    std::unique_ptr<interfaces::StakingStatus> makeStakingStatus() override { m_seen.staking = true; return nullptr; }
    std::unique_ptr<interfaces::Wallet> makeWallet() override { m_seen.wallet = true; return nullptr; }
    std::shared_ptr<interfaces::WalletTxSource> makeWalletTxSource() override { m_seen.wallet_tx_source = true; return nullptr; }
    std::shared_ptr<interfaces::WalletCoinSource> makeWalletCoinSource() override { m_seen.wallet_coin_source = true; return nullptr; }
    std::unique_ptr<interfaces::MRC> makeMRC() override { m_seen.mrc = true; return nullptr; }
    std::unique_ptr<interfaces::VotingManager> makeVotingManager() override { m_seen.voting = true; return nullptr; }
    std::unique_ptr<interfaces::ResearcherContext> makeResearcherContext() override { m_seen.researcher = true; return nullptr; }
    std::unique_ptr<interfaces::PSGTPoolContext> makePSGTPoolContext() override { m_seen.psgt = true; return nullptr; }
    std::unique_ptr<interfaces::SideStakeManager> makeSideStakeManager() override { m_seen.sidestake = true; return nullptr; }

    Seen& m_seen;
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
// wire, so the client rejects.
//
// The error TEXT is asserted, not just its non-emptiness. "!ok && !error.empty()"
// cannot distinguish an authentication rejection from any other way the
// handshake can fail -- a dropped connection, a marshalling fault, the catch-all
// at the end of ClientHandshake -- all of which produce exactly the same shape.
// Matching the distinctive substring is what makes this test evidence that the
// cookie comparison ran and refused.
BOOST_AUTO_TEST_CASE(wire_handshake_rejects_wrong_cookie)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link(CookieServeFactory("right-cookie", id));

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "wrong-cookie", "main");
    BOOST_CHECK(!r.ok);
    BOOST_REQUIRE(!r.error.empty());
    BOOST_CHECK_MESSAGE(r.error.find("rejected the authentication cookie") != std::string::npos,
                        "handshake failed for the wrong reason: " << r.error);
}

// The auth gate over the REAL transport. ipc_serve_init_tests covers this
// in-process, which proves ServeInit throws but not that the throw survives the
// wire: the exception is raised inside a capnp server method, captured by
// kj::runCatchingExceptions, marshalled back as an RPC error and re-raised on the
// client. If any link in that chain swallowed it, the in-process test would still
// pass while an unauthenticated peer got a live capability.
BOOST_AUTO_TEST_CASE(wire_capabilities_are_refused_before_authentication)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    id.identity_token = "tok";
    WireLink link(CookieServeFactory("the-cookie", id));

    // No authenticate() call at all: every gated method must fail.
    BOOST_CHECK_THROW(link.client->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(link.client->getIdentity(), std::exception);
    BOOST_CHECK_THROW(link.client->isCoreReady(), std::exception);
    BOOST_CHECK_THROW(link.client->makeNode(), std::exception);
    BOOST_CHECK_THROW(link.client->makeWallet(), std::exception);

    // And the gate still opens afterwards -- the refusals above did not wedge the
    // connection, they just refused the calls.
    BOOST_CHECK(link.client->authenticate("the-cookie"));
    BOOST_CHECK_NO_THROW(link.client->getBuildInfo());
}

// A wrong cookie latches the connection: retrying with the CORRECT cookie on the
// same connection is refused. Over the wire this also confirms the latch lives on
// the server's per-connection ServeInit, not in client-side state.
BOOST_AUTO_TEST_CASE(wire_wrong_cookie_latches_the_connection)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link(CookieServeFactory("right-cookie", id));

    BOOST_CHECK(!link.client->authenticate("wrong-cookie"));
    BOOST_CHECK(!link.client->authenticate("right-cookie"));
    BOOST_CHECK_THROW(link.client->getBuildInfo(), std::exception);
}

// The authentication deadline, end to end: a peer that connects and never
// authenticates is dropped, and the single connection slot it was occupying is
// returned to service.
//
// This is the assertion that matters for availability. max_connections is 1 and
// accept() is not re-armed while the slot is taken, so before the deadline existed
// any local process could connect, say nothing, and lock the real GUI out for as
// long as it stayed alive.
//
// The squatter's disconnect is waited for explicitly rather than assumed after a
// fixed sleep: it is the same event that drives the server-side re-arm, so
// observing it means the second client's handshake below cannot hang waiting for a
// listener that never came back.
BOOST_AUTO_TEST_CASE(wire_unauthenticated_connection_is_dropped_and_slot_reclaimed)
{
    interfaces::NodeIdentity id;
    id.network = "main";

    UnixSocket sock;
    auto server = ipc::capnp::MakeCapnpProtocol();
    server->listen(sock.releaseBoundFd(), "ipc-wire-server", CookieServeFactory("cookie", id),
                   /*auth_deadline=*/std::chrono::seconds{1});

    // The squatter: connects and then does nothing at all.
    std::atomic<bool> squatter_dropped{false};
    auto squatter_proto = ipc::capnp::MakeCapnpProtocol();
    auto squatter = squatter_proto->connect(sock.connectClient(), "ipc-wire-squatter",
                                            [&squatter_dropped] { squatter_dropped = true; });
    BOOST_REQUIRE(squatter != nullptr);

    // Wait for the node to drop it. Generous cap: this asserts the drop happens,
    // not how promptly, and a loaded machine must not turn that into a flake.
    for (int i = 0; i < 200 && !squatter_dropped; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    BOOST_REQUIRE_MESSAGE(squatter_dropped,
                          "the node never dropped a connection that did not authenticate");

    // The slot is free and accept() is re-armed: a real client connects and
    // completes the full handshake.
    auto gui_proto = ipc::capnp::MakeCapnpProtocol();
    auto gui = gui_proto->connect(sock.connectClient(), "ipc-wire-gui", /*on_disconnect=*/{});
    BOOST_REQUIRE(gui != nullptr);
    ipc::HandshakeResult r = ipc::ClientHandshake(*gui, "cookie", "main");
    BOOST_CHECK(r.ok);
    BOOST_CHECK(r.error.empty());

    // Teardown in the same order WireLink uses: proxies, then incoming
    // connections, then each protocol (which joins its own loop thread).
    gui.reset();
    squatter.reset();
    server->disconnectIncoming();
    gui_proto.reset();
    squatter_proto.reset();
    server.reset();
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

// Every interfaces::Init factory must actually reach the server over the wire.
//
// This is the test that would have caught the coin channel shipping without a
// Cap'n Proto schema. makeWalletCoinSource existed on interfaces::Init, had an
// auth-gated ServeInit override, and its DTOs all carried
// INTERFACES_ASSERT_MARSHALABLE -- but with no ordinal in init.capnp, mpgen
// generated no override on ProxyClient<Init>. The client call resolved to the
// base default, returned nullptr without dispatching, and the multiprocess GUI
// threw at startup. Everything compiled and every existing lint passed.
//
// Asserting the SERVER saw each call is what makes this robust: a factory that
// legitimately returns nullptr (no wallet attached, say) is indistinguishable
// client-side from one that was never marshalled, so a client-side null check
// would prove nothing.
BOOST_AUTO_TEST_CASE(wire_every_init_factory_reaches_the_server)
{
    RecordingInner::Seen seen;

    interfaces::NodeIdentity id;
    id.network = "main";
    id.identity_token = "tok";

    WireLink link([&seen, &id](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<RecordingInner>(seen), "the-cookie", id);
    });

    // ServeInit gates every factory behind RequireAuth(), so authenticate first.
    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "the-cookie", "main");
    BOOST_REQUIRE(r.ok);

    // Each call returns nullptr by construction; the return is deliberately
    // ignored. What is asserted is that the invocation arrived server-side.
    link.client->isCoreReady();
    link.client->makeNode();
    link.client->makeStakingStatus();
    link.client->makeWallet();
    link.client->makeWalletTxSource();
    link.client->makeWalletCoinSource();
    link.client->makeMRC();
    link.client->makeVotingManager();
    link.client->makeResearcherContext();
    link.client->makePSGTPoolContext();
    link.client->makeSideStakeManager();

    BOOST_CHECK_MESSAGE(seen.core_ready.load(), "isCoreReady did not reach the server");
    BOOST_CHECK_MESSAGE(seen.node.load(), "makeNode did not reach the server");
    BOOST_CHECK_MESSAGE(seen.staking.load(), "makeStakingStatus did not reach the server");
    BOOST_CHECK_MESSAGE(seen.wallet.load(), "makeWallet did not reach the server");
    BOOST_CHECK_MESSAGE(seen.wallet_tx_source.load(), "makeWalletTxSource did not reach the server");
    BOOST_CHECK_MESSAGE(seen.wallet_coin_source.load(),
                        "makeWalletCoinSource did not reach the server -- it is missing an ordinal "
                        "in src/ipc/capnp/init.capnp, so ProxyClient<Init> has no override and the "
                        "call never left the client process");
    BOOST_CHECK_MESSAGE(seen.mrc.load(), "makeMRC did not reach the server");
    BOOST_CHECK_MESSAGE(seen.voting.load(), "makeVotingManager did not reach the server");
    BOOST_CHECK_MESSAGE(seen.researcher.load(), "makeResearcherContext did not reach the server");
    BOOST_CHECK_MESSAGE(seen.psgt.load(), "makePSGTPoolContext did not reach the server");
    BOOST_CHECK_MESSAGE(seen.sidestake.load(), "makeSideStakeManager did not reach the server");
}

BOOST_AUTO_TEST_SUITE_END()
