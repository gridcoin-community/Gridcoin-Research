// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_PSGTTOASTDAMP_H
#define GRIDCOIN_QT_PSGTTOASTDAMP_H

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <vector>

//!
//! \brief Decides whether a PSGT pool change should raise a "needs your
//! signature" toast.
//!
//! One pending spend collects a revision per co-signer, and each of them fires
//! the pool-changed notification. walletMustSignRevision only stops the toast
//! once this wallet has signed, so up to that point the same request is
//! announced once per co-signer who signs ahead of it. Keying on the spend
//! instead of the revision announces it once.
//!
//! The spend is identified by the hash of its UNSIGNED transaction
//! (PSGTPoolRow::tx_hash_hex), not by the pool image. The image is the
//! arrangement's redeem-script id, and the pool lets an initiator supersede a
//! pending spend with a DIFFERENT transaction under the same image
//! (PSGTPool::Add, "same image, different unsigned tx"), delivered as the same
//! UPDATED change as signature progress. That is a new request and must be
//! announced, so the image would be the wrong key. Every signature revision of
//! one spend shares its unsigned-transaction hash; a supersede changes it.
//!
//! Session-scoped and deliberately not persisted. The behaviour being damped is
//! repetition within one sitting; a restart re-announcing a request the wallet
//! still has not signed is wanted, not a leak.
//!
class PSGTToastDamp
{
public:
    //! One pool entry, reduced to the two identities this decision needs.
    struct Entry
    {
        std::string revision_hex; //!< The revision the notification names.
        std::string tx_hash_hex;  //!< The unsigned transaction: the spend the revision belongs to.
    };

    //!
    //! \brief Whether a pool change naming \p revision_hex should be announced.
    //!
    //! Forgets spends that are no longer in \p pool first, so a spend that was
    //! superseded or removed and is later submitted again counts as the new
    //! request it is. Then records the named revision's spend when it answers
    //! true, so the co-signer revisions that follow are silent.
    //!
    //! \param pool The pool as it stands, used to resolve the spend.
    //! \param revision_hex The revision the notification named.
    //!
    //! \return \c true for the first revision of a spend. A revision that does
    //! not resolve to a spend is announced rather than swallowed on the
    //! strength of a lookup that failed. The caller gates on
    //! walletMustSignRevision first, and that already answers false for a
    //! revision the pool has dropped, so what reaches this is the narrow race
    //! between those two calls.
    //!
    bool ShouldToastRevision(const std::vector<Entry>& pool, const std::string& revision_hex)
    {
        Prune(pool);

        const auto entry = std::find_if(pool.begin(), pool.end(), [&revision_hex](const Entry& candidate) {
            return candidate.revision_hex == revision_hex;
        });

        if (entry == pool.end()) {
            return true;
        }

        return m_announced.insert(entry->tx_hash_hex).second;
    }

    //!
    //! \brief Forgets spends that are no longer in the pool.
    //!
    //! A spend that leaves the pool -- removed, expired, completed, evicted by a
    //! conflict, or superseded by a different transaction -- and is later
    //! submitted again is a new request, and is announced again. Called on
    //! removal, and by ShouldToastRevision before every decision.
    //!
    //! \param pool The pool as it stands.
    //!
    void Prune(const std::vector<Entry>& pool)
    {
        std::set<std::string> live;

        for (const Entry& entry : pool) {
            live.insert(entry.tx_hash_hex);
        }

        for (auto it = m_announced.begin(); it != m_announced.end();) {
            it = live.count(*it) ? std::next(it) : m_announced.erase(it);
        }
    }

    //! \brief How many spends are currently remembered. For tests.
    std::size_t AnnouncedCount() const { return m_announced.size(); }

private:
    std::set<std::string> m_announced;
};

#endif // GRIDCOIN_QT_PSGTTOASTDAMP_H
