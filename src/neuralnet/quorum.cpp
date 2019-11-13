#include "base58.h"
#include "main.h"
#include "neuralnet/quorum.h"
#include "neuralnet/tally.h"
#include "util/reverse_iterator.h"

#include <openssl/md5.h>
#include <unordered_map>

using namespace NN;

namespace {
//!
//! \brief A vote for a superblock for legacy quorum consensus.
//!
class QuorumVote
{
public:
    const QuorumHash m_hash;           //!< Hash of the superblock to vote for.
    const CBitcoinAddress m_address;   //!< Address of the voting wallet.
    const CBlockIndex* const m_pindex; //!< Block that contains the vote.

    //!
    //! \brief Initialize a legacy quorum vote.
    //!
    //! \param hash    Hash of the superblock to vote for.
    //! \param address Default address of the voting wallet.
    //! \param pindex  Block that contains the vote.
    //!
    QuorumVote(QuorumHash hash, CBitcoinAddress address, const CBlockIndex* pindex)
        : m_hash(hash)
        , m_address(std::move(address))
        , m_pindex(pindex)
    {
    }
};

//!
//! \brief Orchestrates superblock voting for legacy quorum consensus.
//!
//! For the legacy network-wide superblock selection mechanism before block
//! version 11, a node will vote for a superblock that it considers correct
//! by embedding the hash of that superblock in the claim section of blocks
//! that it generates. Another node stakes the final superblock if the hash
//! of the local pending superblock matches the most popular hash by weight
//! of the preceeding votes.
//!
//! For better security, superblock staking and validation using data from
//! scraper convergence replaces the problematic quorum mechanism in block
//! version 11 and above.
//!
class LegacyConsensus
{
    //!
    //! \brief Number of blocks to search backward for pending superblock votes.
    //!
    static constexpr int STANDARD_LOOKBACK = 100;

public:
    //!
    //! \brief Determine whether the provided address participates in the
    //! quorum consensus at the specified time.
    //!
    //! \param address Default wallet address of a node in the network.
    //! \param time    Timestamp to check participation at.
    //!
    //! \return \c true if the address matches the subset of addrsses that
    //! particpate in the quorum on the day of the year of \p time.
    //!
    static bool Participating(const std::string& grc_address, const int64_t time)
    {
        // This hash is approximately 25% of the MD5 range (90% for testnet):
        static const arith_uint256 reference_hash = fTestNet
            ? arith_uint256("0x00000000000000000000000000000000ed182f81388f317df738fd9994e7020b")
            : arith_uint256("0x000000000000000000000000000000004d182f81388f317df738fd9994e7020b");

        std::string input = grc_address + "_" + std::to_string(GetDayOfYear(time));
        std::vector<unsigned char> address_day_hash(16);

        MD5(reinterpret_cast<const unsigned char*>(input.data()),
            input.size(),
            address_day_hash.data());

        return arith_uint256("0x" + HexStr(address_day_hash)) < reference_hash;
    }

    //!
    //! \brief Get the hash of the pending superblock with the greatest vote
    //! weight.
    //!
    //! \param pindex Provides context about the chain tip used to calculate
    //! quorum vote weights.
    //!
    //! \return Quorum hash of the most popular superblock or an invalid hash
    //! when no nodes voted in the current superblock cycle.
    //!
    QuorumHash FindPopularHash(const CBlockIndex* const pindex)
    {
        if (!pindex) {
            return QuorumHash();
        }

        RefillVoteCache(pindex);

        const PopularityMap popularity_map = BuildPopularityMap(pindex->nHeight);
        auto popular = popularity_map.cbegin();

        if (popular == popularity_map.end()) {
            return QuorumHash();
        }

        if (popularity_map.size() == 1) {
            return popular->first;
        }

        for (auto iter = std::next(popular); iter != popularity_map.end(); ++iter) {
            if (iter->second > popular->second) {
                popular = iter;
            }
        }

        return popular->first;
    }

    //!
    //! \brief Store a node's vote for a pending superblock in the cache.
    //!
    //! \param quorum_hash Hash of the superblock the node voted for.
    //! \param grc_address Default wallet address of the node.
    //! \param pindex      The block that contains the vote to store.
    //!
    void RecordVote(
        const QuorumHash& quorum_hash,
        const std::string& grc_address,
        const CBlockIndex* const pindex)
    {
        if (!pindex || !quorum_hash.Valid()) {
            return;
        }

        CBitcoinAddress address(grc_address);

        if (!address.IsValid()) {
            if (fDebug) {
                LogPrintf("Quorum::RecordVote: invalid address: %s HASH: %s",
                    grc_address,
                    quorum_hash.ToString());
            }

            return;
        }

        if (!Participating(grc_address, pindex->nTime)) {
            if (fDebug) {
                LogPrintf("Quorum::RecordVote: not participating: %s HASH: %s",
                    grc_address,
                    quorum_hash.ToString());
            }

            return;
        }

        m_votes.emplace(pindex->nHeight, QuorumVote(
            quorum_hash,
            std::move(address),
            pindex));
    }

    //!
    //! \brief Remove a node's vote for a pending superblock from the cache.
    //!
    //! \param pindex The block that contains the vote to remove.
    //!
    void ForgetVote(const CBlockIndex* const pindex)
    {
        if (!pindex) {
            return;
        }

        m_votes.erase(pindex->nHeight);
    }

private:
    //!
    //! \brief Associates superblock hashes with the sum of their vote weights.
    //!
    typedef std::unordered_map<QuorumHash, double> PopularityMap;

    //!
    //! \brief Cache of recent superblock votes.
    //!
    //! This class caches superblock votes in received blocks to avoid loading
    //! blocks from disk to calculate pending vote weights. This significantly
    //! increases the initial block synchronization performance when comparing
    //! it to the previous implementation but uses slightly more memory.
    //!
    std::map<int64_t, QuorumVote> m_votes;

    //!
    //! \brief Load superblock votes from disk to prepare the cache for a
    //! recount of pending superblock popularity.
    //!
    //! Typically, this method will only read from disk when the application
    //! first starts or when a reorganization disconnects a sizable batch of
    //! blocks from the chain.
    //!
    //! For simplicity, this caching strategy maintains a constant-sized set
    //! of the most recent blocks needed for a full lookback. We can improve
    //! the memory consumption of the cache by choosing a more precise limit
    //! based on memoization of the heights that voting begins. The cache is
    //! used for historical superblock validation only, so the memory gained
    //! is not really worth the additional complexity.
    //!
    //! \param pindex Block to load votes backward from.
    //!
    void RefillVoteCache(const CBlockIndex* pindex)
    {
        const int64_t min_height = std::max<int64_t>(
            Tally::CurrentSuperblock()->m_height + 1,
            pindex->nHeight - STANDARD_LOOKBACK);

        if (!m_votes.empty()) {
            pindex = m_votes.begin()->second.m_pindex;

            if (pindex->nHeight <= min_height) {
                PurgeVoteCache(min_height);
                return;
            }
        }

        while (pindex && pindex->nHeight > min_height) {
            CBlock block;
            block.ReadFromDisk(pindex);

            if (block.GetClaim().m_quorum_hash.Valid()) {
                Claim claim = block.PullClaim();

                RecordVote(
                    claim.m_quorum_hash,
                    std::move(claim.m_quorum_address),
                    pindex);
            }

            pindex = pindex->pprev;
        }
    }

    //!
    //! \brief Erase stale entries from the vote cache.
    //!
    //! \param min_height Purge votes in blocks below this height.
    //!
    void PurgeVoteCache(const int64_t min_height)
    {
        auto iter = m_votes.cbegin();

        while (m_votes.size() > STANDARD_LOOKBACK && iter->first < min_height) {
            iter = m_votes.erase(iter);
        }
    }

    //!
    //! \brief Recalculate the vote weight for recent pending superblocks.
    //!
    //! \param last_height Height of the chain tip that defines the high end of
    //! the voting window.
    //!
    //! \return Sums of vote weights of the pending superblocks found in blocks
    //! within the voting window.
    //!
    PopularityMap BuildPopularityMap(const int64_t last_height) const
    {
        const int64_t min_height = std::max<int64_t>(
            Tally::CurrentSuperblock()->m_height + 1,
            last_height - STANDARD_LOOKBACK);

        PopularityMap popularity_map;
        std::map<CBitcoinAddress, QuorumHash> addresses_seen;

        for (const auto& vote_pair : reverse_iterate(m_votes)) {
            const int64_t vote_height = vote_pair.first;
            const QuorumVote& vote = vote_pair.second;

            if (vote_height < min_height) {
                break;
            }

            const auto address_iter = addresses_seen.find(vote.m_address);

            // Ignore addresses that voted multiple times for the same hash:
            if (address_iter != addresses_seen.end()
                && address_iter->second == vote.m_hash)
            {
                continue;
            }

            // Sum vote weights to determine overall hash popularity:
            const double distance = last_height - vote_height + 10;
            const double multiplier = distance < 40 ? 400 : 200;

            popularity_map[vote.m_hash] += (1 / distance) * multiplier;
            addresses_seen[vote.m_address] = vote.m_hash;
        }

        return popularity_map;
    }
}; // LegacyConsensus

//!
//! \brief Orchestrates superblock voting for legacy quorum consensus.
//!
LegacyConsensus g_legacy_consensus;

} // anonymous namespace


// -----------------------------------------------------------------------------
// Class: Quorum
// -----------------------------------------------------------------------------

bool Quorum::Participating(const std::string& grc_address, const int64_t time)
{
    return LegacyConsensus::Participating(grc_address, time);
}

QuorumHash Quorum::FindPopularHash(const CBlockIndex* const pindex)
{
    return g_legacy_consensus.FindPopularHash(pindex);
}

void Quorum::RecordVote(
    const NN::QuorumHash quorum_hash,
    const std::string& grc_address,
    const CBlockIndex* const pindex)
{
    g_legacy_consensus.RecordVote(quorum_hash, grc_address, pindex);
}

void Quorum::ForgetVote(const CBlockIndex* const pindex)
{
    g_legacy_consensus.ForgetVote(pindex);
}

bool Quorum::ValidateSuperblockClaim(
    const Claim& claim,
    const CBlockIndex* const pindex)
{
    if (!NN::Tally::SuperblockNeeded()) {
        return error("ValidateSuperblockClaim(): superblock too early.");
    }

    const CBitcoinAddress address(claim.m_quorum_address);

    if (!address.IsValid()) {
        return error("ValidateSuperblockClaim(): "
            "invalid quorum address: %s", claim.m_quorum_address);
    }

    if (!Participating(claim.m_quorum_address, pindex->nTime)) {
        return error("ValidateSuperblockClaim(): "
            "ineligible quorum participant: %s", claim.m_quorum_address);
    }

    const QuorumHash popular_hash = FindPopularHash(pindex);

    if (claim.m_superblock.GetHash() != popular_hash) {
        return error("ValidateSuperblockClaim(): "
            "superblock hash mismatch: %s popular: %s",
            claim.m_superblock.GetHash().ToString(),
            popular_hash.ToString());
    }

    return true;
}
