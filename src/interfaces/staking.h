// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_STAKING_H
#define GRIDCOIN_INTERFACES_STAKING_H

#include <memory>
#include <optional>
#include <string>

namespace interfaces {

//! Staking/miner status interface for the GUI: queries only -- status-change
//! notifications arrive through Node::handleMinerStatusChanged. The same
//! boundary rules as interfaces::Node apply (src/interfaces/README.md).
//!
//! The try* variants return std::nullopt instead of blocking when cs_main is
//! contended; they exist for GUI-thread slots that must never wait on core
//! locks. The blocking getNetworkWeight() serves lazy first-read paths that
//! historically blocked.
class StakingStatus
{
public:
    virtual ~StakingStatus() = default;

    //! Whether the miner is currently staking.
    virtual bool isStaking() = 0;

    //! Human-readable summary of why the miner is not staking (empty when
    //! there is nothing to report).
    virtual std::string getErrors() = 0;

    //! Estimated network staking weight. Blocking: takes cs_main.
    virtual double getNetworkWeight() = 0;

    //! Non-blocking variant of getNetworkWeight().
    virtual std::optional<double> tryGetNetworkWeight() = 0;

    //! Estimated time to stake in days, or std::nullopt when cs_main is
    //! contended.
    virtual std::optional<double> tryGetEstimatedTimeToStake() = 0;
};

//! Return an in-process StakingStatus interface implementation.
std::unique_ptr<StakingStatus> MakeStakingStatus();

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_STAKING_H
