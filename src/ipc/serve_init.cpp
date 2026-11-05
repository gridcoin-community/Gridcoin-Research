// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/serve_init.h"

#include "interfaces/connection_scope.h"
#include "ipc/handshake.h"
// Complete types needed for the unique_ptr/shared_ptr the delegated factory
// methods return (interfaces/init.h only forward-declares them).
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

#include "ipc/peercred.h"
#include "logging.h"
#include "sync.h"

#ifdef WIN32
#include "ipc/win_attest.h"
#include "ipc/win_endpoint.h"
#include "util/win_acl.h"
#endif

#include <atomic>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ipc {

AttachSlot::~AttachSlot() = default;

namespace {

class AttachSlotImpl final : public AttachSlot
{
public:
    bool TryClaim(uint64_t conn_id) override
    {
        LOCK(m_mutex);
        if (m_holder && *m_holder != conn_id) return false;
        m_holder = conn_id;
        return true;
    }

    void Release(uint64_t conn_id) override
    {
        LOCK(m_mutex);
        if (m_holder == conn_id) m_holder.reset();
    }

    std::optional<uint64_t> Holder() const override
    {
        LOCK(m_mutex);
        return m_holder;
    }

private:
    //! A leaf: nothing else is locked or called under it.
    mutable Mutex m_mutex;
    std::optional<uint64_t> m_holder GUARDED_BY(m_mutex){};
};

//! Names each served connection for the attach slot. Starts at 1 and is never
//! reused, so a connection's late Release can never free a later one's slot.
std::atomic<uint64_t> g_next_connection_id{1};

//! Auth-gating wrapper served to IPC clients. A FRESH ServeInit is built per
//! accepted connection (CapnpProtocol::listen via the make_serve_init factory),
//! so the authenticated flag is PER-CONNECTION: it starts false and every peer
//! must present the cookie itself -- a new connection can never inherit an
//! earlier peer's authenticated session, and the flag is destroyed with the
//! connection. authenticate() only ever *sets* the flag on a valid cookie (it
//! never clears it), so a later bad-cookie call on the SAME connection cannot
//! de-authenticate it.
//!
//! One GUI at a time: every ServeInit of a listener shares one AttachSlot. The
//! connection that claims it at authentication is the attached GUI and gets the
//! makeX factories; another that authenticates meanwhile can still complete the
//! handshake reads (build info, identity, readiness) and ask holdsAttachSlot(),
//! but every factory refuses it, and the listener drops it at the authentication
//! deadline.
//!
//! Each connection owns a ConnectionScope of teardown hooks. The listener calls
//! connectionClosed() on the IPC event-loop thread when the connection is torn
//! down, before it can serve another GUI; it runs the hooks (the slot hook first,
//! which in the daemon ends the wallet's IPC capability) and only then gives the
//! slot back.
//!
//! Each connection also gets one Windows identity check (beginAttest, then
//! finishAttest), served before authentication. On Windows it owns the
//! connection's one ipc::win::AttestServer, made by the beginAttest that begins
//! the check, and accepts the facts that server's Finish returned and no other
//! connection's. With ServeInitOptions::require_attestation, authenticate()
//! compares the cookie only once that check has admitted the peer.
//!
//! This wrapper is hand-maintained and must stay in step with interfaces::Init:
//! every virtual there needs an override here that calls RequireAuth() first.
//! test/lint/lint-serve-init-complete.py enforces that (a missing override fails
//! closed but silently; an override without RequireAuth() fails open).
class ServeInit : public interfaces::Init
{
public:
    ServeInit(std::unique_ptr<interfaces::Init> inner, std::string cookie, interfaces::NodeIdentity identity,
              std::shared_ptr<AttachSlot> slot, std::function<void()> on_slot_closed, ServeInitOptions options)
        : m_inner(std::move(inner)), m_cookie(std::move(cookie)), m_identity(std::move(identity)),
          m_slot(std::move(slot)), m_on_slot_closed(std::move(on_slot_closed)), m_options(options)
    {
    }

    bool authenticate(const std::string& cookie) override
    {
        // Fail closed on an empty expected cookie. ConstantTimeEqual("", "") is
        // true, so without this an empty m_cookie would authenticate EVERY peer.
        // Not reachable today (WriteCookie always produces 64 hex chars), but this
        // is the one place where getting it wrong is unauthenticated wallet access,
        // so it is checked rather than assumed.
        if (m_cookie.empty()) {
            LogPrintf("ERROR: %s: refusing to authenticate: no IPC cookie is set", __func__);
            return false;
        }

        // One strike. The design of record says the node disconnects on a cookie
        // mismatch; previously a wrong cookie merely returned false and left the
        // peer free to retry forever on the same connection -- an offline guessing
        // oracle, and a way to hold a connection slot indefinitely.
        //
        // Latching the failure is what this wrapper can enforce on its own: a
        // connection that got the cookie wrong can never authenticate, so it is
        // inert, and ipc::IPC_AUTH_DEADLINE reclaims its slot. (The drop is not
        // instantaneous: authenticate() runs on a server worker thread and closing
        // the connection is an event-loop-thread operation, so an immediate close
        // would mean re-entering connection teardown across threads from inside the
        // auth path. A bounded delay buys the same property without that risk.)
        //
        // A legitimate GUI is unaffected: it presents one cookie and disconnects
        // itself if the node rejects it.

        // Claim the connection's single attempt. Losing this race means another
        // worker already holds or has finished the attempt, so this call is refused
        // WITHOUT its cookie being compared -- concurrent callers must not each get
        // a guess.
        AuthState expected = AuthState::Pending;
        if (!m_auth_state.compare_exchange_strong(expected, AuthState::InProgress)) {
            LogPrintf("WARN: %s: refusing an authentication attempt on a connection whose single "
                      "attempt was already claimed", __func__);
            return false;
        }

        // Where the identity check is required, the cookie is compared only on a
        // connection whose peer it admitted. This attempt is the connection's one,
        // so a check that admits the peer later does not reopen it.
        if (m_options.require_attestation && m_attest_state.load() != AttestState::Attested) {
            m_auth_state.store(AuthState::Failed);
            LogPrintf("WARN: %s: refusing to authenticate an IPC peer the Windows identity check has not "
                      "admitted; this connection can no longer authenticate and will be dropped", __func__);
            return false;
        }

        // Return whether THIS cookie is valid (the client checks the result). Both
        // outcomes are terminal, so a later call on the same connection can neither
        // de-authenticate it nor buy another guess.
        const bool ok = ConstantTimeEqual(cookie, m_cookie);
        // The slot is settled before the connection reads as authenticated, so
        // nothing that passes RequireAuth() sees a claim still in progress.
        if (ok) ClaimAttachSlot();
        m_auth_state.store(ok ? AuthState::Authenticated : AuthState::Failed);
        if (!ok) {
            LogPrintf("WARN: %s: IPC peer presented an invalid cookie; this connection can no "
                      "longer authenticate and will be dropped", __func__);
        }
        return ok;
    }

    //! Served before authentication, like authenticate(), and not gated: it is
    //! part of how the peer comes to be authenticated. Every answer says the
    //! check is served. Only the call that begins the connection's one check
    //! names a pipe, and only where it is required and can run.
    interfaces::AttestChallenge beginAttest() override
    {
        interfaces::AttestChallenge challenge;
        challenge.served = true;

        // Claim the connection's one check. Losing this race means another call
        // already began it, or has already finished it.
        AttestState expected = AttestState::Pending;
        if (!m_attest_state.compare_exchange_strong(expected, AttestState::Beginning)) {
            LogPrintf("WARN: %s: refusing to begin a second Windows identity check on this connection", __func__);
            return challenge;
        }
        if (!m_options.require_attestation) {
            // Nothing to begin, and so nothing to finish.
            m_attest_state.store(AttestState::Failed);
            return challenge;
        }

#ifdef WIN32
        std::optional<std::pair<std::string, std::string>> begun;
        try {
            m_attest_self = util::winacl::CurrentTokenUser();
            m_attest_group.reset();
            if (const std::optional<std::string> group = win::AttachGroupSid()) {
                m_attest_group = util::winacl::ParseSidString(*group);
                if (!m_attest_group) throw std::logic_error("the attach group's SID does not parse");
            }
            m_attest_server = std::make_unique<win::AttestServer>(m_attest_self, m_attest_group);
            begun = m_attest_server->Begin();
        } catch (const std::exception& e) {
            LogPrintf("IPC: could not begin the Windows identity check: %s", e.what());
        }
        if (!begun) {
            m_attest_server.reset();
            m_attest_state.store(AttestState::Failed);
            return challenge;
        }
        challenge.pipe_suffix = std::move(begun->first);
        challenge.nonce_hex = std::move(begun->second);
        // The members above are set before the check reads as begun, and only the
        // finishAttest() that claims it from Begun reads them.
        m_attest_state.store(AttestState::Begun);
#else
        LogPrintf("IPC: this connection requires the Windows identity check, which this build cannot run");
        m_attest_state.store(AttestState::Failed);
#endif
        return challenge;
    }

    //! Served before authentication and not gated, as beginAttest() is. True
    //! once at most, for the check beginAttest() began. On Windows this blocks
    //! for up to ipc::win::ATTEST_FINISH_WAIT and impersonates the peer on the
    //! calling thread; init.capnp gives it a context, so it runs on the
    //! connection's worker thread, never the event loop.
    bool finishAttest() override
    {
        AttestState expected = AttestState::Begun;
        if (!m_attest_state.compare_exchange_strong(expected, AttestState::Finishing)) {
            LogPrintf("WARN: %s: refusing to finish a Windows identity check this connection has not begun, "
                      "or has already finished", __func__);
            return false;
        }

#ifdef WIN32
        bool admitted = false;
        try {
            admitted = AdmitAttestedPeer();
        } catch (const std::exception& e) {
            LogPrintf("IPC: the Windows identity check failed: %s", e.what());
        }
        // Single use, and no other call reaches it once the check is Finishing.
        m_attest_server.reset();
        m_attest_state.store(admitted ? AttestState::Attested : AttestState::Failed);
        return admitted;
#else
        // A check is begun only on Windows.
        m_attest_state.store(AttestState::Failed);
        return false;
#endif
    }

    //! Not served over capnp (no ordinal in init.capnp); the local listener calls
    //! it to decide whether this connection beat the authentication deadline.
    bool isAuthenticated() override { return m_auth_state.load() == AuthState::Authenticated; }

    bool holdsAttachSlot() override
    {
        RequireAuth();
        return m_slot_held->load();
    }

    //! Not served over capnp (no ordinal in init.capnp) and deliberately not
    //! gated: the local listener runs it at teardown for every connection,
    //! authenticated or not. The hooks run first, so whatever this connection
    //! held is ended before another connection can claim the slot.
    void connectionClosed() override
    {
        m_scope->close();
        m_slot->Release(m_conn_id);
    }

    interfaces::BuildInfo getBuildInfo() override
    {
        RequireAuth();
        return GetLocalBuildInfo();
    }

    interfaces::NodeIdentity getIdentity() override
    {
        RequireAuth();
        return m_identity;
    }

    bool isCoreReady() override
    {
        RequireAuth();
        return m_inner->isCoreReady();
    }

    std::unique_ptr<interfaces::Node> makeNode() override
    {
        RequireAuth();
        RequireAttachSlot();
        return BindToConnection(m_inner->makeNode());
    }

    std::unique_ptr<interfaces::StakingStatus> makeStakingStatus() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makeStakingStatus();
    }

    std::unique_ptr<interfaces::Wallet> makeWallet() override
    {
        RequireAuth();
        RequireAttachSlot();
        // The wallet's transfers and capabilities end with this connection.
        return BindToConnection(m_inner->makeWallet());
    }

    std::shared_ptr<interfaces::WalletTxSource> makeWalletTxSource() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makeWalletTxSource();
    }

    std::shared_ptr<interfaces::WalletCoinSource> makeWalletCoinSource() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makeWalletCoinSource();
    }

    std::unique_ptr<interfaces::MRC> makeMRC() override
    {
        RequireAuth();
        RequireAttachSlot();
        return BindToConnection(m_inner->makeMRC());
    }

    std::unique_ptr<interfaces::VotingManager> makeVotingManager() override
    {
        RequireAuth();
        RequireAttachSlot();
        return BindToConnection(m_inner->makeVotingManager());
    }

    std::unique_ptr<interfaces::ResearcherContext> makeResearcherContext() override
    {
        RequireAuth();
        RequireAttachSlot();
        return BindToConnection(m_inner->makeResearcherContext());
    }

    std::unique_ptr<interfaces::PSGTPoolContext> makePSGTPoolContext() override
    {
        RequireAuth();
        RequireAttachSlot();
        return BindToConnection(m_inner->makePSGTPoolContext());
    }

    std::unique_ptr<interfaces::SideStakeManager> makeSideStakeManager() override
    {
        RequireAuth();
        RequireAttachSlot();
        return BindToConnection(m_inner->makeSideStakeManager());
    }

    std::unique_ptr<interfaces::GuiState> makeGuiState() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makeGuiState();
    }

private:
    void RequireAuth() const
    {
        if (m_auth_state.load() != AuthState::Authenticated) {
            throw std::runtime_error("IPC peer is not authenticated");
        }
    }

    //! Bind an interface that runs operations under the client's capability to
    //! this connection, so once the connection has closed those operations run
    //! under none (bindConnectionScope).
    template <typename T>
    std::unique_ptr<T> BindToConnection(std::unique_ptr<T> made) const
    {
        if (made) made->bindConnectionScope(m_scope);
        return made;
    }

    void RequireAttachSlot() const
    {
        if (!m_slot_held->load()) {
            throw std::runtime_error("another GUI is already attached to this core");
        }
    }

#ifdef WIN32
    //! Finish this connection's check and decide on the peer it identified:
    //! ClassifyTokenFacts must not refuse it, and ServerPeerPolicy must allow
    //! the identity it read. Each refusal is logged with its reason.
    bool AdmitAttestedPeer()
    {
        const std::optional<win::TokenFacts> facts = m_attest_server->Finish();
        if (!facts) return false;

        const win::Classification verdict = win::ClassifyTokenFacts(*facts, m_attest_self, m_attest_group.has_value());
        if (verdict.cls == win::PeerClass::Refused) {
            LogPrintf("IPC: refusing the peer: %s", verdict.reason);
            return false;
        }

        PeerIdentity peer;
        peer.sid = util::winacl::FormatSid(facts->user);
        peer.integrity_rid = facts->integrity_rid;
        peer.attach_group_member = facts->member_of_attach_group;
        if (ServerPeerPolicy(peer) == PeerVerdict::Refuse) return false;

        LogPrintf("IPC: attested peer %s as %s", peer.sid,
                  verdict.cls == win::PeerClass::Owner ? "owner" : "attach-group");
        return true;
    }
#endif

    //! Claim the attach slot, once, as the connection authenticates.
    //!
    //! The listener can tear the connection down on the event-loop thread while
    //! this call is still running on its worker. What the code guarantees:
    //!  - A closed connection never keeps the slot. If the scope is closed before
    //!    the claim, nothing is claimed. If it closes between the claim and the
    //!    re-check below, the claim is given back here once close() has run the
    //!    hooks (close() on a closed scope waits for them). If it closes after
    //!    the re-check, connectionClosed() gives it back.
    //!  - The slot hook is registered before the claim and does its work only if
    //!    the held flag is set when it runs. If it runs after the flag is set, it
    //!    finishes before the slot is released, by connectionClosed() or here. If
    //!    it runs before, it does nothing: this connection has handed out nothing
    //!    yet (no factory is served until authenticate() has stored
    //!    Authenticated, after this returns), and another GUI may hold the slot
    //!    by then.
    void ClaimAttachSlot()
    {
        if (m_on_slot_closed) {
            m_scope->addCloseHook([held = m_slot_held, fn = m_on_slot_closed] {
                if (held->load()) fn();
            });
        }
        if (m_scope->closed()) return;

        if (!m_slot->TryClaim(m_conn_id)) {
            LogPrintf("IPC: another GUI is attached; this connection gets no capabilities");
            return;
        }
        m_slot_held->store(true);

        if (m_scope->closed()) {
            // close() on a closed scope returns once its hooks have run.
            m_scope->close();
            m_slot_held->store(false);
            m_slot->Release(m_conn_id);
        }
    }

    std::unique_ptr<interfaces::Init> m_inner;
    std::string m_cookie;
    interfaces::NodeIdentity m_identity;
    //! The listener's slot, shared by all its connections.
    std::shared_ptr<AttachSlot> m_slot;
    //! Run at teardown when this connection held the slot (see MakeServeInit).
    std::function<void()> m_on_slot_closed;
    const ServeInitOptions m_options;
    //! Names this connection to the slot.
    const uint64_t m_conn_id{g_next_connection_id.fetch_add(1)};
    //! This connection's teardown hooks. Shared with the wallet it hands out.
    const std::shared_ptr<interfaces::ConnectionScope> m_scope{interfaces::MakeConnectionScope()};
    //! Whether this connection holds the slot. Shared with the slot hook, which
    //! may outlive this object in the scope. ClaimAttachSlot stores this before
    //! its re-check loads the scope's closed flag; close() stores that flag
    //! before the slot hook loads this. Each side's store-then-load must stay
    //! seq_cst so at least one sees the other's store; do not relax to
    //! acquire/release.
    const std::shared_ptr<std::atomic<bool>> m_slot_held{std::make_shared<std::atomic<bool>>(false)};
    //! The connection gets exactly ONE authentication attempt, and the claim on
    //! that attempt is a single atomic transition.
    //!
    //! A previous revision used two atomic bools and did `if (m_auth_failed) return
    //! false;` before testing the cookie. Each bool was atomic, but the pair
    //! read-then-write was not: a single connection is served by more than one
    //! thread (libmultiprocess dispatches each client thread's calls to its own
    //! server worker), so N workers could all observe m_auth_failed == false, all
    //! reach ConstantTimeEqual, and all test a different cookie before any of them
    //! stored true. That turned "one strike" into "N concurrent strikes" and handed
    //! back the guessing oracle the latch existed to remove.
    //!
    //! Pending -> InProgress is a compare_exchange, so exactly one worker can ever
    //! claim the attempt; every other caller loses the race and is refused without
    //! its cookie being examined at all.
    enum class AuthState : uint8_t {
        Pending,       //!< no attempt claimed yet
        InProgress,    //!< a worker holds the single attempt
        Authenticated, //!< terminal: the cookie matched
        Failed,        //!< terminal: the cookie did not match
    };
    std::atomic<AuthState> m_auth_state{AuthState::Pending};

    //! The connection's one identity check. Pending -> Beginning is a
    //! compare_exchange, so one beginAttest() call begins it; Begun ->
    //! Finishing is another, so one finishAttest() call finishes it. The
    //! members of the check below are written by the call that holds Beginning,
    //! before it stores Begun, and then used only by the call that holds
    //! Finishing, so they need no lock.
    enum class AttestState : uint8_t {
        Pending,   //!< not begun
        Beginning, //!< a call is beginning it
        Begun,     //!< the pipe is there for the peer to open and write
        Finishing, //!< a call is finishing it
        Attested,  //!< terminal: the peer was admitted
        Failed,    //!< terminal: not admitted, or nothing to check
    };
    std::atomic<AttestState> m_attest_state{AttestState::Pending};
#ifdef WIN32
    //! This process's token user and the attach group, as the check began with.
    util::winacl::Sid m_attest_self;
    std::optional<util::winacl::Sid> m_attest_group;
    std::unique_ptr<win::AttestServer> m_attest_server;
#endif
};
} // namespace

std::shared_ptr<AttachSlot> MakeAttachSlot()
{
    return std::make_shared<AttachSlotImpl>();
}

std::unique_ptr<interfaces::Init> MakeServeInit(std::unique_ptr<interfaces::Init> inner,
                                                std::string cookie,
                                                interfaces::NodeIdentity identity,
                                                std::shared_ptr<AttachSlot> slot,
                                                std::function<void()> on_slot_closed,
                                                ServeInitOptions options)
{
    // Without a slot every connection would be served as the attached GUI.
    if (!slot) throw std::invalid_argument("MakeServeInit: no attach slot");
    return std::make_unique<ServeInit>(std::move(inner), std::move(cookie), std::move(identity),
                                       std::move(slot), std::move(on_slot_closed), options);
}

} // namespace ipc
