// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include <boost/optional.hpp>
#include <unordered_map>

class CBlockIndex;

namespace GRC {

class Cpid;
class ResearchAccount;

//!
//! \brief Maps BOINC CPIDs to the accounts that track research reward accrual.
//!
typedef std::unordered_map<Cpid, ResearchAccount> ResearchAccountMap;

//!
//! \brief An optional type that contains a pointer to a block index object or
//! does not.
//!
typedef boost::optional<const CBlockIndex*> BlockPtrOption;

//!
//! \brief Stores the research reward context for a CPID used to calculate
//! accrued research rewards.
//!
class ResearchAccount
{
public:
    CAmount m_accrual;                    //!< Research accrued last superblock.
    CAmount m_total_research_subsidy;     //!< Total lifetime research paid.
    uint32_t m_total_magnitude;           //!< Total lifetime magnitude sum.
    uint32_t m_accuracy;                  //!< Non-zero magnitude payment count.

    const CBlockIndex* m_first_block_ptr; //!< First block with research reward.
    const CBlockIndex* m_last_block_ptr;  //!< Last block with research reward.

    //!
    //! \brief Initialize an empty research account.
    //!
    //! \param accrual Research reward accrued as of the last superblock. New
    //! accounts may carry pending accrual even though the CPIDs never staked
    //! a block before.
    //!
    ResearchAccount(const CAmount accrual = 0)
        : m_accrual(accrual)
        , m_total_research_subsidy(0)
        , m_total_magnitude(0)
        , m_accuracy(0)
        , m_first_block_ptr(nullptr)
        , m_last_block_ptr(nullptr)
    {
    }

    //!
    //! \brief Determine whether this is a new research account.
    //!
    //! \return \c true if no block contains a reward for the account's CPID.
    //!
    bool IsNew() const
    {
        return m_first_block_ptr == nullptr;
    }

    //!
    //! \brief Determine whether the account is considered active.
    //!
    //! \return \c true if the account is not new and the CPID staked a block
    //! within the last six months.
    //!
    bool IsActive(const int64_t height) const
    {
        if (IsNew()) {
            return false;
        }

        const int64_t block_span = height - LastRewardHeight();

        // If a CPID's last staked block occurred earlier than six months ago,
        // the account is considered inactive by legacy rules.
        //
        // This condition emulates the GetHistoricalMagnitude() function which
        // scans backward from pindexBest, one block below the claimed height,
        // so we decrease the block span by one to match the original.
        //
        // Note that "six months" here is usually closer to seven months since
        // the network emits fewer blocks per day than the constant implies.
        //
        return block_span - 1 <= BLOCKS_PER_DAY * 30 * 6;
    }

    //!
    //! \brief Get a pointer to the first block with a research reward earned by
    //! the account.
    //!
    //! \return A pointer to the block index if the account earned a research
    //! reward before.
    //!
    BlockPtrOption FirstRewardBlock() const
    {
        if (m_first_block_ptr == nullptr) {
            return boost::none;
        }

        return BlockPtrOption(m_first_block_ptr);
    }

    //!
    //! \brief Get the hash of the first block with a research reward earned by
    //! the account.
    //!
    //! \return The SHA256 hash of the account's first reward block.
    //!
    uint256 FirstRewardBlockHash() const
    {
        if (const BlockPtrOption pindex = FirstRewardBlock()) {
            return (*pindex)->GetBlockHash();
        }

        return uint256();
    }

    //!
    //! \brief Get the height of the first block with a research reward earned
    //! by the account.
    //!
    //! \return A block height of zero if the account never earned a research
    //! reward before.
    //!
    uint32_t FirstRewardHeight() const
    {
        if (const BlockPtrOption pindex = FirstRewardBlock()) {
            return (*pindex)->nHeight;
        }

        return 0;
    }

    //!
    //! \brief Get the timestamp of the first block with a research reward
    //! earned by the account.
    //!
    //! \return A timestamp of zero if the account never earned a research
    //! reward before.
    //!
    int64_t FirstRewardTime() const
    {
        if (const BlockPtrOption pindex = FirstRewardBlock()) {
            return (*pindex)->nTime;
        }

        return 0;
    }

    //!
    //! \brief Get a pointer to the last block with a research reward earned by
    //! the account.
    //!
    //! \return A pointer to the block index if the account earned a research
    //! reward before.
    //!
    BlockPtrOption LastRewardBlock() const
    {
        if (m_last_block_ptr == nullptr) {
            return boost::none;
        }

        return BlockPtrOption(m_last_block_ptr);
    }

    //!
    //! \brief Get the hash of the last block with a research reward earned by
    //! the account.
    //!
    //! \return The SHA256 hash of the account's last reward block.
    //!
    uint256 LastRewardBlockHash() const
    {
        if (const BlockPtrOption pindex = LastRewardBlock()) {
            return (*pindex)->GetBlockHash();
        }

        return uint256();
    }

    //!
    //! \brief Get the height of the last block with a research reward earned
    //! by the account.
    //!
    //! \return A block height of zero if the account never earned a research
    //! reward before.
    //!
    uint32_t LastRewardHeight() const
    {
        if (const BlockPtrOption pindex = LastRewardBlock()) {
            return (*pindex)->nHeight;
        }

        return 0;
    }

    //!
    //! \brief Get the timestamp of the last block with a research reward earned
    //! by the account.
    //!
    //! \return A timestamp of zero if the account never earned a research
    //! reward before.
    //!
    int64_t LastRewardTime() const
    {
        if (const BlockPtrOption pindex = LastRewardBlock()) {
            return (*pindex)->nTime;
        }

        return 0;
    }

    //!
    //! \brief Get the magnitude of the CPID in the last block with a research
    //! reward earned by the account.
    //!
    //! \return A magnitude of zero if the account never earned a research
    //! reward before.
    //!
    uint16_t LastRewardMagnitude() const
    {
        if (const BlockPtrOption pindex = LastRewardBlock()) {
            return (*pindex)->Magnitude();
        }

        return 0;
    }

    //!
    //! \brief Get the average magnitude of the CPID over every block with a
    //! research reward earned by the account.
    //!
    //! \return A magnitude of zero if the account never earned a research
    //! reward before.
    //!
    double AverageLifetimeMagnitude() const
    {
        if (m_accuracy <= 0) {
            return 0.0;
        }

        return (double)m_total_magnitude / m_accuracy;
    }
}; // ResearchAccount

//!
//! \brief A traversable range of all the research accounts stored in the tally.
//!
//! This type provides \c const access to the research accounts in the tally
//! without exposing the internal storage or implementation.
//!
class ResearchAccountRange
{
    typedef ResearchAccountMap StorageType;

public:
    typedef StorageType::const_iterator const_iterator;
    typedef StorageType::size_type size_type;

    //!
    //! \brief Initialize the wrapper.
    //!
    //! \param accounts Points to the accounts stored in the tally.
    //!
    ResearchAccountRange(const StorageType& accounts) : m_accounts(accounts)
    {
    }

    //!
    //! \brief Returns an iterator to the beginning.
    //!
    const_iterator begin() const
    {
        return m_accounts.begin();
    }

    //!
    //! \brief Returns an iterator to the end.
    //!
    const_iterator end() const
    {
        return m_accounts.end();
    }

    //!
    //! \brief Get the number of accounts in the tally.
    //!
    size_type size() const
    {
        return m_accounts.size();
    }

private:
    const StorageType& m_accounts; //!< The accounts stored in the tally.
};
}
