// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <gridcoin/autogreylist_v2.h>

#include "chainparams.h"
#include "gridcoin/claim.h"
#include "gridcoin/support/block_finder.h"
#include "node/blockstorage.h"

#include <set>

using namespace GRC;

AutoGreylistV2::Result AutoGreylistV2::Compute(
    SuperblockPtr head_ptr,
    const WhitelistSnapshot& whitelist,
    const Whitelist::ProjectEntryMap& project_first_actives,
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks,
    CBlockIndex* walk_start)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    Result result;

    if (head_ptr.IsEmpty() || !whitelist.Populated()) {
        return result;
    }

    // If the head superblock is pre-v3 there are no total-credit records at all: every prior
    // superblock is also pre-v3, so there is nothing to walk.
    if (head_ptr->m_version < 3) {
        return result;
    }

    // ---- Pass 1: collect. -----------------------------------------------------------------
    //
    // One backward traversal gathers, per project, the recorded total credit (or absence) at
    // each admissible walk position, with the head at position 0. Collecting the whole window
    // before evaluating is what later allows corrections that need bounded lookahead inside
    // the window (the initial-state latch) without reversing the walk -- the current endpoint
    // stays unitary, one anchor shared by every project.
    //
    // A position is admissible for a project when the superblock's timestamp is at or after
    // the project's first-activation timestamp: no entries may be held against a project from
    // before it was ever whitelisted. Timestamps are monotonic over the chain, so per project
    // the admissible positions form a contiguous prefix 0..k of the walk.
    struct CollectedProject {
        //! (walk position, total credit or absence), ascending position order.
        std::vector<std::pair<uint8_t, std::optional<uint64_t>>> m_updates;
        bool m_baseline_admissible = false;
        std::optional<uint64_t> m_baseline_tc;
    };

    std::map<std::string, CollectedProject> collected;

    // Head (position 0).
    for (const auto& iter : whitelist) {
        const auto project_first_active = project_first_actives.find(iter.m_name);

        if (project_first_active == project_first_actives.end()
            || head_ptr.m_timestamp < project_first_active->second->m_timestamp) {
            continue;
        }

        CollectedProject& entry = collected[iter.m_name];
        entry.m_baseline_admissible = true;

        const auto project = head_ptr->m_projects_all_cpids_total_credits
                                 .m_projects_all_cpid_total_credits.find(iter.m_name);

        if (project != head_ptr->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.end()) {
            entry.m_baseline_tc = project->second;
        }
    }

    // Positions 1..40: the walk, matching the V1 traversal shape (skip non-superblock blocks;
    // stop at the first pre-v3 superblock, since everything older is also pre-v3).
    unsigned int superblock_count = 1;

    CBlockIndex* index_ptr;
    {
        if (unit_test_blocks == nullptr) {
            index_ptr = (walk_start != nullptr) ? walk_start
                                                : GRC::BlockFinder::FindByHeight(head_ptr.m_height - 1);
        } else {
            // This only works if the unit_test_blocks are all superblocks, which they are by
            // the construction of the unit test fixtures.
            const auto iter = unit_test_blocks->find(head_ptr.m_height - 1);

            index_ptr = (iter != unit_test_blocks->end()) ? iter->second.first : nullptr;
        }
    }

    // Phantom-head detection (DESIGN.md section 3): when the candidate head was built from
    // the SAME convergence as the most recent committed superblock (it just staked and the
    // convergence has not been rebuilt), the two carry identical total credits, and walking
    // both counts the same data twice -- TC[1] >= TC[0] then fires a false ZCD for every
    // project. The superblock's own convergence hint identifies its source convergence, so
    // the rule is intrinsic and deterministic on every node: if the FIRST committed
    // superblock behind the head carries the head's (non-zero) convergence hint, it is a
    // re-derivation of the same data and is skipped -- position 1 becomes the superblock
    // before it. Only the first can legitimately match; an older match would be a 32-bit
    // hint collision, so the comparison is not applied deeper.
    bool first_superblock_behind_head = true;

    while (index_ptr != nullptr && index_ptr->pprev != nullptr && superblock_count <= 40) {
        if (!index_ptr->IsSuperblock()) {
            index_ptr = index_ptr->pprev;
            continue;
        }

        SuperblockPtr superblock_ptr;

        if (unit_test_blocks == nullptr) {
            CBlock block;

            if (!ReadBlockFromDisk(block, index_ptr, Params().GetConsensus())) {
                // Note the V1 walker retries this same block forever on a read failure; a
                // failed read here instead ends the walk with the positions collected so
                // far. Unreachable on any healthy node either way -- a block index entry
                // whose block is unreadable is a corrupted datadir.
                error("%s: Failed to read block from disk with requested height %u",
                      __func__, index_ptr->nHeight);
                break;
            }

            superblock_ptr = block.GetClaim().m_superblock;
            superblock_ptr.Rebind(index_ptr);
        } else {
            const auto iter = unit_test_blocks->find(index_ptr->nHeight);

            if (iter != unit_test_blocks->end()) {
                superblock_ptr = iter->second.second;
            }
        }

        if (superblock_ptr->m_version < 3) {
            break;
        }

        if (first_superblock_behind_head) {
            first_superblock_behind_head = false;

            if (head_ptr->m_convergence_hint != 0
                && superblock_ptr->m_convergence_hint == head_ptr->m_convergence_hint) {
                index_ptr = index_ptr->pprev;
                continue;
            }
        }

        for (const auto& iter : whitelist) {
            const auto project_first_active = project_first_actives.find(iter.m_name);

            if (project_first_active == project_first_actives.end()
                || superblock_ptr.m_timestamp < project_first_active->second->m_timestamp) {
                continue;
            }

            const auto found = collected.find(iter.m_name);

            // Timestamps are monotonic, so a project admissible at this older superblock was
            // admissible at the head and therefore has a baseline. Guarded rather than
            // assumed (the V1 walker dereferences the equivalent lookup unchecked).
            if (found == collected.end() || !found->second.m_baseline_admissible) {
                continue;
            }

            const auto project = superblock_ptr->m_projects_all_cpids_total_credits
                                     .m_projects_all_cpid_total_credits.find(iter.m_name);

            if (project != superblock_ptr->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.end()) {
                found->second.m_updates.push_back(
                    std::make_pair(static_cast<uint8_t>(superblock_count), project->second));
            } else {
                found->second.m_updates.push_back(
                    std::make_pair(static_cast<uint8_t>(superblock_count), std::optional<uint64_t>()));
            }
        }

        ++superblock_count;

        index_ptr = index_ptr->pprev;
    }

    // ---- Pass 1b: latch evidence extension (capped). ----------------------------------------
    //
    // The latch classifies a recorded zero as corruption iff a non-zero total credit exists
    // at an OLDER position -- but "older" is a property of the chain, not of the 40-position
    // window. A zero in a run touching the window EDGE (the exact shape of the WCG 2026-08-06
    // firing, where the corrupt zero sat at position 40) has no older in-window evidence, so
    // the walk continues past the window for LATCH EVIDENCE ONLY: for each project whose
    // oldest in-window data leaves a trailing zero unresolved, scan older superblocks until
    // the first admissible non-zero (all its in-window zeros are then corrupt), the project's
    // first-activation boundary or a pre-v3 superblock (conclusively no evidence: the zeros
    // are genuine initial state), or the cap of 40 additional superblocks (deterministic on
    // every node; beyond the cap, zeros are genuine). Typical cost is a single extra
    // superblock read -- the first older superblock almost always carries non-zero credit.
    std::set<std::string> latch_beyond_window;

    {
        std::set<std::string> unresolved;

        for (const auto& iter : collected) {
            if (!iter.second.m_baseline_admissible) {
                continue;
            }

            // The oldest in-window non-zero, and whether any zero sits at a still-older
            // in-window position (the trailing run the in-window latch cannot classify).
            std::optional<uint8_t> oldest_nonzero;
            std::optional<uint8_t> oldest_zero;

            if (iter.second.m_baseline_tc) {
                (*iter.second.m_baseline_tc != 0 ? oldest_nonzero : oldest_zero) = 0;
            }

            for (const auto& update : iter.second.m_updates) {
                if (update.second) {
                    (*update.second != 0 ? oldest_nonzero : oldest_zero) = update.first;
                }
            }

            if (oldest_zero && (!oldest_nonzero || *oldest_zero > *oldest_nonzero)) {
                unresolved.insert(iter.first);
            }
        }

        unsigned int extension_count = 0;

        while (!unresolved.empty() && index_ptr != nullptr && index_ptr->pprev != nullptr
               && extension_count < 40) {
            if (!index_ptr->IsSuperblock()) {
                index_ptr = index_ptr->pprev;
                continue;
            }

            SuperblockPtr superblock_ptr;

            if (unit_test_blocks == nullptr) {
                CBlock block;

                if (!ReadBlockFromDisk(block, index_ptr, Params().GetConsensus())) {
                    error("%s: Failed to read block from disk with requested height %u (latch scan)",
                          __func__, index_ptr->nHeight);
                    break;
                }

                superblock_ptr = block.GetClaim().m_superblock;
                superblock_ptr.Rebind(index_ptr);
            } else {
                const auto iter = unit_test_blocks->find(index_ptr->nHeight);

                if (iter != unit_test_blocks->end()) {
                    superblock_ptr = iter->second.second;
                }
            }

            if (superblock_ptr->m_version < 3) {
                break;
            }

            for (auto name_iter = unresolved.begin(); name_iter != unresolved.end();) {
                const auto project_first_active = project_first_actives.find(*name_iter);

                // Past the project's first activation there can be no older evidence:
                // resolved, genuine.
                if (project_first_active == project_first_actives.end()
                    || superblock_ptr.m_timestamp < project_first_active->second->m_timestamp) {
                    name_iter = unresolved.erase(name_iter);
                    continue;
                }

                const auto project = superblock_ptr->m_projects_all_cpids_total_credits
                                         .m_projects_all_cpid_total_credits.find(*name_iter);

                if (project != superblock_ptr->m_projects_all_cpids_total_credits
                                   .m_projects_all_cpid_total_credits.end()
                    && project->second != 0) {
                    latch_beyond_window.insert(*name_iter);
                    name_iter = unresolved.erase(name_iter);
                    continue;
                }

                ++name_iter;
            }

            ++extension_count;

            index_ptr = index_ptr->pprev;
        }
    }

    // ---- Pass 2: evaluate, entirely in memory. ----------------------------------------------
    for (const auto& iter : collected) {
        if (!iter.second.m_baseline_admissible) {
            continue;
        }

        // The initial-state latch (DESIGN.md section 11): a recorded zero is corruption iff
        // a NON-ZERO total credit exists at an OLDER position in the window -- a lifetime
        // counter cannot return to zero. latch_j is the OLDEST non-zero position. Because
        // the walk is backward, the oldest non-zero is reached LAST; computing the latch
        // over the fully collected window is what makes the direction trap (latching on the
        // NEWEST non-zero, which would damn a new project's genuine initial zeros as
        // corruption) structurally impossible. A project with no non-zero anywhere has no
        // latch, and all of its zeros are genuine values.
        std::optional<uint8_t> latch_j;

        if (iter.second.m_baseline_tc && *iter.second.m_baseline_tc != 0) {
            latch_j = 0;
        }

        for (const auto& update : iter.second.m_updates) {
            if (update.second && *update.second != 0) {
                latch_j = update.first; // ascending order: the last one seen is the oldest.
            }
        }

        // Effective value: normalize a latched-corrupt zero to missing; everything else
        // passes through. The ZCD, bookmark and WAS arms all consume the effective value
        // (zeros and NAs both count as ZCDs); the history records the raw value. Evidence
        // from the capped beyond-window scan makes EVERY in-window zero corrupt (an older
        // non-zero exists past the window edge, so no in-window zero can be initial state).
        const bool evidence_beyond_window = latch_beyond_window.count(iter.first) > 0;

        const auto effective = [&latch_j, evidence_beyond_window](
                                   const std::optional<uint64_t>& recorded, uint8_t j)
            -> std::optional<uint64_t> {
            if (recorded && *recorded == 0
                && (evidence_beyond_window || (latch_j && j < *latch_j))) {
                return std::nullopt;
            }

            return recorded;
        };

        GreylistCandidateV2 candidate(iter.first,
                                      effective(iter.second.m_baseline_tc, 0),
                                      iter.second.m_baseline_tc);

        for (const auto& update : iter.second.m_updates) {
            candidate.UpdateGreylistCandidateEntry(effective(update.second, update.first),
                                                   update.second, update.first);
        }

        if (candidate.m_meets_greylisting_crit) {
            result.m_auto_greylisted.insert(iter.first);
        }

        result.m_candidates.insert(std::make_pair(iter.first, std::move(candidate)));
    }

    return result;
}

Superblock::ProjectStatus AutoGreylistV2::DeriveProjectStatusRecord(const Result& result,
                                                                     const WhitelistSnapshot& whitelist)
{
    Superblock::ProjectStatus record;

    for (const auto& project : whitelist) {
        ProjectEntryStatus status = project.m_status.Value();

        if (status != ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE
            && (status == ProjectEntryStatus::ACTIVE || status == ProjectEntryStatus::MAN_GREYLISTED)
            && result.m_auto_greylisted.count(project.m_name) > 0) {
            status = ProjectEntryStatus::AUTO_GREYLISTED;
        }

        if (status == ProjectEntryStatus::AUTO_GREYLISTED || status == ProjectEntryStatus::MAN_GREYLISTED) {
            record.m_project_status.insert(std::make_pair(project.m_name, status));
        }
    }

    return record;
}
