// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/kernel.h"
#include "txdb.h"
#include "random.h"
#include "streams.h"
#include "util.h"
#include "consensus/validation.h"
#include "script/interpreter.h"

#include <node/blockstorage.h>

#include <algorithm>

using namespace std;
using namespace GRC;

// Moved from main.cpp (issue #3125 C9); declared in kernel.h.

//Gridcoin Minimum Stake Age (16 Hours)
unsigned int nStakeMinAge = 16 * 60 * 60; // 16 hours
unsigned int nStakeMaxAge = -1; // unlimited

namespace {
//!
//! \brief Represents a candidate block for new stake modifier selection.
//!
class StakeModifierCandidate
{
public:
    int64_t m_block_time;        //!< Timestamp of the block.
    uint256 m_block_hash;        //!< Hash of the block.
    const CBlockIndex* m_pindex; //!< Points to the block's index record.

    //!
    //! \brief Cache of the candidate's selection hash.
    //!
    //! This contains the hash of the block's proof hash and the previous stake
    //! modifier value. During candidate selection, SelectBlockFromCandidates()
    //! can potentially compare this hash many times. By caching the hash value
    //! here, we avoid recomputing it for each iteration of the selection loop.
    //!
    mutable std::optional<arith_uint256> m_selection_hash;

    //!
    //! \brief Initialize a new candidate entry.
    //!
    //! \param pindex Points to block index for the candidate.
    //!
    StakeModifierCandidate(const CBlockIndex* const pindex)
        : m_block_time(pindex->GetBlockTime())
        , m_block_hash(pindex->GetBlockHash())
        , m_pindex(pindex)
    {
    }

    //!
    //! \brief Determine whether this candidate represents a lesser value.
    //!
    //! The stake modifier selection algorithm sorts candidates by block times
    //! in ascending order and compares the block hashes when two entries have
    //! equal timestamps.
    //!
    //! Upgrading to Bitcoin's latest uint256 type changes the backing storage
    //! from an array of 32-bit integers to a byte array. Since the comparison
    //! operator implementations changed as well, the less-than comparison for
    //! sorting the vector of candidates can produce different block orderings
    //! for the historical stake modifier than the original order from before.
    //!
    //! To ensure that we reproduce the same stake modifier values out of this
    //! candidate set, we adopt Peercoin's strategy for sorting the collection
    //! that behaves like the old uint256 implementation:
    //!
    //! \param other Candidate to compare the object to.
    //!
    //! \return \c true if the candidate compares less than the other.
    //!
    bool operator<(const StakeModifierCandidate& other) const
    {
        if (m_block_time != other.m_block_time) {
            return m_block_time < other.m_block_time;
        }

        // Timestamps equal. Compare block hashes:
        const uint32_t* pa = (uint32_t*)m_block_hash.data();
        const uint32_t* pb = (uint32_t*)other.m_block_hash.data();

        int cnt = 256 / 32;

        do {
            --cnt;
            if (pa[cnt] != pb[cnt])
                return pa[cnt] < pb[cnt];
        } while(cnt);

        return false; // Elements are equal
    }

    //!
    //! \brief Compute the hash of the candidate block's proof hash and the
    //! previous stake modifier for the next stake modifier selection.
    //!
    //! \param previous_stake_modifier Included in the hash.
    //!
    //! \return Hash to compare with the other candidates for selection.
    //!
    arith_uint256 GetSelectionHash(const uint64_t previous_stake_modifier) const
    {
        if (!m_selection_hash) {
            CHashWriter hasher(SER_GETHASH, 0);
            hasher << m_pindex->hashProof << previous_stake_modifier;

            m_selection_hash = UintToArith256(hasher.GetHash());

            // The selection hash is divided by 2**32 so that the selection
            // favors proof-of-stake blocks over proof-of-work blocks every
            // time. This preserves the energy efficiency property:
            //
            if (m_pindex->IsProofOfStake()) {
                *m_selection_hash >>= 32;
            }
        }

        return *m_selection_hash;
    }
}; // StakeModifierCandidate

//!
//! \brief Calculate a legacy RSA weight value from the supplied claim block.
//!
//! This function exists support the \c CalculateLegacyV3HashProof() function
//! used to carry the stake modifiers and proof hashes of version 7 blocks to
//! version 8 and later. RSA weight is an input parameter to the legacy proof
//! hash algorithm of the version 3 staking kernel, so we reproduce the value
//! from the claim context in the same way as the old version.
//!
//! \param bb A serialized "BoincBlock" string from a coinbase transaction's
//! \c hashBoinc field that contains the claim context to extract RSA weight
//! component values from.
//!
//! \return Sum of the RSA weight and magnitude from the claim, or zero when
//! the claim contains no CPID (for a non-cruncher).
//!
int64_t GetRSAWeightByBlock(const std::string& bb)
{
    constexpr size_t cpid_offset = 0;
    constexpr size_t rsa_weight_offset = 13;
    constexpr size_t magnitude_offset = 15;

    int64_t rsa_weight_sum = 0;

    // General-purpose deserialization of claim contexts in the hashBoinc field
    // no longer parses out the RSA weight field, so we handle the special case
    // for legacy version 7 blocks by extracting only the CPID, RSA weight, and
    // magnitude fields:
    //
    for (size_t n = 0, offset = 0, end = bb.find("<|>");
        n <= magnitude_offset && end != std::string::npos;
        n++)
    {
        if (n == cpid_offset && end - offset != 32) {
            return 0;
        } else if (n == rsa_weight_offset || n == magnitude_offset) {
            int64_t rsa_weight = 0;

            if (!ParseInt64(bb.substr(offset, end - offset), &rsa_weight)) {
                error("%s: Unable to parse rsa weight from hashBoinc.", __func__);
            }

            rsa_weight_sum += rsa_weight;
        }

        offset = end + 3;
        end = bb.find("<|>", offset);
    }

    if (rsa_weight_sum < 0) {
        return 0;
    }

    return rsa_weight_sum;
}
} // anonymous namespace

unsigned int GRC::nModifierInterval = 10 * 60; // time to elapse before new modifier is computed

// Get time weight
int64_t GRC::GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    // Kernel hash weight starts from 0 at the min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low
    return min(nIntervalEnd - nIntervalBeginning - nStakeMinAge, (int64_t)nStakeMaxAge);
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: no generation at genesis block");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert (nSection >= 0 && nSection < 64);
    return (nModifierInterval * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval()
{
    int64_t nSelectionInterval = 0;
    for (int nSection=0; nSection<64; nSection++)
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(
    std::vector<StakeModifierCandidate>& vSortedByTimestamp,
    std::map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop,
    uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    arith_uint256 hashBest = 0;
    *pindexSelected = nullptr;

    for (auto const& item : vSortedByTimestamp)
    {
        if (fSelected && item.m_block_time > nSelectionIntervalStop)
        {
            break;
        }

        if (mapSelectedBlocks.count(item.m_block_hash) > 0)
        {
            continue;
        }

        arith_uint256 hashSelection = item.GetSelectionHash(nStakeModifierPrev);

        if (fSelected && hashSelection < hashBest)
        {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) item.m_pindex;
        }
        else if (!fSelected)
        {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) item.m_pindex;
        }
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && gArgs.GetBoolArg("-printstakemodifier"))
        LogPrintf("SelectBlockFromCandidates: selection hash=%s", hashBest.ToString());

    return fSelected;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool GRC::ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev)
    {
        fGeneratedStakeModifier = true;
        return true;  // genesis block's modifier is 0
    }
    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");

    LogPrint(BCLog::LogFlags::VERBOSE, "ComputeNextStakeModifier: prev modifier=0x%016" PRIx64 " time=%s", nStakeModifier, DateTimeStrFormat(nModifierTime));

    if (nModifierTime / nModifierInterval >= pindexPrev->GetBlockTime() / nModifierInterval)
        return true;

    // A bug caused by the grandfather rule in legacy clients botched the stake
    // modifier on mainnet. This resets the correct modifier at this height:
    //
    if (pindexPrev->nHeight == 1009994
        && pindexPrev->GetBlockHash() == uint256S("439b96fd59c3d585a6b93ee63b6e1d78361d7eb9b299657dee6a2c5400ccba29"))
    {
        nStakeModifier = 0xdf209a3032807577;
        fGeneratedStakeModifier = true;
        return true;
    }

    // Sort candidate blocks by timestamp
    std::vector<StakeModifierCandidate> vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * nModifierInterval / GetTargetSpacing(pindexPrev->nHeight));
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval();
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / nModifierInterval) * nModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;

    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart)
    {
        //vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        vSortedByTimestamp.emplace_back(pindex);
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;

    // Shuffle before sort
    for(int i = vSortedByTimestamp.size() - 1; i > 1; --i)
    std::swap(vSortedByTimestamp[i], vSortedByTimestamp[GetRand(i)]);

    std::sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound=0; nRound<min(64, (int)vSortedByTimestamp.size()); nRound++)
    {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);
        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && gArgs.GetBoolArg("-printstakemodifier"))
            LogPrintf("ComputeNextStakeModifier: selected round %d stop=%s height=%d bit=%d", nRound, DateTimeStrFormat(nSelectionIntervalStop), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && gArgs.GetBoolArg("-printstakemodifier"))
    {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate)
        {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (auto const& item : mapSelectedBlocks)
        {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake()? "S" : "W");
        }
        LogPrintf("ComputeNextStakeModifier: selection height [%d, %d] map %s", nHeightFirstCandidate,
            pindexPrev->nHeight, strSelectionMap);
    }
    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE))
    {
        LogPrintf("ComputeNextStakeModifier: new modifier=0x%016" PRIx64 " time=%s", nStakeModifierNew, DateTimeStrFormat(pindexPrev->GetBlockTime()));
    }

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

namespace {
//!
//! \brief Find the branch of the block whose parent is \p pindexPrev.
//!
//! \param pindexPrev Parent of the block being checked.
//! \param out_branch The branch's blocks that are not on the main chain, from
//! the parent down. Empty when the parent is on the main chain.
//!
//! \return The fork point: the newest ancestor of the block that is on the main
//! chain, which is the parent itself when the parent is on the main chain.
//!
const CBlockIndex* FindBranch(
    const CBlockIndex* const pindexPrev,
    std::vector<const CBlockIndex*>& out_branch) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const CBlockIndex* fork_point = pindexPrev;

    for (; fork_point != nullptr && !fork_point->IsInMainChain(); fork_point = fork_point->pprev) {
        out_branch.push_back(fork_point);
    }

    return fork_point;
}

//!
//! \brief Search a branch's blocks for a transaction.
//!
//! \return \c true if found; \p out_header and \p out_txprev then hold the
//! block's header and the transaction.
//!
bool FindInBranch(
    const uint256& prevout_hash,
    const std::vector<const CBlockIndex*>& branch,
    CBlockHeader& out_header,
    CTransaction& out_txprev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    for (const CBlockIndex* const pindex : branch) {
        CBlock block;

        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
            return error("%s: failed to read block %s", __func__, pindex->GetBlockHash().ToString());
        }

        for (const auto& tx : block.vtx) {
            if (tx.GetHash() == prevout_hash) {
                out_txprev = tx;
                out_header = pindex->GetBlockHeader();

                return true;
            }
        }
    }

    return false;
}

//!
//! \brief Make sure the staked input of a block whose parent is \p pindexPrev
//! comes from that block's own history.
//!
//! The transaction index follows the main chain. A transaction broadcast
//! before a fork is usually mined on both sides, in a different block on each,
//! so for a block on a side branch the index can point at the other side's
//! copy, whose block time is not the one the block's kernel was computed with.
//! The block in the checked block's own history that confirms the input
//! supplies the header; an input that is not in that history at all fails.
//!
//! \param prevout_hash Hash of the input transaction.
//! \param out_header   On entry, the header of the block the transaction index
//! names. Replaced by the header of the branch's own block if that differs.
//! \param out_txprev   Replaced by the transaction from the branch's own block
//! if that differs.
//! \param pindexPrev   Parent of the block being checked.
//!
//! \return \c false if the transaction is not in the block's own history.
//!
bool TakeStakedInputFromOwnHistory(
    const uint256& prevout_hash,
    CBlockHeader& out_header,
    CTransaction& out_txprev,
    const CBlockIndex* const pindexPrev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    std::vector<const CBlockIndex*> branch;
    const CBlockIndex* const fork_point = FindBranch(pindexPrev, branch);

    if (fork_point == nullptr) {
        return error("%s: block %s has no ancestor on the main chain", __func__, pindexPrev->GetBlockHash().ToString());
    }

    const auto indexed = mapBlockIndex.find(out_header.GetHash());

    if (indexed == mapBlockIndex.end()) {
        return error("%s: input tx %s is indexed in unknown block %s",
                     __func__, prevout_hash.ToString(), out_header.GetHash().ToString());
    }

    // A main-chain block at or below the fork point is history the checked
    // block shares. (The index only names main-chain blocks; the
    // IsInMainChain() check keeps a stale entry from being taken as shared
    // history.)
    if (indexed->second->IsInMainChain() && indexed->second->nHeight <= fork_point->nHeight) {
        return true;
    }

    if (FindInBranch(prevout_hash, branch, out_header, out_txprev)) {
        return true;
    }

    return error("%s: input tx %s is not in the history of the block after %s",
                 __func__, prevout_hash.ToString(), pindexPrev->GetBlockHash().ToString());
}
} // anonymous namespace

bool GRC::ReadStakedInput(
    CTxDB& txdb,
    const uint256 prevout_hash,
    CBlockHeader& out_header,
    CTransaction& out_txprev,
    CBlockIndex* pindexPrev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CTxIndex tx_index;

    // Get transaction index for the previous transaction
    if (!txdb.ReadTxIndex(prevout_hash, tx_index)) {
        // The index covers the main chain, so an input it does not hold can
        // only be in a side branch: the blocks above the checked block's
        // newest ancestor on the main chain, from its parent down. Only those
        // are searched.
        if (pindexPrev != nullptr) {
            std::vector<const CBlockIndex*> branch;
            FindBranch(pindexPrev, branch);

            if (FindInBranch(prevout_hash, branch, out_header, out_txprev)) {
                return true;
            }
        }

        // Previous transaction not in main chain, may occur during initial download
        return error("%s: tx index not found for input tx %s", __func__, prevout_hash.GetHex());
    }

    const CDiskTxPos pos = tx_index.pos;
    CAutoFile file(OpenBlockFile(pos.nFile, pos.nBlockPos, "rb"), SER_DISK, CLIENT_VERSION);

    if (file.IsNull()) {
        return error("%s: OpenBlockFile failed", __func__);
    }

    try {
        file >> out_header;

        if (fseek(file.Get(), pos.nTxPos, SEEK_SET) != 0) {
            return error("%s: tx fseek failed", __func__);
        }

        file >> out_txprev;
    } catch (...) {
        return error("%s: deserialize or I/O error", __func__);
    }

    // From block version 15 a block takes its staked input from its own
    // history, not from the main chain's copy.
    if (pindexPrev != nullptr && IsV15Enabled(pindexPrev->nHeight + 1)) {
        return TakeStakedInputFromOwnHistory(prevout_hash, out_header, out_txprev, pindexPrev);
    }

    return true;
}

bool GRC::CalculateLegacyV3HashProof(
    CTxDB& txdb,
    const CBlock& block,
    const double por_nonce,
    CValidationState& state,
    uint256& out_hash_proof) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const CTransaction& coinstake = block.vtx[1];
    const COutPoint& prevout = coinstake.vin[0].prevout;

    CTransaction input_tx;
    CBlockHeader input_block;

    if (!ReadStakedInput(txdb, prevout.hash, input_block, input_tx, nullptr)) {
        return state.DoS(1, error("Read staked input failed."));
    }

    CHashWriter out(SER_GETHASH, 0);

    out << GetRSAWeightByBlock(block.vtx[0].hashBoinc)
        << input_block.nTime
        << input_tx.nTime
        << input_tx.GetHash()
        << prevout.n
        << coinstake.nTime
        << por_nonce;

    out_hash_proof = out.GetHash();

    return true;
}

// GridcoinCoin kernel protocol (credit goes to Black-Coin)
// coinstake must meet hash target according to the protocol:
// kernel (input 0) must meet the formula
//     hash(nStakeModifier + txPrev.block.nTime + txPrev.nTime + txPrev.vout.hash + txPrev.vout.n + nTime) < bnTarget * nWeight
// this ensures that the chance of getting a coinstake is proportional to the
// amount of coin age one owns.
// The reason this hash is chosen is the following:
//   nStakeModifier: scrambles computation to make it very difficult to precompute
//                   future proof-of-stake
//   txPrev.block.nTime: prevent nodes from guessing a good timestamp to
//                       generate transaction for future advantage
//   txPrev.nTime: slightly scrambles computation
//   txPrev.vout.hash: hash of txPrev, to reduce the chance of nodes
//                     generating coinstake at the same time
//   txPrev.vout.n: output number of txPrev, to reduce the chance of nodes
//                  generating coinstake at the same time
//   nTime: current timestamp
//   block/tx hash should not be used here as they can be generated in vast
//   quantities so as to generate blocks faster, degrading the system back into
//   a proof-of-work situation.
//


// V8 Kernel Protocol
// Tomas Brod 05.06.2017 (dd.mm.yyy)
// Plug proof-of-work exploit.
//
// The stake modifier stops an owner from choosing outputs that will stake at
// times of their choosing. Without it, anyone creating a transaction could
// compute whether its outputs would win at future times, and grind the
// transaction until they did. It is also why the transaction hash can be in
// the kernel, although the ppcoin developers advised against it (the earlier
// Gridcoin kernel already used it): nobody knows the modifier a coin will
// stake with when the coin is created.
//
// Why the modifier is unknown when an output is created:
// - A block generates a new modifier when its parent's time falls in a later
//   10-minute bucket (nModifierInterval) than the block that generated the
//   current one (ComputeNextStakeModifier). The modifier takes one bit, the
//   last bit of the block hash, from each of up to 64 blocks. They are chosen,
//   by a hash of each block's proof and the previous modifier, from the blocks
//   between 21135 s (about 5.9 h) before the start of the parent's bucket and
//   the parent.
// - The kernel uses the modifier in effect at its parent (FindStakeModifierRev).
// - An output cannot stake until nStakeMinAge after the block that confirmed
//   it (CheckProofOfStakeV8). With 16 h (57600 s) on mainnet, every block
//   feeding a modifier the output can stake with is stamped at least 35137 s
//   (9.76 h) after the output's block: 57600 s less two buckets (1200 s), the
//   selection window (21135 s) and the 128 s past bound (48 s from v15, which
//   gives 35217 s), less the length of any stretch with no blocks.
//
// Limits:
// - A stretch of about 10 h or more with no blocks between creating and
//   staking an output removes that margin.
// - Testnet (1 h) and regtest (0) have shorter minimum ages.
// - A staker chooses the entropy bit of its own blocks and can withhold blocks
//   near the end of a selection window. That gives it a few alternative
//   modifiers, at the cost of the blocks it withholds. This is inherent to the
//   modifier design.
//
// CheckProofOfStakeV8 checks the kernel input's signature with
// SCRIPT_VERIFY_P2SH only. ConnectInputs checks it again with the block's full
// consensus flags (GetBlockScriptFlags) when a block above the last checkpoint
// is connected. Below the last checkpoint ConnectInputs skips signature checks,
// so the P2SH-only check is the only one there.
//
// Note: Rsa or Magnitude weight not included due to multiplication issue.
// Note: Payment age and magnitude restrictions not included as they are not
// important in my view and are too restrictive for honest users.

uint256 GRC::CalculateStakeHashV8(
    const CBlockHeader& CoinBlock,
    const CTransaction& CoinTx,
    unsigned CoinTxN,
    unsigned nTimeTx,
    uint64_t StakeModifier)
{
    CHashWriter ss(SER_GETHASH, 0);

    ss << StakeModifier;
    ss << MaskStakeTime(CoinBlock.nTime);
    ss << CoinTx.GetHash();
    ss << CoinTxN;
    ss << MaskStakeTime(nTimeTx);

    return ss.GetHash();
}

uint256 GRC::CalculateStakeHashV8(
    unsigned int nBlockTime,
    const CTransaction& CoinTx,
    unsigned CoinTxN,
    unsigned nTimeTx,
    uint64_t StakeModifier)
{
    CHashWriter ss(SER_GETHASH, 0);

    ss << StakeModifier;
    ss << MaskStakeTime((uint32_t) nBlockTime);
    ss << CoinTx.GetHash();
    ss << CoinTxN;
    ss << MaskStakeTime(nTimeTx);

    return ss.GetHash();
}

int64_t GRC::CalculateStakeWeightV8(const CTransaction &CoinTx, unsigned CoinTxN)
{
    return CalculateStakeWeightV8(CoinTx.vout[CoinTxN].nValue);
}

int64_t GRC::CalculateStakeWeightV8(const CAmount& nValueIn)
{
    return nValueIn / STAKE_WEIGHT_UNIT_V8;
}

CAmount GRC::CalculateStakeValueV8(int64_t weight)
{
    return weight * STAKE_WEIGHT_UNIT_V8;
}

// Find the stake modifier a kernel on pindexPrev uses: the modifier of the most
// recent block at or before pindexPrev that generated one. The miner
// (CreateCoinStake) and validation (CheckProofOfStakeV8) both pass the block's
// parent, so they always agree. The walk ends, because the genesis block always
// counts as having generated a modifier (ComputeNextStakeModifier), and it
// covers only the blocks since the last modifier, normally those of one
// 10-minute bucket. The V8 kernel notes above CalculateStakeHashV8 say what the
// modifier protects against.
bool GRC::FindStakeModifierRev(uint64_t& nStakeModifier, CBlockIndex* pindexPrev, int& nHeight_mod) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    nStakeModifier = 0;
    CBlockIndex* pindex_mod = pindexPrev;

    while (1)
    {
        if(!pindex_mod)
            return error("FindStakeModifierRev: no previous block from %d",pindexPrev->nHeight);

        if (pindex_mod->GeneratedStakeModifier())
        {
            nStakeModifier = pindex_mod->nStakeModifier;
            nHeight_mod = pindex_mod->nHeight;
            return true;
        }
        pindex_mod = pindex_mod->pprev;
    }
}

// Block Version 8+ check procedure

arith_uint256 GRC::GetKernelTarget(unsigned int nBits, int64_t block_time, const CBlockIndex* pindexPrev)
{
    arith_uint256 base;
    base.SetCompact(nBits);

    if (!IsV15Enabled(pindexPrev->nHeight + 1)) {
        return base;
    }

    // Signed elapsed time. A stamp up to 48 s before the parent is valid and gets no easing.
    const int64_t elapsed = block_time - pindexPrev->GetBlockTime();
    return EaseTarget(base, elapsed);
}

int64_t GRC::GetStakeTime(const CBlockIndex* pindexPrev, int64_t adjusted_now)
{
    if (!IsV15Enabled(pindexPrev->nHeight + 1)) {
        return adjusted_now;
    }

    // Stamps from v12 are on the 16 s grid, so parent - 48 is too.
    return std::max<int64_t>(pindexPrev->GetBlockTime() - V15_PAST_TIME_BOUND, MaskStakeTime(adjusted_now));
}

bool GRC::CheckProofOfStakeV8(
    CTxDB& txdb,
    CBlockIndex* pindexPrev, //previous block in chain index
    CBlock& Block, //block to check
    bool generated_by_me,
    CValidationState& state,
    uint256& hashProofOfStake) EXCLUSIVE_LOCKS_REQUIRED(cs_main) //proof hash out-parameter
{
    //Block Transaction 0 is coin:base
    //Block Transaction 1 is coin:stake
    //First input of coinstake is the kernel

    assert(Block.nVersion >= 8);

    if (!Block.IsProofOfStake())
        return error("%s: called on non-coinstake block %s", __func__, Block.GetHash().ToString());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTransaction& tx = Block.vtx[1];
    const COutPoint& prevout = tx.vin[0].prevout;

    // Read txPrev and header of its block
    CBlockHeader header;
    CTransaction txPrev;

    if (!ReadStakedInput(txdb, prevout.hash, header, txPrev, pindexPrev))
        return state.DoS(10, error("%s: read staked input failed", __func__));

    if (!VerifySignature(txPrev, tx, SCRIPT_VERIFY_P2SH, 0, 0))
        return state.DoS(100, error("%s: VerifySignature failed on coinstake %s", __func__, tx.GetHash().ToString()));

    // Check times
    if (tx.nTime < txPrev.nTime)
        return state.DoS(100, error("%s: nTime violation", __func__));

    if (header.nTime + nStakeMinAge > tx.nTime)
        return state.DoS(100, error("%s: min age violation", __func__));

    if (Block.nVersion >= 12) {
        if (tx.nTime != MaskStakeTime(tx.nTime)) {
            return state.DoS(100, error("%s: mask violation", __func__));
        }
    }

    uint64_t StakeModifier = 0;
    int nHeight_mod = 0;

    if (!FindStakeModifierRev(StakeModifier, pindexPrev, nHeight_mod))
        return error("%s: unable to find stake modifier", __func__);

    hashProofOfStake = CalculateStakeHashV8(header, txPrev, prevout.n, tx.nTime, StakeModifier);

    //Stake refactoring TomasBrod
    int64_t Weight = CalculateStakeWeightV8(txPrev, prevout.n);
    arith_uint320 bnHashProof = arith_uint320(hashProofOfStake);

    // Base target, eased from v15 when the block is long after its parent
    arith_uint320 bnTarget = GetKernelTarget(Block.nBits, Block.GetBlockTime(), pindexPrev);

    if (bnTarget != arith_uint320(arith_uint256().SetCompact(Block.nBits))) {
        LogPrint(BCLog::LogFlags::VERBOSE, "CheckProofOfStakeV8: eased kernel target, elapsed %d s",
                 Block.GetBlockTime() - pindexPrev->GetBlockTime());
    }

    // Weighted target
    bnTarget *= arith_uint320(Weight);


    LogPrint(BCLog::LogFlags::VERBOSE,
             "CheckProofOfStakeV8:%s Time1 %.f, Time2 %.f, Time3 %.f, Bits %u, Weight %.f\n"
             " Stk %72s\n"
             " Trg %72s", generated_by_me?" Local,":"",
             (double)header.nTime, (double)txPrev.nTime, (double)tx.nTime,
             Block.nBits, (double)Weight,
             hashProofOfStake.GetHex(), bnTarget.GetHex()
             );

    // Now check if proof-of-stake hash meets target protocol
    if (bnHashProof > bnTarget) {
        return state.DoS(100, error("%s: invalid proof (proof: %s, target: %s)", __func__, bnHashProof.GetHex(), bnTarget.GetHex()));
    }

    return true;
}
