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
#include <type_traits>
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
//! Walker corrections carried by this type (all consensus-affecting, all riding the single
//! AutoGreylistRedesignHeight gate; the differential harness enumerates each delta exactly):
//!
//! - Zeros as missing data, with the initial-state latch: the CALLER (AutoGreylistV2::Compute)
//!   derives an EFFECTIVE total credit per position -- a recorded zero is normalized to
//!   std::nullopt iff a non-zero total credit exists at an OLDER position, in the window or
//!   in the capped beyond-window evidence scan (at most 40 additional superblocks; see
//!   Compute) --
//!   and this type consumes the effective value for the ZCD, bookmark and WAS arms alike,
//!   while the history records the value as it actually appears on chain. A recorded total
//!   credit is a cumulative lifetime counter: an OLDER superblock recording non-zero
//!   lifetime credit contradicts a newer zero (the counter cannot return to zero), so the
//!   zero is corruption, not data. Genuine initial-state zeros -- those with no non-zero at
//!   any older position -- stay values.
//!
//! - WAS divisor contraction (F8): the divisors are the positions of the deepest effective
//!   data actually spanned by each endpoint difference (m_TC_7_SB_interval /
//!   m_TC_40_SB_interval), not min(processed, 7|40) -- a missing ENDPOINT contracts the
//!   interval, while missing data in the MIDDLE telescopes away and does not shrink the
//!   divisor.
//!
//! - Exact-fraction WAS: (sum7 * d40) / (sum40 * d7) instead of V1's truncated integer
//!   averages. Required for the contraction to be meaningful (900/39 truncating to 23 is a
//!   4.5% error at realistic magnitudes) and removes the truncate-to-zero pathology class.
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
        , m_TC_7_SB_interval(0)
        , m_TC_40_SB_interval(0)
        , m_update_history()
    {
    }

    //!
    //! \brief Baseline (head) constructor.
    //!
    //! The EFFECTIVE and RECORDED values are taken separately and neither is defaulted, so
    //! every call site must state both. They diverge exactly when the head's recorded zero is
    //! latched as corruption: the bookmarks (and hence the WAS) see std::nullopt, while the
    //! history keeps reporting the zero as it appears on chain -- the head is the one
    //! position the update path's recorded value does not cover, and hiding the datapoint
    //! that drove a greylisting would make the corruption undiagnosable.
    //!
    //! \param project_name The whitelist key.
    //! \param TC_initial_bookmark Effective head total credit (normalized), or std::nullopt.
    //! \param recorded_total_credit Total credit as recorded in the head superblock.
    //!
    GreylistCandidateV2(std::string project_name,
                        std::optional<uint64_t> TC_initial_bookmark,
                        std::optional<uint64_t> recorded_total_credit)
        : m_project_name(project_name)
        , m_zcd_20_SB_count(0)
        , m_TC_7_SB_sum(0)
        , m_TC_40_SB_sum(0)
        , m_meets_greylisting_crit(false)
        , m_TC_initial_bookmark(TC_initial_bookmark)
        , m_TC_bookmark(TC_initial_bookmark)
        , m_sb_from_baseline_processed(0)
        , m_TC_7_SB_interval(0)
        , m_TC_40_SB_interval(0)
        , m_update_history()
    {
        m_update_history.push_back(UpdateHistoryEntry(0, recorded_total_credit));
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
    //! 40-superblock average, as the EXACT fraction
    //!
    //!     (m_TC_7_SB_sum * d40) / (m_TC_40_SB_sum * d7)
    //!
    //! where d7/d40 are the contracted intervals: the deepest positions <= 7 / <= 40 HOLDING
    //! effective data. A missing ENDPOINT therefore contracts its interval, while missing
    //! data in the MIDDLE telescopes away in the endpoint difference and leaves the divisor
    //! alone. Note the intervals track data PRESENCE, not the position of the last sum
    //! assignment: on a deliberate rollback (data present, non-positive delta) the divisor
    //! stays at the data position while the sum keeps an older endpoint -- preserving V1's
    //! penalized-by-omission shape rather than softening it. Approved vectors: data at j=0..5 with NA at 6,7 gives
    //! sum7 = bookmark - TC[5] over divisor 5; NA at j=3 alone leaves divisor 7; the
    //! initial-state latch case (9 producing superblocks over 31 genuine initial zeros)
    //! gives exactly 13/3 = 4.3333 -- which only the exact-fraction form can produce
    //! (900/39 truncates to 23 under integer averaging).
    //!
    Fraction GetWAS() const
    {
        if (!m_sb_from_baseline_processed) {
            return Fraction(0);
        }

        // No effective data spanned by an interval means no computable average: report 0,
        // consistent with V1's zero-sum behavior (the 40-side also guards the denominator).
        if (m_TC_7_SB_interval == 0 || m_TC_40_SB_interval == 0) {
            return Fraction(0);
        }

        uint64_t TC_7_SB_sum = m_TC_7_SB_sum;
        uint64_t TC_40_SB_sum = m_TC_40_SB_sum;

        // Keep the cross products, and the downstream criteria comparison against 1/10,
        // comfortably inside int64_t. The bound only triggers at total-credit deltas above
        // ~1.1e16 -- four orders of magnitude past any real BOINC project -- and the halving
        // is applied to both sums, so the ratio is preserved to within rounding and remains
        // deterministic across nodes.
        while (TC_7_SB_sum > (uint64_t) std::numeric_limits<int64_t>::max() / 800
               || TC_40_SB_sum > (uint64_t) std::numeric_limits<int64_t>::max() / 800) {
            TC_7_SB_sum >>= 1;
            TC_40_SB_sum >>= 1;
        }

        if (TC_40_SB_sum == 0) {
            return Fraction(0);
        }

        return Fraction((int64_t) (TC_7_SB_sum * m_TC_40_SB_interval),
                        (int64_t) (TC_40_SB_sum * m_TC_7_SB_interval));
    }

    //!
    //! \brief Apply one walk position, with the audit behavior (benefit of the doubt for a
    //! missing head at sb_from_baseline == 1) hard-coded on.
    //!
    //! The ZCD, bookmark and WAS arms all consume the EFFECTIVE value; the history records
    //! the RECORDED one. A latched-corrupt zero therefore behaves exactly like missing data
    //! everywhere (including the ZCD "no statistics" arm -- zeros and NAs both count as
    //! ZCDs), while getautogreylist show_history keeps showing the corruption rather than
    //! hiding it. Neither parameter is defaulted: a call site must state both.
    //!
    //! \param effective_total_credit Latch-normalized total credit, or std::nullopt.
    //! \param recorded_total_credit Total credit as recorded on chain, or std::nullopt.
    //! \param sb_from_baseline Position in the backward walk (1 == the superblock before the
    //! head).
    //!
    void UpdateGreylistCandidateEntry(std::optional<uint64_t> effective_total_credit,
                                      std::optional<uint64_t> recorded_total_credit,
                                      uint8_t sb_from_baseline)
    {
        const std::optional<uint64_t>& total_credit = effective_total_credit;

        if (sb_from_baseline > 0) {
            // ZCD arm. Walking backwards: total credit at this (older) position >= the
            // bookmark (newer position) means zero or negative forward credit -> a ZCD, as
            // does a missing entry. The head-missing case at position 1 is excused (benefit
            // of the doubt): a project absent from the head superblock alone is most likely
            // a transient scraper failure, not a project outage.
            //
            // Known inherited edge (V1-identical, pinned by the differential harness): when
            // the head AND position 1 are both missing, the bookmark is still disengaged at
            // the first position carrying real data, and an engaged optional compares >= a
            // disengaged one -- so that first data point counts as a ZCD even when credit
            // grew. V1 behaves the same (with raw zeros it additionally counts position 1,
            // which the latch excuses here). Correcting the comparison-baseline semantics
            // belongs to the deferred walker-correctness pass (F7 class), not this gate.
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

            // Divisor contraction (F8): the intervals track the deepest position holding
            // effective data, whether or not the sum assignment above fired -- a deliberate
            // rollback (data present, endpoint difference non-positive) keeps its V1
            // penalized-by-omission shape, while genuinely MISSING endpoints contract.
            if (total_credit) {
                if (sb_from_baseline <= 7) {
                    m_TC_7_SB_interval = sb_from_baseline;
                }

                if (sb_from_baseline <= 40) {
                    m_TC_40_SB_interval = sb_from_baseline;
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

        m_update_history.push_back(UpdateHistoryEntry(sb_from_baseline, recorded_total_credit));
    }

    //!
    //! \brief The lookback update history (see UpdateHistoryEntry).
    //!
    const std::vector<UpdateHistoryEntry>& GetUpdateHistory() const
    {
        return m_update_history;
    }

    //! \brief The whitelist key. NOT const, deviating from V1's member: this type is a
    //! std::map value inside GreylistComputation, which the facade copies BY ASSIGNMENT when
    //! caching results -- a const member deletes the copy-assignment operator, and libc++
    //! (macOS) instantiates map::operator= for that path where libstdc++ happens not to.
    //! The class still never mutates it after construction.
    std::string m_project_name;

    uint8_t m_zcd_20_SB_count;           //!< Zero Credit Days over the 20-SB lookback.
    uint64_t m_TC_7_SB_sum;              //!< Endpoint TC difference over the 7-SB lookback.
    uint64_t m_TC_40_SB_sum;             //!< Endpoint TC difference over the 40-SB lookback.
    bool m_meets_greylisting_crit;       //!< Whether the greylist criteria are met at the head.

private:
    std::optional<uint64_t> m_TC_initial_bookmark; //!< Head-anchored bookmark (reverse walk).
    std::optional<uint64_t> m_TC_bookmark;         //!< Rolling bookmark of the last seen TC.
    uint8_t m_sb_from_baseline_processed;          //!< Last walk position applied.
    uint8_t m_TC_7_SB_interval;                    //!< Deepest position <= 7 with effective data.
    uint8_t m_TC_40_SB_interval;                   //!< Deepest position <= 40 with effective data.
    std::vector<UpdateHistoryEntry> m_update_history; //!< Lookback history.
};

//! GreylistCandidateV2 must remain a regular type: it is a std::map value that the facade
//! copies by assignment when caching computations. A const member (or any other change that
//! deletes copy assignment) breaks the build only on libc++ (macOS), so pin it here where
//! every platform checks it.
static_assert(std::is_copy_assignable<GreylistCandidateV2>::value,
              "GreylistCandidateV2 must be copy-assignable (std::map value semantics)");

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
    //! \param walk_start Optional explicit start of the backward walk (the block index entry
    //! at head_ptr.m_height - 1). When provided it replaces the BlockFinder main-chain
    //! search, which matters on validation paths: during a reorg connect the block being
    //! validated (and its ancestry) need not be on the main chain yet, but pindex->pprev
    //! always reaches the right history. Ignored when unit_test_blocks is provided.
    //!
    static Result Compute(SuperblockPtr head_ptr,
                          const WhitelistSnapshot& whitelist,
                          const Whitelist::ProjectEntryMap& project_first_actives,
                          std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>>
                              unit_test_blocks = nullptr,
                          CBlockIndex* walk_start = nullptr);

    //!
    //! \brief Derive the superblock m_project_status record from a computed result and the
    //! registry state.
    //!
    //! THE ONE RECORD RULE, shared by the producer (candidate stamping) and the validator
    //! (acceptance-time recomputation) so the two cannot drift: a project's recorded status
    //! is its registry status with the auto-greylist promotion applied -- ACTIVE or
    //! MAN_GREYLISTED plus membership in the computed set promotes to AUTO_GREYLISTED,
    //! AUTO_GREYLIST_OVERRIDE is never promoted -- and only AUTO_GREYLISTED and
    //! MAN_GREYLISTED entries are recorded (ACTIVE is omitted to conserve space, matching
    //! the historical write-site behavior).
    //!
    //! \param result The computed greylist result for the candidate head.
    //! \param whitelist A Snapshot(GreylistState::NONE, ALL_BUT_DELETED) view (raw registry
    //! status, no overlay -- the promotion is applied here, from the result).
    //!
    static Superblock::ProjectStatus DeriveProjectStatusRecord(const Result& result,
                                                               const WhitelistSnapshot& whitelist);
};

} // namespace GRC

#endif // GRIDCOIN_AUTOGREYLIST_V2_H
