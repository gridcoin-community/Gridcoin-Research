// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "key.h"
#include "main.h"
#include "gridcoin/mrc.h"
#include "gridcoin/account.h"
#include "gridcoin/tally.h"
#include "gridcoin/beacon.h"
#include "util.h"

using namespace GRC;

namespace {
//!
//! \brief Get the hash of a subset of the data in the mrc object used as
//! input to sign or verify a research reward claim.
//!
//! \param mrc           MRC to generate a hash for.
//! \param last_block_hash Hash of the block at the head of the chain.
//! \param mrc    mrc transaction of the block that contains
//! the claim.
//!
//! \return Hash of the CPID and last block hash contained in the claim.
//!
uint256 GetMRCHash(
    const MRC& mrc,
    const uint256& last_block_hash)
{
    const CpidOption cpid = mrc.m_mining_id.TryCpid();

    if (!cpid) {
        return uint256();
    }

        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher << *cpid << last_block_hash;

        return hasher.GetHash();
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Class: MRC
// -----------------------------------------------------------------------------

MRC::MRC() : MRC(CURRENT_VERSION)
{
}

MRC::MRC(uint32_t version)
    : m_version(version)
    , m_research_subsidy(0)
    , m_magnitude(0)
    , m_magnitude_unit(0)
{
}

bool MRC::WellFormed() const
{
    if (m_version <= 0 || m_version > MRC::CURRENT_VERSION) {
        return false;
    }

    if (m_version == 1) {
        return true;
    }

    if (!m_mining_id.Valid()) {
        return false;
    }

    if (m_client_version.empty()) {
        return false;
    }

    if (m_mining_id.Which() == MiningId::Kind::CPID) {
        if (m_research_subsidy <= 0 || m_signature.empty()) {
            return false;
        }
    }

    return true;
}

bool MRC::HasResearchReward() const
{
    return m_mining_id.Which() == MiningId::Kind::CPID;
}

CAmount MRC::ComputeMRCFee()
{
    CAmount fee = 0;

    // This is the 14 days where fees will be 100% if someone tries to slip an MRC through.
    const int64_t zero_payout_interval = 14 * 24 * 60 * 60;

    // Initial fee fraction at end of zero_payout_interval (beginning of valid MRC interval). This is expressed as
    // separate numerator and denominator for integer math. This is equivalent to 40% fees at the end of the
    // zero_payout_interval
    int64_t fee_fraction_numerator = 2;
    int64_t fee_fraction_denominator = 5;

    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) return fee;

    const ResearchAccount& account = Tally::GetAccount(*cpid);
    const int64_t last_reward_time = account.LastRewardTime();

    // Get the block index of the head of the chain at the time the MRC was filled out.
    CBlockIndex* prev_block_pindex = mapBlockIndex[m_last_block_hash];

    int64_t payment_time = prev_block_pindex->nTime;

    int64_t mrc_payment_interval = 0;

    // If there is a last reward recorded in the accrual system, then the payment interval for the MRC request starts
    // there and goes to the head of the chain for the MRC in the mempool. If not, we use the age of the beacon.
    if (!last_reward_time) {
        const BeaconOption beacon = GetBeaconRegistry().Try(*cpid);;

        if (beacon) {
            mrc_payment_interval = beacon->Age(prev_block_pindex->nTime);
        } else {
            // This should not happen, because we should have an active beacon, but just in case return fee of zero.
            return fee;
        }
    } else {
        mrc_payment_interval = payment_time - last_reward_time;
    }

    // If the payment interval for the MRC is less than the zero payout interval, then set the fees equal to the entire
    // m_research_subsidy, which means the entire accrual will be forfeited. This should not happen because the sending
    // node will use the same rules to validate and not allow sends within zero_payout_interval; however, this implements
    // a serious penalty for someone trying to abuse MRC with a modified client. If a rogue node operator sends an MRC
    // where they payment interval is smaller than zero_payout_interval, and it makes it through, the entire rewards will
    // be taken as fees.
    if (mrc_payment_interval < zero_payout_interval) return m_research_subsidy;

    // TODO: do overflow check analysis.
    // This is a simple model that is very deterministic and should not cause consensus problems. It is essentially
    // The straight line estimate of the m_research_subsidy at mrc_payment_interval * the fee fraction, which is (pure math)
    // m_research_subsidy * (zero_payout_interval / mrc_payment_interval) * fee_fraction.
    //
    // If the magnitude of the cpid with the MRC is constant, this has the effect of holding fees constant at
    // the value they would have been at the zero_payout_interval and keeping them there as the mrc_payment_interval gets
    // larger. From the point of view of the actual MRC, the payment fees as a percentage of the m_research_subsidty
    // decline as c/t where c is a constant and t is elapsed time.
    //
    // If the MRC is exactly at the end of the zero_payout_interval, the fees are effectively
    // fee_fraction * m_research_subsidy.
    fee = m_research_subsidy * zero_payout_interval * fee_fraction_numerator /
            (mrc_payment_interval * fee_fraction_denominator);

    return fee;
}

bool MRC::Sign(CKey& private_key)
{
    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) {
        return false;
    }

    const uint256 hash = GetMRCHash(*this, m_last_block_hash);

    if (!private_key.Sign(hash, m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool MRC::VerifySignature(
    const CPubKey& public_key,
    const uint256& last_block_hash) const
{
    CKey key;

    if (!key.SetPubKey(public_key)) {
        return false;
    }

    const uint256 hash = GetMRCHash(*this, last_block_hash);

    return key.Verify(hash, m_signature);
}

uint256 MRC::GetHash() const
{
    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

    // MRC contracts do not use the contract action specifier:
    Serialize(hasher, ContractAction::UNKNOWN);

    return hasher.GetHash();
}
