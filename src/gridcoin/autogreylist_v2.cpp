// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <gridcoin/autogreylist_v2.h>

#include "chainparams.h"
#include "gridcoin/claim.h"
#include "gridcoin/support/block_finder.h"
#include "node/blockstorage.h"

using namespace GRC;

AutoGreylistV2::Result AutoGreylistV2::Compute(
    SuperblockPtr head_ptr,
    const WhitelistSnapshot& whitelist,
    const Whitelist::ProjectEntryMap& project_first_actives,
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks)
    EXCLUSIVE_LOCKS_REQUIRED (cs_main)
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
            index_ptr = GRC::BlockFinder::FindByHeight(head_ptr.m_height - 1);
        } else {
            // This only works if the unit_test_blocks are all superblocks, which they are by
            // the construction of the unit test fixtures.
            const auto iter = unit_test_blocks->find(head_ptr.m_height - 1);

            index_ptr = (iter != unit_test_blocks->end()) ? iter->second.first : nullptr;
        }
    }

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

    // ---- Pass 2: evaluate, entirely in memory. ----------------------------------------------
    for (const auto& iter : collected) {
        if (!iter.second.m_baseline_admissible) {
            continue;
        }

        GreylistCandidateV2 candidate(iter.first, iter.second.m_baseline_tc);

        for (const auto& update : iter.second.m_updates) {
            candidate.UpdateGreylistCandidateEntry(update.second, update.first);
        }

        if (candidate.m_meets_greylisting_crit) {
            result.m_auto_greylisted.insert(iter.first);
        }

        result.m_candidates.insert(std::make_pair(iter.first, std::move(candidate)));
    }

    return result;
}
