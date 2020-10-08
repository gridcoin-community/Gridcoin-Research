// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/staking/status.h"

#include <algorithm>

using namespace GRC;

namespace {
//!
//! \brief Text descriptions to display when a wallet cannot stake.
//!
//! The sequence of this array matches the items enumerated on
//! MinerStatus::ReasonNotStakingCategory. Update this list as
//! well when those change.
//!
constexpr const char* STAKING_ERROR_STRINGS[] {
    "None",
    "No Mature Coins",
    "No coins",
    "Entire balance reserved",
    "No UTXOs available due to reserve balance",
    "Wallet locked",
    "Testnet-only version",
    "Offline",
};
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Variables
// -----------------------------------------------------------------------------

MinerStatus g_miner_status;

// -----------------------------------------------------------------------------
// Class: MinerStatus
// -----------------------------------------------------------------------------

MinerStatus::MinerStatus(void)
{
    Clear();
    ClearReasonsNotStaking();
    CreatedCnt = AcceptedCnt = KernelsFound = 0;
}

void MinerStatus::Clear()
{
    WeightSum = ValueSum = WeightMin = WeightMax = 0;
    Version = 0;
    nLastCoinStakeSearchInterval = 0;
}

bool MinerStatus::SetReasonNotStaking(ReasonNotStakingCategory not_staking_error)
{
    bool inserted = false;

    if (std::find(vReasonNotStaking.begin(), vReasonNotStaking.end(), not_staking_error) == vReasonNotStaking.end())
    {
       vReasonNotStaking.insert(vReasonNotStaking.end(), not_staking_error);

       if (not_staking_error != NONE)
       {
           if (!ReasonNotStaking.empty()) ReasonNotStaking += "; ";
           ReasonNotStaking += STAKING_ERROR_STRINGS[static_cast<int>(not_staking_error)];
       }

       if (not_staking_error > NO_MATURE_COINS) able_to_stake = false;

       inserted = true;
    }

    return inserted;
}

void MinerStatus::ClearReasonsNotStaking()
{
    vReasonNotStaking.clear();
    ReasonNotStaking.clear();
    able_to_stake = true;
}
