// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "gridcoin/claim.h"
#include "gridcoin/pool.h"
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
#include <util/string.h>

#include <set>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

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
    return PollTitleKey(key.substr(0, key.find(';')));
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
            if (payload.m_claim.m_version == 1) {
                // Legacy single address (converted by serialization)
                if (payload.m_claim.m_balance_claim.m_address_claims.size() != 1) {
                    return false;
                }
                return VerifyAddressClaim(
                    payload.m_claim.m_balance_claim.m_address_claims[0],
                    message);
            } else {
                // Version 2+: multiple addresses
                return VerifyBalanceClaim(payload.m_claim.m_balance_claim, message);
            }
        } catch (const InvalidPollError& e) {
            LogPrint(LogFlags::VOTE, "%s: bad poll claim", __func__);
            return false;
        }
    }

private:
    CTxDB& m_txdb; //!< Used to resolve unspent amount claims.

    //!
    //! \brief Determine whether the balance claim (multiple addresses)
    //! establishes eligibility for creating the poll.
    //!
    //! \param claim   The balance claim to verify minimum balance for.
    //! \param message Serialized context for claim signature verification.
    //!
    //! \return \c true If the resolved amount for the claim meets the
    //! minimum balance requirement to create a poll.
    //!
    bool VerifyBalanceClaim(const BalanceClaim& claim, const ClaimMessage& message)
    {
        CAmount total_amount = 0;

        for (const auto& address_claim : claim.m_address_claims) {
            if (!address_claim.VerifySignature(message)) {
                LogPrint(LogFlags::VOTE, "%s: bad address signature", __func__);
                return false;
            }

            const CTxDestination address = address_claim.m_public_key.GetID();

            for (const auto& txo : address_claim.m_outpoints) {
                total_amount += Resolve(txo, address);
            }
        }

        return total_amount >= POLL_REQUIRED_BALANCE;
    }

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
    bool VerifyAddressClaim(const AddressClaim& claim, const ClaimMessage& message)
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
//! \brief The block index entry for a block hash, or nullptr if the hash is not in the block index.
//!
const CBlockIndex* LookupBlockIndex(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const auto iter = mapBlockIndex.find(hash);

    return iter == mapBlockIndex.end() ? nullptr : iter->second;
}

//!
//! \brief The db version stored in LevelDB under (key_type + "_db", "version"), or 0 when there is none.
//!
uint32_t StoredVersion(const std::string& key_type)
{
    uint32_t version = 0;
    std::pair<std::string, std::string> key = std::make_pair(key_type + "_db", std::string("version"));

    if (!CTxDB("r").ReadGenericSerializable(key, version)) {
        version = 0;
    }

    return version;
}

//!
//! \brief Global poll registry instance.
//!
PollRegistry g_poll_registry;

//! Walk the main chain forward from the block after \p pindex_searched, at most to \p pindex_bound's height, until the
//! first block whose time exceeds \p end_time. Records the first block whose time is at or past \p end_time in
//! \p pindex_at_or_past (if not yet found) and the first block past it in \p pindex_past_end, and leaves
//! \p pindex_searched at the last block examined. Does nothing once \p pindex_past_end is set. The caller has checked
//! that \p pindex_searched and \p pindex_bound are on the main chain.
void SearchForEnd(const CBlockIndex*& pindex_searched, const CBlockIndex*& pindex_at_or_past,
                  const CBlockIndex*& pindex_past_end, const int64_t end_time, const CBlockIndex* pindex_bound)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    while (!pindex_past_end && pindex_searched->pnext && pindex_searched->pnext->nHeight <= pindex_bound->nHeight) {
        pindex_searched = pindex_searched->pnext;

        if (!pindex_at_or_past && pindex_searched->nTime >= end_time) {
            pindex_at_or_past = pindex_searched;
        }

        if (pindex_searched->nTime > end_time) {
            pindex_past_end = pindex_searched;
        }
    }
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

// Definition of the static registry lock. Declared static so the thread-safety
// annotations on the nested Sequence/Iterator classes resolve to one
// unambiguous object (see the declaration in registry.h).
CCriticalSection PollRegistry::cs_poll_registry;

PollRegistry& GRC::GetPollRegistry()
{
    return g_poll_registry;
}

std::optional<SuperblockPtr> GRC::LastWellFormedSuperblock(const CBlockIndex* pindex)
{
    for (; pindex; pindex = pindex->pprev) {
        // Apparently the superblock flags in pindex are broken for superblocks earlier than 1034768 on mainnet and
        // earlier than 196562 on testnet.
        // TODO: Repair the index flags loaded in LevelDB. (This will require a special correction routine at startup.)
        if (!((OnTestnet() && pindex->nHeight < 196562) || (!OnTestnet() && pindex->nHeight < 1034768)
              || pindex->IsSuperblock())) {
            continue;
        }

        const ClaimOption claim = GetClaimByIndex(pindex);

        if (claim && claim->ContainsSuperblock() && claim->m_superblock->WellFormed()) {
            SuperblockPtr superblock = claim->m_superblock;
            superblock.Rebind(pindex);

            return superblock;
        }
    }

    return std::nullopt;
}

std::string GRC::GetCurrentPollTitle()
{
    // cs_main before cs_poll_registry, the order block connection takes them: whether a poll is open is judged by
    // blocks on the tip. Both are held across the whole judgment, so no reorg lands in between.
    LOCK2(cs_main, PollRegistry::cs_poll_registry);

    if (pindexBest) {
        if (const PollReference* poll_ref = GetPollRegistry().TryLatestActive(pindexBest)) {
            return poll_ref->Title();
        }
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
    , m_weight_type(PollWeightType::UNKNOWN)
    , m_ptitle(nullptr)
    , m_timestamp(0)
    , m_duration_days(0)
    , m_pindex(nullptr)
    , m_votes({})
    , m_votes_head()
    , m_expiration_warn_notified(false)
    , m_stored(false)
    , m_deleted(false)
    , m_registry(nullptr)
    , m_end_search_from(nullptr)
    , m_end_searched(nullptr)
    , m_at_or_past_end(nullptr)
    , m_past_end(nullptr)
    , m_off_main_chain_logged(false)
{
}

PollOption GRC::ReadPollFromDisk(CTxDB& txdb, const uint256& txid)
{
    CTransaction tx;

    if (!txdb.ReadDiskTx(txid, tx)) {
        error("%s: failed to read poll tx from disk", __func__);
        return std::nullopt;
    }

    for (auto& contract : tx.PullContracts()) {
        if (contract.m_type == ContractType::POLL) {
            auto payload = contract.PullPayloadAs<PollPayload>();
            // The time for the poll is the time of the containing transaction.
            payload.m_poll.m_timestamp = tx.nTime;

            return std::move(payload.m_poll);
        }
    }

    error("%s: transaction does not contain a poll contract", __func__);

    return std::nullopt;
}

PollOption PollReference::TryReadFromDisk(CTxDB& txdb) const
{
    PollOption poll = ReadPollFromDisk(txdb, m_txid);

    if (poll) {
        // Neither the timestamp nor the magnitude weight factor is serialized. Resolve the factor from the block
        // AddPoll recorded, rather than looking the poll's block up again: that lookup read the chain index without
        // cs_main, and took cs_main inside callers holding only cs_poll_registry. Nothing is written back to this
        // reference, which callers share.
        // The copy's timestamp is the poll's start, which from v15 is its block's time.
        poll->m_timestamp = StartTime();
        poll->m_magnitude_weight_factor = GetMagnitudeWeightFactor();

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: reference.m_timestamp = %" PRId64 " , poll.m_timestamp = %" PRId64 " , "
                 "poll.m_magnitude_weight_factor = %s",
                 __func__,
                 m_timestamp,
                 poll->m_timestamp,
                 poll->m_magnitude_weight_factor.ToString());
    }

    return poll;
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
    // m_title is the poll's own title, empty for a default-constructed reference. (m_ptitle points at the display
    // map's key only for the poll that owns its title, so a later poll with the same title key has none.)
    return m_title;
}

std::vector<uint256> PollReference::Votes() const
{
    // The cache below is written here and the chain it reads changes on block connection, so both are read under
    // the registry lock, and the caller gets a copy that stays valid after it is released.
    LOCK(PollRegistry::cs_poll_registry);

    // An in-memory poll (registered by the historical lookup) keeps the list it linked.
    if (!m_stored || m_registry == nullptr) {
        return m_votes;
    }

    // A stored poll's votes are its vote link chain. The cached list stays valid while the chain's head is the one
    // it was read at.
    const uint256 head = m_registry->VoteHead(m_txid);

    if (head != m_votes_head) {
        m_votes = m_registry->WalkVotes(m_txid);
        m_votes_head = head;
    }

    return m_votes;
}

int64_t PollReference::Time() const
{
    return m_timestamp;
}

int64_t PollReference::StartTime() const
{
    // From v15 a poll starts at the time of its block. Below the v15 height, and for a reference with no
    // block, it starts at the time of its transaction (#3470).
    if (m_pindex && IsV15Enabled(m_pindex->nHeight)) {
        return m_pindex->nTime;
    }

    return m_timestamp;
}

int64_t PollReference::Age(const int64_t now) const
{
    return now - StartTime();
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
    return StartTime() + (int64_t) (m_duration_days * 86400);
}

CBlockIndex* PollReference::GetStartingBlockIndexPtr() const
{
    // The context exposes block indexes as const; the entries themselves are not,
    // and callers walk the chain from here (as in Tally's m_start_pindex).
    return const_cast<CBlockIndex*>(m_pindex);
}

std::optional<int> PollReference::GetStartingHeight() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const CBlockIndex* pindex = GetStartingBlockIndexPtr();

    if (pindex != nullptr) {
        return pindex->nHeight;
    }

    return std::nullopt;
}

Fraction PollReference::GetMagnitudeWeightFactor() const
{
    // The factor in force at the poll's start, which from v15 is its block's time.
    return Poll::ResolveMagnitudeWeightFactor(m_pindex, StartTime());
}

std::optional<CAmount> GRC::ActiveVoteWeight(const PollSnapshot& snapshot, const PollResult& result)
{
    // Instrument this so we can log real time performance.
    g_timer.InitTimer(__func__, LogInstance().WillLogCategory(BCLog::LogFlags::VOTE));

    // Unfortunately, cs_main must be locked for the duration of this method, because it uses the chain index pointers.
    LOCK(cs_main);

    // The range comes from the snapshot the tally ran on. The chain helpers below take non-const block index
    // pointers; the entries are only read.
    CBlockIndex* const pindex_start = const_cast<CBlockIndex*>(snapshot.m_pindex_start);

    // If pindex_start is a nullptr, this is a degenerate poll reference. Return std::nullopt.
    if (pindex_start == nullptr) return std::nullopt;

    const CBlockIndex* pindex_end = snapshot.m_pindex_end;


    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Poll start height = %i.",
             __func__, pindex_start->nHeight);

    // If the poll is still active, then pindex_end will be nullptr, so then use the snapshot's pinned tip
    // rather than re-reading the live pindexBest.
    // This keeps every active poll in a table build anchored to the same end height; on a genuine tip
    // change the poll-result cache re-tallies against the new tip.
    if (pindex_end == nullptr) {
        pindex_end = snapshot.m_pindex_tip;

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Poll is still active. Using head of the chain (%i) as end height.",
                 __func__, pindex_end->nHeight);
    } else {
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Poll end height = %i.",
                 __func__, pindex_end->nHeight);
    }

    arith_uint256 active_vote_weight_tally = 0;
    unsigned int blocks = 0;

    // If poll weight type is balance only, then simply sum up the netweights over the block range of the poll.
    if (snapshot.m_weight_type == PollWeightType::BALANCE) {
        for (CBlockIndex* pindex = pindex_start; pindex ; pindex = pindex->pnext) {

            arith_uint256 net_weight = GetAvgNetworkWeight(pindex);

            active_vote_weight_tally += net_weight;
            ++blocks;

            if (pindex == pindex_end) {
                break;
            }
        }

        if (!blocks) {
            return std::nullopt;
        }

        return (active_vote_weight_tally / blocks).GetLow64();

        // If it is not balance only (handled by above), and is not BALANCE_AND_MAGNITUDE (everything else) then return nullopt.
        // In reality all weight types other than BALANCE and BALANCE_AND_MAGNITUDE are deprecated in Fern onwards, so this
        // only applies for legacy polls.
    } else if (snapshot.m_weight_type != PollWeightType::BALANCE_AND_MAGNITUDE) {
        return std::nullopt;
    }

    // Determine the pools that did NOT vote in the poll (via the result passed in). Only pools
    // that did not vote contribute to the magnitude correction for pools. The pool set is
    // anchored to the poll's start height so every node tallying the same poll uses the
    // identical registry view, even after post-V15 POOL contracts mutate the registry. Pre-V15
    // the registry contains only the grandfathered builtins seeded by PoolRegistry's
    // constructor, matching the legacy g_mining_pools list bit-for-bit (see plan §3 and the
    // BuiltinPools_match_g_mining_pools_pre_v15 test).
    std::vector<GRC::Pool> pools_not_voting;
    const std::vector<GRC::Pool> mining_pools =
        GRC::GetPoolRegistry().ActivePoolsAtHeight(pindex_start->nHeight);

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: %u out of %u pool cpids voted.",
             __func__,
             result.m_pools_voted.size(),
             mining_pools.size());

    for (const auto& pool : mining_pools) {
        if (std::find(result.m_pools_voted.begin(), result.m_pools_voted.end(),
                      pool.m_cpid) == result.m_pools_voted.end()) {
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
    arith_uint256 scaled_pool_magnitude = 0;
    arith_uint256 scaled_network_magnitude = 0;

    // The factor the votes were weighed with: the one PollResult::BuildFor put in
    // the result's poll. Resolving it again here could pick up a protocol entry
    // connected during the tally, and the vote percentage would mix two factors.
    const Fraction magnitude_weight_factor = result.m_poll.m_magnitude_weight_factor;

    // Lambda for active_vote_weight tally.
    const auto tally_active_vote_weight = [&](
            const arith_uint256 net_weight,
            const arith_uint256 money_supply,
            const arith_uint256 scaled_pool_magnitude,
            const arith_uint256 scaled_network_magnitude)
    {
        // Unlike the complementary calculation in EnableMagnitudeWeight for vote weights, this calculation is not
        // subject to overflow because it uses 256 bit integers. In the end it the RESULT is cast to a 64 bit integer, but
        // by then all of the multiplication and division are done. magnitude_weight_factor as a fraction something that is
        // not expected to stray any farther than the interval [1/10, 1/1], where the current default value of 100 / 567 is
        // in that interval. So we should never have an overflow problem here.
        active_vote_weight_tally += net_weight + money_supply * (scaled_network_magnitude - scaled_pool_magnitude)
                                                              * arith_uint256(magnitude_weight_factor.GetNumerator())
                                                              / arith_uint256(magnitude_weight_factor.GetDenominator())
                                                              / scaled_network_magnitude;

        ++blocks;
    };

    // The last well formed superblock at or before the start of the poll gives the first pool magnitudes.
    const std::optional<SuperblockPtr> start_superblock = LastWellFormedSuperblock(pindex_start);

    if (start_superblock) {
        for (const auto& pool : pools_not_voting) {
            scaled_pool_magnitude += (*start_superblock)->m_cpids.MagnitudeOf(pool.m_cpid).Scaled();
        }

        scaled_network_magnitude = (*start_superblock)->m_cpids.TotalScaledMagnitude();

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: last superblock before poll start: block height = %i, "
                                        "pool_magnitude = %f, network_magnitude = %f",
                 __func__,
                 start_superblock->m_height,
                 scaled_pool_magnitude.getdouble() / 100.0,
                 scaled_network_magnitude.getdouble() / 100.0
                 );
    }

    // If no well formed superblock was found at or before the start of the poll, or the network magnitude was 0, then
    // AVW cannot be accurately calculated for this poll, so return std::nullopt. This really will only happen to polls
    // REALLY early in the chain.
    if (!start_superblock || scaled_network_magnitude == 0) {
        LogPrintf("WARNING: %s: No valid superblock found prior to start of poll txid %s, title \"%s\", so no active vote "
                  "weight can be calculated.",
                 __func__,
                 snapshot.m_txid.GetHex(),
                 snapshot.m_title
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

        arith_uint256 net_weight = GetAvgNetworkWeight(pindex);

        arith_uint256 money_supply = pindex->nMoneySupply;

        tally_active_vote_weight(net_weight, money_supply, scaled_pool_magnitude, scaled_network_magnitude);

        // If voting logging category is active, log the first block and every superblock
        if (blocks == 1 || pindex->IsSuperblock()) {
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: tally_active_vote_weight: net_weight = %f, money_supply = %f, "
                                            "pool_magnitude = %f, network_magnitude = %f, magnitude weight factor = %s, "
                                            "block height = %i, blocks = %u, active_vote_weight_tally = %f, "
                                            "active_vote_weight = %f.",
                     __func__,
                     net_weight.getdouble() / (double) COIN,
                     money_supply.getdouble() / (double) COIN,
                     scaled_pool_magnitude.getdouble() / 100.0,
                     scaled_network_magnitude.getdouble() / 100.0,
                     magnitude_weight_factor.ToString(),
                     pindex->nHeight,
                     blocks,
                     active_vote_weight_tally.getdouble() / (double) COIN,
                     (active_vote_weight_tally / arith_uint256(blocks)).getdouble() / (double) COIN
                     );
        }

        // Log the last block and break if at pindex_end.
        if (pindex == pindex_end) {
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: tally_active_vote_weight: net_weight = %f, money_supply = %f, "
                                            "pool_magnitude = %f, network_magnitude = %f, magnitude weight factor = %s, "
                                            "block height = %i, blocks = %u, active_vote_weight_tally = %f, "
                                            "active_vote_weight = %f.",
                     __func__,
                     net_weight.getdouble() / (double) COIN,
                     money_supply.getdouble() / (double) COIN,
                     scaled_pool_magnitude.getdouble() / 100.0,
                     scaled_network_magnitude.getdouble() / 100.0,
                     magnitude_weight_factor.ToString(),
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
    g_timer.GetTimes("finished execution for poll id " + snapshot.m_txid.GetHex() + " title \"" + snapshot.m_title + "\"", __func__);

    if (!blocks) {
        LogPrintf("WARNING: %s: No blocks tallied for poll txid %s, title \"%s\", so no active vote weight can be "
                  "calculated.",
                 __func__,
                 snapshot.m_txid.GetHex(),
                 snapshot.m_title
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
                 snapshot.m_txid.GetHex(),
                 snapshot.m_title
                 );

        return std::nullopt;
    }
}

std::string PollReference::NotifyTypeToString(const PollNotificationType& notify_type) const
{
    switch (notify_type) {
    case PollNotificationType::POLL_ADD:
        return "added";
    case PollNotificationType::POLL_DELETE:
        return "deleted";
    case PollNotificationType::POLL_EXPIRE_WARNING:
        return "expiration_warning";
    case PollNotificationType::POLL_NOTIFY_TEST:
        return "test";
    default:
        return "unknown";
    }
}

std::string PollReference::Notify(const PollNotificationType& notify_type) const
{
    std::string strCmd = gArgs.GetArg("-pollnotify", std::string {});

#if HAVE_SYSTEM
    // Support running an external command on poll creation. Do not notify for already expired polls. This is especially
    // important during a sync to avoid spamming poll notifications.
    //
    // Judged by the wall clock on purpose, not by blocks like every display of whether a poll is open:
    // a notification is about the user's present. Judged by the block being connected, a node syncing through
    // history would find every poll open at its own block and announce each one.
    if (!Expired(GetAdjustedTime())) {
        if (!strCmd.empty()) {
            // The placeholders %s1 and %s2 are replaced with the txid and the notification type.
            ReplaceAll(strCmd, "%s1", m_txid.ToString());
            ReplaceAll(strCmd, "%s2", NotifyTypeToString(notify_type));
            boost::thread t(runCommand, strCmd); // thread runs free

            // -pollnotify is a sensitive setting, so the log names the poll, not the command.
            LogPrint(BCLog::LogFlags::MISC, "INFO: %s: poll notify command run for %s (%s)", __func__,
                     m_txid.ToString(), NotifyTypeToString(notify_type));

            if (notify_type == POLL_EXPIRE_WARNING) {
                m_expiration_warn_notified = true;
            }
        }
    }
#endif

    return strCmd;
}

bool PollReference::IsExpiringWarningNotified() const
{
    return m_expiration_warn_notified;
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
PollRegistry::PollRegistry()
    : m_latest_poll(nullptr)
    , m_poll_db(1)
    , m_vote_db(1)
    , m_initialized(false)
    , m_registering_legacy_poll(false)
{
}

int PollRegistry::Initialize()
{
    AssertLockHeld(cs_main);
    LOCK(cs_poll_registry);

    if (!m_initialized) {
        // The two dbs depend on each other: a version change in either clears both, so neither holds records the
        // other cannot explain.
        if (StoredVersion("poll") != m_poll_db.m_version || StoredVersion("vote") != m_vote_db.m_version) {
            m_poll_db.clear();
            m_vote_db.clear();
        }

        PollEntryMap unused_pending;
        PollEntryMap unused_expired;
        PollEntryMap unused_first;

        m_poll_db.m_load_observer = [this](const PollEntry& entry, const PollEntryMap&, const PollEntryMap&)
            EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_poll_registry) { IndexPollRecord(entry); };
        m_poll_db.Initialize(m_poll_entries, unused_pending, unused_expired, unused_first, false);
        m_poll_db.m_load_observer = nullptr;

        VoteLinkMap unused_vote_pending;
        VoteLinkMap unused_vote_expired;
        VoteLinkMap unused_vote_first;

        m_vote_db.Initialize(m_vote_links, unused_vote_pending, unused_vote_expired, unused_vote_first, false);

        m_initialized = true;
    }

    int poll_height = 0;
    int vote_height = 0;
    m_poll_db.LoadDBHeight(poll_height);
    m_vote_db.LoadDBHeight(vote_height);

    return std::max(poll_height, vote_height);
}

int PollRegistry::GetDBHeight()
{
    LOCK(cs_poll_registry);

    // Every stored record advances its own db's height, so every POLL and VOTE contract below the higher of the two
    // heights was processed.
    int poll_height = 0;
    int vote_height = 0;
    m_poll_db.LoadDBHeight(poll_height);
    m_vote_db.LoadDBHeight(vote_height);

    return std::max(poll_height, vote_height);
}

void PollRegistry::SetDBHeight(int& height)
{
    LOCK(cs_poll_registry);

    m_poll_db.StoreDBHeight(height);
    m_vote_db.StoreDBHeight(height);
}

uint64_t PollRegistry::PassivateDB()
{
    LOCK(cs_poll_registry);

    return m_poll_db.passivate_db() + m_vote_db.passivate_db();
}

void PollRegistry::ResetInMemoryOnly()
{
    LOCK(cs_poll_registry);

    ClearInMemory();
}

void PollRegistry::ClearInMemory()
{
    m_polls.clear();
    m_polls_by_txid.clear();
    m_latest_poll = nullptr;
    m_poll_entries.clear();
    m_vote_links.clear();
    m_title_index.clear();
    m_poll_db.clear_in_memory_only();
    m_vote_db.clear_in_memory_only();
    m_initialized = false;
    registry_traversal_in_progress = 0;
    reorg_occurred_during_reg_traversal = false;
}

bool PollRegistry::IsStored(const uint256& hash)
{
    return m_poll_db.contains(hash) || m_vote_db.contains(hash);
}

uint256 PollRegistry::VoteHead(const uint256& poll_txid) const
{
    const auto iter = m_vote_links.find(poll_txid);

    return iter == m_vote_links.end() ? uint256() : iter->second->m_hash;
}

std::optional<uint256> PollRegistry::TitleOwner(const std::string& title_key) const
{
    const auto iter = m_title_index.find(title_key);

    if (iter == m_title_index.end()) {
        return std::nullopt;
    }

    return iter->second;
}

std::vector<uint256> PollRegistry::WalkVotes(const uint256& poll_txid) const
{
    std::vector<uint256> votes;
    std::set<uint256> seen;

    for (uint256 hash = VoteHead(poll_txid); !hash.IsNull();) {
        // A link chain that loops back on itself is an error. Cut the walk and rebuild the registry from the chain
        // at the next start.
        if (!seen.insert(hash).second) {
            error("%s: vote link chain of poll %s loops at %s; poll registry flagged for a rebuild",
                  __func__, poll_txid.ToString(), hash.ToString());
            FlagRebuild();
            break;
        }

        const auto link = m_vote_db.peek(hash);

        if (!link) {
            error("%s: vote link %s of poll %s is missing", __func__, hash.ToString(), poll_txid.ToString());
            break;
        }

        votes.push_back(link->m_hash);
        hash = link->m_previous_hash;
    }

    std::reverse(votes.begin(), votes.end());

    return votes;
}

bool PollRegistry::EraseVoteLink(const uint256& vote_txid)
{
    // A vote that was never linked (outside the window, or for a poll the registry does not hold) has no record.
    if (!m_vote_db.contains(vote_txid)) {
        return true;
    }

    const auto link = m_vote_db.peek(vote_txid);
    const uint256 poll_txid = link->m_poll_txid;

    // A disconnect pops a poll's links newest first, so the link must be the poll's newest.
    if (VoteHead(poll_txid) != vote_txid) {
        error("%s: vote %s is not the newest link of poll %s", __func__, vote_txid.ToString(), poll_txid.ToString());
        return false;
    }

    m_vote_db.erase(vote_txid);

    if (link->m_previous_hash.IsNull()) {
        m_vote_links.erase(poll_txid);
    } else {
        m_vote_links[poll_txid] = m_vote_db.find(link->m_previous_hash)->second;
    }

    // Keep the poll's cached list in step when it was current.
    if (const auto iter = m_polls_by_txid.find(poll_txid);
        iter != m_polls_by_txid.end() && iter->second.m_votes_head == vote_txid && !iter->second.m_votes.empty()) {
        iter->second.m_votes.pop_back();
        iter->second.m_votes_head = link->m_previous_hash;
    }

    return true;
}

void PollRegistry::FlagRebuild() const
{
    uint32_t version = 0;
    std::pair<std::string, std::string> key = std::make_pair(std::string("poll_db"), std::string("version"));

    if (!CTxDB("rw").WriteGenericSerializable(key, version)) {
        error("%s: could not flag the poll registry for a rebuild", __func__);
    }
}

void PollRegistry::IndexPollRecord(const PollEntry& entry)
{
    const std::string title_key = PollTitleKey(entry.m_title);

    if (entry.m_status == PollEntryStatus::ACTIVE) {
        PollReference& ref = m_polls_by_txid[entry.m_poll_txid];
        ref.m_txid = entry.m_poll_txid;
        ref.m_title = entry.m_title;
        ref.m_payload_version = entry.m_payload_version;
        ref.m_type = static_cast<PollType>(entry.m_type);
        ref.m_weight_type = static_cast<PollWeightType>(entry.m_weight_type);
        ref.m_timestamp = entry.m_timestamp;
        ref.m_duration_days = entry.m_duration_days;
        if (const CBlockIndex* pindex = LookupBlockIndex(entry.m_block_hash)) {
            ref.m_pindex = pindex;
        }
        ref.m_stored = true;
        ref.m_deleted = false;
        ref.m_registry = this;

        // The first registration in chain order owns the title, deleted or not; only it is the poll a REMOVE
        // by title names (below v15) and the poll announced as new. Every ACTIVE poll is shown: a later poll with the
        // same title key (below v15 only) is listed and found by its title beside the owner. A poll's ACTIVE
        // record is indexed once: a load meets it once, and a removal's revert follows the removal that took the
        // poll out of the map.
        m_title_index.emplace(title_key, entry.m_poll_txid);
        ref.m_ptitle = &m_polls.emplace(title_key, &ref)->first;

        return;
    }

    // DELETED: the poll leaves the display map; it keeps its txid entry and its title.
    if (const auto iter = m_polls_by_txid.find(entry.m_poll_txid); iter != m_polls_by_txid.end()) {
        iter->second.m_deleted = true;
        EraseShown(title_key, &iter->second);
    }
}

void PollRegistry::EraseShown(const std::string& title_key, const PollReference* poll_ref)
{
    const auto [first, last] = m_polls.equal_range(title_key);

    for (auto iter = first; iter != last; ++iter) {
        if (iter->second == poll_ref) {
            m_polls.erase(iter);
            return;
        }
    }
}

const PollRegistry::Sequence PollRegistry::Polls() const
{
    // Deliberately does NOT take cs_poll_registry. It used to, which was
    // actively misleading: the returned Sequence holds a reference to the
    // guarded m_polls map, so the lock has to be held for the whole traversal,
    // and taking it here only covered the construction. EXCLUSIVE_LOCKS_REQUIRED
    // on the declaration now makes the caller's obligation checkable.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-reference"
#endif
    return Sequence(m_polls);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}

const PollReference* PollRegistry::TryLatestActive(const CBlockIndex* pindex_tip) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    // Whether a poll is open is judged by blocks on the tip, like the poll table and the result RPCs,
    // never by the wall clock.
    if (m_latest_poll && OpenAt(*m_latest_poll, pindex_tip)) {
        return m_latest_poll;
    }

    const PollReference* latest_open = nullptr;

    // Maybe more than one poll has the same timestamp... therefore use >=.
    for (auto iter = m_polls.cbegin(); iter != m_polls.cend(); ++iter) {
        if ((!latest_open || iter->second->m_timestamp >= latest_open->m_timestamp)
            && OpenAt(*iter->second, pindex_tip)) {
            latest_open = iter->second;
        }
    }

    return latest_open;
}

const PollReference* PollRegistry::TryByTxid(const uint256 txid) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    const auto iter = m_polls_by_txid.find(txid);

    // A poll a master-key REMOVE deleted stays registered by its txid and keeps its title, but is gone from lookups.
    if (iter == m_polls_by_txid.end() || iter->second.m_deleted) {
        return nullptr;
    }

    return &iter->second;
}

PollSnapshot PollRegistry::Snapshot(const PollReference& ref, const CBlockIndex* pindex_tip) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    PollSnapshot snapshot = SnapshotWithoutVotes(ref, pindex_tip);
    snapshot.m_votes = ref.Votes();

    return snapshot;
}

bool PollRegistry::OpenAt(const PollReference& ref, const CBlockIndex* pindex_tip) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    return !SnapshotWithoutVotes(ref, pindex_tip).m_finished;
}

PollSnapshot PollRegistry::SnapshotWithoutVotes(const PollReference& ref, const CBlockIndex* pindex_tip) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    PollSnapshot snapshot;

    snapshot.m_txid = ref.m_txid;
    snapshot.m_payload_version = ref.m_payload_version;
    snapshot.m_type = ref.m_type;
    snapshot.m_weight_type = ref.m_weight_type;
    snapshot.m_title = ref.m_title;
    snapshot.m_start_time = ref.StartTime();
    snapshot.m_end_time = ref.Expiration();
    snapshot.m_pindex_tip = pindex_tip;

    // Every judgment below is made on the chain that ends at the pinned tip, never on the wall clock.
    // The walks below follow pnext, which describes only the current main chain, so the pinned tip must still be on
    // it. It is checked here, under cs_main.
    //
    // This throw is the same signal as PollRegistry::reorg_occurred_during_reg_traversal, and the callers already
    // retry on it: BuildPollTable starts over on a freshly pinned tip, and getpollresults and votedetails report a
    // reorg in progress.
    // A caller that holds cs_main from pinning the tip until its last snapshot (listpolls, the interface methods the
    // GUI calls) cannot see a reorg in between and never gets this throw.
    if (!pindex_tip->IsInMainChain()) {
        throw InvalidDuetoReorgFork();
    }

    // The poll's block is on the main chain for every poll the registry holds, unless the poll's record is corrupt: a
    // reorg that disconnects the block reverts the poll under the same cs_main, and every caller in the node looks the
    // poll up and takes this snapshot under one hold of cs_main. The snapshot does not throw for such a poll: it
    // reads the poll as one with no block, as it reads a skeleton reference, and logs it once per reference.
    const CBlockIndex* pindex_poll = ref.m_pindex;

    if (pindex_poll && !pindex_poll->IsInMainChain()) {
        if (!ref.m_off_main_chain_logged) {
            LogPrintf("WARNING: %s: Block %s of poll %s is not on the main chain. Reading the poll as one with no "
                      "block; start with -clearpollhistory to rebuild the poll registry.",
                      __func__, pindex_poll->GetBlockHash().ToString(), ref.m_txid.ToString());
            ref.m_off_main_chain_logged = true;
        }

        pindex_poll = nullptr;
    }

    snapshot.m_pindex_start = pindex_poll;
    snapshot.m_v15 = pindex_poll && IsV15Enabled(pindex_poll->nHeight);

    if (pindex_poll) {
        AdvanceEndSearch(ref, pindexBest);

        // The pinned tip's chain is the main chain up to the tip's height.
        if (ref.m_past_end && ref.m_past_end->nHeight <= pindex_tip->nHeight) {
            snapshot.m_pindex_past_end = ref.m_past_end;
        }
    }

    // A poll whose first block past the end is at or above the v15 height closes there: it is finished at
    // that block, and a later block does not reopen it. A poll whose own block is below the height
    // closes this way too when its first block past the end is at or above it; only its end follows v15. A poll
    // that closes below the height keeps the rule its tally used: finished when the tip's time exceeds the end. (A tip
    // past the end puts the first block past the end on the tip's chain, so the else branch also covers an open poll.)
    snapshot.m_v15_end = snapshot.m_pindex_past_end && IsV15Enabled(snapshot.m_pindex_past_end->nHeight);

    if (snapshot.m_v15_end) {
        snapshot.m_finished = true;
    } else {
        snapshot.m_finished = pindex_tip->nTime > snapshot.m_end_time;
    }

    // A finished poll's active vote weight range ends at its closing block, the block before its first block past the
    // end, when it has closed by block (m_v15_end), and otherwise at its first block at or past the end; an open
    // poll's ends at the pinned tip (m_pindex_end stays nullptr). Either end is on the tip's chain
    // at or below the tip: for a poll with m_v15_end the closing block is the parent of the first block past the end,
    // which is; and a poll finished by the tip's time has its first block at or past the end at the tip or before it
    // (or at the poll's own block, when that is the tip). A poll read with no block has no range, so it keeps nullptr
    // too, not an end its search cached before its block left the main chain.
    if (snapshot.m_finished && pindex_poll) {
        snapshot.m_pindex_end = snapshot.m_v15_end ? snapshot.m_pindex_past_end->pprev : ref.m_at_or_past_end;
    }

    // The block that bounds the tally by blocks: the closing block once the poll has closed by block, and
    // the pinned tip while a poll whose block is at or above the v15 height is open. Every other poll keeps the time
    // rules of #3470 in its tally. A poll whose block is below the height has no bound while it is open: until its
    // first block past the end exists, every block after the poll's block on the tip's chain is stamped at or before
    // the end, so the two rules agree on all of them.
    if (snapshot.m_v15_end) {
        snapshot.m_pindex_bound = snapshot.m_pindex_end;
    } else if (snapshot.m_v15 && !snapshot.m_finished) {
        snapshot.m_pindex_bound = pindex_tip;
    }

    return snapshot;
}

void PollRegistry::AdvanceEndSearch(const PollReference& ref, const CBlockIndex* pindex_bound) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    const int64_t end_time = ref.Expiration();

    // The search for the poll's end runs once per poll and stops at the first block past the end; an open poll's
    // resumes after the last block it examined. Everything it found stays valid exactly while that block stays on the
    // main chain (the blocks before it are then unchanged). A reorg that disconnects it, or a reference re-created with
    // another block, starts the search again from the poll's block, which counts toward the first block at or past the
    // end but not toward the first block past it.
    if (ref.m_end_search_from != ref.m_pindex || ref.m_end_searched == nullptr
        || !ref.m_end_searched->IsInMainChain()) {
        ref.m_end_search_from = ref.m_pindex;
        ref.m_end_searched = ref.m_pindex;
        ref.m_at_or_past_end = ref.m_pindex->nTime >= end_time ? ref.m_pindex : nullptr;
        ref.m_past_end = nullptr;
    }

    SearchForEnd(ref.m_end_searched, ref.m_at_or_past_end, ref.m_past_end, end_time, pindex_bound);
}

bool PollRegistry::VoteInWindow(const PollReference& poll_ref, const CBlockIndex* pindex_vote,
                                const bool legacy_vote) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    const CBlockIndex* const pindex_poll = poll_ref.m_pindex;

    // From v15 a vote is linked only from a block after the poll's block.
    if (pindex_poll && IsV15Enabled(pindex_poll->nHeight) && pindex_vote->nHeight <= pindex_poll->nHeight) {
        return false;
    }

    // The poll's first block past the end on the vote's chain, if it is the vote's block or a block before it. The
    // search runs no further than the block before the vote's, and only for a vote whose block is at or above the v15
    // height: below it, no first block past the end at or before the vote's block can be at or above the height, and
    // the #3470 rule below decides alone.
    const CBlockIndex* past_end = nullptr;

    if (pindex_poll && pindex_vote->nHeight > pindex_poll->nHeight && IsV15Enabled(pindex_vote->nHeight)) {
        AdvanceEndSearch(poll_ref, pindex_vote->pprev);

        if (poll_ref.m_past_end && poll_ref.m_past_end->nHeight < pindex_vote->nHeight) {
            past_end = poll_ref.m_past_end;
        }
    }

    if (!past_end && pindex_vote->nTime > poll_ref.Expiration()) {
        past_end = pindex_vote;
    }

    // A poll whose first block past the end is at or above the v15 height has closed by block: no vote after its
    // closing block is linked.
    if (past_end && IsV15Enabled(past_end->nHeight)) {
        return false;
    }

    // Otherwise the #3470 rules: a vote from version 2 is linked when its block's time does not exceed the end, and a
    // legacy vote by its title alone.
    return legacy_vote || !poll_ref.Expired(pindex_vote->nTime);
}

PollReference* PollRegistry::TryBy(const uint256 txid) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return const_cast<PollReference*>(TryByTxid(txid));
}

const PollReference* PollRegistry::TryByTitle(const std::string& title_key) const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    const auto owner = m_title_index.find(title_key);

    if (owner == m_title_index.end()) {
        return nullptr;
    }

    const auto iter = m_polls_by_txid.find(owner->second);

    if (iter == m_polls_by_txid.end() || iter->second.m_deleted) {
        return nullptr;
    }

    return &iter->second;
}

std::vector<const PollReference*> PollRegistry::FindByTitle(const std::string& title_key) const
    EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    // Block order, whatever order the polls entered the map in (a removal's revert puts its poll back at the end): a
    // stored poll's record holds its block's height; a pre-V11 poll loaded on demand has its block index.
    std::vector<std::pair<std::pair<int, uint256>, const PollReference*>> keyed;
    const auto [first, last] = m_polls.equal_range(title_key);

    for (auto iter = first; iter != last; ++iter) {
        const PollReference* ref = iter->second;
        int height = ref->m_pindex ? ref->m_pindex->nHeight : -1;

        if (const auto entry = m_poll_entries.find(ref->m_txid); entry != m_poll_entries.end()) {
            height = entry->second->m_block_height;
        }

        keyed.emplace_back(std::make_pair(height, ref->m_txid), ref);
    }

    std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<const PollReference*> polls;

    for (const auto& [key, ref] : keyed) {
        polls.push_back(ref);
    }

    return polls;
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
        const auto iter = mapBlockIndex.find(block_hash);

        if (iter != mapBlockIndex.end() && iter->second->IsInMainChain()) {
            pindex_poll = iter->second;
        }
    }

    // Only a poll in a main-chain block is loaded. One still in the mempool has no
    // block, and AddPoll refuses it: registered now, it would stay pinned to no
    // block after it confirms.
    if (pindex_poll == nullptr) {
        return nullptr;
    }

    // Below v15 the handler registers every poll from V11 on, so in production this registers pre-V11 polls, in
    // memory only (until the legacy store replaces this path). From v15 a poll the registry does not hold is one
    // whose title is taken: it is not registered by rule, and not loaded here either. The m_registering_legacy_poll
    // flag lets the add pass the V11 floor and keeps it out of the dbs.
    if (IsV15Enabled(pindex_poll->nHeight)) {
        return nullptr;
    }

    m_registering_legacy_poll = true;

    for (const auto& contract : tx.GetContracts()) {
        if (contract.m_type == ContractType::POLL && contract.m_action == ContractAction::ADD) {
            Add({contract, tx, pindex_poll});
        }
    }

    m_registering_legacy_poll = false;

    // Get the poll reference now that the poll contract(s) (if any) in the provided transaction hash have been validated
    // and loaded in the registry.
    PollReference* ref = TryBy(txid);

    // If the poll "contract" is loaded successfully into the registry, walk the contracts in the chain for
    // the duration of the poll to populate the votes. We can't use the general GRC::ReplayContracts here
    // because it is too broad.
    if (ref != nullptr) {
        CBlock block;

        // Link the poll's votes by the rule AddVote applies as each block
        // connects: a vote is linked only from the transaction after the poll's
        // own, a version 2+ vote only if its block is not stamped after the
        // poll ends, and a legacy vote by the poll's title in lower case. A
        // block may carry an earlier time than its parent, so the walk runs to
        // the tip. No block from version 11 on carries a legacy vote, so there
        // a block stamped after the poll ends is passed over unread.
        for (CBlockIndex* pindex = pindex_poll; pindex; pindex = pindex->pnext) {
            if (IsV11Enabled(pindex->nHeight) && ref->Expired(pindex->nTime)) continue;

            // If the block doesn't contain contract(s) or can't read, skip.
            if (!pindex->IsContract() || !ReadBlockFromDisk(block, pindex, Params().GetConsensus())) continue;

            // In the poll's own block, the votes ahead of the poll are not its.
            bool after_poll = pindex != pindex_poll;

            // Skip coinbase and coinstake transactions:
            for (unsigned int i = 2; i < block.vtx.size(); ++i) {
                if (!after_poll) {
                    after_poll = block.vtx[i].GetHash() == ref->Txid();
                    continue;
                }

                for (auto contract : block.vtx[i].GetContracts()) {

                    // Only process votes, and only a contract that adds one, as
                    // when a block connects (a REMOVE links no vote).
                    if (contract.m_type != ContractType::VOTE || contract.m_action != ContractAction::ADD) continue;

                    // Post fern poll votes
                    if (contract.m_version >= 2) {
                        const auto vote = contract.SharePayloadAs<Vote>();

                        if (vote->m_poll_txid == ref->Txid() && !ref->Expired(pindex->nTime)) {
                            ref->LinkVote(block.vtx[i].GetHash());
                        }

                        continue;
                    }

                    // Legacy poll votes: as when a block connects, a contract
                    // that is not well-formed is passed over, and a vote is
                    // matched to the poll by its title in lower case.
                    if (!contract.WellFormed()) continue;

                    const ContractPayload vote = contract.m_body.AssumeLegacy();
                    const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

                    if (title.empty()) continue;

                    if (title == PollTitleKey(ref->Title())) {
                        ref->LinkVote(block.vtx[i].GetHash());
                    }
                } // for contract
            } // for vtx
        } // for pindex
    }

    return ref;
}

void PollRegistry::Reset() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    LOCK(cs_poll_registry);

    ClearInMemory();

    // -clearpollhistory and -clearvotehistory each clear both dbs: one registry backs both contract types, and
    // neither db can be explained without the other.
    m_poll_db.clear();
    m_vote_db.clear();
}

bool PollRegistry::Validate(const Contract& contract, const CTransaction& tx, int& DoS) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
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

bool PollRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Vote contract claims do not affect consensus. Vote claim validation
    // occurs on-demand while computing the results of the poll:
    //
    if (ctx.m_contract.m_type == ContractType::VOTE) {
        return true;
    }

    const auto payload = ctx->SharePayloadAs<PollPayload>();

    // Enforce v3 polls at PollV3Height
    if (IsPollV3Enabled(ctx.m_pindex->nHeight) && payload->m_version < 3) {
        return false;
    }

    // Reject v2+ claims before multi-address activation (bidirectional enforcement)
    if (!IsPollMultiAddressEnabled(ctx.m_pindex->nHeight)
        && payload->m_claim.m_version >= 2) {
        DoS = 25;
        LogPrint(LogFlags::CONTRACT,
            "%s: rejected v2 poll claim before multi-address activation at height %d",
            __func__,
            ctx.m_pindex->nHeight);
        return false;
    }

    // Enforce multi-address claims at PollMultiAddressHeight
    if (IsPollMultiAddressEnabled(ctx.m_pindex->nHeight)
        && payload->m_claim.m_version < 2) {
        DoS = 25;
        LogPrint(LogFlags::CONTRACT,
            "%s: rejected v1 poll claim after multi-address activation at height %d",
            __func__,
            ctx.m_pindex->nHeight);
        return false;
    }

    // From block version 15, a poll claim names each address key and each
    // output once.
    if (IsV15Enabled(ctx.m_pindex->nHeight) && !payload->m_claim.EntriesDistinct()) {
        DoS = 25;
        LogPrint(LogFlags::CONTRACT,
            "%s: rejected poll claim whose address keys or outputs are not distinct at height %d",
            __func__,
            ctx.m_pindex->nHeight);
        return false;
    }

    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

void PollRegistry::Add(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    LOCK(cs_poll_registry);

    DetectReorg();

    // The poll registry's floor is V11 on every node. Below V11 it applies and reverts nothing, live, in the startup
    // replay or in a rebuild, although ConnectBlock applies contracts from the research-age height (364,501 on
    // mainnet), and a cleared registry replays only from V11. The floor is deliberate: the poll registry's scope is
    // what the poll history shows, and from v15 its title index is consensus, so both must be the same on every
    // node. Pre-V11 polls are reached through TryByTxidWithAddHistoricalPollAndVotes, which registers them in memory
    // only (until the legacy store replaces it).
    if (ctx.m_pindex && ctx.m_pindex->nHeight < Params().GetConsensus().BlockV11Height && !m_registering_legacy_poll) {
        return;
    }

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

    // V11 floor: see the comment in Add.
    if (ctx.m_pindex && ctx.m_pindex->nHeight < Params().GetConsensus().BlockV11Height) {
        return;
    }

    if (ctx->m_type == ContractType::VOTE) {
        DeleteVote(ctx);
    } else {
        DeletePoll(ctx);
    }
}

void PollRegistry::Revert(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    LOCK(cs_poll_registry);

    DetectReorg();

    // V11 floor: see the comment in Add.
    if (ctx.m_pindex && ctx.m_pindex->nHeight < Params().GetConsensus().BlockV11Height) {
        return;
    }

    const uint256 hash = ctx.m_tx.GetHash();

    if (ctx->m_type == ContractType::VOTE) {
        // A VOTE REMOVE linked nothing, so its revert erases nothing.
        if (ctx->m_action != ContractAction::ADD) {
            return;
        }

        if (m_vote_db.contains(hash)) {
            // A disconnect pops a poll's links newest first. For a link that is not its poll's newest, nothing is
            // erased: the error is logged and the registry is rebuilt from the chain at the next start. No assert:
            // asserts are live in release builds.
            if (!EraseVoteLink(hash)) {
                error("%s: poll registry flagged for a rebuild at the next start", __func__);
                FlagRebuild();
            }

            return;
        }

        // A vote linked to an in-memory poll from the historical lookup.
        DeleteVote(ctx);

        return;
    }

    if (!m_poll_db.contains(hash)) {
        // Not stored: a REMOVE that resolved to nothing, or an in-memory poll from the historical lookup.
        if (ctx->m_action == ContractAction::ADD) {
            DeletePoll(ctx);
        }

        return;
    }

    const std::shared_ptr<PollEntry> record = m_poll_db.peek(hash);
    const uint256 poll_txid = record->m_poll_txid;

    if (record->m_status == PollEntryStatus::DELETED) {
        // Reverting a REMOVE: its DELETED record goes, the poll's ADD record is current again, and the poll returns to
        // the display map.
        m_poll_db.erase(hash);

        const auto add_record = m_poll_db.find(record->m_previous_hash);

        if (add_record == m_poll_db.end()) {
            error("%s: poll %s has no ADD record behind its DELETED record; poll registry flagged for a rebuild",
                  __func__, poll_txid.ToString());
            FlagRebuild();
            return;
        }

        m_poll_entries[poll_txid] = add_record->second;
        IndexPollRecord(*add_record->second);

        if (const auto iter = m_polls_by_txid.find(poll_txid); iter != m_polls_by_txid.end() && !m_replay_in_progress) {
            iter->second.Notify(PollReference::PollNotificationType::POLL_ADD);
        }

        if (!m_replay_in_progress) {
            uiInterface.NewPollReceived(record->m_timestamp);
        }

        return;
    }

    // Reverting a poll's ADD. Its votes were in its own block after it or in later blocks, all popped already.
    if (m_vote_links.count(poll_txid)) {
        error("%s: poll %s still has vote links when its ADD is reverted; poll registry flagged for a rebuild",
              __func__, poll_txid.ToString());
        FlagRebuild();
        return;
    }

    DeletePoll(ctx);
}

void PollRegistry::AddPoll(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    const auto payload = ctx->SharePayloadAs<PollPayload>();

    // Reverting a master-key REMOVE reaches Add with the REMOVE's own transaction,
    // which registers nothing. The removed poll comes back with the startup replay.
    if (ctx->m_action == ContractAction::REMOVE) {
        LogPrint(BCLog::LogFlags::VOTE, "%s: not registering a poll from reverted REMOVE %s.",
                 __func__, ctx.m_tx.GetHash().ToString());
        return;
    }

    // A poll is registered from the block that contains it. Without one, the poll
    // would stay pinned to no block, because the add from its block would find the
    // txid already present below.
    if (ctx.m_pindex == nullptr) {
        LogPrint(BCLog::LogFlags::VOTE, "%s: poll %s has no block; not registered.",
                 __func__, ctx.m_tx.GetHash().ToString());
        return;
    }

    const uint256 txid = ctx.m_tx.GetHash();

    // In a contract replay inserts may overlap what was already inserted. Check to see if poll already exists, and
    // if so skip this add.
    if (m_polls_by_txid.find(txid) != m_polls_by_txid.end()) {
        return;
    }

    const std::string& poll_title = payload->m_poll.m_title;

    if (m_registering_legacy_poll) {
        // A pre-V11 poll found by TryByTxidWithAddHistoricalPollAndVotes: registered in memory only, with no record
        // and no title-index entry. It is shown beside any poll with its title key.
        PollReference& poll_ref = m_polls_by_txid[txid];
        poll_ref.m_ptitle = &m_polls.emplace(PollTitleKey(poll_title), &poll_ref)->first;
        poll_ref.m_txid = txid;
        poll_ref.m_title = poll_title;
        poll_ref.m_payload_version = payload->m_version;
        poll_ref.m_type = payload->m_poll.m_type.Value();
        poll_ref.m_weight_type = payload->m_poll.m_weight_type.Value();
        poll_ref.m_timestamp = ctx.m_tx.nTime;
        poll_ref.m_duration_days = payload->m_poll.m_duration_days;
        poll_ref.m_pindex = ctx.m_pindex;
        poll_ref.m_registry = this;

        return;
    }

    // From v15 a poll whose title key is taken registers nothing: by a poll the title index holds -- every poll
    // registered since V11, active or deleted -- including one earlier in this block, which applied first. The
    // transaction stays valid and its burn is spent; no block is refused over a title.
    if (IsV15Enabled(ctx.m_pindex->nHeight) && TitleOwner(PollTitleKey(poll_title))) {
        LogPrint(BCLog::LogFlags::VOTE, "%s: poll %s not registered: its title is taken.",
                 __func__, txid.ToString());
        return;
    }

    PollEntry entry;
    entry.m_poll_txid = txid;
    entry.m_title = poll_title;
    entry.m_payload_version = payload->m_version;
    entry.m_type = static_cast<uint32_t>(payload->m_poll.m_type.Value());
    entry.m_weight_type = static_cast<uint32_t>(payload->m_poll.m_weight_type.Value());
    entry.m_timestamp = ctx.m_tx.nTime;
    entry.m_duration_days = payload->m_poll.m_duration_days;
    entry.m_block_hash = ctx.m_pindex->GetBlockHash();
    entry.m_block_height = ctx.m_pindex->nHeight;
    entry.m_hash = txid;
    entry.m_previous_hash = uint256();
    entry.m_status = PollEntryStatus::ACTIVE;

    if (!m_poll_db.insert(txid, ctx.m_pindex->nHeight, entry)) {
        error("%s: poll record %s was not stored", __func__, txid.ToString());
        return;
    }

    m_poll_entries[txid] = m_poll_db.find(txid)->second; // just inserted: in memory, nothing loaded
    IndexPollRecord(entry);

    PollReference& poll_ref = m_polls_by_txid.at(txid);
    poll_ref.m_pindex = ctx.m_pindex; // the connecting block, whose index the load would find by hash

    // A later poll with a title key already taken (below v15 only) is listed and found by its title, but only
    // the title's owner is announced.
    if (TryByTitle(PollTitleKey(poll_title)) != &poll_ref) {
        return;
    }

    m_latest_poll = &poll_ref;

    if (!m_replay_in_progress) {
        poll_ref.Notify(PollReference::PollNotificationType::POLL_ADD);

        // Judged by the wall clock on purpose; see PollReference::Notify.
        if (!poll_ref.Expired(GetAdjustedTime())) {
            uiInterface.NewPollReceived(poll_ref.Time());
        }
    }

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Added poll %s to the registry, m_polls.size() = %u.",
             __func__,
             *poll_ref.m_ptitle,
             m_polls.size());
}

void PollRegistry::AddVote(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    // Only a vote's ADD links a vote. A master-key VOTE REMOVE reaches Add only when it is reverted, and it links
    // nothing, so its revert stores nothing either.
    if (ctx->m_action != ContractAction::ADD) {
        return;
    }

    const uint256 vote_txid = ctx.m_tx.GetHash();
    PollReference* poll_ref = nullptr;

    if (ctx->m_version >= 2) {
        const auto vote = ctx->SharePayloadAs<Vote>();

        poll_ref = TryBy(vote->m_poll_txid);
    } else {
        // A legacy vote names its poll by title in lower case. (On mainnet legacy votes exist only below V11.)
        const ContractPayload vote = ctx->m_body.AssumeLegacy();
        const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

        if (!title.empty()) {
            poll_ref = TryBy(title);
        }
    }

    if (poll_ref == nullptr) {
        return;
    }

    if (!VoteInWindow(*poll_ref, ctx.m_pindex, ctx->m_version < 2)) {
        LogPrint(LogFlags::VOTE,
            "%s: ignored vote %s outside the window of poll %s",
            __func__,
            vote_txid.ToString(),
            poll_ref->Txid().ToString());

        return;
    }

    if (!poll_ref->m_stored) {
        // An in-memory poll from the historical lookup: link in memory.
        if (std::find(poll_ref->m_votes.begin(), poll_ref->m_votes.end(), vote_txid) != poll_ref->m_votes.end()) {
            return;
        }

        poll_ref->LinkVote(vote_txid);
    } else {
        // Already linked: the startup replay of the db-height block, or a duplicate apply. contains() leaves a
        // passivated link passivated (find() would load it and make a later insert refuse it).
        if (m_vote_db.contains(vote_txid)) {
            return;
        }

        VoteLinkEntry link;
        link.m_poll_txid = poll_ref->m_txid;
        link.m_timestamp = ctx.m_tx.nTime;
        link.m_hash = vote_txid;
        link.m_previous_hash = VoteHead(poll_ref->m_txid);
        link.m_block_hash = ctx.m_pindex->GetBlockHash();
        link.m_block_height = ctx.m_pindex->nHeight;
        link.m_status = VoteLinkStatus::ACTIVE;

        if (!m_vote_db.insert(vote_txid, ctx.m_pindex->nHeight, link)) {
            error("%s: vote link %s was not stored", __func__, vote_txid.ToString());
            return;
        }

        // Keep the poll's cached list in step when it is current; otherwise it is read again on next use.
        if (poll_ref->m_votes_head == link.m_previous_hash) {
            poll_ref->m_votes.push_back(vote_txid);
            poll_ref->m_votes_head = vote_txid;
        }

        m_vote_links[poll_ref->m_txid] = m_vote_db.find(vote_txid)->second; // just inserted: nothing loaded
    }

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Added vote %s to poll %s.", __func__, vote_txid.GetHex(), poll_ref->m_title);

    // Judged by the wall clock on purpose; see PollReference::Notify.
    if (!m_replay_in_progress && !poll_ref->Expired(GetAdjustedTime())) {
        uiInterface.NewVoteReceived(poll_ref->Txid());
    }
}

void PollRegistry::DeletePoll(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    const auto payload = ctx->SharePayloadAs<PollPayload>();

    int64_t poll_time = payload->m_poll.m_timestamp;

    PollReference* poll_ref = nullptr;

    if (ctx->m_action == ContractAction::REMOVE) {
        // A master-key REMOVE names the poll by title.
        poll_ref = TryBy(PollTitleKey(payload->m_poll.m_title));
    } else {
        // The poll's own ADD, reverted: a revert removes only the poll its own
        // contract added, which it finds by its transaction.
        const auto iter = m_polls_by_txid.find(ctx.m_tx.GetHash());

        if (iter != m_polls_by_txid.end()) {
            poll_ref = &iter->second;
        }
    }

    if (!poll_ref) {
        LogPrint(BCLog::LogFlags::VOTE, "%s: poll \"%s\" (%s) not found in registry.",
                 __func__, payload->m_poll.m_title, ctx.m_tx.GetHash().ToString());
        return;
    }

    if (!m_replay_in_progress) {
        poll_ref->Notify(PollReference::PollNotificationType::POLL_DELETE);
    }

    const uint256 txid = poll_ref->Txid();
    const std::string title_key = PollTitleKey(poll_ref->m_title);

    // Either way the poll leaves the display map.
    EraseShown(title_key, poll_ref);

    m_latest_poll = nullptr;

    if (ctx->m_action == ContractAction::REMOVE) {
        // A master-key REMOVE: the poll stays registered by its txid and keeps its title. For a stored poll the
        // REMOVE stores a DELETED record that links back to the poll's current record, so a restart rebuilds the
        // same state.
        poll_ref->m_deleted = true;

        if (poll_ref->m_stored && !m_poll_db.contains(ctx.m_tx.GetHash())) {
            PollEntry entry = *m_poll_entries.at(txid);
            entry.m_previous_hash = entry.m_hash;
            entry.m_hash = ctx.m_tx.GetHash();
            entry.m_block_hash = ctx.m_pindex->GetBlockHash();
            entry.m_block_height = ctx.m_pindex->nHeight;
            entry.m_status = PollEntryStatus::DELETED;

            if (m_poll_db.insert(entry.m_hash, ctx.m_pindex->nHeight, entry)) {
                m_poll_entries[txid] = m_poll_db.find(entry.m_hash)->second; // just inserted: nothing loaded
            } else {
                error("%s: deletion record %s was not stored", __func__, entry.m_hash.ToString());
            }
        }
    } else {
        // The poll's own ADD, reverted: its record, its title (if it owned it) and its entry go.
        if (poll_ref->m_stored) {
            m_poll_db.erase(txid);
            m_poll_entries.erase(txid);
        }

        if (const auto owner = m_title_index.find(title_key); owner != m_title_index.end() && owner->second == txid) {
            m_title_index.erase(owner);
        }

        m_polls_by_txid.erase(txid);
    }

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Deleted poll %s to the registry, m_polls.size() = %u.",
             __func__,
             payload->m_poll.m_title,
             m_polls.size());

    if (!m_replay_in_progress) {
        uiInterface.NewPollReceived(poll_time);
    }
}

void PollRegistry::DeleteVote(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    if (ctx->m_version >= 2) {
        const auto vote = ctx->SharePayloadAs<Vote>();

        if (PollReference* poll_ref = TryBy(vote->m_poll_txid)) {
            if (poll_ref->m_stored) {
                EraseVoteLink(ctx.m_tx.GetHash());
            } else {
                poll_ref->UnlinkVote(ctx.m_tx.GetHash());
            }

            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Deleted vote %s from poll %s, poll_ref->Votes().size() = %u.",
                     __func__,
                     ctx.m_tx.GetHash().GetHex(),
                     poll_ref->m_title,
                     poll_ref->Votes().size());

            // Judged by the wall clock on purpose; see PollReference::Notify.
            if (!m_replay_in_progress && !poll_ref->Expired(GetAdjustedTime())) {
                uiInterface.NewVoteReceived(poll_ref->Txid());
            }
        }

        return;
    }

    const auto vote = ctx->SharePayload();
    const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

    if (title.empty()) {
        return;
    }

    if (PollReference* poll_ref = TryBy(title)) {
        if (poll_ref->m_stored) {
            EraseVoteLink(ctx.m_tx.GetHash());
        } else {
            poll_ref->UnlinkVote(ctx.m_tx.GetHash());
        }

        // Judged by the wall clock on purpose; see PollReference::Notify.
        if (!m_replay_in_progress && !poll_ref->Expired(GetAdjustedTime())) {
            uiInterface.NewVoteReceived(poll_ref->Txid());
        }
    }
}

void PollRegistry::DetectReorg()
{
    // Reorg detector
    // Note that doing the reorg detection here, in the contract handler, means that we only flag a reorg IF
    // a transaction happened to occur that involves a poll or vote contract in the scope of the reorg, because
    // these handlers are only triggered by those two contract types.
    // PollRegistry::Snapshot throws the same InvalidDuetoReorgFork when the pinned tip is off the main chain, and
    // callers retry exactly as they do on this flag.
    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: registry_traversal_in_progress = %d, reorg_occurred_during_reg_traversal = %d, ",
             __func__,
             registry_traversal_in_progress.load(),
             reorg_occurred_during_reg_traversal.load()
             );

    if (registry_traversal_in_progress > 0 && g_reorg_in_progress) {
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

Sequence::Sequence(const PollMapByTitle& polls) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
    : m_polls(polls)
{
}

Sequence::Iterator Sequence::begin() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return Iterator(m_polls.begin(), m_polls.end());
}

Sequence::Iterator Sequence::end() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return Iterator(m_polls.end());
}

// -----------------------------------------------------------------------------
// Class: PollRegistry::Sequence::Iterator
// -----------------------------------------------------------------------------

using Iterator = PollRegistry::Sequence::Iterator;

Iterator::Iterator(BaseIterator iter, BaseIterator end) : m_iter(iter), m_end(end)
{
}

Iterator::Iterator(BaseIterator end) : m_iter(end), m_end(end)
{
}

const PollReference& Iterator::Ref() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return *m_iter->second;
}

PollOption Iterator::TryPollFromDisk() const EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    return m_iter->second->TryReadFromDisk();
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
