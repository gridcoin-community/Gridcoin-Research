// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/staking.h"

#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "main.h"
#include "sync.h"

#include <memory>

namespace interfaces {
namespace {

//! In-process StakingStatus implementation wrapping g_miner_status and the
//! staking difficulty helpers. The try* variants preserve the GUI's
//! historical TRY_LOCK(cs_main) semantics: skip the refresh rather than
//! block a GUI-thread slot on a contended lock.
class StakingStatusImpl : public StakingStatus
{
public:
    bool isStaking() override { return g_miner_status.StakingActive(); }

    std::string getErrors() override { return g_miner_status.FormatErrors(); }

    double getNetworkWeight() override
    {
        LOCK(cs_main);
        return GRC::GetEstimatedNetworkWeight();
    }

    std::optional<double> tryGetNetworkWeight() override
    {
        TRY_LOCK(cs_main, locked);

        if (!locked) {
            return std::nullopt;
        }

        return GRC::GetEstimatedNetworkWeight();
    }

    std::optional<double> tryGetEstimatedTimeToStake() override
    {
        TRY_LOCK(cs_main, locked);

        if (!locked) {
            return std::nullopt;
        }

        return GRC::GetEstimatedTimetoStake();
    }
};

} // namespace

std::unique_ptr<StakingStatus> MakeStakingStatus()
{
    return std::make_unique<StakingStatusImpl>();
}

} // namespace interfaces
