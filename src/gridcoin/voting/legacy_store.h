// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_LEGACY_STORE_H
#define GRIDCOIN_VOTING_LEGACY_STORE_H

#include "serialize.h"
#include "sync.h"
#include "uint256.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

extern RecursiveMutex cs_main;

namespace GRC {
//!
//! \brief What the legacy store keeps for a poll below V11: what a poll reference needs, the poll's block, and its
//! votes through the closing block.
//!
struct LegacyPollEntry
{
    uint256 m_txid;                               //!< Hash of the poll transaction.
    uint32_t m_payload_version = 0;               //!< Version of the poll payload.
    uint8_t m_type = 0;                           //!< The poll's PollType.
    uint8_t m_weight_type = 0;                    //!< The poll's PollWeightType.
    std::string m_title;                          //!< The poll's title as written.
    int64_t m_timestamp = 0;                      //!< Time of the poll transaction.
    uint32_t m_duration_days = 0;                 //!< Days the poll is open.
    uint256 m_block_hash;                         //!< The block that holds the poll.
    int m_height = 0;                             //!< That block's height.
    std::vector<std::pair<uint256, int>> m_votes; //!< Linked votes (txid, block height), in chain order.

    SERIALIZE_METHODS(LegacyPollEntry, obj)
    {
        READWRITE(obj.m_txid, obj.m_payload_version, obj.m_type, obj.m_weight_type, obj.m_title, obj.m_timestamp,
                  obj.m_duration_days, obj.m_block_hash, obj.m_height, obj.m_votes);
    }
};

//!
//! \brief Polls below V11, filled on demand and kept in their own LevelDB key space. Not a RegistryDB:
//! entries never change, so there is nothing to revert. Display only; validation never reads it. It has no lock of
//! its own: the poll registry holds it and calls it under cs_poll_registry.
//!
class LegacyPollStore
{
public:
    //! The outcome of a lookup by txid.
    enum class Lookup
    {
        FOUND,       //!< The entry is served.
        NOT_LEGACY,  //!< No poll below V11 on the main chain has that txid.
        BELOW_FLOOR, //!< The poll is below the floor.
        //! The node's chain does not yet hold the poll's first block past its end (a node syncing from zero), or a
        //! flagged block in its window could not be read: nothing is stored, and a later query fills the poll.
        INCOMPLETE,
    };

    static constexpr uint32_t VERSION = 1;

    //!
    //! \brief Open the store for polls from \p floor to below \p ceiling: clear its key space if the stored version
    //! differs, then read its entries. The entries load once; the bounds are set on every call.
    //!
    //! \param floor   The lowest height served: the research-age height, unless -legacypollfloor moves it.
    //! \param ceiling The network's V11 height.
    //!
    void Initialize(int floor, int ceiling);

    //!
    //! \brief Serve the poll's entry, or fill it once from the chain if the poll qualifies.
    //!
    //! \param entry Set to the entry when the lookup is FOUND.
    //!
    Lookup Fetch(const uint256& txid, const LegacyPollEntry*& entry) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! \brief Every entry at or above the floor.
    std::vector<const LegacyPollEntry*> Entries() const;

    //! \brief Erase the store's entries, on disk and in memory.
    void Clear();

    //! \brief The lowest height served.
    int Floor() const { return m_floor; }

private:
    int m_floor = 0;
    int m_ceiling = 0;
    bool m_loaded = false;
    std::map<uint256, LegacyPollEntry> m_entries;

    //!
    //! \brief Compute a poll's entry from the chain.
    //!
    //! \return FOUND with \p entry filled, or why the poll does not qualify.
    //!
    Lookup Compute(const uint256& txid, LegacyPollEntry& entry) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);
};
} // namespace GRC

#endif // GRIDCOIN_VOTING_LEGACY_STORE_H
