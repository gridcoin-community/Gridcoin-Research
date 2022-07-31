// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/mrc.h"

#include "amount.h"
#include "key.h"
#include "main.h"
#include "gridcoin/account.h"
#include "gridcoin/claim.h"
#include "gridcoin/tally.h"
#include "gridcoin/beacon.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "util.h"
#include "wallet/wallet.h"

using namespace GRC;

namespace {
//!
//! \brief Get the hash of a subset of the data in the mrc object used as
//! input to sign or verify a research reward claim.
//!
//! \param last_block_hash: Hash of the block at the head of the chain.
//! \param mrc: mrc transaction of the block that contains the claim.
//!
//! \return Hash of the CPID, mrc fee, and last block hash contained in the claim.
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

        hasher << *cpid << mrc.m_fee << last_block_hash;

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

    if (!m_mining_id.Valid() || m_mining_id.Which() != MiningId::Kind::CPID) {
        return false;
    }

    if (m_client_version.empty()) {
        return false;
    }

    if (m_research_subsidy <= 0 || m_signature.empty()) {
        return false;
    }

    return true;
}

CAmount MRC::ComputeMRCFee() const
{
    CAmount fee = 0;

    // This is the amount of time where fees will be 100% if someone tries to slip an MRC through.
    const int64_t& zero_payout_interval = Params().GetConsensus().MRCZeroPaymentInterval;

    // Initial fee fraction at end of zero_payout_interval (beginning of valid MRC interval). This is expressed as
    // separate numerator and denominator for integer math. This is fraction of the reward that will be paid as fees
    // at/past the end of the zero_payout_interval.
    const Fraction& fee_fraction = Params().GetConsensus().InitialMRCFeeFractionPostZeroInterval;

    const CpidOption cpid = m_mining_id.TryCpid();

    if (!cpid) return fee;

    const ResearchAccount& account = Tally::GetAccount(*cpid);
    const int64_t last_reward_time = account.LastRewardTime();

    // Get the block index of the head of the chain at the time the MRC was filled out.
    auto block_index_element = mapBlockIndex.find(m_last_block_hash);

    // If the mrc last block hash is not in the block index map, just return zero.
    if (block_index_element == mapBlockIndex.end()) return fee;

    CBlockIndex* prev_block_pindex = block_index_element->second;

    int64_t payment_time = prev_block_pindex->nTime;

    int64_t mrc_payment_interval = 0;

    // If there is a last reward recorded in the accrual system, then the payment interval for the MRC request starts
    // there and goes to the head of the chain for the MRC in the mempool. If not, we use the age of the beacon.
    if (!last_reward_time) {
        const BeaconOption beacon = GetBeaconRegistry().Try(*cpid);;

        if (beacon) {
            mrc_payment_interval = beacon->Age(payment_time);
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
    // where the payment interval is smaller than zero_payout_interval, and it makes it through, the entire rewards will
    // be taken as fees. If a rogue operator tries to submit an MRC with different fees than should be included, it will
    // fail validation when the staking node checks the MRC when it goes to bind into the block, and the MRC sender will
    // not get paid and still had to pay the burn fee for the MRC transaction.
    //
    // TODO: Put the payment_time in the MRC object so that fees can be validated in AcceptToMemoryPool and DoS exerted
    // for invalid fees.
    if (mrc_payment_interval < zero_payout_interval) return m_research_subsidy;

    // This is a simple model that is very deterministic and will not cause consensus problems. It is essentially the
    // straight line estimate of the m_research_subsidy at mrc_payment_interval * the fee fraction, which is (pure math)
    // m_research_subsidy * (zero_payout_interval / mrc_payment_interval) * fee_fraction.
    //
    // If the magnitude of the cpid with the MRC is constant, this has the effect of holding fees constant at
    // the value they would have been at the zero_payout_interval and keeping them there as the mrc_payment_interval gets
    // larger. From the point of view of the actual MRC, the payment fees as a percentage of the m_research_subsidty
    // decline as c/t where c is a constant and t is elapsed time.
    //
    // If the MRC is exactly at the end of the zero_payout_interval, the fees are effectively
    // fee_fraction * m_research_subsidy.
    //
    // Overflow analysis. The accrual limit in the snapshot computer is 16384. For a zero_payout_interval of 14 days,
    // m_research_subsidy * zero_payout_interval = 19818086400. std::numeric_limits<int64_t>::max() / 19818086400
    // = 465401747207, which means the numerator of the fee_fraction would have to be that number or higher to cause an
    // overflow of the below. This is not likely to happen for any reasonable choice of the fee_fraction.
    fee = m_research_subsidy * zero_payout_interval * fee_fraction.GetNumerator()
                             / mrc_payment_interval / fee_fraction.GetDenominator();

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
    const uint256 hash = GetMRCHash(*this, last_block_hash);

    return public_key.Verify(hash, m_signature);
}

uint256 MRC::GetHash() const
{
    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

    // MRC contracts do not use the contract action specifier:
    Serialize(hasher, ContractAction::UNKNOWN);

    return hasher.GetHash();
}

bool GRC::MRCContractHandler::Validate(const Contract& contract, const CTransaction& tx, int& DoS) const
{
    // Fully validate the incoming MRC txn.
    return ValidateMRC(contract, tx, DoS);
}

bool GRC::MRCContractHandler::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

namespace {
//!
//! \brief Sign the mrc.
//!
//! \param pwallet Supplies beacon private keys for signing.
//! \param pindex   Block index of last block.
//! \param mrc   An initialized mrc to sign.
//! \param mrc_tx The transaction for the mrc.
//!
//! \return \c true if the miner holds active beacon keys used to successfully
//! sign the mrc.
//!
bool TrySignMRC(
    CWallet* pwallet,
    CBlockIndex* pindex,
    GRC::MRC& mrc) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    // lock needs to be taken on pwallet here.
    LOCK(pwallet->cs_wallet);

    const GRC::CpidOption cpid = mrc.m_mining_id.TryCpid();

    if (!cpid) {
        return false; // Skip beacon signature for investors.
    }

    const GRC::BeaconOption beacon = GRC::GetBeaconRegistry().Try(*cpid);

    if (!beacon) {
        return error("%s: No active beacon", __func__);
    }

    // We use pindex->nTime here because the ending interval of the payment is aligned to the last block
    // (the head of the chain), not the MRC transaction time.
    if (beacon->Expired(pindex->nTime)) {
        return error("%s: Beacon expired", __func__);
    }

    CKey beacon_key;

    if (!pwallet->GetKey(beacon->m_public_key.GetID(), beacon_key)) {
        return error("%s: Missing beacon private key", __func__);
    }

    if (!beacon_key.IsValid()) {
        return error("%s: Invalid beacon key", __func__);
    }

    // Note that the last block hash has already been recorded in mrc for binding into the signature.
    if (!mrc.Sign(beacon_key)) {
        return error("%s: Signature failed. Check beacon key", __func__);
    }

    LogPrint(BCLog::LogFlags::MINER,
             "%s: Signed for CPID %s and block hash %s with signature %s",
             __func__,
             cpid->ToString(),
             pindex->GetBlockHash().ToString(),
             HexStr(mrc.m_signature));

    return true;
}
} // anonymous namespace

void GRC::CreateMRC(CBlockIndex* pindex,
                    MRC& mrc,
                    CAmount &nReward,
                    CAmount &fee,
                    CWallet* pwallet, bool no_sign) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

    bool err = true;

    switch (researcher->Status())
    {
    case GRC::ResearcherStatus::ACTIVE:
        // Not an error.
        err = false;
        break;
    case GRC::ResearcherStatus::NO_BEACON:
        throw MRC_error(strprintf("%s: CPID eligible but no active beacon key so MRC cannot be formed.", __func__));
    case GRC::ResearcherStatus::INVESTOR:
        throw MRC_error(strprintf("%s: MRC request cannot be sent while wallet is in investor mode.", __func__));
    case GRC::ResearcherStatus::NO_PROJECTS:
        // This is handled as no positive research reward pending below.
        err = false;
        break;
    case GRC::ResearcherStatus::POOL:
        throw MRC_error(strprintf("%s: MRC request cannot be sent while wallet is in pool mode.", __func__));
    }
    if (err) assert(false);

    mrc.m_mining_id = researcher->Id();

    if (const GRC::CpidOption cpid = mrc.m_mining_id.TryCpid()) {
        mrc.m_research_subsidy = GRC::Tally::GetAccrual(*cpid, pindex->nTime, pindex);

        // If no pending research subsidy value exists, bail.
        if (mrc.m_research_subsidy <= 0) {
            throw MRC_error(strprintf("%s: No positive research reward pending at time of mrc.", __func__));
        } else {
            nReward = mrc.m_research_subsidy;
            mrc.m_magnitude = GRC::Quorum::GetMagnitude(*cpid).Floating();
        }
    }

    mrc.m_client_version = FormatFullVersion().substr(0, GRC::Claim::MAX_VERSION_SIZE);
    mrc.m_organization = gArgs.GetArg("-org", "").substr(0, GRC::Claim::MAX_ORGANIZATION_SIZE);

    mrc.m_last_block_hash = pindex->GetBlockHash();

    CAmount computed_mrc_fee = mrc.ComputeMRCFee();

    // If no input fee provided (i.e. zero), then use computed_mrc_fee and also set parameter to this value. If an input
    // fee is provided, it must be bound by the computed_mrc_fee and the m_research_subsidy (inclusive).
    if (!fee) {
        fee = mrc.m_fee = computed_mrc_fee;
    } else if (fee >= computed_mrc_fee && fee <= mrc.m_research_subsidy) {
        mrc.m_fee = fee;
    } else {
        // Set the output fee equal to computed (for help with error handling)
        CAmount provided_fee = fee;
        fee = computed_mrc_fee;

        if (provided_fee < computed_mrc_fee) {
            throw MRC_error(strprintf("%s: Invalid fee specified for mrc. The specified fee of %s is less than "
                                      "the minimum calculated fee of %s.",
                                      __func__,
                                      FormatMoney(provided_fee),
                                      FormatMoney(computed_mrc_fee)));
        } else {
            throw MRC_error(strprintf("%s: Invalid fee specified for mrc. The specified fee of %s is greater than "
                                      "the computed research reward of %s.",
                                      __func__,
                                      FormatMoney(provided_fee),
                                      FormatMoney(mrc.m_research_subsidy)));

        }
    }

    if (!no_sign && !TrySignMRC(pwallet, pindex, mrc)) {
        throw MRC_error(strprintf("%s: Failed to sign mrc.", __func__));
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: for %s mrc request created: magnitude %d, "
                                       "research rewards %s, mrc fee %s.",
                __func__,
                mrc.m_mining_id.ToString(),
                mrc.m_magnitude,
                FormatMoney(mrc.m_research_subsidy),
                FormatMoney(mrc.m_fee));
}
