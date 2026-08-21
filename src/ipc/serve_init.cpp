// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/serve_init.h"

#include "ipc/handshake.h"
// Complete types needed for the unique_ptr/shared_ptr the delegated factory
// methods return (interfaces/init.h only forward-declares them).
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

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace ipc {
namespace {

//! Auth-gating wrapper served to IPC clients. A FRESH ServeInit is built per
//! accepted connection (CapnpProtocol::listen via the make_serve_init factory),
//! so the authenticated flag is PER-CONNECTION: it starts false and every peer
//! must present the cookie itself -- a new connection can never inherit an
//! earlier peer's authenticated session, and the flag is destroyed with the
//! connection. authenticate() only ever *sets* the flag on a valid cookie (it
//! never clears it), so a later bad-cookie call on the SAME connection cannot
//! de-authenticate it.
//!
//! This wrapper is hand-maintained and must stay in step with interfaces::Init:
//! every virtual there needs an override here that calls RequireAuth() first.
//! test/lint/lint-serve-init-complete.py enforces that (a missing override fails
//! closed but silently; an override without RequireAuth() fails open).
class ServeInit : public interfaces::Init
{
public:
    ServeInit(std::unique_ptr<interfaces::Init> inner, std::string cookie, interfaces::NodeIdentity identity)
        : m_inner(std::move(inner)), m_cookie(std::move(cookie)), m_identity(std::move(identity))
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
        // oracle, and a way to hold the single connection slot indefinitely.
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
        return m_inner->makeNode();
    }

    std::unique_ptr<interfaces::StakingStatus> makeStakingStatus() override
    {
        RequireAuth();
        return m_inner->makeStakingStatus();
    }

    std::unique_ptr<interfaces::Wallet> makeWallet() override
    {
        RequireAuth();
        return m_inner->makeWallet();
    }

    std::shared_ptr<interfaces::WalletTxSource> makeWalletTxSource() override
    {
        RequireAuth();
        return m_inner->makeWalletTxSource();
    }

    std::shared_ptr<interfaces::WalletCoinSource> makeWalletCoinSource() override
    {
        RequireAuth();
        return m_inner->makeWalletCoinSource();
    }

    std::unique_ptr<interfaces::MRC> makeMRC() override
    {
        RequireAuth();
        return m_inner->makeMRC();
    }

    std::unique_ptr<interfaces::VotingManager> makeVotingManager() override
    {
        RequireAuth();
        return m_inner->makeVotingManager();
    }

    std::unique_ptr<interfaces::ResearcherContext> makeResearcherContext() override
    {
        RequireAuth();
        return m_inner->makeResearcherContext();
    }

    std::unique_ptr<interfaces::PSGTPoolContext> makePSGTPoolContext() override
    {
        RequireAuth();
        return m_inner->makePSGTPoolContext();
    }

    std::unique_ptr<interfaces::SideStakeManager> makeSideStakeManager() override
    {
        RequireAuth();
        return m_inner->makeSideStakeManager();
    }

private:
    void RequireAuth() const
    {
        if (m_auth_state.load() != AuthState::Authenticated) {
            throw std::runtime_error("IPC peer is not authenticated");
        }
    }

    std::unique_ptr<interfaces::Init> m_inner;
    std::string m_cookie;
    interfaces::NodeIdentity m_identity;
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

std::unique_ptr<interfaces::Init> MakeServeInit(std::unique_ptr<interfaces::Init> inner,
                                                std::string cookie,
                                                interfaces::NodeIdentity identity)
{
    return std::make_unique<ServeInit>(std::move(inner), std::move(cookie), std::move(identity));
}

} // namespace ipc
