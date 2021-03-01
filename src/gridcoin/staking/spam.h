// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "kernel.h"
#include "uint256.h"

#include <array>
#include <cmath>
#include <map>
#include <vector>

namespace GRC {
//!
//! \brief Records proof-of-stake claims for spam protection.
//!
//! This type provides support for spam mitigation at the chain tip by tracking
//! the staking kernel proofs used to generate a new block. Nodes reject blocks
//! with staked inputs spent for another block to prevent a malicious or broken
//! node from spamming multiple blocks that compete for the best chain.
//!
//! This problem is a characteristic of proof-of-stake consensus protocols. The
//! outputs used to create the kernel proof have no practical link to the block
//! generated for that stake. If they did, the protocol would incentivize block
//! producers to manipulate new blocks to synthesize suitable proofs--a process
//! that looks a lot like proof-of-work consensus.
//!
//! Because a particular block does not depend on the properties of a stake, an
//! actor can theoretically produce an unlimited set of blocks from an eligible
//! proof and submit all of those blocks as candidates for next in the chain to
//! flood the network and disrupt consensus. By disallowing nodes to generate a
//! block with the same proof as another near the chain tip, we can depress the
//! threat posed by this type of attack.
//!
//! This class replaces the original \c setStakeSeen sets to relieve the memory
//! pressure from the block index. For now, a node will use it to reproduce the
//! original behavior, but we may want to revisit spam mitigation techniques in
//! the future.
//!
class SeenStakes
{
public:
    //!
    //! \brief Store a kernel proof hash from a block.
    //!
    //! \param Proof hash of the stake used to generate a block.
    //!
    void Remember(uint256 hashProof)
    {
        m_proofs_seen[GetOffset(hashProof)] = hashProof;
    }

    //!
    //! \brief Store a kernel proof from an orphan block.
    //!
    //! \param coinstake Coinstake transaction from the block.
    //!
    void RememberOrphan(const CTransaction& coinstake)
    {
        const COutPoint& stake_prevout = coinstake.vin[0].prevout;
        const int64_t stake_time = MaskStakeTime(coinstake.nTime);

        m_orphan_proofs_seen[stake_prevout].emplace_back(stake_time);
    }

    //!
    //! \brief Determine whether a node sent a block with a kernel proof hash
    //! seen in another block.
    //!
    //! \param Proof hash of the stake used to generate a block.
    //!
    //! \return \c true if this container holds a matching proof hash.
    //!
    bool ContainsProof(const uint256& hashProof) const
    {
        return m_proofs_seen[GetOffset(hashProof)] == hashProof;
    }

    //!
    //! \brief Determine whether a node sent an orphan block with a kernel proof
    //! seen in another orphan block.
    //!
    //! \param Proof hash of the stake used to generate a block.
    //!
    //! \return \c true if this container holds a matching proof hash.
    //!
    bool ContainsOrphan(const CTransaction& coinstake) const
    {
        const COutPoint& stake_prevout = coinstake.vin[0].prevout;
        const auto iter_pair = m_orphan_proofs_seen.find(stake_prevout);

        if (iter_pair == m_orphan_proofs_seen.end()) {
            return false;
        }

        const int64_t stake_time = MaskStakeTime(coinstake.nTime);
        const auto& timestamps = iter_pair->second;
        const auto end = timestamps.end();

        return std::find(timestamps.begin(), end, stake_time) != end;
    }

    //!
    //! \brief Remove a kernel proof for an orphan block.
    //!
    //! \param coinstake Coinstake transaction from the block.
    //!
    void ForgetOrphan(const CTransaction& coinstake)
    {
        const COutPoint& stake_prevout = coinstake.vin[0].prevout;
        auto iter_pair = m_orphan_proofs_seen.find(stake_prevout);

        if (iter_pair == m_orphan_proofs_seen.end()) {
            return;
        }

        const int64_t stake_time = MaskStakeTime(coinstake.nTime);
        auto& timestamps = iter_pair->second;
        auto end = timestamps.end();

        timestamps.erase(std::remove(timestamps.begin(), end, stake_time), end);

        if (timestamps.empty()) {
            m_orphan_proofs_seen.erase(iter_pair);
        }
    }

    //!
    //! \brief Fill the container with proof hashes near the chain tip.
    //!
    //! \param pindex Points to the block index for the chain tip.
    //!
    void Refill(const CBlockIndex* pindex)
    {
        for (size_t i = 0; pindex && i < m_proofs_seen.size(); ++i) {
            pindex = pindex->pprev;
        }

        for (; pindex; pindex = pindex->pnext) {
            Remember(pindex->hashProof);
        }
    }

private:
    //!
    //! \brief A hash table of recently-observed kernel proof hashes.
    //!
    //! The size of the container must be a power of 2. Realistically, the 2048
    //! entries are likely overkill since a block must:
    //!
    //!  - satisfy the requirements of the staking protocol
    //!  - contain a valid coinstake transaction (previously unspent input)
    //!  - produce a proof not already in this collection
    //!
    //! The extra wiggle room gives the container more tolerance for collisions
    //! by increasing the output range of the hash function. The hash table can
    //! store about two days' volume of staking proofs.
    //!
    std::array<uint256, 2048> m_proofs_seen;

    //!
    //! \brief A set of observed stake inputs from orphan blocks.
    //!
    //! Orphan blocks do not undergo proof-of-stake validation, so we cannot
    //! store the computed proof hash. This collection contains outpoints of
    //! staked inputs mapped to timestamps of the coinstake transactions.
    //!
    std::map<COutPoint, std::vector<int64_t>> m_orphan_proofs_seen;

    //!
    //! \brief Get the hash table slot offset for a proof hash.
    //!
    //! \param Proof hash of the stake used to generate a block.
    //!
    //! \return Hash of the proof hash in the range of the size of the proofs
    //! table.
    //!
    size_t GetOffset(const uint256& hashProof) const
    {
        // This basic hashing function maps a proof hash value to an offset in
        // the m_proofs_seen container. It implements the form of:
        //
        //   h_a,b(x) = (ax + b) >> (w - M)
        //
        // ...where:
        //
        //   x   is the input value to hash
        //   a   is a random odd positive integer
        //   b   is a random non-negative integer less than 2^(w - M)
        //   w   is the number of bits in the hashing space (word size)
        //   M   is the number of bits in the target digest space
        //
        // ...as described here:
        //
        //   https://en.wikipedia.org/wiki/Universal_hashing#Avoiding_modular_arithmetic
        //
        // ...to produce a distribution for values of x with a minimal rate of
        // collision.
        //
        using limit_t = std::numeric_limits<size_t>;

        static const size_t w = sizeof(size_t) * 8;
        static const size_t M = std::log2(m_proofs_seen.size());
        static const size_t a = (GetRand(limit_t::max()) * 2) + 1;
        static const size_t b = GetRand(std::pow(2, w - M) - 1);

        size_t x = 0;

        for (size_t i = 0; i < (256 / sizeof(size_t)); i += sizeof(size_t)) {
            x += *reinterpret_cast<const size_t*>(hashProof.begin() + i);
        }

        return (a * x + b) >> (w - M);
    }
}; // SeenStakes
} // namespace GRC
