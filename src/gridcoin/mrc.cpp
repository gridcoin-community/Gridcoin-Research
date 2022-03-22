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

bool GRC::MRCContractHandler::Validate(const Contract& contract, const CTransaction& tx) const
{
    // The MRC transaction should only have one contract on it, and the contract type should be MRC (which we already
    // know to arrive at this virtual method implementation).
    if (tx.GetContracts().size() != 1) return false;

    // Check that the burn in the contract is equal or greater than the required burn.
    CAmount burn_amount = 0;

    for (const auto& output : tx.vout) {
        if (output.scriptPubKey == (CScript() << OP_RETURN)) {
            burn_amount += output.nValue;
        }
    }

    GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

    LogPrintf("INFO: %s: mrc m_client_version = %s, m_fee = %s, m_last_block_hash = %s, m_magnitude = %u, "
              "m_magnitude_unit = %f, m_mining_id = %s, m_organization = %s, m_research_subsidy = %s, "
              "m_version = %s",
              __func__,
              mrc.m_client_version,
              FormatMoney(mrc.m_fee),
              mrc.m_last_block_hash.GetHex(),
              mrc.m_magnitude,
              mrc.m_magnitude_unit,
              mrc.m_mining_id.ToString(),
              mrc.m_organization,
              FormatMoney(mrc.m_research_subsidy),
              mrc.m_version);

    if (burn_amount < mrc.RequiredBurnAmount()) {
        return error("%s: Burn amount of %s in mrc contract is less than the required %s.",
                     __func__,
                     FormatMoney(mrc.m_fee),
                     FormatMoney(mrc.RequiredBurnAmount())
                     );
    }

    // We cannot fully validate incoming mrc transactions to the mempool. During testing under stress, a node can send an
    // mrc transaction right before the receipt of a block staked by another node, which then results in the just submitted
    // transaction being invalid on nodes that receive it after the staked block, but valid on the sending node (because
    // the order is reversed). To avoid this we will relieve the strict requirement here and do a partial validation which
    // checks the burn fee and the MRC contract signature to ensure the MRC contract is validly signed by an active beacon
    // holder. The block level validations handle the full MRC validation. This completes the concept of a "bad" or "stale"
    // mrc transaction being accepted into the memory pool. They will also be bound into the block, but they will be
    // "absorbed," meaning they will not result in a coinstake payout to the mrc requester. The deterrent to prevent someone
    // from flooding the mempool with invalid mrc transactions that pass this level of checking is the loss of the burn fees
    // AND the trouble to create the valid beacon context and signature for each MRC request transaction.
    return ValidateMRC(tx, mrc);
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

bool GRC::CreateMRC(CBlockIndex* pindex,
                    MRC& mrc,
                    CAmount &nReward,
                    CAmount &fee,
                    CWallet* pwallet) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

    mrc.m_mining_id = researcher->Id();

    if (researcher->Status() == GRC::ResearcherStatus::NO_BEACON) {
        error("%s: CPID eligible but no active beacon key so MRC cannot be formed.", __func__);

        return false;
    }

    if (const GRC::CpidOption cpid = mrc.m_mining_id.TryCpid()) {
        mrc.m_research_subsidy = GRC::Tally::GetAccrual(*cpid, pindex->nTime, pindex);

        // If no pending research subsidy value exists, bail.
        if (mrc.m_research_subsidy <= 0) {
            error("%s: No positive research reward pending at time of mrc.", __func__);

            return false;
        } else {
            nReward = mrc.m_research_subsidy;
            mrc.m_magnitude = GRC::Quorum::GetMagnitude(*cpid).Floating();
        }
    }

    mrc.m_client_version = FormatFullVersion().substr(0, GRC::Claim::MAX_VERSION_SIZE);
    mrc.m_organization = gArgs.GetArg("-org", "").substr(0, GRC::Claim::MAX_ORGANIZATION_SIZE);

    mrc.m_last_block_hash = pindex->GetBlockHash();
    mrc.m_fee = mrc.ComputeMRCFee();
    fee = mrc.m_fee;

    if (!TrySignMRC(pwallet, pindex, mrc)) {
        error("%s: Failed to sign mrc.", __func__);

        return false;
    }

    LogPrintf(
        "INFO: %s: for %s mrc %s magnitude %d Research %s",
        __func__,
        mrc.m_mining_id.ToString(),
        FormatMoney(nReward),
        mrc.m_magnitude,
        FormatMoney(mrc.m_research_subsidy));

    return true;
}

