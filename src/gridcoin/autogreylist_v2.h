// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_AUTOGREYLIST_V2_H
#define GRIDCOIN_AUTOGREYLIST_V2_H

#include <gridcoin/project.h>
#include <gridcoin/superblock.h>
#include "util.h" // Fraction

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace GRC {

//!
//! \brief Greylist candidate state for one project, V2 walker.
//!
//! A deliberate near-duplicate of the frozen V1 AutoGreylist::GreylistCandidateEntry rather
//! than a shared type: sharing would mean editing frozen consensus code and re-interleaving
//! per-version flags through it, which is the structure the V1/V2 class fork exists to
//! remove. V1 is immutable by necessity (it IS the pre-gate consensus behavior), so the
//! duplication carries no maintenance cost, and V1 is deleted wholesale once the redesign
//! activation is past any plausible reorg depth.
//!
//! Differences from V1, by construction rather than by flag:
//!
//! - The audit-height "benefit of the doubt" behavior is hard-coded ON. V2 only runs at or
//!   above AutoGreylistRedesignHeight, which is never below AutoGreylistAuditHeight
//!   (mainnet 3,989,800, already crossed), so the pre-audit branch cannot be reached.
//! - The trivial constructor initializes the bookmarks to std::nullopt (disengaged), not to
//!   an ENGAGED optional holding zero. V1's zero-engaged initialization is the corrupt
//!   "recorded zero" state; this type cannot be born into it.
//!
//! The walker-correctness changes (zeros-as-NA with the latch, the WAS divisor contraction)
//! land in a separate stage; at this stage the arithmetic is V1-equivalent so the
//! differential harness can assert exact equality.
//!
class GreylistCandidateV2
{
public:
    //!
    //! \brief One historical update, recorded as the walk progresses. Note this history is a
    //! LOOKBACK from the head (sb_from_baseline ascends into the past), not forward-looking.
    //!
    struct UpdateHistoryEntry
    {
        UpdateHistoryEntry(uint8_t sb_from_baseline_processed, std::optional<uint64_t> total_credit)
            : m_sb_from_baseline_processed(sb_from_baseline_processed)
            , m_total_credit(total_credit)
        {
        }

        uint8_t m_sb_from_baseline_processed;
        std::optional<uint64_t> m_total_credit;
    };

    //!
    //! \brief Trivial constructor. Required because this type is used as a std::map value.
    //!
    GreylistCandidateV2()
        : m_project_name(std::string {})
        , m_zcd_20_SB_count(0)
        , m_TC_7_SB_sum(0)
        , m_TC_40_SB_sum(0)
        , m_meets_greylisting_crit(false)
        , m_TC_initial_bookmark(std::nullopt)
        , m_TC_bookmark(std::nullopt)
        , m_sb_from_baseline_processed(0)
        , m_update_history()
    {
    }

    //!
    //! \brief Baseline (head) constructor.
    //!
    //! \param project_name The whitelist key.
    //! \param TC_initial_bookmark Total credit recorded for the project in the head
    //! superblock, or std::nullopt when the project is absent from it.
    //!
    GreylistCandidateV2(std::string project_name, std::optional<uint64_t> TC_initial_bookmark)
        : m_project_name(project_name)
        , m_zcd_20_SB_count(0)
        , m_TC_7_SB_sum(0)
        , m_TC_40_SB_sum(0)
        , m_meets_greylisting_crit(false)
        , m_TC_initial_bookmark(TC_initial_bookmark)
        , m_TC_bookmark(TC_initial_bookmark)
        , m_sb_from_baseline_processed(0)
        , m_update_history()
    {
        m_update_history.push_back(UpdateHistoryEntry(0, TC_initial_bookmark));
    }

    //!
    //! \brief Zero Credit Days over the (up to) 20 superblock lookback.
    //!
    uint8_t GetZCD() const
    {
        return m_zcd_20_SB_count;
    }

    //!
    //! \brief Whitelist Activity Score: the 7-superblock average total-credit delta over the
    //! 40-superblock average. V1-equivalent arithmetic, including the divisor's use of
    //! min(processed, 7|40) -- the divisor contraction (F8) is a later, gated stage.
    //!
    Fraction GetWAS() const
    {
        if (!m_sb_from_baseline_processed) {
            return Fraction(0);
        }

        uint64_t TC_7_SB_avg = m_TC_7_SB_sum / std::min<uint64_t>(m_sb_from_baseline_processed, 7);
        uint64_t TC_40_SB_avg = m_TC_40_SB_sum / std::min<uint64_t>(m_sb_from_baseline_processed, 40);

        if (TC_7_SB_avg > (uint64_t) std::numeric_limits<int64_t>::max()
            || TC_40_SB_avg > (uint64_t) std::numeric_limits<int64_t>::max()) {
            TC_7_SB_avg /= 2;
            TC_40_SB_avg /= 2;
        }

        // Guard on the DENOMINATOR alone (matches V1 as fixed for the truncation-to-zero
        // divide): a zero 40-SB average means negligible long-term work availability -> 0.0.
        if (TC_40_SB_avg == 0) {
            return Fraction(0);
        }

        return Fraction(TC_7_SB_avg, TC_40_SB_avg);
    }

    //!
    //! \brief Apply one walk position. V1-equivalent with the audit behavior (benefit of the
    //! doubt for a missing head at sb_from_baseline == 1) hard-coded on.
    //!
    //! \param total_credit Total credit recorded at this position, or std::nullopt.
    //! \param sb_from_baseline Position in the backward walk (1 == the superblock before the
    //! head).
    //!
    void UpdateGreylistCandidateEntry(std::optional<uint64_t> total_credit, uint8_t sb_from_baseline)
    {
        if (sb_from_baseline > 0) {
            // ZCD arm. Walking backwards: total credit at this (older) position >= the
            // bookmark (newer position) means zero or negative forward credit -> a ZCD, as
            // does a missing entry. The head-missing case at position 1 is excused (benefit
            // of the doubt): a project absent from the head superblock alone is most likely
            // a transient scraper failure, not a project outage.
            if (sb_from_baseline <= 20) {
                const bool head_scraper_failure = (sb_from_baseline == 1 && !m_TC_initial_bookmark);

                if (!head_scraper_failure && ((total_credit && total_credit >= m_TC_bookmark) || !total_credit)) {
                    ++m_zcd_20_SB_count;
                }
            }

            // WAS arm. The sums are endpoint differences from the head bookmark (equivalent
            // to summing per-superblock deltas while the series is clean); a missing entry
            // leaves them untouched.
            if (!m_TC_initial_bookmark && total_credit) {
                m_TC_initial_bookmark = total_credit;
            }

            if (total_credit && m_TC_initial_bookmark > total_credit) {
                if (sb_from_baseline <= 7) {
                    m_TC_7_SB_sum = *m_TC_initial_bookmark - *total_credit;
                }

                if (sb_from_baseline <= 40) {
                    m_TC_40_SB_sum = *m_TC_initial_bookmark - *total_credit;
                }
            }

            m_sb_from_baseline_processed = sb_from_baseline;
        }

        if (total_credit) {
            m_TC_bookmark = total_credit;
        }

        const uint8_t zcd = GetZCD();
        const Fraction was = GetWAS();

        // Greylist criteria, with the 7-superblock stabilization grace period.
        m_meets_greylisting_crit = (sb_from_baseline >= 7 && (zcd > 7 || was < Fraction(1, 10)));

        m_update_history.push_back(UpdateHistoryEntry(sb_from_baseline, total_credit));
    }

    //!
    //! \brief The lookback update history (see UpdateHistoryEntry).
    //!
    const std::vector<UpdateHistoryEntry>& GetUpdateHistory() const
    {
        return m_update_history;
    }

    const std::string m_project_name;    //!< The whitelist key.
    uint8_t m_zcd_20_SB_count;           //!< Zero Credit Days over the 20-SB lookback.
    uint64_t m_TC_7_SB_sum;              //!< Endpoint TC difference over the 7-SB lookback.
    uint64_t m_TC_40_SB_sum;             //!< Endpoint TC difference over the 40-SB lookback.
    bool m_meets_greylisting_crit;       //!< Whether the greylist criteria are met at the head.

private:
    std::optional<uint64_t> m_TC_initial_bookmark; //!< Head-anchored bookmark (reverse walk).
    std::optional<uint64_t> m_TC_bookmark;         //!< Rolling bookmark of the last seen TC.
    uint8_t m_sb_from_baseline_processed;          //!< Last walk position applied.
    std::vector<UpdateHistoryEntry> m_update_history; //!< Lookback history.
};

//!
//! \brief The V2 greylist walker: a single, pure, value-returning entry point.
//!
//! One entry point is the structural fix for the "silently exempt path" defect class: every
//! producer (superblock construction, validation, reporting) derives greylist state through
//! this one function, so a correction applied here cannot miss a path.
//!
class AutoGreylistV2
{
public:
    //!
    //! \brief Candidate map type for the computed result.
    //!
    typedef std::map<std::string, GreylistCandidateV2> Candidates;

    //!
    //! \brief The computed result: per-project candidate detail plus the derived membership.
    //!
    struct Result
    {
        Candidates m_candidates;                 //!< Per-project ZCD/WAS/history detail.
        std::set<std::string> m_auto_greylisted; //!< Projects meeting the greylist criteria.
    };

    //!
    //! \brief Run the (up to) 40-superblock backward walk from the provided head superblock
    //! and compute greylist candidate state for every admissible whitelisted project.
    //!
    //! Pure: reads the chain (or the unit-test substitute) and returns a value; mutates no
    //! shared state. Caching, keying and record stamping are the facade's business.
    //!
    //! \param head_ptr The head superblock (committed superblock, or a candidate bound to
    //! the tip / containing block). Must be version 3 or later, with the total-credit map
    //! populated.
    //! \param whitelist A Snapshot(GreylistState::NONE, ALL_BUT_DELETED) view.
    //! \param project_first_actives First-activation entries (admissibility timestamps).
    //! \param unit_test_blocks Test seam, exactly as the V1 walker's: a height-keyed map of
    //! superblock-bearing block index entries substituting for chain access.
    //!
    static Result Compute(SuperblockPtr head_ptr,
                          const WhitelistSnapshot& whitelist,
                          const Whitelist::ProjectEntryMap& project_first_actives,
                          std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>>
                              unit_test_blocks = nullptr);
};

} // namespace GRC

#endif // GRIDCOIN_AUTOGREYLIST_V2_H
