// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/init.h"
// Complete types for the unique_ptrs/shared_ptrs the gated factories return.
#include "interfaces/gui_state.h"
#include "interfaces/mrc.h"
#include "interfaces/node.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/staking.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_coin_source.h"
#include "interfaces/wallet_tx_source.h"
#include "ipc/handshake.h"
#include "key_io.h"
#include "ipc/serve_init.h"
#include "test/staking_test_peers.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
//! Inner Init the served wrapper delegates to. isCoreReady() is overridden so we
//! can observe that it is reachable only after authentication; getIdentity() would
//! be ignored (ServeInit serves the identity it was constructed with), so it is
//! left as the base default.
struct StubInit : public interfaces::Init {
    bool isCoreReady() override { return true; }
};

//! Inner Init that serves the wallet in pwalletMain and the interfaces that use
//! its capability, as the daemon's does.
struct MainWalletInit : public interfaces::Init {
    std::unique_ptr<interfaces::Wallet> makeWallet() override
    {
        return pwalletMain ? interfaces::MakeWallet(pwalletMain) : nullptr;
    }
    std::unique_ptr<interfaces::Node> makeNode() override { return interfaces::MakeNode(); }
};

//! The slot hook gridcoinresearchd installs: the attached GUI going away ends the
//! wallet's IPC capability.
void DaemonSlotHook()
{
    if (pwalletMain) pwalletMain->EndIpcElevation();
}

//! Long enough that only a hang reaches it, so a loaded machine cannot turn a
//! correct run into a failure.
constexpr std::chrono::seconds WAIT_CAP{10};

//! Matches an exception whose message contains the given text.
struct HasMessage {
    explicit HasMessage(std::string part) : m_part(std::move(part)) {}
    bool operator()(const std::exception& e) const { return std::string{e.what()}.find(m_part) != std::string::npos; }
    std::string m_part;
};

//! Joins on every exit. A failed BOOST_REQUIRE throws past the explicit join, and
//! destroying a joinable std::thread calls std::terminate, which would take the
//! whole test binary down. The threads it joins wait on bounded gates only.
struct JoinOnExit {
    std::thread& m_thread;
    ~JoinOnExit() { if (m_thread.joinable()) m_thread.join(); }
};
} // namespace

BOOST_AUTO_TEST_SUITE(ipc_serve_init_tests)

// The serve-side counterpart to ipc_handshake_tests' client coverage: ServeInit
// must refuse to serve build/identity/delegated methods until authenticate()
// succeeds with the right cookie, and must serve THIS process's build info + the
// identity it was handed (not the inner Init's).
BOOST_AUTO_TEST_CASE(serve_init_gates_everything_on_authentication)
{
    interfaces::NodeIdentity id;
    id.identity_token = "expected-token";
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "s3cr3t-cookie", id, ipc::MakeAttachSlot(), {});

    // Pre-auth: nothing is served, and the connection reports itself unauthenticated
    // (which is what the listener's deadline consults).
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(serve->getIdentity(), std::exception);
    BOOST_CHECK_THROW(serve->isCoreReady(), std::exception);

    // Correct cookie: authenticate() succeeds and the gate opens.
    BOOST_CHECK(serve->authenticate("s3cr3t-cookie"));
    BOOST_CHECK(serve->isAuthenticated());
    BOOST_CHECK_NO_THROW(serve->getBuildInfo());
    BOOST_CHECK(serve->isCoreReady());

    // getIdentity() serves the identity ServeInit was constructed with.
    const interfaces::NodeIdentity served = serve->getIdentity();
    BOOST_CHECK_EQUAL(served.identity_token, "expected-token");
    BOOST_CHECK_EQUAL(served.network, "main");
}

// One strike: a wrong cookie latches the connection permanently unauthenticated,
// so it cannot be used as a guessing oracle and cannot go on holding one of the
// node's connection slots in the hope of getting it right. The listener's
// authentication deadline then reclaims the slot. A legitimate GUI presents one
// cookie and gives up on rejection, so it never sees this.
BOOST_AUTO_TEST_CASE(serve_init_wrong_cookie_latches_the_connection)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "s3cr3t-cookie", id, ipc::MakeAttachSlot(), {});

    BOOST_CHECK(!serve->authenticate("wrong-cookie"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);

    // The RIGHT cookie is now refused too: retrying on this connection is not a
    // path back in. This is the assertion that distinguishes a latch from merely
    // "authenticate() returned false".
    BOOST_CHECK(!serve->authenticate("s3cr3t-cookie"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(serve->isCoreReady(), std::exception);

    // Recovery is by reconnecting, which builds a fresh ServeInit.
    auto reconnected = ipc::MakeServeInit(std::make_unique<StubInit>(), "s3cr3t-cookie", id, ipc::MakeAttachSlot(), {});
    BOOST_CHECK(reconnected->authenticate("s3cr3t-cookie"));
    BOOST_CHECK(reconnected->isAuthenticated());
}

// An empty expected cookie must authenticate nobody. ConstantTimeEqual("", "") is
// true, so without the explicit guard an empty cookie would open the gate to every
// peer -- and it must not latch either, since nothing the peer did was wrong.
BOOST_AUTO_TEST_CASE(serve_init_empty_cookie_authenticates_nobody)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "", id, ipc::MakeAttachSlot(), {});

    BOOST_CHECK(!serve->authenticate(""));
    BOOST_CHECK(!serve->authenticate("anything"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);
}

BOOST_AUTO_TEST_CASE(serve_init_getbuildinfo_is_this_builds_info)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "cookie", id, ipc::MakeAttachSlot(), {});
    BOOST_CHECK(serve->authenticate("cookie"));
    // Served build info is this process's embedded fingerprint, not the inner's.
    BOOST_CHECK_EQUAL(serve->getBuildInfo().git_commit, ipc::GetLocalBuildInfo().git_commit);
    BOOST_CHECK_EQUAL(serve->getBuildInfo().schema_major, ipc::IPC_SCHEMA_MAJOR);
}

// Per-connection auth (Phase 3): the node builds a FRESH ServeInit for each
// accepted connection (make_serve_init factory), so authentication is per
// instance. A second connection -- a distinct ServeInit -- starts gated even
// after an earlier one authenticated; auth can never leak across connections.
BOOST_AUTO_TEST_CASE(serve_init_authentication_is_per_instance)
{
    interfaces::NodeIdentity id;
    id.network = "main";

    // Both connections share the listener's one attach slot, as in the daemon.
    const auto slot = ipc::MakeAttachSlot();

    // First "connection": authenticate it.
    auto first = ipc::MakeServeInit(std::make_unique<StubInit>(), "cookie", id, slot, {});
    BOOST_CHECK(first->authenticate("cookie"));
    BOOST_CHECK(first->isCoreReady());

    // Second "connection" (a fresh ServeInit) is independently gated: it must
    // present the cookie itself; the first instance's auth does not carry over.
    auto second = ipc::MakeServeInit(std::make_unique<StubInit>(), "cookie", id, slot, {});
    BOOST_CHECK_THROW(second->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(second->isCoreReady(), std::exception);
    BOOST_CHECK(second->authenticate("cookie"));
    BOOST_CHECK(second->isCoreReady());
}

// One GUI at a time: the first connection to authenticate claims the listener's
// attach slot, and a second authenticated connection is told another GUI is
// attached. It may still read what the handshake needs, but every factory is
// refused.
BOOST_AUTO_TEST_CASE(second_connection_does_not_get_the_slot)
{
    auto slot = ipc::MakeAttachSlot();
    auto a = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, {});
    auto b = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, {});

    // Asking before authentication is refused like every other gated method.
    BOOST_CHECK_THROW(a->holdsAttachSlot(), std::exception);

    BOOST_REQUIRE(a->authenticate("c"));
    BOOST_REQUIRE(b->authenticate("c"));
    BOOST_CHECK(a->holdsAttachSlot());
    BOOST_CHECK(!b->holdsAttachSlot());
    BOOST_CHECK(slot->Holder().has_value());

    BOOST_CHECK_NO_THROW(b->getBuildInfo());
    BOOST_CHECK_NO_THROW(b->getIdentity());
    BOOST_CHECK(b->isCoreReady());

    const HasMessage attached{"already attached"};
    BOOST_CHECK_EXCEPTION(b->makeNode(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeStakingStatus(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeWallet(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeWalletTxSource(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeWalletCoinSource(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeMRC(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeVotingManager(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeResearcherContext(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makePSGTPoolContext(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeSideStakeManager(), std::runtime_error, attached);
    BOOST_CHECK_EXCEPTION(b->makeGuiState(), std::runtime_error, attached);

    // The listener keeps a connection past the authentication deadline only when
    // both hold, so this one is dropped then.
    BOOST_CHECK(!b->isAuthenticated() || !b->holdsAttachSlot());

    // The slot holder is served.
    BOOST_CHECK_NO_THROW(a->makeNode());

    // A refused connection going away leaves the holder in place.
    b->connectionClosed();
    BOOST_CHECK(a->holdsAttachSlot());
    auto c = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, {});
    BOOST_REQUIRE(c->authenticate("c"));
    BOOST_CHECK(!c->holdsAttachSlot());
}

// The holder's teardown runs the slot hook before it gives the slot back, so a
// capability of the GUI that went away is ended before another GUI can attach.
// While the hook is still running, a new connection is refused the slot; once it
// has finished, the next connection gets it.
BOOST_AUTO_TEST_CASE(slot_hook_runs_before_release)
{
    auto slot = ipc::MakeAttachSlot();
    std::promise<void> entered;
    std::promise<void> gate;
    const std::shared_future<void> gate_future{gate.get_future()};
    std::atomic<bool> hook_done{false};

    auto a = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, [&] {
        entered.set_value();
        gate_future.wait_for(WAIT_CAP);
        hook_done = true;
    });
    BOOST_REQUIRE(a->authenticate("c"));

    std::thread closer;
    JoinOnExit join_closer{closer};
    closer = std::thread([&] { a->connectionClosed(); });
    BOOST_REQUIRE(entered.get_future().wait_for(WAIT_CAP) == std::future_status::ready);

    // The hook is blocked, so the slot is still a's.
    auto c = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, {});
    BOOST_REQUIRE(c->authenticate("c"));
    BOOST_CHECK(!c->holdsAttachSlot());

    gate.set_value();
    closer.join();
    BOOST_CHECK(hook_done);
    BOOST_CHECK(!slot->Holder().has_value());

    auto d = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, {});
    BOOST_REQUIRE(d->authenticate("c"));
    BOOST_CHECK(d->holdsAttachSlot());
}

// A connection that never authenticated never held the slot: closing it releases
// nothing and does not run the slot hook (which would end the attached GUI's
// capability).
BOOST_AUTO_TEST_CASE(unauthenticated_close_releases_nothing)
{
    auto slot = ipc::MakeAttachSlot();
    int a_hook_runs = 0;
    int squatter_hook_runs = 0;
    auto a = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, [&] { ++a_hook_runs; });
    BOOST_REQUIRE(a->authenticate("c"));
    const auto holder = slot->Holder();
    BOOST_REQUIRE(holder.has_value());

    auto squatter = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, [&] { ++squatter_hook_runs; });
    squatter->connectionClosed();
    BOOST_CHECK(slot->Holder() == holder);
    BOOST_CHECK_EQUAL(squatter_hook_runs, 0);
    BOOST_CHECK_EQUAL(a_hook_runs, 0);

    // Nor does one whose single attempt failed.
    auto wrong = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, [&] { ++squatter_hook_runs; });
    BOOST_CHECK(!wrong->authenticate("not the cookie"));
    wrong->connectionClosed();
    BOOST_CHECK(slot->Holder() == holder);
    BOOST_CHECK_EQUAL(squatter_hook_runs, 0);

    // The holder's own close runs its hook once, however often it is called.
    a->connectionClosed();
    a->connectionClosed();
    BOOST_CHECK_EQUAL(a_hook_runs, 1);
    BOOST_CHECK(!slot->Holder().has_value());
}

// Teardown can run on the event-loop thread while an authenticate() call of the
// same connection is still running on its worker. A claim that would land after
// the connection was closed must not keep the slot, or no GUI could attach again
// until the core restarts; and the slot hook must not run for it, since another
// GUI may be attached by then and the hook would end that GUI's capability.
BOOST_AUTO_TEST_CASE(authenticate_after_close_does_not_keep_the_slot)
{
    auto slot = ipc::MakeAttachSlot();
    int hook_runs = 0;
    auto late = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, [&] { ++hook_runs; });

    late->connectionClosed();
    late->authenticate("c");

    BOOST_CHECK(!slot->Holder().has_value());
    BOOST_CHECK_EQUAL(hook_runs, 0);
    BOOST_CHECK_EXCEPTION(late->makeNode(), std::runtime_error, HasMessage("already attached"));

    auto next = ipc::MakeServeInit(std::make_unique<StubInit>(), "c", {}, slot, {});
    BOOST_REQUIRE(next->authenticate("c"));
    BOOST_CHECK(next->holdsAttachSlot());
}

// The slot is keyed by connection: a TryClaim by the holder is idempotent, and a
// Release by anyone else does nothing.
BOOST_AUTO_TEST_CASE(attach_slot_is_keyed_by_connection)
{
    auto slot = ipc::MakeAttachSlot();
    BOOST_CHECK(!slot->Holder().has_value());

    BOOST_CHECK(slot->TryClaim(7));
    BOOST_CHECK(slot->TryClaim(7));
    BOOST_CHECK(!slot->TryClaim(8));
    BOOST_CHECK(slot->Holder() == std::optional<uint64_t>{7});

    slot->Release(8);
    BOOST_CHECK(slot->Holder() == std::optional<uint64_t>{7});

    slot->Release(7);
    BOOST_CHECK(!slot->Holder().has_value());
    BOOST_CHECK(slot->TryClaim(8));
}

// The attached GUI going away ends the wallet's IPC capability, through the hook
// the daemon installs, and a refused second GUI going away does not. The
// capability is begun over the served wallet, as the GUI begins it, so the
// wallet the slot holder made is the one whose capability ends.
BOOST_AUTO_TEST_CASE(slot_holder_close_ends_the_ipc_capability)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    auto slot = ipc::MakeAttachSlot();
    auto a = ipc::MakeServeInit(std::make_unique<MainWalletInit>(), "c", {}, slot, DaemonSlotHook);
    auto b = ipc::MakeServeInit(std::make_unique<MainWalletInit>(), "c", {}, slot, DaemonSlotHook);
    BOOST_REQUIRE(a->authenticate("c"));
    BOOST_REQUIRE(b->authenticate("c"));
    BOOST_REQUIRE(a->holdsAttachSlot());

    std::unique_ptr<interfaces::Wallet> wallet = a->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);
    BOOST_REQUIRE(wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    BOOST_REQUIRE(fx.wallet.IpcCapability().has_value());

    b->connectionClosed();
    BOOST_CHECK(fx.wallet.IpcCapability().has_value());

    a->connectionClosed();
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    wallet.reset();
}

// The serving Init binds what it hands out to its connection: once the holder's
// connection has closed and the next GUI has attached and begun a capability,
// the console the first connection made does not run under it.
BOOST_AUTO_TEST_CASE(served_interfaces_are_bound_to_their_connection)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    const std::vector<std::string> args{EncodeDestination(fx.id), "a message"};

    auto slot = ipc::MakeAttachSlot();
    auto a = ipc::MakeServeInit(std::make_unique<MainWalletInit>(), "c", {}, slot, DaemonSlotHook);
    BOOST_REQUIRE(a->authenticate("c"));
    std::unique_ptr<interfaces::Node> a_node = a->makeNode();
    BOOST_REQUIRE(a_node != nullptr);
    a->connectionClosed();

    auto d = ipc::MakeServeInit(std::make_unique<MainWalletInit>(), "c", {}, slot, DaemonSlotHook);
    BOOST_REQUIRE(d->authenticate("c"));
    BOOST_REQUIRE(d->holdsAttachSlot());
    std::unique_ptr<interfaces::Wallet> d_wallet = d->makeWallet();
    std::unique_ptr<interfaces::Node> d_node = d->makeNode();
    BOOST_REQUIRE(d_wallet != nullptr);
    BOOST_REQUIRE(d_node != nullptr);
    BOOST_REQUIRE(d_wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::RpcRequest));

    interfaces::RpcConsoleResult result = a_node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.wallet_unlock_needed);
    result = d_node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(result.ok);

    d->connectionClosed();
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_SUITE_END()
