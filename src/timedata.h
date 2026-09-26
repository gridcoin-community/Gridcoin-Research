// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TIMEDATA_H
#define GRIDCOIN_TIMEDATA_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

//!
//! \brief Peer clock-offset votes behind the network-adjusted time: one vote
//! per connected outbound peer, counted once per peer network group.
//!
//! AddTimeData() records each outbound peer's VERSION time offset here under
//! the peer's node id, and RemoveTimeData() withdraws it when that peer's
//! connection is deleted. Only live connections vote. Peers in the same
//! network group (CNetAddr::GetGroup(): /16 for IPv4, /32 for IPv6, and so on)
//! count once, with the most recently cast of their votes. Half of the live
//! outbound groups can move the median halfway (a tie splits the middle two
//! votes) and a majority moves it fully, so shifting it takes that share of our
//! simultaneous outbound connections, which automatic outbound selection
//! already spreads across distinct groups. A peer that connects, votes and
//! disconnects leaves nothing behind.
//!
//! The node's own clock always holds one vote at offset 0, the "0" the
//! 200-sample median filter this replaces was seeded with.
//!
//! Not thread-safe; the caller serializes access.
//!
class TimeOffsetVotes
{
public:
    using Group = std::vector<unsigned char>;
    using Owner = int64_t; //!< The voting peer's NodeId.

    //!
    //! \brief Stored votes kept at most. Live outbound connections normally
    //! number far fewer (eight by default); this bounds memory for
    //! configurations with very many outbound connections (-connect, a raised
    //! outbound limit). Past it, the vote cast longest ago is dropped.
    //!
    static constexpr size_t MAX_PEER_VOTES = 199;

    //!
    //! \brief Counted votes, the node's own included, required before a median
    //! is produced.
    //!
    static constexpr size_t MIN_VOTES = 5;

    //!
    //! \brief Record a peer's offset sample as its vote, replacing any earlier
    //! vote from the same peer.
    //!
    //! \param owner  The peer's node id.
    //! \param group  The peer's network group.
    //! \param offset Peer time minus our time, in seconds.
    //!
    //! \return Median() after recording the vote.
    //!
    std::optional<int64_t> Add(Owner owner, const Group& group, int64_t offset);

    //!
    //! \brief The median of the counted votes, or nothing while there are
    //! fewer than MIN_VOTES.
    //!
    std::optional<int64_t> Median() const;

    //!
    //! \brief Withdraw a peer's vote.
    //!
    //! \return Whether the peer had a vote to withdraw.
    //!
    bool Remove(Owner owner);

    //!
    //! \brief The counted votes, the node's own included, in ascending order:
    //! one per network group, the group's most recently cast.
    //!
    std::vector<int64_t> Sorted() const;

    //!
    //! \brief Number of counted votes, the node's own included.
    //!
    size_t Size() const { return Sorted().size(); }

private:
    struct Vote
    {
        Group group;
        int64_t offset;
        uint64_t sequence; //!< Order cast; the highest counts for a group.
    };

    std::map<Owner, Vote> m_votes;
    uint64_t m_sequence = 0;
};

#endif // GRIDCOIN_TIMEDATA_H
