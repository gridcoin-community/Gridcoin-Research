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
#include "interfaces/gui_state.h"
#include "interfaces/handler.h"
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
#include "key_io.h"
#include "logging.h"
#include "node/gui_cache.h"
#include "sync.h"
#include "test/gui_cache_fakes.h"
#include "test/staking_test_peers.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib> // mkdtemp
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

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
        std::atomic<bool> gui_state{false};
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
    std::unique_ptr<interfaces::GuiState> makeGuiState() override { m_seen.gui_state = true; return nullptr; }

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

//! The server side of a GUI state subscription. The inner Init's makeGuiState
//! returns a StoringGuiState, which keeps here the fns the client registers, so
//! a case can push through them from the server, as the node's cache timer
//! does. It keeps them as the node's GuiState does: behind interfaces::GuardNotify.
//! A kept fn is a proxy for the client's callback, so a case clears the store
//! before the WireLink goes (the server's loop must outlive the proxy).
struct GuiStateStore {
    //! The node has destroyed the connection's Init: its side of the
    //! connection is torn down.
    std::atomic<bool> init_released{false};

    Mutex mutex;
    interfaces::GuiState::ChangedFn changed GUARDED_BY(mutex);
    interfaces::GuiState::MRCQuoteFn quote GUARDED_BY(mutex);
    interfaces::GuiStateSnapshot current GUARDED_BY(mutex);

    bool HasChanged()
    {
        LOCK(mutex);
        return static_cast<bool>(changed);
    }

    //! Call the client's changed fn, as the cache's timer does: synchronously,
    //! holding no lock (the fn is copied out first).
    void Push(const interfaces::GuiStateSnapshot& snapshot)
    {
        interfaces::GuiState::ChangedFn fn = WITH_LOCK(mutex, return changed);
        fn(snapshot);
    }

    void AnswerQuote(uint64_t request_id, const interfaces::MRCSnapshot& snapshot)
    {
        interfaces::GuiState::MRCQuoteFn fn = WITH_LOCK(mutex, return quote);
        fn(request_id, snapshot);
    }

    void Clear()
    {
        interfaces::GuiState::ChangedFn changed_fn;
        interfaces::GuiState::MRCQuoteFn quote_fn;
        {
            LOCK(mutex);
            std::swap(changed_fn, changed);
            std::swap(quote_fn, quote);
        }
    }
};

//! Clears a GuiStateStore at any exit of a case. Declared after the case's
//! WireLink, so it runs before the link goes: the callback proxies the store
//! keeps are released while the server's loop still runs, also when a REQUIRE
//! ends the case early.
class ClearAtExit
{
public:
    explicit ClearAtExit(GuiStateStore& store) : m_store(store) {}
    ~ClearAtExit() { m_store.Clear(); }

    ClearAtExit(const ClearAtExit&) = delete;
    ClearAtExit& operator=(const ClearAtExit&) = delete;

private:
    GuiStateStore& m_store;
};

struct StoringGuiState : interfaces::GuiState {
    explicit StoringGuiState(GuiStateStore& store) : m_store(store) {}

    interfaces::GuiStateSnapshot current() override { return WITH_LOCK(m_store.mutex, return m_store.current); }

    std::unique_ptr<interfaces::Handler> handleChanged(ChangedFn fn) override
    {
        LOCK(m_store.mutex);
        m_store.changed = interfaces::GuardNotify(std::move(fn));
        return interfaces::MakeCleanupHandler([] {});
    }

    void requestUpdate() override {}

    void requestMRCQuote(CAmount, uint64_t) override {}

    std::unique_ptr<interfaces::Handler> handleMRCQuote(MRCQuoteFn fn) override
    {
        LOCK(m_store.mutex);
        m_store.quote = interfaces::GuardNotify(std::move(fn));
        return interfaces::MakeCleanupHandler([] {});
    }

    GuiStateStore& m_store;
};

struct GuiStateInner : interfaces::Init {
    explicit GuiStateInner(GuiStateStore& store) : m_store(store) {}
    ~GuiStateInner() override { m_store.init_released = true; }

    std::unique_ptr<interfaces::GuiState> makeGuiState() override
    {
        return std::make_unique<StoringGuiState>(m_store);
    }

    GuiStateStore& m_store;
};

using namespace std::chrono_literals;

//! Long enough for the node's cache timer to have acted on anything it was
//! going to act on, at a 4 s interval on a manual clock: a wake and a settle.
constexpr auto TIMER_CHANCE = node::GuiCache::SETTLE + 250ms;

//! What the client's callbacks received, on the client's IPC worker thread.
template <typename T>
class Received
{
public:
    void Set(T value)
    {
        LOCK(m_mutex);
        m_value = std::move(value);
    }

    std::optional<T> Get()
    {
        LOCK(m_mutex);
        return m_value;
    }

private:
    Mutex m_mutex;
    std::optional<T> m_value GUARDED_BY(m_mutex);
};

//! A snapshot with a distinct value in a field of every part, so a field that
//! does not cross shows as a mismatch.
interfaces::GuiStateSnapshot DistinctSnapshot()
{
    interfaces::GuiStateSnapshot snap;
    snap.sequence = 7;
    snap.height = 123;
    snap.tip_time = 1700000000;
    snap.difficulty = 2.5;
    snap.initial_download = false;
    snap.out_of_sync = false;
    snap.peers_height = 124;
    snap.net_weight = 3.25;
    snap.staking = true;
    snap.coin_weight = 4.5;
    snap.miner_errors = "not staking";
    snap.has_balances = true;
    snap.balances.balance = 11;
    snap.balances.immature_balance = 12;
    snap.psgt_status.v15_enabled = true;
    snap.has_mrc = true;
    snap.mrc.reward = 5;
    snap.mrc.mature_balance = 13;
    snap.has_etts = true;
    snap.etts_seconds = 600;
    snap.has_researcher = true;
    snap.researcher.cpid = "abc";
    snap.researcher.has_cpid = true;
    snap.researcher.magnitude = 50.0;
    snap.researcher.has_magnitude = true;
    snap.researcher.has_active_beacon = true;
    return snap;
}

void CheckSameSnapshot(const interfaces::GuiStateSnapshot& got, const interfaces::GuiStateSnapshot& want)
{
    BOOST_CHECK_EQUAL(got.sequence, want.sequence);
    BOOST_CHECK_EQUAL(got.height, want.height);
    BOOST_CHECK_EQUAL(got.tip_time, want.tip_time);
    BOOST_CHECK_EQUAL(got.difficulty, want.difficulty);
    BOOST_CHECK_EQUAL(got.initial_download, want.initial_download);
    BOOST_CHECK_EQUAL(got.out_of_sync, want.out_of_sync);
    BOOST_CHECK_EQUAL(got.peers_height, want.peers_height);
    BOOST_CHECK_EQUAL(got.net_weight, want.net_weight);
    BOOST_CHECK_EQUAL(got.staking, want.staking);
    BOOST_CHECK_EQUAL(got.coin_weight, want.coin_weight);
    BOOST_CHECK_EQUAL(got.miner_errors, want.miner_errors);
    BOOST_CHECK_EQUAL(got.has_balances, want.has_balances);
    BOOST_CHECK_EQUAL(got.balances.balance, want.balances.balance);
    BOOST_CHECK_EQUAL(got.balances.immature_balance, want.balances.immature_balance);
    BOOST_CHECK_EQUAL(got.psgt_status.v15_enabled, want.psgt_status.v15_enabled);
    BOOST_CHECK_EQUAL(got.has_mrc, want.has_mrc);
    BOOST_CHECK_EQUAL(got.mrc.reward, want.mrc.reward);
    BOOST_CHECK_EQUAL(got.mrc.mature_balance, want.mrc.mature_balance);
    BOOST_CHECK_EQUAL(got.has_etts, want.has_etts);
    BOOST_CHECK_EQUAL(got.etts_seconds, want.etts_seconds);
    BOOST_CHECK_EQUAL(got.has_researcher, want.has_researcher);
    BOOST_CHECK_EQUAL(got.researcher.cpid, want.researcher.cpid);
    // The two presence flags sit beside a value of the same name in the schema,
    // where mpgen once read each as that value's companion and carried neither.
    BOOST_CHECK_EQUAL(got.researcher.has_cpid, want.researcher.has_cpid);
    BOOST_CHECK_EQUAL(got.researcher.magnitude, want.researcher.magnitude);
    BOOST_CHECK_EQUAL(got.researcher.has_magnitude, want.researcher.has_magnitude);
    BOOST_CHECK_EQUAL(got.researcher.has_active_beacon, want.researcher.has_active_beacon);
}

//! Counts, while alive, the notifications interfaces::GuardNotify drops as sent
//! to a disconnected client. Those are logged in the IPC category, which it
//! turns on for its lifetime.
class DroppedNotifications
{
public:
    DroppedNotifications()
        : m_was_enabled(LogInstance().WillLogCategory(BCLog::LogFlags::IPC))
        , m_callback(LogInstance().PushBackCallback([this](const std::string& line) {
              if (line.find("dropped a notification to a disconnected client") != std::string::npos) ++m_count;
          }))
    {
        LogInstance().EnableCategory(BCLog::LogFlags::IPC);
    }

    ~DroppedNotifications()
    {
        LogInstance().DeleteCallback(m_callback);
        if (!m_was_enabled) LogInstance().DisableCategory(BCLog::LogFlags::IPC);
    }

    DroppedNotifications(const DroppedNotifications&) = delete;
    DroppedNotifications& operator=(const DroppedNotifications&) = delete;

    int Count() const { return m_count.load(); }

private:
    const bool m_was_enabled;
    std::atomic<int> m_count{0};
    std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

//! Serves the wallet interface over a wallet the case owns, so wallet calls
//! cross the real wire. No ServeInit: what is under test is the wallet's
//! methods, not the authentication gate.
struct WalletServingInit : interfaces::Init {
    explicit WalletServingInit(CWallet* wallet) : m_wallet(wallet) {}

    std::unique_ptr<interfaces::Wallet> makeWallet() override { return interfaces::MakeWallet(m_wallet); }

    CWallet* m_wallet{nullptr};
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
    link.client->makeGuiState();

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
    BOOST_CHECK_MESSAGE(seen.gui_state.load(), "makeGuiState did not reach the server");
}

// The GUI state push, end to end over the wire: the client registers its fn
// through the remote GuiState, the server calls the fn it was handed (the node's
// cache timer does this on every publish), and the client receives the snapshot
// with every part intact. The snapshot is a struct argument of a callback, which
// no other notification in this tree carries, and nests the MRC, researcher,
// PSGT and wallet structs.
BOOST_AUTO_TEST_CASE(wire_gui_state_push_reaches_the_client)
{
    GuiStateStore store;
    {
        LOCK(store.mutex);
        store.current = DistinctSnapshot();
    }

    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&store, &id](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<GuiStateInner>(store), "the-cookie", id);
    });
    ClearAtExit clear_store(store);
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    // current() returns the snapshot as a result.
    CheckSameSnapshot(gui_state->current(), DistinctSnapshot());

    Received<interfaces::GuiStateSnapshot> received;
    std::unique_ptr<interfaces::Handler> handler =
        gui_state->handleChanged([&received](const interfaces::GuiStateSnapshot& snap) { received.Set(snap); });
    BOOST_REQUIRE(store.HasChanged());

    // The call returns once the client's callback has run.
    interfaces::GuiStateSnapshot pushed = DistinctSnapshot();
    pushed.sequence = 8;
    store.Push(pushed);

    const std::optional<interfaces::GuiStateSnapshot> got = received.Get();
    BOOST_REQUIRE(got);
    BOOST_CHECK_EQUAL(got->height, 123);
    BOOST_CHECK(got->has_mrc);
    BOOST_CHECK_EQUAL(got->mrc.reward, 5);
    CheckSameSnapshot(*got, pushed);
}

// The quote answer over the wire: a request id and an MRC snapshot.
BOOST_AUTO_TEST_CASE(wire_gui_state_quote_reaches_the_client)
{
    GuiStateStore store;
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&store, &id](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<GuiStateInner>(store), "the-cookie", id);
    });
    ClearAtExit clear_store(store);
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    Received<std::pair<uint64_t, interfaces::MRCSnapshot>> received;
    std::unique_ptr<interfaces::Handler> handler =
        gui_state->handleMRCQuote([&received](uint64_t request_id, const interfaces::MRCSnapshot& quote) {
            received.Set({request_id, quote});
        });

    interfaces::MRCSnapshot quote;
    quote.fee = 33;
    quote.min_fee = 28;
    quote.boosted_fee_error = true;
    quote.boosted_fee_error_msg = "too much";
    store.AnswerQuote(9, quote);

    const auto got = received.Get();
    BOOST_REQUIRE(got);
    BOOST_CHECK_EQUAL(got->first, 9U);
    BOOST_CHECK_EQUAL(got->second.fee, 33);
    BOOST_CHECK_EQUAL(got->second.min_fee, 28);
    BOOST_CHECK(got->second.boosted_fee_error);
    BOOST_CHECK_EQUAL(got->second.boosted_fee_error_msg, "too much");
}

// A GUI that goes away while the node still holds its fn: the node's next push
// returns to the caller (the cache's timer) without throwing, and is dropped as
// a notification to a disconnected client, not as some other failure.
BOOST_AUTO_TEST_CASE(push_to_a_disconnected_gui_is_dropped)
{
    GuiStateStore store;
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&store, &id](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<GuiStateInner>(store), "the-cookie", id);
    });
    ClearAtExit clear_store(store);
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    std::atomic<int> calls{0};
    std::unique_ptr<interfaces::Handler> handler =
        gui_state->handleChanged([&calls](const interfaces::GuiStateSnapshot&) { ++calls; });
    BOOST_REQUIRE(store.HasChanged());

    store.Push(DistinctSnapshot());
    BOOST_CHECK_EQUAL(calls.load(), 1);

    // The GUI exits: its proxies go, then its connection (joining its loop). The
    // node then tears its side down on its own; the case waits for that, so
    // that the IPC category, on for the push alone, logs the push and nothing
    // else.
    handler.reset();
    gui_state.reset();
    link.client.reset();
    link.client_proto.reset();
    BOOST_REQUIRE(gui_cache_test::WaitUntil([&store] { return store.init_released.load(); }));

    {
        DroppedNotifications dropped;
        BOOST_CHECK_NO_THROW(store.Push(DistinctSnapshot()));
        BOOST_CHECK_EQUAL(calls.load(), 1);
        BOOST_CHECK_EQUAL(dropped.Count(), 1);
    }
}

// A GUI subscribed through the node's own GuiState (ServeInit over the
// monolithic Init, over a published cache) whose connection drops with the
// subscription alive: the node releases the subscription with the connection,
// so its cache stops computing for it.
BOOST_AUTO_TEST_CASE(a_dropped_gui_connection_ends_its_subscription)
{
    gui_cache_test::FakeCompute compute;
    gui_cache_test::Deliveries deliveries;
    gui_cache_test::PublishedFakeCache published(compute, 4000ms, 30000ms);
    node::GuiCache& cache = published.Cache();

    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&id](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(interfaces::MakeGridcoinInit(), "the-cookie", id);
    });
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    // The subscription's forced update reaches the client over the wire, and so
    // does the next due update.
    std::unique_ptr<interfaces::Handler> handler = gui_state->handleChanged(deliveries.Make());
    BOOST_REQUIRE(deliveries.WaitFor([](const interfaces::GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    compute.Advance(4000ms);
    gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_REQUIRE(deliveries.WaitFor([](const interfaces::GuiStateSnapshot& snap) { return snap.sequence == 2; }));

    // Only the subscription is left alive on the client; then the connection
    // drops under it.
    gui_state.reset();
    link.client.reset();
    link.server->disconnectIncoming();

    // The node cleans up the GUI's objects on its own: once it has, a due block
    // computes nothing.
    BOOST_REQUIRE_MESSAGE(gui_cache_test::WaitUntil([&] {
        const int before = compute.chain_calls.load();
        compute.Advance(4000ms);
        gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
        return compute.chain_calls.load() == before;
    }), "the cache kept computing for a GUI whose connection dropped");

    // And it stays stopped, on the timer too.
    std::this_thread::sleep_for(TIMER_CHANCE);
    const int chain_calls = compute.chain_calls.load();
    const int runs = compute.cs_main_runs.load();
    compute.Advance(4000ms);
    gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), chain_calls);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), runs);
}

// A capability over the real wire: the passphrase crosses as Data, the kind as a
// byte, and the server's wallet holds the capability in its IPC slot, where the
// signing call that follows on the same connection finds it. The wallet stays
// unlocked for staking only throughout.
BOOST_AUTO_TEST_CASE(wire_wallet_elevation_round_trip)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    WireLink link([&fx](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<WalletServingInit>(&fx.wallet);
    });

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    const std::string address = EncodeDestination(fx.id);
    std::string signature;

    BOOST_CHECK(wallet->signMessage(address, "over the wire", signature)
                == interfaces::MessageSignStatus::KeyNotAvailable);

    BOOST_CHECK(!wallet->beginElevation(SecureString("not the passphrase"), CapabilityKind::SignMessage));
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());

    // The GUI checks the passphrase on a thread made for the one call and
    // ended after it (AskPassphraseDialog). Such a call crosses like any other,
    // the thread ends cleanly, and the capability it began serves the calls
    // the GUI then makes from its own thread.
    bool begun = false;
    std::thread checker([&] {
        begun = wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage);
    });
    checker.join();

    BOOST_REQUIRE(begun);
    BOOST_CHECK(fx.wallet.IpcCapability().has_value());

    BOOST_CHECK(wallet->signMessage(address, "over the wire", signature) == interfaces::MessageSignStatus::OK);
    BOOST_CHECK(!signature.empty());

    wallet->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());

    std::string after;
    BOOST_CHECK(wallet->signMessage(address, "over the wire", after) == interfaces::MessageSignStatus::KeyNotAvailable);
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    wallet.reset();
}

// A kind byte outside the enumeration is refused when the server reads it: the
// call fails, no capability is issued, and the connection goes on serving.
BOOST_AUTO_TEST_CASE(wire_capability_kind_out_of_range_is_refused)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    WireLink link([&fx](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<WalletServingInit>(&fx.wallet);
    });

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    BOOST_CHECK_THROW(wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, static_cast<CapabilityKind>(200)),
                      std::exception);
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    BOOST_CHECK(wallet->getLockState().unlocked_for_staking_only);
    BOOST_CHECK(wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    wallet->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());

    wallet.reset();
}

BOOST_AUTO_TEST_SUITE_END()
