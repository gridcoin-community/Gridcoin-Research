// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/serve_init.h"

#include "ipc/handshake.h"
// Complete types needed for the unique_ptr<Node>/<StakingStatus> the delegated
// factory methods return (interfaces/init.h only forward-declares them).
#include "interfaces/node.h"
#include "interfaces/staking.h"

#include <stdexcept>
#include <utility>

namespace ipc {
namespace {

//! Auth-gating wrapper served to IPC clients. The authenticated flag is per
//! served-Init object (one per node process); with the 0600 socket + per-user
//! ownership that is sufficient for the single-GUI v1. Per-connection auth state
//! (for multiple simultaneous clients) is a later hardening.
class ServeInit : public interfaces::Init
{
public:
    ServeInit(std::unique_ptr<interfaces::Init> inner, std::string cookie, interfaces::NodeIdentity identity)
        : m_inner(std::move(inner)), m_cookie(std::move(cookie)), m_identity(std::move(identity))
    {
    }

    bool authenticate(const std::string& cookie) override
    {
        m_authenticated = ConstantTimeEqual(cookie, m_cookie);
        return m_authenticated;
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
