// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "gridcoin/claim.h"
#include "gridcoin/researcher.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/vote.h"
#include "gridcoin/voting/result.h"
#include "gridcoin/support/block_finder.h"
#include "node/blockstorage.h"
#include "txdb.h"
#include "node/ui_interface.h"
#include "validation.h"

using namespace GRC;
using LogFlags = BCLog::LogFlags;

extern bool fQtActive;

extern MiningPools g_mining_pools;

namespace {
//!
//! \brief Extract a poll title from a legacy vote contract.
//!
//! \param key The value of the legacy contract key.
//!
//! \return Normalized title of the poll to associate the vote with.
//!
std::string ParseLegacyVoteTitle(const std::string& key)
{
    std::string title = key.substr(0, key.find(';'));
    title = ToLower(title);

    return title;
}

//!
//! \brief Thrown when verifying an invalid poll contract.
//!
class InvalidPollError : public std::exception
{
public:
    InvalidPollError()
    {
    }
};

class PollValidator
{
public:
    explicit PollValidator(const PollPayload& payload, const CTransaction& tx) : m_payload(payload), m_tx(tx)
    {
    }

    bool Validate(int& DoS)
    {
        if (m_payload.m_version < 2) {
            DoS = 25;
            LogPrint(LogFlags::CONTRACT, "%s: rejected legacy poll", __func__);
            return false;
        }

        // WellFormed() checks:
        // m_type is not UNKNOWN or OUT_OF_BOUND
        // m_weight_type is not UNKNOWN or OUT_OF_BOUND
        // m_response_type is not UNKNOWN or OUT_OF_BOUND
        // m_duration_days is inclusive between MIN_DURATION_DAYS and MAX_DURATION_DAYS
        // m_title is not empty
        // m_url is not empty
        // m_choices are well formed based on the response type
        // For version 2+
        // m_weight type is only BALANCE and BALANCE_AND_MAGNITUDE
        if (!m_payload.m_poll.WellFormed(m_payload.m_version)) {
            DoS = 25;
            LogPrint(LogFlags::CONTRACT, "%s: rejected poll that is not well formed", __func__);
            return false;
        }

        // Make sure poll type is valid for the version of the poll payload.
        // The only valid type for v2 polls is SURVEY.
        // The valid types for v3 polls are SURVEY, PROJECT, DEVELOPMENT, GOVERNANCE, MARKETING, OUTREACH,
        // and COMMUNITY.
        std::vector<PollType> valid_poll_types = GRC::PollPayload::GetValidPollTypes(m_payload.m_version);

        if (std::find(valid_poll_types.begin(), valid_poll_types.end(), m_payload.m_poll.m_type.Value())
                == valid_poll_types.end()) {
            DoS = 25;
            LogPrint(LogFlags::CONTRACT, "%s: rejected poll payload with improper type", __func__);
            return false;
        }

        // Poll payload v3+ validations which depend on poll type
        if (m_payload.m_version >= 3) {
            // Select the rules for the poll type in the payload.
            Poll::PollTypeRules poll_type_rules = Poll::POLL_TYPE_RULES[m_payload.m_poll.m_type.Raw()];

            // v3 polls must meet the by type minimum duration requirements as well as the global requirements above.
            if (m_payload.m_poll.m_duration_days < poll_type_rules.m_mininum_duration) {
                DoS = 25;
                LogPrint(LogFlags::CONTRACT, "%s: rejected v3 poll payload with duration %i, less than the required "
                                             "minimum %i",
                         __func__,
                         m_payload.m_poll.m_duration_days,
                         poll_type_rules.m_mininum_duration);
                return false;
            }

            // v3 polls must be balance + magnitude for the weight type if the by poll type rule specifies a minimum
            // vote weight % of AVW greater than zero.
            if (poll_type_rules.m_min_vote_percent_AVW
                    && m_payload.m_poll.m_weight_type != PollWeightType::BALANCE_AND_MAGNITUDE) {
                DoS = 25;
                LogPrint(LogFlags::CONTRACT, "%s: rejected v3 poll payload with wrong weight type %s for given poll type %s "
                                             "requiring %u vote weight percent of active vote weight for validation.",
                         __func__,
                         m_payload.m_poll.WeightTypeToString(),
                         m_payload.m_poll.PollTypeToString(),
                         poll_type_rules.m_min_vote_percent_AVW);
                return false;
            }
        }

        return true;
    }

private:
    const PollPayload& m_payload;
    CTransaction m_tx;
};




//!
//! \brief Verifies a participant's eligibility to create a poll.
//!
//! Validation for a poll claim occurs while connecting a block. As such, the
//! claim must pass validation at that time. If validation fails, a node will
//! reject the block. A node will also verify the poll claim when it receives
//! the transaction for inclusion in the memory pool so a poll must provide a
//! claim that remains valid until the next block (no spent outputs).
//!
//! This is different from the vote claim validation which occurs while nodes
//! evaluate the results of a poll.
//!
class PollClaimValidator
{
public:
    //!
    //! \brief Initialize a poll claim validator.
    //!
    //! \param txdb Used to resolve unspent amount claims.
    //!
    PollClaimValidator(CTxDB& txdb) : m_txdb(txdb)
    {
    }

    //!
    //! \brief Determine whether the poll claim meets the minimum requirements
    //! to create a poll.
    //!
    //! \param payload   Contains the poll contract to validate.
    //! \param timestamp Transaction that contains the poll.
    //!
    //! \return \c true If the resolved claim meets the requirements to create
    //! a poll.
    //!
    bool Validate(const PollPayload& payload, const CTransaction& tx)
    {
        const ClaimMessage message = PackPollMessage(payload.m_poll, tx);

        try {
            return VerifyClaim(payload.m_claim.m_address_claim, message);
        } catch (const InvalidPollError& e) {
            LogPrint(LogFlags::VOTE, "%s: bad poll claim", __func__);
            return false;
        }
    }

private:
    CTxDB& m_txdb; //!< Used to resolve unspent amount claims.

    //!
    //! \brief Determine whether the address claim establishes eligibility for
    //! creating the poll.
    //!
    //! \param claim   The claim to verify minimum balance for.
    //! \param message Serialized context for claim signature verification.
    //!
    //! \return \c true If the resolved amount for the claim meets the
    //! minimum balance requirement to create a poll.
    //!
    bool VerifyClaim(const AddressClaim& claim, const ClaimMessage& message)
    {
        if (!claim.VerifySignature(message)) {
            LogPrint(LogFlags::VOTE, "%s: bad address signature", __func__);
            return false;
        }

        const CTxDestination address = claim.m_public_key.GetID();
        CAmount amount = 0;

        for (const auto& txo : claim.m_outpoints) {
            amount += Resolve(txo, address);
        }

        return amount >= POLL_REQUIRED_BALANCE;
    }

    //!
    //! \brief Resolve the claimed amount for an output.
    //!
    //! \param txo     Refers to the output to resolve balance weight for.
    //! \param address Must match the address of the resolved output.
    //!
    //! \return Claimed amount in units of 1/100000000 GRC.
    //!
    //! \throws InvalidPollError If the output fails to validate or if an IO
    //! error occurs.
    //!
    int64_t Resolve(const COutPoint& txo, const CTxDestination& address)
    {
        CTxIndex tx_index;

        if (!m_txdb.ReadTxIndex(txo.hash, tx_index)) {
            LogPrint(LogFlags::VOTE, "%s: failed to read tx index", __func__);
            throw InvalidPollError();
        }

        if (txo.n >= tx_index.vSpent.size()) {
            error("%s: txo out of spent range", __func__);
            throw InvalidPollError(); // should never happen
        }

        if (!tx_index.vSpent[txo.n].IsNull()) {
            error("%s: found spent txo", __func__);
            throw InvalidPollError();
        }

        CTransaction tx;

        if (!ReadTxFromDisk(tx, tx_index.pos)) {
            throw InvalidPollError();
        }

        if (txo.n >= tx.vout.size()) {
            LogPrint(LogFlags::VOTE, "%s: txo out of range", __func__);
            throw InvalidPollError();
        }

        const CTxOut& output = tx.vout[txo.n];

        if (output.nValue < COIN) {
            LogPrint(LogFlags::VOTE, "%s: txo < 1 GRC", __func__);
            throw InvalidPollError();
        }

        CTxDestination dest;

        if (!ExtractDestination(output.scriptPubKey, dest)) {
            LogPrint(LogFlags::VOTE, "%s: invalid txo address", __func__);
            throw InvalidPollError();
        }

        if (dest != address) {
            LogPrint(LogFlags::VOTE, "%s: txo address mismatch", __func__);
            throw InvalidPollError();
        }

        return output.nValue;
    }
}; // PollClaimValidator

//!
//! \brief Global poll registry instance.
//!
PollRegistry g_poll_registry;
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

PollRegistry& GRC::GetPollRegistry()
{
    return g_poll_registry;
}

std::string GRC::GetCurrentPollTitle()
{
    LOCK(GetPollRegistry().cs_poll_registry);

    if (const PollReference* poll_ref = GetPollRegistry().TryLatestActive()) {
        return poll_ref->Title();
    }

    return _("No current polls");
}

ClaimMessage GRC::PackPollMessage(const Poll& poll, const CTransaction& tx)
{
    std::vector<uint8_t> bytes;
    CVectorWriter writer(SER_NETWORK, PROTOCOL_VERSION, bytes, 0);

    writer << poll;
    writer << tx.nTime;

    for (const auto& txin : tx.vin) {
        writer << txin.prevout;
    }

    return bytes;
}

// -----------------------------------------------------------------------------
// Class: PollReference
// -----------------------------------------------------------------------------
PollReference::PollReference()
    : m_txid(uint256{})
    , m_payload_version(0)
    , m_type(PollType::UNKNOWN)
    , m_ptitle(nullptr)
    , m_timestamp(0)
    , m_duration_days(0)
    , m_votes({})
{
}

PollOption PollReference::TryReadFromDisk(CTxDB& txdb) const
{
    CTransaction tx;

    if (!txdb.ReadDiskTx(m_txid, tx)) {
        error("%s: failed to read poll tx from disk", __func__);
        return std::nullopt;
    }

    for (auto& contract : tx.PullContracts()) {
        if (contract.m_type == ContractType::POLL) {
            auto payload = contract.PullPayloadAs<PollPayload>();
            payload.m_poll.m_timestamp = m_timestamp;

            return std::move(payload.m_poll);
        }
    }

    error("%s: transaction does not contain a poll contract", __func__);

    return std::nullopt;
}

PollOption PollReference::TryReadFromDisk() const
{
    CTxDB txdb("r");

    return TryReadFromDisk(txdb);
}

uint256 PollReference::Txid() const
{
    return m_txid;
}

uint32_t PollReference::GetPollPayloadVersion() const
{
    return m_payload_version;
}

PollType PollReference::GetPollType() const
{
    return m_type;
}

const std::string& PollReference::Title() const
{
    if (!m_ptitle) {
        static const std::string empty("");
        return empty;
    }

    return m_title;
}

const std::vector<uint256>& PollReference::Votes() const
{
    return m_votes;
}

int64_t PollReference::Time() const
{
    return m_timestamp;
}

int64_t PollReference::Age(const int64_t now) const
{
    return now - m_timestamp;
}

bool PollReference::Expired(const int64_t now) const
{
    // The casting is done to suppress the comparison of signed vs unsigned. The multiplication will not
    // overflow until 68 years, which is longer than the longest possible poll.
    return Age(now) > (int64_t) (m_duration_days * 86400);
}

int64_t PollReference::Expiration() const
{
    // The casting is done to suppress the comparison of signed vs unsigned. The multiplication will not
    // overflow until 68 years, which is longer than the longest possible poll.
    return m_timestamp + (int64_t) (m_duration_days * 86400);
}

CBlockIndex* PollReference::GetStartingBlockIndexPtr() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    uint256 block_hash;
    CTransaction tx;

    GetTransaction(m_txid, tx, block_hash);

    auto iter = mapBlockIndex.find(block_hash);

    if (iter == mapBlockIndex.end()) {
        return nullptr;
    }

    return iter->second;
}

CBlockIndex* PollReference::GetEndingBlockIndexPtr(CBlockIndex* pindex_start) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!pindex_start) {
        pindex_start = GetStartingBlockIndexPtr();

        // If there is still no pindex_start, there cannot be an end either.
        if (!pindex_start) {
            return nullptr;
        }
    }

    // Has poll ended?
    if (Expired(GetAdjustedTime())) {
        // Find and return the last block that contains valid votes for the poll.
        return GRC::BlockFinder::FindByMinTimeFromGivenIndex(Expiration(), pindex_start);
    }

    return nullptr;
}

std::optional<int> PollReference::GetStartingHeight() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const CBlockIndex* pindex = GetStartingBlockIndexPtr();

    if (pindex != nullptr) {
        return pindex->nHeight;
    }

    return std::nullopt;
}

std::optional<int> PollReference::GetEndingHeight() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CBlockIndex* pindex = GetEndingBlockIndexPtr();

    if (pindex != nullptr) {
        return pindex->nHeight;
    }

    return std::nullopt;
}

std::optional<CAmount> PollReference::GetActiveVoteWeight(const PollResultOption& result) const
{
    // Instrument this so we can log real time performance.
    g_timer.InitTimer(__func__, LogInstance().WillLogCategory(BCLog::LogFlags::VOTE));

    // Unfortunately, cs_main must be locked for the duration of this method, because it uses the chain index pointers.
    LOCK(cs_main);

    // Get the start and end of the poll.
    CBlockIndex* const pindex_start = GetStartingBlockIndexPtr();

    // If pindex_start is a nullptr, this is a degenerate poll reference. Return std::nullopt.
    if (pindex_start == nullptr) return std::nullopt;

    const CBlockIndex* pindex_end = GetEndingBlockIndexPtr(pindex_start);


    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Poll start height = %i.",
             __func__, pindex_start->nHeight);

    // If the poll is still active, then pindex_end will be nullptr, so then use the current chain head.
    if (pindex_end == nullptr) {
        pindex_end = pindexBest;

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Poll is still active. Using head of the chain (%i) as end height.",
                 __func__, pindex_end->nHeight);
    } else {
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Poll end height = %i.",
                 __func__, pindex_end->nHeight);
    }

    // determine the pools that did NOT vote in the poll (via the result passed in). Only pools that did not
    // vote contribute to the magnitude correction for pools.
    std::vector<MiningPool> pools_not_voting;
    const std::vector<MiningPool>& mining_pools = g_mining_pools.GetMiningPools();

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: %u out of %u pool cpids voted.",
             __func__,
             result->m_pools_voted.size(),
             mining_pools.size());

    for (const auto& pool : mining_pools) {
        if (result && std::find(result->m_pools_voted.begin(), result->m_pools_voted.end(),
                                pool.m_cpid) == result->m_pools_voted.end()) {
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: pool with cpid %s did not vote and will be removed from magnitude "
                                            "in the AVW calculation.",
                     __func__,
                     pool.m_cpid.ToString());

            pools_not_voting.push_back(pool);
        }
    }

    // Since this calculation by its very nature is going to be heavyweight, we are going to
    // dispense with using the heavyweight averaging functions, and instead accumulate the necessary
    // values directly in a for loop that traverses the index. This will do it all in a single index
    // pass, which is the most efficient.
    // Note we must use bignums here, because the second term of the active_vote_weight_tally will overflow
    // otherwise. We are also avoiding floating point calculations, because avw will be used in consensus rules in
    // the future.
    arith_uint256 active_vote_weight_tally = 0;
    arith_uint256 scaled_pool_magnitude = 0;
    arith_uint256 scaled_network_magnitude = 0;
    unsigned int blocks = 0;

    // Lambda for active_vote_weight tally.
    const auto tally_active_vote_weight = [&](
            const arith_uint256 net_weight,
            const arith_uint256 money_supply,
            const arith_uint256 scaled_pool_magnitude,
            const arith_uint256 scaled_network_magnitude)
    {
        active_vote_weight_tally += net_weight + money_supply * (scaled_network_magnitude - scaled_pool_magnitude)
                                                              * arith_uint256(100)
                                                              / arith_uint256(567)
                                                              / scaled_network_magnitude;

        ++blocks;
    };

    // Rewind from pindex_start to find last superblock before start of the poll to pick up first pool magnitudes
    bool superblock_well_formed = false;

    for (CBlockIndex* pindex = pindex_start; pindex; pindex = pindex->pprev)
    {
        // Apparently the superblock flags in pindex are broken for superblocks earlier than 1034768 on mainnet and
        // earlier than 196562 on testnet.
        // TODO: Repair the index flags loaded in LevelDB. (This will require a special correction routine at startup.)
        if ((fTestNet && pindex->nHeight < 196562) || (!fTestNet && pindex->nHeight < 1034768) || pindex->IsSuperblock()) {

            const GRC::ClaimOption claim = GetClaimByIndex(pindex);

            // Add up the magnitudes of the pools in the last superblock before the start of the poll.
            if (claim && claim->ContainsSuperblock()) {
                const GRC::Superblock& superblock = *claim->m_superblock;

                superblock_well_formed = superblock.WellFormed();

                for (const auto& pool : pools_not_voting) {
                    scaled_pool_magnitude += superblock.m_cpids.MagnitudeOf(pool.m_cpid).Scaled();
                }

                scaled_network_magnitude = superblock.m_cpids.TotalScaledMagnitude();
            }

            // Stop after processing the first superblock found that is well formed rewinding from the poll start.
            if (superblock_well_formed) {
                LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: last superblock before poll start: block height = %i, "
                                                "pool_magnitude = %f, network_magnitude = %f",
                         __func__,
                         pindex->nHeight,
                         scaled_pool_magnitude.getdouble() / 100.0,
                         scaled_network_magnitude.getdouble() / 100.0
                         );
                break;
            }
        }
    }

    // If we are past the search and find phase for the last preceding superblock above and no well formed superblock
    // was found, or the network magnitude was 0, then AVW cannot be accurately calculated for this poll, so return
    // std::nullopt. This really will only happen to polls REALLY early in the chain.
    if (!superblock_well_formed || scaled_network_magnitude == 0) {
        LogPrintf("WARNING: %s: No valid superblock found prior to start of poll txid %s, title \"%s\", so no active vote "
                  "weight can be calculated.",
                 __func__,
                 Txid().GetHex(),
                 Title()
                 );

        return std::nullopt;
    }

    // Scan the index for the poll duration and compute AVW.
    for (CBlockIndex* pindex = pindex_start; pindex; pindex = pindex->pnext) {
        // Refresh pool magnitude and network magnitude if the index points to a superblock. The pool_magnitude and
        // network magnitude remain constant for all subsequent blocks until replaced by the values from a fresh superblock
        // in the scan.
        if (pindex->IsSuperblock()) {
            scaled_pool_magnitude = 0;
            scaled_network_magnitude = 0;

            const GRC::ClaimOption claim = GetClaimByIndex(pindex);

            // Add up the magnitudes of the pools in the last superblock before the start of the poll.
            if (claim && claim->ContainsSuperblock()) {
                const GRC::Superblock& superblock = *claim->m_superblock;

                for (const auto& pool : pools_not_voting) {
                    scaled_pool_magnitude += superblock.m_cpids.MagnitudeOf(pool.m_cpid).Scaled();
                }

                scaled_network_magnitude = superblock.m_cpids.TotalScaledMagnitude();
            }
        }

        // Please refer to https://gridcoin.us/assets/docs/grc-bluepaper-section-1.pdf equations 1 and 16 and footnote 5.
        // This method of computing net_weight is from first principles using the target from the nBits representation
        // recorded in the index, rather than the GetEstimatedNetworkWeight() function, which uses double fp arithmetic.
        arith_uint256 target;
        target.SetCompact(pindex->nBits);

        arith_uint256 net_weight = ~arith_uint256() / arith_uint256(450) / target * arith_uint256(COIN);

        arith_uint256 money_supply = pindex->nMoneySupply;

        tally_active_vote_weight(net_weight, money_supply, scaled_pool_magnitude, scaled_network_magnitude);

        // If voting logging category is active, log the first block and every superblock
        if (blocks == 1 || pindex->IsSuperblock()) {
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: tally_active_vote_weight: net_weight = %f, money_supply = %f, "
                                            "pool_magnitude = %f, network_magnitude = %f, block height = %i, "
                                            "blocks = %u, active_vote_weight_tally = %f, active_vote_weight = %f.",
                     __func__,
                     net_weight.getdouble() / (double) COIN,
                     money_supply.getdouble() / (double) COIN,
                     scaled_pool_magnitude.getdouble() / 100.0,
                     scaled_network_magnitude.getdouble() / 100.0,
                     pindex->nHeight,
                     blocks,
                     active_vote_weight_tally.getdouble() / (double) COIN,
                     (active_vote_weight_tally / arith_uint256(blocks)).getdouble() / (double) COIN
                     );
        }

        // Log the last block and break if at pindex_end.
        if (pindex == pindex_end) {
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: tally_active_vote_weight: net_weight = %f, money_supply = %f, "
                                            "pool_magnitude = %f, network_magnitude = %f, block height = %i, "
                                            "blocks = %u, active_vote_weight_tally = %f, active_vote_weight = %f.",
                     __func__,
                     net_weight.getdouble() / (double) COIN,
                     money_supply.getdouble() / (double) COIN,
                     scaled_pool_magnitude.getdouble() / 100.0,
                     scaled_network_magnitude.getdouble() / 100.0,
                     pindex->nHeight,
                     blocks,
                     active_vote_weight_tally.getdouble() / (double) COIN,
                     (active_vote_weight_tally / arith_uint256(blocks)).getdouble() / (double) COIN
                     );

            break;
        }
    }

    // Testing shows that the final 256 bit division below adds a negligible amount of execution time after this point, so
    // a single timer call to print out execution time is good enough.
    g_timer.GetTimes("finished execution for poll id " + Txid().GetHex() + " title \"" + Title() + "\"", __func__);

    if (!blocks) {
        LogPrintf("WARNING: %s: No blocks tallied for poll txid %s, title \"%s\", so no active vote weight can be "
                  "calculated.",
                 __func__,
                 Txid().GetHex(),
                 Title()
                 );

        return std::nullopt;
    }

    arith_uint256 active_vote_weight = active_vote_weight_tally / arith_uint256(blocks);

    // Overflow protection. This should never happen.
    if (active_vote_weight < arith_uint256(std::numeric_limits<int64_t>::max())) {
        return static_cast<CAmount>(active_vote_weight.GetLow64());
    } else {
        error("%s: Overflow in calculation for poll txid %s, title \"%s\", so no active vote weight can be calculated.",
                 __func__,
                 Txid().GetHex(),
                 Title()
                 );

        return std::nullopt;
    }
}

void PollReference::LinkVote(const uint256 txid)
{
    m_votes.emplace_back(txid);
}

void PollReference::UnlinkVote(const uint256 txid)
{
    for (auto it = m_votes.crbegin(), end = m_votes.crend(); it != end; ++it) {
        if (*it == txid) {
            m_votes.erase(std::next(it).base());
            return;
        }
    }
}

// -----------------------------------------------------------------------------
// Class: PollRegistry
// -----------------------------------------------------------------------------
const PollRegistry::Sequence PollRegistry::Polls() const
{
    LOCK(GetPollRegistry().cs_poll_registry);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-reference"
    return Sequence(m_polls);
#pragma clang diagnostic pop

}

const PollReference* PollRegistry::TryLatestActive() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    int64_t now = GetAdjustedTime();

    if (m_latest_poll && !m_latest_poll->Expired(now)) {
        return m_latest_poll;
    }

    if (m_polls.empty()) {
        return nullptr;
    }

    const PollReference* latest_not_expired = nullptr;
    int64_t latest_not_expired_timestamp = 0;

    // Go through m_polls finding the latest poll that is not expired.
    for (auto iter = m_polls.cbegin(); iter != m_polls.cend(); ++iter) {
        // Maybe more than one poll has the same timestamp... therefore use >=.
        if (!iter->second.Expired(now) && iter->second.m_timestamp >= latest_not_expired_timestamp) {
            latest_not_expired = &iter->second;
            latest_not_expired_timestamp = latest_not_expired->m_timestamp;
        }
    }

    return latest_not_expired;
}

const PollReference* PollRegistry::TryByTxid(const uint256 txid) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    const auto iter = m_polls_by_txid.find(txid);

    if (iter == m_polls_by_txid.end()) {
        return nullptr;
    }

    return iter->second;
}

PollReference* PollRegistry::TryBy(const uint256 txid) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return const_cast<PollReference*>(TryByTxid(txid));
}

const PollReference* PollRegistry::TryByTitle(const std::string& title) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    const auto iter = m_polls.find(title);

    if (iter == m_polls.end()) {
        return nullptr;
    }

    return &iter->second;
}

PollReference* PollRegistry::TryBy(const std::string& title) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return const_cast<PollReference*>(TryByTitle(title));
}

const PollReference* PollRegistry::TryByTxidWithAddHistoricalPollAndVotes(const uint256 txid)
EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    // Check and see if it is already in the registry and return the existing ref immediately if found. (This is
    // the equivalent of the plain TryByTxid() functionality.)
    if (const PollReference* existing_ref = TryByTxid(txid)) {
        return existing_ref;
    }

    CTransaction tx;
    uint256 block_hash;
    CBlockIndex* pindex_poll = nullptr;

    if (GetTransaction(txid, tx, block_hash) && !block_hash.IsNull()) {
        pindex_poll = mapBlockIndex[block_hash];
    }

    for (const auto& contract : tx.GetContracts()) {
        if (contract.m_type == ContractType::POLL && contract.m_action == ContractAction::ADD) {
            Add({contract, tx, pindex_poll});
        }
    }

    // Get the poll reference now that the poll contract(s) (if any) in the provided transaction hash have been validated
    // and loaded in the registry.
    PollReference* ref = TryBy(txid);

    // If the poll "contract" is loaded successfully into the registry, walk the contracts in the chain for
    // the duration of the poll to populate the votes. We can't use the general GRC::ReplayContracts here
    // because it is too broad.
    if (ref != nullptr) {
        // Get the last block that contains valid votes for the poll. (No need to scan past this point for
        // a historical poll.)
        CBlockIndex* pindex_end = ref->GetEndingBlockIndexPtr();

        CBlock block;

        // pindex starts at the poll contract (from above) and ends at pindex_end, the last block that can contain
        // a valid vote.
        for (CBlockIndex* pindex = pindex_poll; pindex; pindex = pindex->pnext) {
            // If the block doesn't contain contract(s) or can't read, skip.
            if (!pindex->IsContract() || !ReadBlockFromDisk(block, pindex, Params().GetConsensus())) continue;

            // Skip coinbase and coinstake transactions:
            for (unsigned int i = 2; i < block.vtx.size(); ++i) {
                for (auto contract : block.vtx[i].GetContracts()) {

                    // Only process votes.
                    if (contract.m_type != ContractType::VOTE) continue;

                    // Below is similar to AddVote, but doesn't require expiry checking, because the block
                    // scan range has already been limited to the poll's duration by the for loop, pindex_poll,
                    // and pindex_end.

                    // Post fern poll votes
                    if (contract.m_version >= 2) {
                        const auto vote = contract.SharePayloadAs<Vote>();

                        // This is the critical part that separates this from the regular AddVote.
                        if (vote->m_poll_txid == ref->Txid()) {
                            ref->LinkVote(block.vtx[i].GetHash());
                        }

                        continue;
                    }

                    // Legacy poll votes
                    const ContractPayload vote = contract.m_body.AssumeLegacy();
                    const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

                    if (title.empty()) continue;

                    if (title == ref->Title()) {
                        ref->LinkVote(block.vtx[i].GetHash());
                    }
                } // for contract
            } // for vtx

            // Finished scan to load votes for this poll.
            if (pindex == pindex_end) break;
        } // for pindex
    }

    return ref;
}

void PollRegistry::Reset()
{
    LOCK(cs_poll_registry);

    m_polls.clear();
    m_polls_by_txid.clear();
    m_latest_poll = nullptr;
    registry_traversal_in_progress = false;
    reorg_occurred_during_reg_traversal = false;
}

bool PollRegistry::Validate(const Contract& contract, const CTransaction& tx, int& DoS) const
{
    // Vote contract claims do not affect consensus. Vote claim validation
    // occurs on-demand while computing the results of the poll:
    //
    if (contract.m_type == ContractType::VOTE) {
        return true;
    }

    // Legacy poll contracts do not invoke any contextual validation:
    //
    if (contract.m_version == 1) {
        return true;
    }

    // V2+ validations
    const auto payload = contract.SharePayloadAs<PollPayload>();

    if (!PollValidator(*payload, tx).Validate(DoS)) return false;

    CTxDB txdb("r");

    if (!PollClaimValidator(txdb).Validate(*payload, tx)) {
        DoS = 25;
        return false;
    }

    return true;
}

bool PollRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    // Vote contract claims do not affect consensus. Vote claim validation
    // occurs on-demand while computing the results of the poll:
    //
    if (ctx.m_contract.m_type == ContractType::VOTE) {
        return true;
    }

    const auto payload = ctx->SharePayloadAs<PollPayload>();

    // This is why we had to introduce BlockValidate, and this is critical
    // to ensure that v2 poll payloads are not allowed in blocks for the v3
    // height and beyond.
    if (IsPollV3Enabled(ctx.m_pindex->nHeight) && payload->m_version < 3) {
        return false;
    }

    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

void PollRegistry::Add(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    LOCK(cs_poll_registry);

    DetectReorg();

    if (ctx->m_type == ContractType::VOTE) {
        AddVote(ctx);
    } else {
        AddPoll(ctx);
    }
 }

void PollRegistry::Delete(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    LOCK(cs_poll_registry);

    DetectReorg();

    if (ctx->m_type == ContractType::VOTE) {
        DeleteVote(ctx);
    } else {
        DeletePoll(ctx);
    }
}

void PollRegistry::AddPoll(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    const auto payload = ctx->SharePayloadAs<PollPayload>();

    // In a contract replay inserts may overlap what was already inserted. Check to see if poll already exists, and
    // if so skip this add.
    if (m_polls_by_txid.find(ctx.m_tx.GetHash()) != m_polls_by_txid.end()) {
        return;
    }
    std::string poll_title = payload->m_poll.m_title;

    // The title used as the key for the m_poll map keyed by title, and also checked for duplicates, should
    // not be case-sensitive, regardless of whether v1 or v2+. We should not be allowing the insertion of two v2 polls
    // with the same title except for a difference in case.
    auto result_pair = m_polls.emplace(ToLower(poll_title), PollReference());

    if (result_pair.second) {
        const std::string& title = result_pair.first->first;

        PollReference& poll_ref = result_pair.first->second;
        poll_ref.m_ptitle = &title;
        poll_ref.m_title = poll_title;
        poll_ref.m_payload_version = payload->m_version;
        poll_ref.m_type = payload->m_poll.m_type.Value();
        poll_ref.m_timestamp = ctx.m_tx.nTime;
        poll_ref.m_duration_days = payload->m_poll.m_duration_days;

        m_latest_poll = &poll_ref;

        auto result_pair = m_polls_by_txid.emplace(ctx.m_tx.GetHash(), &poll_ref);
        poll_ref.m_txid = result_pair.first->first;

        if (fQtActive && !poll_ref.Expired(GetAdjustedTime())) {
            uiInterface.NewPollReceived(poll_ref.Time());
        }

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Added poll %s to the registry, m_polls.size() = %u.",
                 __func__,
                 *poll_ref.m_ptitle,
                 m_polls.size());
    }
}

void PollRegistry::AddVote(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    if (ctx->m_version >= 2) {
        const auto vote = ctx->SharePayloadAs<Vote>();

        if (PollReference* poll_ref = TryBy(vote->m_poll_txid)) {
            if (poll_ref->Expired(ctx.m_pindex->nTime)) {
                LogPrint(LogFlags::VOTE,
                    "%s: ignored vote %s for finished poll %s",
                    __func__,
                    ctx.m_tx.GetHash().ToString(),
                    poll_ref->Txid().ToString());
            } else {
                // In a contract replay inserts may overlap what was already inserted. Check to see if vote already exists, and
                // if so skip this add. This gets expensive as the number of votes increases.
                // TODO: consider changing Votes to a map.
                auto existing_vote = std::find(poll_ref->Votes().begin(), poll_ref->Votes().end(), ctx.m_tx.GetHash());
                if (existing_vote != poll_ref->Votes().end()) {
                    return;
                }

                poll_ref->LinkVote(ctx.m_tx.GetHash());

                LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Added vote %s to poll %s, poll_ref->Votes().size() = %u.",
                         __func__,
                         ctx.m_tx.GetHash().GetHex(),
                         *poll_ref->m_ptitle,
                         poll_ref->Votes().size());

            }
        }

        return;
    }

    const ContractPayload vote = ctx->m_body.AssumeLegacy();
    const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

    if (title.empty()) {
        return;
    }

    if (PollReference* poll_ref = TryBy(title)) {
        // In a contract replay inserts may overlap what was already inserted. Check to see if vote already exists, and
        // if so skip this add. This gets expensive as the number of votes increases.
        // TODO: consider changing Votes to a map.
        auto existing_vote = std::find(poll_ref->Votes().begin(), poll_ref->Votes().end(), ctx.m_tx.GetHash());
        if (existing_vote != poll_ref->Votes().end()) {
            return;
        }
        poll_ref->LinkVote(ctx.m_tx.GetHash());
    }
}

void PollRegistry::DeletePoll(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    const auto payload = ctx->SharePayloadAs<PollPayload>();

    m_polls.erase(ToLower(payload->m_poll.m_title));

    m_polls_by_txid.erase(ctx.m_tx.GetHash());
    m_latest_poll = nullptr;

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Deleted poll %s to the registry, m_polls.size() = %u.",
             __func__,
             payload->m_poll.m_title,
             m_polls.size());

}

void PollRegistry::DeleteVote(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    if (ctx->m_version >= 2) {
        const auto vote = ctx->SharePayloadAs<Vote>();

        if (PollReference* poll_ref = TryBy(vote->m_poll_txid)) {
            poll_ref->UnlinkVote(ctx.m_tx.GetHash());

            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Deleted vote %s from poll %s, poll_ref->Votes().size() = %u.",
                     __func__,
                     ctx.m_tx.GetHash().GetHex(),
                     *poll_ref->m_ptitle,
                     poll_ref->Votes().size());
        }

        return;
    }

    const auto vote = ctx->SharePayload();
    const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

    if (title.empty()) {
        return;
    }

    if (PollReference* poll_ref = TryBy(title)) {
        poll_ref->UnlinkVote(ctx.m_tx.GetHash());
    }
}

void PollRegistry::DetectReorg()
{
    // Reorg detector
    // Note that doing the reorg detection here, in the contract handler, means that we only flag a reorg IF
    // a transaction happened to occur that involves a poll or vote contract in the scope of the reorg, because
    // these handlers are only triggered by those two contract types.
    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: registry_traversal_in_progress = %u, reorg_occurred_during_reg_traversal = %u, ",
             __func__,
             registry_traversal_in_progress,
             reorg_occurred_during_reg_traversal
             );

    if (registry_traversal_in_progress && g_reorg_in_progress) {
        reorg_occurred_during_reg_traversal = true;
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Setting reorg_occurred_during_reg_traversal to true.", __func__);
    } else {
        reorg_occurred_during_reg_traversal = false;
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Setting reorg_occurred_during_reg_traversal to false.", __func__);
    }
}
// -----------------------------------------------------------------------------
// Class: PollRegistry::Sequence
// -----------------------------------------------------------------------------

using Sequence = PollRegistry::Sequence;

Sequence::Sequence(const PollMapByTitle& polls, const FilterFlag flags) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
    : m_polls(polls), m_flags(flags)
{
}

Sequence Sequence::Where(const FilterFlag flags) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return Sequence(m_polls, flags);
}

Sequence Sequence::OnlyActive(const bool active_only) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    int flags = m_flags;

    if (active_only) {
        flags = (flags & ~FINISHED) | ACTIVE;
    }

    return Sequence(m_polls, static_cast<FilterFlag>(flags));
}

Sequence::Iterator Sequence::begin() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    int64_t now = 0;

    if (!((m_flags & ACTIVE) && (m_flags & FINISHED))) {
        now = GetAdjustedTime();
    }

    return Iterator(m_polls.begin(), m_polls.end(), m_flags, now);
}

Sequence::Iterator Sequence::end() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return Iterator(m_polls.end());
}

// -----------------------------------------------------------------------------
// Class: PollRegistry::Sequence::Iterator
// -----------------------------------------------------------------------------

using Iterator = PollRegistry::Sequence::Iterator;

Iterator::Iterator(
    BaseIterator iter,
    BaseIterator end,
    const FilterFlag flags,
    const int64_t now)
    : m_iter(iter)
    , m_end(end)
    , m_flags(flags)
    , m_now(now)
{
    SeekNextMatch();
}

Iterator::Iterator(BaseIterator end) : m_iter(end), m_end(end)
{
}

const PollReference& Iterator::Ref() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return m_iter->second;
}

PollOption Iterator::TryPollFromDisk() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return m_iter->second.TryReadFromDisk();
}

Iterator::reference Iterator::operator*() const
{
    return *this;
}

Iterator::pointer Iterator::operator->() const
{
    return this;
}

Iterator& Iterator::operator++() EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    ++m_iter;
    SeekNextMatch();

    return *this;
}

Iterator Iterator::operator++(int) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    Iterator copy(*this);
    ++(*this);

    return copy;
}

bool Iterator::operator==(const Iterator& other) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return m_iter == other.m_iter;
}

bool Iterator::operator!=(const Iterator& other) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return m_iter != other.m_iter;
}

void Iterator::SeekNextMatch() EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    if (m_flags == FilterFlag::NO_FILTER) {
        return;
    }

    while (m_iter != m_end) {
        if (m_now > 0) {
            if (m_flags & ACTIVE) {
                if (!m_iter->second.Expired(m_now)) {
                    break;
                }
            } else {
                if (m_iter->second.Expired(m_now)) {
                    break;
                }
            }
        }

        ++m_iter;
    }
}
