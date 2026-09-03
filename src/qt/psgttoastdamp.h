// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_PSGTTOASTDAMP_H
#define GRIDCOIN_QT_PSGTTOASTDAMP_H

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

//!
//! \brief Decides whether a PSGT pool change should raise a "needs your
//! signature" toast.
//!
//! One multisig arrangement collects a revision per co-signer, and each of them
//! fires the pool-changed notification. walletMustSignRevision only stops the
//! toast once this wallet has signed, so up to that point the same request is
//! announced once per co-signer who signs ahead of it. Keying on the
//! arrangement instead of the revision announces it once: the revisions of one
//! arrangement share a pool image, which is what identifies the arrangement.
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
        std::string image_hex;    //!< The arrangement the revision belongs to.
    };

    //!
    //! \brief Whether a pool change naming \p revision_hex should be announced.
    //!
    //! Records the arrangement when it answers true, so the co-signer revisions
    //! that follow are silent.
    //!
    //! \param pool The pool as it stands, used to resolve the arrangement.
    //! \param revision_hex The revision the notification named.
    //!
    //! \return \c true for the first revision of an arrangement. A revision
    //! that does not resolve to an arrangement is announced rather than
    //! swallowed on the strength of a lookup that failed. The caller gates on
    //! walletMustSignRevision first, and that already answers false for a
    //! revision the pool has dropped, so what reaches this is the narrow race
    //! between those two calls.
    //!
    bool ShouldToastRevision(const std::vector<Entry>& pool, const std::string& revision_hex)
    {
        const auto entry = std::find_if(pool.begin(), pool.end(), [&revision_hex](const Entry& candidate) {
            return candidate.revision_hex == revision_hex;
        });

        if (entry == pool.end()) {
            return true;
        }

        return m_announced.insert(entry->image_hex).second;
    }

    //!
    //! \brief Forgets arrangements that are no longer in the pool.
    //!
    //! An arrangement that leaves the pool and is later submitted again is a
    //! new request, and is announced again. Called on removal, the only point
    //! at which the pool shrinks.
    //!
    //! \param pool The pool as it stands after the removal.
    //!
    void Prune(const std::vector<Entry>& pool)
    {
        std::set<std::string> live;

        for (const Entry& entry : pool) {
            live.insert(entry.image_hex);
        }

        for (auto it = m_announced.begin(); it != m_announced.end();) {
            it = live.count(*it) ? std::next(it) : m_announced.erase(it);
        }
    }

    //! \brief How many arrangements are currently remembered. For tests.
    std::size_t AnnouncedCount() const { return m_announced.size(); }

private:
    std::set<std::string> m_announced;
};

#endif // GRIDCOIN_QT_PSGTTOASTDAMP_H
