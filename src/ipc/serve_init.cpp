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
#include "interfaces/wallet_tx_source.h"

#include <stdexcept>
#include <utility>

namespace ipc {
namespace {

//! Auth-gating wrapper served to IPC clients. The authenticated flag lives on
//! this served object, which is shared across connections; the node caps the
//! listener at one connection (see CapnpProtocol::listen), so in the single-GUI
//! v1 there is no second peer to bleed state to. authenticate() only ever *sets*
//! the flag on a valid cookie -- it never clears it -- so a later bad-cookie call
//! cannot de-authenticate the established session. Per-connection auth state (for
//! multiple simultaneous clients) is a later hardening.
class ServeInit : public interfaces::Init
{
public:
    ServeInit(std::unique_ptr<interfaces::Init> inner, std::string cookie, interfaces::NodeIdentity identity)
        : m_inner(std::move(inner)), m_cookie(std::move(cookie)), m_identity(std::move(identity))
    {
    }

    bool authenticate(const std::string& cookie) override
    {
        // Return whether THIS cookie is valid (the client checks the result), but
        // only ever set -- never clear -- the sticky session flag.
        const bool ok = ConstantTimeEqual(cookie, m_cookie);
        if (ok) m_authenticated = true;
        return ok;
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
        if (!m_authenticated) {
            throw std::runtime_error("IPC peer is not authenticated");
        }
    }

    std::unique_ptr<interfaces::Init> m_inner;
    std::string m_cookie;
    interfaces::NodeIdentity m_identity;
    bool m_authenticated{false};
};
} // namespace

std::unique_ptr<interfaces::Init> MakeServeInit(std::unique_ptr<interfaces::Init> inner,
                                                std::string cookie,
                                                interfaces::NodeIdentity identity)
{
    return std::make_unique<ServeInit>(std::move(inner), std::move(cookie), std::move(identity));
}

} // namespace ipc
