// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/connection_scope.h"
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
#include "ipc/serve_init.h"
#include "key_io.h"
#include "test/staking_test_peers.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef WIN32
#include "ipc/win_attest.h"
#include "test/wine.h"
#include "util/win_acl.h"

#include <utility>
#include <vector>

#include <windows.h>
#endif

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
//! wallet's IPC capability. Unchecked, as there: it runs as the connection is torn
//! down, and its job is to end the slot.
void DaemonSlotHook()
{
    if (pwalletMain) pwalletMain->EndIpcElevation({});
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

//! The options the daemon serves with on Windows: the identity check required.
ipc::ServeInitOptions RequireAttestation()
{
    ipc::ServeInitOptions options;
    options.require_attestation = true;
    return options;
}

#ifdef WIN32
//! Whether these cases can rely on Windows' own security model. Wine impersonates
//! the process's own token and keeps no real access control, so under it they only
//! log.
bool SkipUnderWine()
{
    if (!grc_test::RunningUnderWine()) return false;
    BOOST_TEST_MESSAGE("Wine has no Windows security model for pipes and tokens; skipping");
    return true;
}

//! The integrity level of this process's token, a SECURITY_MANDATORY_*_RID. The
//! core admits only Medium, so a case run elevated expects a refusal.
uint32_t ProcessIntegrityRid()
{
    HANDLE raw = nullptr;
    BOOST_REQUIRE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw));
    const util::winacl::UniqueHandle token(raw);
    const std::vector<unsigned char> buffer =
        util::winacl::QueryTokenInformation(token.get(), TokenIntegrityLevel, "TokenIntegrityLevel");
    const PSID label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data())->Label.Sid;
    return *::GetSidSubAuthority(label, *::GetSidSubAuthorityCount(label) - 1u);
}

//! Whether \p e's message is \p text, exactly.
struct IsMessage {
    explicit IsMessage(std::string text) : m_text(std::move(text)) {}
    bool operator()(const std::exception& e) const { return std::string{e.what()} == m_text; }
    std::string m_text;
};

//! What the GUI reports for a node that does not run the identity check.
const std::string NOT_CHECKED_MESSAGE{"The node did not complete the Windows identity check; use matching builds of "
                                      "gridcoinresearch and gridcoinresearchd."};

//! What the GUI reports for a node that ran the check and refused it.
const std::string REFUSED_MESSAGE{"The node refused this GUI's identity (wrong account, not in Gridcoin GUI Users, "
                                  "or running elevated). See the node's debug.log."};

//! A node of an older build: it does not have beginAttest at all, so the call fails
//! as an IPC call to an unknown method does.
struct OlderNodeInit : public interfaces::Init {
    interfaces::AttestChallenge beginAttest() override { throw std::runtime_error("method not implemented"); }
};

//! A node that runs a real check and refuses the peer whatever it read. Counts its
//! finishAttest calls.
struct RefusingNodeInit : public interfaces::Init {
    explicit RefusingNodeInit(util::winacl::Sid self) : m_server(std::move(self), std::nullopt) {}

    interfaces::AttestChallenge beginAttest() override
    {
        interfaces::AttestChallenge challenge;
        challenge.served = true;
        if (const auto begun = m_server.Begin()) {
            challenge.pipe_suffix = begun->first;
            challenge.nonce_hex = begun->second;
        }
        return challenge;
    }

    bool finishAttest() override
    {
        ++m_finish_calls;
        m_server.Finish();
        return false;
    }

    ipc::win::AttestServer m_server;
    int m_finish_calls{0};
};
#endif // WIN32
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
    BOOST_REQUIRE(fx.wallet.IpcCapability({}).has_value());

    b->connectionClosed();
    BOOST_CHECK(fx.wallet.IpcCapability({}).has_value());

    a->connectionClosed();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    wallet.reset();
}

// The serving Init binds what it hands out to its connection: once the holder's
// connection has closed and the next GUI has attached and begun a capability,
// the wallet and the console the first connection made neither use nor end it,
// and that wallet cannot begin one in its place.
BOOST_AUTO_TEST_CASE(served_interfaces_are_bound_to_their_connection)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    const std::string address = EncodeDestination(fx.id);
    const std::vector<std::string> args{address, "a message"};

    auto slot = ipc::MakeAttachSlot();
    auto a = ipc::MakeServeInit(std::make_unique<MainWalletInit>(), "c", {}, slot, DaemonSlotHook);
    BOOST_REQUIRE(a->authenticate("c"));
    std::unique_ptr<interfaces::Wallet> a_wallet = a->makeWallet();
    std::unique_ptr<interfaces::Node> a_node = a->makeNode();
    BOOST_REQUIRE(a_wallet != nullptr);
    BOOST_REQUIRE(a_node != nullptr);
    a->connectionClosed();

    auto d = ipc::MakeServeInit(std::make_unique<MainWalletInit>(), "c", {}, slot, DaemonSlotHook);
    BOOST_REQUIRE(d->authenticate("c"));
    BOOST_REQUIRE(d->holdsAttachSlot());
    std::unique_ptr<interfaces::Wallet> d_wallet = d->makeWallet();
    std::unique_ptr<interfaces::Node> d_node = d->makeNode();
    BOOST_REQUIRE(d_wallet != nullptr);
    BOOST_REQUIRE(d_node != nullptr);

    BOOST_REQUIRE(d_wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    const std::optional<CapabilityId> d_id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(d_id.has_value());

    std::string signature;
    BOOST_CHECK(a_wallet->signMessage(address, "a message", signature) == interfaces::MessageSignStatus::KeyNotAvailable);
    a_wallet->endElevation();
    BOOST_CHECK(fx.wallet.IpcCapability({}) == d_id);

    // A begins another kind, so what the slot holds afterwards tells the two
    // apart. Comparing ids alone would not: the unit tests' generator repeats
    // its draws, so a new capability can take a freed id's number.
    BOOST_CHECK(!a_wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    BOOST_CHECK(fx.wallet.IpcCapability({}) == d_id);
    {
        CapabilityScope scope(fx.wallet, fx.wallet.IpcCapability({}), CapabilityKind::SignMessage);
        BOOST_CHECK(scope.Active());
    }
    BOOST_CHECK(d_wallet->signMessage(address, "a message", signature) == interfaces::MessageSignStatus::OK);

    BOOST_REQUIRE(d_wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::RpcRequest));
    interfaces::RpcConsoleResult result = a_node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.wallet_unlock_needed);
    result = d_node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(result.ok);

    d->connectionClosed();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

//! A side-stake manager that only records the connection it is bound to.
struct BindRecordingSideStakeManager : public interfaces::SideStakeManager {
    std::shared_ptr<std::shared_ptr<interfaces::ConnectionScope>> m_bound;

    explicit BindRecordingSideStakeManager(std::shared_ptr<std::shared_ptr<interfaces::ConnectionScope>> bound)
        : m_bound(std::move(bound)) {}

    interfaces::SideStakeSnapshot entries() override { return {}; }
    uint64_t localRevision() override { return 0; }
    interfaces::SideStakeEditResult addLocal(const std::string&, double, const std::string&) override { return {}; }
    interfaces::SideStakeEditResult setAllocation(const std::string&, double) override { return {}; }
    interfaces::SideStakeEditResult setDescription(const std::string&, const std::string&) override { return {}; }
    interfaces::SideStakeEditResult deleteLocal(const std::string&) override { return {}; }
    std::unique_ptr<interfaces::Handler> handleRwSettingsUpdated(interfaces::RwSettingsUpdatedFn) override
    {
        return nullptr;
    }
    std::unique_ptr<interfaces::Handler> handleMandatorySideStakeChanged(
        interfaces::MandatorySideStakeChangedFn) override
    {
        return nullptr;
    }
    void bindConnectionScope(std::shared_ptr<interfaces::ConnectionScope> scope) override
    {
        *m_bound = std::move(scope);
    }
};

// The side-stake manager runs its edits under the client's Settings capability,
// so the serving Init binds it to its connection as it binds the wallet: the
// manager sees the connection close.
BOOST_AUTO_TEST_CASE(served_side_stake_manager_is_bound_to_its_connection)
{
    auto bound = std::make_shared<std::shared_ptr<interfaces::ConnectionScope>>();

    struct SideStakeInit : public interfaces::Init {
        std::shared_ptr<std::shared_ptr<interfaces::ConnectionScope>> m_bound;
        std::unique_ptr<interfaces::SideStakeManager> makeSideStakeManager() override
        {
            return std::make_unique<BindRecordingSideStakeManager>(m_bound);
        }
    };
    auto inner = std::make_unique<SideStakeInit>();
    inner->m_bound = bound;

    auto slot = ipc::MakeAttachSlot();
    auto served = ipc::MakeServeInit(std::move(inner), "c", {}, slot, DaemonSlotHook);
    BOOST_REQUIRE(served->authenticate("c"));

    std::unique_ptr<interfaces::SideStakeManager> sidestakes = served->makeSideStakeManager();
    BOOST_REQUIRE(sidestakes != nullptr);
    BOOST_REQUIRE(*bound != nullptr);
    BOOST_CHECK(!(*bound)->closed());

    served->connectionClosed();
    BOOST_CHECK((*bound)->closed());
}

// With the identity check required, as the daemon serves on Windows, no cookie is
// compared until the check has admitted the connection's peer. authenticate()
// fails, and the failure uses up the connection's one attempt, so the right cookie
// is refused from then on as well.
BOOST_AUTO_TEST_CASE(serve_init_requires_attestation_when_configured)
{
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {},
                                    RequireAttestation());

    BOOST_CHECK(!serve->authenticate("the-cookie"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK(!serve->authenticate("the-cookie"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(serve->isCoreReady(), std::exception);
}

// A connection gets one identity check. Finishing a check that was never begun
// fails; a second beginAttest() is answered, but with no pipe; and the one check
// finishes once. The first answer has a pipe only where the check can run.
BOOST_AUTO_TEST_CASE(serve_init_attestation_one_shot)
{
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {},
                                    RequireAttestation());

    BOOST_CHECK(!serve->finishAttest());

    const interfaces::AttestChallenge first = serve->beginAttest();
    BOOST_CHECK(first.served);
#ifdef WIN32
    if (!SkipUnderWine()) {
        BOOST_CHECK(ipc::win::IsValidPipeSuffix(first.pipe_suffix));
        BOOST_CHECK(ipc::win::IsValidNonceHex(first.nonce_hex));
    }
#else
    BOOST_CHECK(first.pipe_suffix.empty());
    BOOST_CHECK(first.nonce_hex.empty());
#endif

    const interfaces::AttestChallenge second = serve->beginAttest();
    BOOST_CHECK(second.served);
    BOOST_CHECK(second.pipe_suffix.empty());
    BOOST_CHECK(second.nonce_hex.empty());

    // No client wrote the nonce, so the check fails, and it cannot be finished again.
    BOOST_CHECK(!serve->finishAttest());
    BOOST_CHECK(!serve->finishAttest());
    BOOST_CHECK(!serve->authenticate("the-cookie"));
}

// Served as the POSIX daemon serves, without the requirement, the check is still
// answered, so a GUI can tell this node knows it, but nothing is begun, and the
// cookie alone decides authentication.
BOOST_AUTO_TEST_CASE(serve_init_attestation_not_required_on_posix_path)
{
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {});

    const interfaces::AttestChallenge challenge = serve->beginAttest();
    BOOST_CHECK(challenge.served);
    BOOST_CHECK(challenge.pipe_suffix.empty());
    BOOST_CHECK(challenge.nonce_hex.empty());
    BOOST_CHECK(!serve->finishAttest());

    BOOST_CHECK(serve->authenticate("the-cookie"));
    BOOST_CHECK(serve->isAuthenticated());
}

#ifdef WIN32
// The core and the GUI in one process: the GUI opens the pipe the core named, checks
// that it is this account's, writes the nonce, and the core admits it, so the cookie
// is then accepted. The core admits Medium integrity only, so a run that is elevated
// is refused at both steps instead.
BOOST_AUTO_TEST_CASE(serve_init_attest_then_authenticate_in_process)
{
    if (SkipUnderWine()) return;
    const bool medium = ProcessIntegrityRid() == ipc::win::INTEGRITY_MEDIUM_RID;
    BOOST_TEST_MESSAGE("this process runs at " << (medium ? "Medium" : "another") << " integrity");

    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {},
                                    RequireAttestation());

    const interfaces::AttestChallenge challenge = serve->beginAttest();
    BOOST_REQUIRE(challenge.served);
    BOOST_REQUIRE(ipc::win::IsValidPipeSuffix(challenge.pipe_suffix));
    BOOST_REQUIRE(ipc::win::IsValidNonceHex(challenge.nonce_hex));

    std::string error;
    std::optional<ipc::win::AttestClientPipe> pipe =
        ipc::win::AttestClientPipe::Open(challenge.pipe_suffix, util::winacl::CurrentTokenUser(), error);
    BOOST_REQUIRE_MESSAGE(pipe.has_value(), error);
    BOOST_REQUIRE_MESSAGE(pipe->WriteNonce(challenge.nonce_hex, error), error);

    BOOST_CHECK_EQUAL(serve->finishAttest(), medium);
    pipe.reset();

    // The connection's one check is used.
    BOOST_CHECK(!serve->finishAttest());
    BOOST_CHECK(serve->beginAttest().pipe_suffix.empty());

    BOOST_CHECK_EQUAL(serve->authenticate("the-cookie"), medium);
    BOOST_CHECK_EQUAL(serve->isAuthenticated(), medium);
    if (medium) BOOST_CHECK_NO_THROW(serve->getBuildInfo());
}

// An authentication refused for want of the check is the connection's one
// attempt: a check that admits the peer afterwards does not reopen it.
BOOST_AUTO_TEST_CASE(serve_init_attest_after_refused_authentication_does_not_reopen_it)
{
    if (SkipUnderWine()) return;
    const bool medium = ProcessIntegrityRid() == ipc::win::INTEGRITY_MEDIUM_RID;

    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {},
                                    RequireAttestation());
    BOOST_CHECK(!serve->authenticate("the-cookie"));

    if (medium) {
        BOOST_CHECK_NO_THROW(ipc::win::RunClientAttestation(*serve, util::winacl::CurrentTokenUser()));
    } else {
        BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(*serve, util::winacl::CurrentTokenUser()),
                              std::runtime_error, IsMessage(REFUSED_MESSAGE));
    }
    BOOST_CHECK(!serve->authenticate("the-cookie"));
    BOOST_CHECK(!serve->isAuthenticated());
}

// The GUI's half against the serving Init: it runs the check through the Init's two
// methods, and the cookie is accepted afterwards. Elevated, the core refuses it, and
// the GUI says so.
BOOST_AUTO_TEST_CASE(run_client_attestation_against_the_serving_init)
{
    if (SkipUnderWine()) return;
    const bool medium = ProcessIntegrityRid() == ipc::win::INTEGRITY_MEDIUM_RID;

    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {},
                                    RequireAttestation());
    if (medium) {
        BOOST_CHECK_NO_THROW(ipc::win::RunClientAttestation(*serve, util::winacl::CurrentTokenUser()));
    } else {
        BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(*serve, util::winacl::CurrentTokenUser()),
                              std::runtime_error, IsMessage(REFUSED_MESSAGE));
    }
    BOOST_CHECK_EQUAL(serve->authenticate("the-cookie"), medium);
}

// A node that does not run the check -- one that does not serve it, one of an
// older build whose beginAttest fails, or one that answers without a pipe -- is
// refused before anything is written, and the GUI names the cause.
BOOST_AUTO_TEST_CASE(run_client_attestation_refuses_a_node_without_the_check)
{
    const util::winacl::Sid self = util::winacl::CurrentTokenUser();

    StubInit not_serving;
    BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(not_serving, self), std::runtime_error,
                          IsMessage(NOT_CHECKED_MESSAGE));

    OlderNodeInit older;
    BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(older, self), std::runtime_error,
                          IsMessage(NOT_CHECKED_MESSAGE));

    auto unchecked = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {});
    BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(*unchecked, self), std::runtime_error,
                          HasMessage("did not begin the Windows identity check"));
}

// A node that runs the check and refuses the peer: the GUI says it was refused, after
// the one finishAttest call.
BOOST_AUTO_TEST_CASE(run_client_attestation_reports_a_refusal)
{
    if (SkipUnderWine()) return;
    const util::winacl::Sid self = util::winacl::CurrentTokenUser();

    RefusingNodeInit refusing(self);
    BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(refusing, self), std::runtime_error,
                          IsMessage(REFUSED_MESSAGE));
    BOOST_CHECK_EQUAL(refusing.m_finish_calls, 1);
}

// The GUI checks the pipe's owner before it writes the nonce. Expecting another
// account (SYSTEM), it refuses this account's pipe, names the owner, and never asks
// the node to finish, so the node admits nobody.
BOOST_AUTO_TEST_CASE(run_client_attestation_checks_the_pipe_owner)
{
    if (SkipUnderWine()) return;

    RefusingNodeInit node(util::winacl::CurrentTokenUser());
    BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(node, util::winacl::wellknown::System()),
                          std::runtime_error, HasMessage("not by the expected node account S-1-5-18"));
    BOOST_CHECK_EQUAL(node.m_finish_calls, 0);

    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "the-cookie", {}, ipc::MakeAttachSlot(), {},
                                    RequireAttestation());
    BOOST_CHECK_EXCEPTION(ipc::win::RunClientAttestation(*serve, util::winacl::wellknown::System()),
                          std::runtime_error, HasMessage("not by the expected node account S-1-5-18"));
    BOOST_CHECK(!serve->finishAttest());
    BOOST_CHECK(!serve->authenticate("the-cookie"));
}
#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
