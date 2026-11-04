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

#include "logging.h"
#include "sync.h"

#include <atomic>
#include <cstdint>
#include <stdexcept>
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
//! This wrapper is hand-maintained and must stay in step with interfaces::Init:
//! every virtual there needs an override here that calls RequireAuth() first.
//! test/lint/lint-serve-init-complete.py enforces that (a missing override fails
//! closed but silently; an override without RequireAuth() fails open).
class ServeInit : public interfaces::Init
{
public:
    ServeInit(std::unique_ptr<interfaces::Init> inner, std::string cookie, interfaces::NodeIdentity identity,
              std::shared_ptr<AttachSlot> slot, std::function<void()> on_slot_closed)
        : m_inner(std::move(inner)), m_cookie(std::move(cookie)), m_identity(std::move(identity)),
          m_slot(std::move(slot)), m_on_slot_closed(std::move(on_slot_closed))
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
        return m_inner->makeNode();
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
        std::unique_ptr<interfaces::Wallet> wallet = m_inner->makeWallet();
        // The wallet's transfers and capabilities end with this connection.
        if (wallet) wallet->bindConnectionScope(m_scope);
        return wallet;
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
        return m_inner->makeMRC();
    }

    std::unique_ptr<interfaces::VotingManager> makeVotingManager() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makeVotingManager();
    }

    std::unique_ptr<interfaces::ResearcherContext> makeResearcherContext() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makeResearcherContext();
    }

    std::unique_ptr<interfaces::PSGTPoolContext> makePSGTPoolContext() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makePSGTPoolContext();
    }

    std::unique_ptr<interfaces::SideStakeManager> makeSideStakeManager() override
    {
        RequireAuth();
        RequireAttachSlot();
        return m_inner->makeSideStakeManager();
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

    void RequireAttachSlot() const
    {
        if (!m_slot_held->load()) {
            throw std::runtime_error("another GUI is already attached to this core");
        }
    }

    //! Claim the attach slot, once, as the connection authenticates.
    //!
    //! The listener can tear the connection down on the event-loop thread while
    //! this call is still running on its worker, so the claim is made safe against
    //! a close() at any point:
    //!  - The slot hook is registered before the claim and acts only once the
    //!    claim has succeeded, so a close() that begins after the claim runs it
    //!    before connectionClosed() gives the slot back, and one that begins
    //!    before runs nothing (another GUI may be attached by then, and the hook
    //!    would end its capability).
    //!  - A claim that lands after the close began is given back here, once the
    //!    running hooks have finished. connectionClosed() found nothing to
    //!    release then, and without this no GUI could attach again until the
    //!    core restarted.
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
    //! Names this connection to the slot.
    const uint64_t m_conn_id{g_next_connection_id.fetch_add(1)};
    //! This connection's teardown hooks. Shared with the wallet it hands out.
    const std::shared_ptr<interfaces::ConnectionScope> m_scope{interfaces::MakeConnectionScope()};
    //! Whether this connection holds the slot. Shared with the slot hook, which
    //! may outlive this object in the scope.
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
                                                std::function<void()> on_slot_closed)
{
    // Without a slot every connection would be served as the attached GUI.
    if (!slot) throw std::invalid_argument("MakeServeInit: no attach slot");
    return std::make_unique<ServeInit>(std::move(inner), std::move(cookie), std::move(identity),
                                       std::move(slot), std::move(on_slot_closed));
}

} // namespace ipc
