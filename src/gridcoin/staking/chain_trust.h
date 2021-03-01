// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "arith_uint256.h"

#include <array>
#include <vector>

namespace GRC {
//!
//! \brief Calculates and stores chain trust values.
//!
//! Chain trust is a metric that represents the difficulty sum of a chain. We
//! calculate it for a particular block by adding that block's own difficulty
//! value to the total difficulty of all the blocks below it. The trust value
//! of a chain protects against some attack types by enabling nodes to select
//! the chain that required the most effort to produce. For Gridcoin, this is
//! the chain with the greatest amount of staked coins over time. A node will
//! reorganize to a competing chain only if the fork exhibits a greater chain
//! trust value.
//!
//! This class caches chain trust values to determine when a block produces a
//! chain with greater trust. It replaces the chain trust fields in the block
//! index to conserve memory.
//!
class ChainTrustCache
{
public:
    ChainTrustCache()
        : m_best_pindex(new CBlockIndex()) // Genesis bootstrap placeholder
    {
    }

    //!
    //! \brief Fill the cache from the block index and calculate the trust of
    //! the best chain.
    //!
    //! \param genesis Blockchain index for the genesis block.
    //! \param tip     Blockchain index for the highest known block.
    //!
    void Initialize(const CBlockIndex* genesis, const CBlockIndex* tip)
    {
        int64_t start_time = GetTimeMillis();

        if (m_best_pindex->phashBlock == nullptr) {
            delete m_best_pindex; // Free genesis bootstrap placeholder
        }

        m_best_pindex = tip;
        m_best_trust = 0;
        m_long_cache.Reserve(tip);

        for (const CBlockIndex* pindex = genesis; pindex; pindex = pindex->pnext) {
            m_best_trust += pindex->GetBlockTrust();

            if (m_long_cache.Expects(pindex)) {
                m_long_cache.Push(m_best_trust);
            }
        }

        LogPrintf(
            "Gridcoin: Time to calculate chain trust %15" PRId64 "ms",
            GetTimeMillis() - start_time);
    }

    //!
    //! \brief Get the chain trust value of the current chain.
    //!
    //! \return Chain trust value at the current best block.
    //!
    arith_uint256 Best() const
    {
        return m_best_trust;
    }

    //!
    //! \brief Determine whether a block exhibits greater chain trust than the
    //! current best.
    //!
    //! \param pindex Points to the block index entry to compare.
    //!
    //! \return \c true if the specified block exhibits greater chain trust.
    //!
    bool Favors(const CBlockIndex* pindex) const
    {
        return GetTrust(pindex) > Best();
    }

    //!
    //! \brief Calculate the chain trust value of the specified block.
    //!
    //! \param pindex Points to the block index entry to calculate trust for.
    //!
    //! \return Chain trust value for the specified block.
    //!
    arith_uint256 GetTrust(const CBlockIndex* pindex) const
    {
        arith_uint256 chain_trust;

        if (pindex == nullptr) {
            return chain_trust;
        }

        // First, look for a cached chain trust value near the chain tip:
        //
        if (pindex->nHeight >= m_best_pindex->nHeight
            || m_short_cache.MightContain(pindex))
        {
            for (; pindex != nullptr; pindex = pindex->pprev) {
                if (pindex == m_best_pindex) {
                    return chain_trust + m_best_trust;
                } else if (const auto trust_option = m_short_cache.Try(pindex)) {
                    return chain_trust + *trust_option;
                }

                chain_trust += pindex->GetBlockTrust();
            }
        }

        // Otherwise, calculate chain trust up to one of the cached snapshots:
        //
        for (; pindex != nullptr; pindex = pindex->pprev) {
            if (m_long_cache.Expects(pindex)) {
                return chain_trust + m_long_cache.Fetch(pindex);
            }

            chain_trust += pindex->GetBlockTrust();
        }

        return chain_trust;
    }

    //!
    //! \brief Remember the specified block as the tip of the best chain.
    //!
    //! \param pindex Points to the block index entry selected as the tip of
    //! the chain.
    //!
    void SetBest(const CBlockIndex* pindex)
    {
        m_best_trust = pindex->GetBlockTrust() + GetTrust(pindex->pprev);
        m_best_pindex = pindex;

        if (m_long_cache.Expects(pindex)) {
            m_long_cache.Store(pindex, m_best_trust);
        } else {
            m_short_cache.Store(pindex, m_best_trust);
        }
    }

private:
    //!
    //! \brief A circular buffer that caches chain trust values for blocks
    //! recently-connected to the chain.
    //!
    //! This cache speeds up access to chain trust values for rapid changes to
    //! the blocks at the chain tip. It retains the chain trust for forks that
    //! occur to reduce the overhead of trust calculation triggered by a chain
    //! reorganization.
    //!
    class ShortTrustCache
    {
        //!
        //! \brief Number of entries to store in the cache.
        //!
        static constexpr size_t SIZE = 32;

    public:
        //!
        //! \brief Determine whether the cache may contain the specified block.
        //!
        //! \param pindex Points the block index entry to check.
        //!
        //! \return \c true if the block height falls within the cached range.
        //!
        bool MightContain(const CBlockIndex* const pindex) const
        {
            for (const auto& slot : m_index_cache) {
                if (slot != nullptr && slot->nHeight <= pindex->nHeight) {
                    return true;
                }
            }

            return false;
        }

        //!
        //! \brief Get the cached chain trust value for the specified block if
        //! it exists in the cache.
        //!
        //! \param pindex Points to the block index entry to fetch trust for.
        //!
        //! \return Either contains a trust value for the block or does not.
        //!
        boost::optional<arith_uint256> Try(const CBlockIndex* const pindex) const
        {
            for (size_t i = 0; i < SIZE; ++i) {
                if (m_index_cache[i] == pindex) {
                    return m_trust_cache[i];
                }
            }

            return boost::none;
        }

        //!
        //! \brief Store a trust value in the cache.
        //!
        //! \param pindex Points to the block index entry of the trust value.
        //! \param trust  Chain trust value to associate with the block.
        //!
        void Store(const CBlockIndex* const pindex, const arith_uint256 trust)
        {
            const size_t offset = m_position % SIZE;

            m_index_cache[offset] = pindex;
            m_trust_cache[offset] = trust;

            ++m_position;
        }

    private:
        //!
        //! \brief Circular buffer that stores block index entries of the chain
        //! trust values in the cache.
        //!
        //! Since we scan the block index entries far more than we access the
        //! trust values, the cache splits storage into two arrays to improve
        //! CPU cache friendliness somewhat.
        //!
        std::array<const CBlockIndex*, SIZE> m_index_cache;

        //!
        //! \brief Circular buffer that stores chain trust values in the cache.
        //!
        std::array<arith_uint256, SIZE> m_trust_cache;

        size_t m_position = 0; //!< Circular buffer counter
    }; // ShortTrustCache

    //!
    //! \brief A persistent cache of historical chain trust snapshots created
    //! at regular intervals in the blockchain.
    //!
    //! This cache reduces the effort needed to calculate the chain trust for
    //! historical blocks by providing the aggregated trust for points in the
    //! chain. A trust calculation can start with one of these points instead
    //! of scanning the entire chain.
    //!
    class LongTrustCache
    {
        //!
        //! \brief Height interval to store chain trust snapshots at.
        //!
        //! TODO: make this configurable for users and services that want a
        //! more dense cache to improve "getblock" RPC performance.
        //!
        static constexpr size_t LONG_CACHE_INTERVAL = 10000;

    public:
        //!
        //! \brief Determine whether the cache stores a chain trust snapshot
        //! for the specified block.
        //!
        //! \param pindex Points to the block index entry to check.
        //!
        //! \return \c true if the block height matches the cache interval.
        //!
        bool Expects(const CBlockIndex* const pindex) const
        {
            return pindex->nHeight % LONG_CACHE_INTERVAL == 0;
        }

        //!
        //! \brief Get the cached chain trust value for the specified block.
        //!
        //! \param pindex Points to a block index entry for an interval height.
        //!
        //! \return Chain trust value for the block.
        //!
        arith_uint256 Fetch(const CBlockIndex* const pindex) const
        {
            return m_cache[GetOffset(pindex)];
        }

        //!
        //! \brief Store a trust value in the cache.
        //!
        //! \param pindex Points to the block index entry of the trust value.
        //! \param trust  Chain trust value to associate with the block.
        //!
        void Store(const CBlockIndex* const pindex, const arith_uint256 trust)
        {
            const size_t offset = GetOffset(pindex);

            if (offset >= m_cache.size()) {
                assert(offset == m_cache.size());
                Reserve(pindex); // Just grow by 1. This doesn't happen often.
                Push(trust);
            } else {
                m_cache[offset] = trust;

                // Invalidate cached snapshots greater than this height:
                m_cache.erase(m_cache.begin() + offset + 1, m_cache.end());
            }
        }

        //!
        //! \brief Reallocate storage for the cache if needed.
        //!
        //! \param pindex The height that determines the size of the cache.
        //!
        void Reserve(const CBlockIndex* const pindex)
        {
            m_cache.reserve(GetOffset(pindex) + 1);
        }

        //!
        //! \brief Append a chain trust value directly to the end of the cache.
        //!
        //! \param trust Chain trust value for the block height at the tip of
        //! the cache.
        //!
        void Push(const arith_uint256 trust)
        {
            m_cache.emplace_back(trust);
        }

    private:
        //!
        //! \brief Get the cache offset for the height of the specified block.
        //!
        //! \param pindex Block index entry with a height to map to an offset.
        //!
        //! \return Offset to store a trust value at for the specified block.
        //!
        static size_t GetOffset(const CBlockIndex* const pindex)
        {
            return pindex->nHeight / LONG_CACHE_INTERVAL;
        }

        //!
        //! \brief Maps block heights to cached chain trust values.
        //!
        //! This stores chain trust values only for blocks that occur at each
        //! height interval. We scale the height by the interval to determine
        //! the offset in the container.
        //!
        std::vector<arith_uint256> m_cache;
    }; // LongTrustCache

    const CBlockIndex* m_best_pindex; //!< Block index entry for the chain tip.
    arith_uint256 m_best_trust;       //!< Chain trust value for the chain tip.
    ShortTrustCache m_short_cache;    //!< Caches chain trust at the chain tip.
    LongTrustCache m_long_cache;      //!< Caches chain trust at height intervals.
}; // ChainTrustCache
} // namespace GRC
