// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/claim.h"
#include "main.h"
#include "node/blockstorage.h"
#include "gridcoin/support/block_finder.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "node/ui_interface.h"

#include <algorithm>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
// This is the global singleton for the whitelist. It also contains a smart pointer to the AutoGreylist cache singleton object.
Whitelist g_whitelist;
} // anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

Whitelist& GRC::GetWhitelist()
{
    return g_whitelist;
}

std::shared_ptr<AutoGreylist> GRC::GetAutoGreylistCache()
{
    return GRC::GetWhitelist().GetAutoGreylist();
}


// -----------------------------------------------------------------------------
// Class: ProjectEntry
// -----------------------------------------------------------------------------

constexpr uint32_t ProjectEntry::CURRENT_VERSION; // For clang

ProjectEntry::ProjectEntry(uint32_t version)
    : m_version(version)
    , m_name()
    , m_url()
    , m_timestamp(0)
    , m_hash()
    , m_previous_hash()
    , m_gdpr_controls(false)
    , m_requires_ext_adapter(false)
    , m_public_key(CPubKey {})
    , m_status(ProjectEntryStatus::UNKNOWN)
{
}

ProjectEntry::ProjectEntry(uint32_t version, std::string name, std::string url)
    : ProjectEntry(version, name, url, false, false, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

ProjectEntry::ProjectEntry(uint32_t version, std::string name, std::string url, bool gdpr_controls)
    : ProjectEntry(version, name, url, gdpr_controls, false, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

ProjectEntry::ProjectEntry(uint32_t version, std::string name, std::string url,
                           bool gdpr_controls, bool requires_ext_adapter, Status status, int64_t timestamp)
    : m_version(version)
    , m_name(name)
    , m_url(url)
    , m_timestamp(timestamp)
    , m_hash()
    , m_previous_hash()
    , m_gdpr_controls(gdpr_controls)
    , m_requires_ext_adapter(requires_ext_adapter)
    , m_public_key(CPubKey {})
    , m_status(status)
{
}

bool ProjectEntry::WellFormed() const
{
    return (!m_name.empty()
            && !m_url.empty()
            && m_status != ProjectEntryStatus::UNKNOWN
            && m_status != ProjectEntryStatus::OUT_OF_BOUND);
}

std::string ProjectEntry::Key() const
{
    return m_name;
}

std::pair<std::string, std::string> ProjectEntry::KeyValueToString() const
{
    return std::make_pair(m_name, m_url);
}

std::string ProjectEntry::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string ProjectEntry::StatusToString(const ProjectEntryStatus& status, const bool& translated) const
{
    if (translated) {
        switch(status) {
        case ProjectEntryStatus::UNKNOWN:                return _("Unknown");
        case ProjectEntryStatus::DELETED:                return _("Deleted");
        case ProjectEntryStatus::MAN_GREYLISTED:         return _("Manually Greylisted");
        case ProjectEntryStatus::AUTO_GREYLISTED:        return _("Automatically Greylisted");
        case ProjectEntryStatus::ACTIVE:                 return _("Active");
        case ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE: return _("Active by Greylist Override");
        case ProjectEntryStatus::OUT_OF_BOUND:           break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case ProjectEntryStatus::UNKNOWN:                return "Unknown";
        case ProjectEntryStatus::DELETED:                return "Deleted";
        case ProjectEntryStatus::MAN_GREYLISTED:         return "Manually Greylisted";
        case ProjectEntryStatus::AUTO_GREYLISTED:        return "Automatically Greylisted";
        case ProjectEntryStatus::ACTIVE:                 return "Active";
        case ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE: return "Active by Greylist Override";
        case ProjectEntryStatus::OUT_OF_BOUND:           break;
        }

        assert(false); // Suppress warning
    }

    // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
    // from some compiler versions.
    return std::string{};
}

std::string ProjectEntry::DisplayName() const
{
    std::string display_name = m_name;
    std::replace(display_name.begin(), display_name.end(), '_', ' ');

    return display_name;
}

std::string ProjectEntry::BaseUrl() const
{
    // Remove the "@" from the URL in the contract. We assume that it always
    // occurs at the very end:
    return m_url.substr(0, m_url.size() - 1);
}

std::string ProjectEntry::DisplayUrl() const
{
    // TODO: remove this after project contracts support arbitrary URLs.
    // WCG project URL refers to a location inaccessible to the end user.
    if (m_name == "World_Community_Grid") {
        return "https://www.worldcommunitygrid.org/";
    }

    return BaseUrl();
}

std::string ProjectEntry::StatsUrl(const std::string& type) const
{
    if (type.empty()) {
        return BaseUrl() + "stats/";
    }

    return BaseUrl() + "stats/" + type + ".gz";
}

std::optional<bool> ProjectEntry::HasGDPRControls() const
{
    std::optional<bool> has_gdpr_controls;

    if (m_version >= 2) {
        has_gdpr_controls = m_gdpr_controls;
    }

    return has_gdpr_controls;
}

std::optional<bool> ProjectEntry::RequiresExtAdapter() const
{
    std::optional<bool> requires_ext_adapter;

    if (m_version >= 4) {
        requires_ext_adapter = m_requires_ext_adapter;
    }

    return requires_ext_adapter;
}

// -----------------------------------------------------------------------------
// Class: Project
// -----------------------------------------------------------------------------

// TODO: Evaluate and remove some of these constructors, some of which are identical
// except the arguments are in a different order to support existing code.
Project::Project(uint32_t version)
    : ProjectEntry(version)
{
}

Project::Project(std::string name, std::string url)
    : ProjectEntry(1, name, url, false, false, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

Project::Project(uint32_t version, std::string name, std::string url)
    : ProjectEntry(version, name, url, false, false,ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

Project::Project(std::string name, std::string url, int64_t timestamp, uint32_t version)
    : ProjectEntry(version, name, url, false, false, ProjectEntryStatus::UNKNOWN, timestamp)
{
}

Project::Project(uint32_t version, std::string name, std::string url, bool gdpr_controls)
    : ProjectEntry(version, name, url, gdpr_controls, false, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

Project::Project(uint32_t version, std::string name, std::string url, bool gdpr_controls,
                 bool requires_ext_adapter, ProjectEntryStatus status)
    : ProjectEntry(version, name, url, gdpr_controls, requires_ext_adapter, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
    // The only two values that make sense for status using this constructor overload are MAN_GREYLISTED and
    // AUTO_GREYLIST_OVERRIDE. The other are handled by the contract action context and the other overloads.
    switch (status) {
    case ProjectEntryStatus::MAN_GREYLISTED:
        m_status = ProjectEntryStatus::MAN_GREYLISTED;
        break;
    case ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE:
        m_status = ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE;
        break;
    case ProjectEntryStatus::ACTIVE:
        break;
    case ProjectEntryStatus::DELETED:
        break;
    case ProjectEntryStatus::AUTO_GREYLISTED:
        break;
    case ProjectEntryStatus::UNKNOWN:
        break;
    case ProjectEntryStatus::OUT_OF_BOUND:
        break;
    }
}

Project::Project(uint32_t version, std::string name, std::string url, bool gdpr_controls, int64_t timestamp)
    : ProjectEntry(version, name, url, gdpr_controls, false, ProjectEntryStatus::UNKNOWN, timestamp)
{
}

Project::Project(std::string name, std::string url, int64_t timestamp, uint32_t version, bool gdpr_controls)
    : ProjectEntry(version, name, url, gdpr_controls, false,  ProjectEntryStatus::UNKNOWN, timestamp)
{
}

Project::Project(ProjectEntry entry)
    : ProjectEntry(entry)
{
}

// -----------------------------------------------------------------------------
// Class: WhitelistSnapshot
// -----------------------------------------------------------------------------

WhitelistSnapshot::WhitelistSnapshot(ProjectListPtr projects, const ProjectEntry::ProjectFilterFlag& filter_used)
    : m_projects(std::move(projects))
      , m_filter(filter_used)
{
}

WhitelistSnapshot::const_iterator WhitelistSnapshot::begin() const
{
    return m_projects->begin();
}

WhitelistSnapshot::const_iterator WhitelistSnapshot::end() const
{
    return m_projects->end();
}

WhitelistSnapshot::size_type WhitelistSnapshot::size() const
{
    return m_projects->size();
}

bool WhitelistSnapshot::Populated() const
{
    return !m_projects->empty();
}

bool WhitelistSnapshot::Contains(const std::string& name) const
{
    if (m_projects == nullptr) {
        return false;
    }

    if (name.empty()) {
        return false;
    }

    if (m_projects->empty()) {
        return false;
    }

    // We could store an unordered map to speed this up, but with so few items
    // in the whitelist, the overhead of a map isn't worth it. Iteration is our
    // primary use-case, and iteration over the vector is fast.
    for (const auto& project : *m_projects) {
        if (project.m_name == name) {
            return true;
        }
    }

    return false;
}

ProjectEntry::ProjectFilterFlag WhitelistSnapshot::FilterUsed() const
{
    return m_filter;
}

WhitelistSnapshot WhitelistSnapshot::Sorted() const
{
    ProjectList sorted(m_projects->begin(), m_projects->end());

    auto ascending_by_name = [](const ProjectEntry& a, const ProjectEntry& b) {
        return std::lexicographical_compare(
            a.m_name.begin(),
            a.m_name.end(),
            b.m_name.begin(),
            b.m_name.end(),
            [](const char ac, const char bc) {
                return ToLower(ac) < ToLower(bc);
            });
    };

    std::sort(sorted.begin(), sorted.end(), ascending_by_name);

    return WhitelistSnapshot(std::make_shared<ProjectList>(sorted), m_filter);
}

// -----------------------------------------------------------------------------
// Class: AutoGreylist - automatic greylisting
// -----------------------------------------------------------------------------

AutoGreylist::AutoGreylist()
    : m_greylist_ptr(std::make_shared<Greylist>())
    , m_superblock_hash(Superblock().GetHash(true))
{
}

AutoGreylist::Greylist::const_iterator AutoGreylist::begin() const
{
    LOCK(autogreylist_lock);

    return m_greylist_ptr->begin();
}

AutoGreylist::Greylist::const_iterator AutoGreylist::end() const
{
    LOCK(autogreylist_lock);

    return m_greylist_ptr->end();
}

AutoGreylist::Greylist::size_type AutoGreylist::size() const
{
    LOCK(autogreylist_lock);

    return m_greylist_ptr->size();
}

bool AutoGreylist::Contains(const std::string& name, const bool& only_auto_greylisted) const
{
    LOCK(autogreylist_lock);

    if (m_greylist_ptr == nullptr) {
        return false;
    }

    if (m_greylist_ptr->empty()) {
        return false;
    }

    auto iter = m_greylist_ptr->find(name);

    if (iter != m_greylist_ptr->end()) {
        if (only_auto_greylisted) {
            return (only_auto_greylisted && iter->second.m_meets_greylisting_crit);
        } else {
            return true;
        }
    } else {
        return false;
    }
}

void AutoGreylist::Refresh() EXCLUSIVE_LOCKS_REQUIRED (cs_main)
{
    SuperblockPtr superblock_ptr = Quorum::CurrentSuperblock();

    if (superblock_ptr.IsEmpty()) {
        return;
    }

    // If the m_superblock_hash has been populated and the current superblock has not changed, then no need to do anything.
    if (m_superblock_hash != Superblock().GetHash() && superblock_ptr->GetHash() == m_superblock_hash) {
        return;
    }

    RefreshWithSuperblock(superblock_ptr);
}

void AutoGreylist::RefreshWithSuperblock(SuperblockPtr superblock_ptr_in,
                                         std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks)
    EXCLUSIVE_LOCKS_REQUIRED (cs_main)
{
    if (superblock_ptr_in.IsEmpty()) {
        return;
    }

    // We need the current whitelist, including all records except deleted. This will include greylisted projects.
    // NOTE that the refresh_greylist is set to false here and MUST be this when called in the AutoGreylist class itself,
    // to avoid an infinite loop; include_override is also set to false because each refresh of the auto greylist must start
    // with the underlying whitelist state.
    const WhitelistSnapshot whitelist = GetWhitelist().Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED, false, false);

    LOCK(autogreylist_lock);

    m_greylist_ptr->clear();

    // No need to go further if the whitelist is empty (ignoring deleted records).
    if (!whitelist.Populated()) {
        return;
    }

    const Whitelist::ProjectEntryMap& project_first_actives = GetWhitelist().GetProjectsFirstActive();

    // If this superblock version is less than 3, then all prior ones must also be less than 3, so bail.
    if (superblock_ptr_in->m_version < 3) {
        return;
    }

    unsigned int superblock_count = 0;

    // Notice the superblock_ptr_in m_projects_all_cpid_total_credits MUST ALEADY BE POPULATED to record the TC state into
    // the auto greylist.
    for (const auto& iter : whitelist) {
        auto project = superblock_ptr_in->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.find(iter.m_name);

        // This record MUST be found, because for the record to be in the whitelist, it must have at least a first record.
        auto project_first_active = project_first_actives.find(iter.m_name);

        // The purpose of this time comparison is to ONLY post greylist candidate entry (updates) for superblocks that are equal
        // to or after the first entry date. Remember we are going backwards here. There cannot be entries held against a
        // whitelisted project from before it was ever whitelisted. This check is required to ensure the greylist rules work
        // correctly for newly whitelisted projects that are within the 40 day window for WAS and 20 day window for ZCD.
        if (project_first_active != project_first_actives.end()
            && superblock_ptr_in.m_timestamp >= project_first_active->second->m_timestamp) {
            if (project != superblock_ptr_in->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.end()) {
                // Record new greylist candidate entry baseline with the total credit for each project present in superblock.
                m_greylist_ptr->insert(std::make_pair(iter.m_name,
                                                      GreylistCandidateEntry(iter.m_name, project->second)));
            } else {
                // Record new greylist candidate entry with nullopt total credit. This is for a project that is in the whitelist,
                // but does not have a project entry in the superblock. This would be because the scrapers could not converge on the
                // project.
                m_greylist_ptr->insert(std::make_pair(iter.m_name, GreylistCandidateEntry(iter.m_name,
                                                                                          std::optional<uint64_t>())));
            }
        }
    }

    ++superblock_count;

    CBlockIndex* index_ptr;
    {
        // Find the block index entry for the block before the provided superblock_ptr. This also implements the unit test
        // substitute input structure.
        if (unit_test_blocks == nullptr) {
            index_ptr = GRC::BlockFinder::FindByHeight(superblock_ptr_in.m_height - 1);
        } else {
            // This only works if the unit_test_blocks are all superblocks, which they should be based on the setup of the
            // unit test.
            auto iter = unit_test_blocks->find(superblock_ptr_in.m_height - 1);

            if (iter != unit_test_blocks->end()) {
                // Get the unit test entry that corresponds to the superblock_ptr_in, which was processed above, and then
                // get CBlockIndex* entry.
                index_ptr = iter->second.first;
            } else {
                index_ptr = nullptr;
            }

        }
    }


    while (index_ptr != nullptr && index_ptr->pprev != nullptr && superblock_count <= 40) {

        if (!index_ptr->IsSuperblock()) {
            index_ptr = index_ptr->pprev;
            continue;
        }

        SuperblockPtr superblock_ptr;

        if (unit_test_blocks == nullptr) {
            // For some reason this is not working.
            //superblock_ptr.ReadFromDisk(index_ptr);

            CBlock block;

            if (!ReadBlockFromDisk(block, index_ptr, Params().GetConsensus())) {
                error("%s: Failed to read block from disk with requested height %u",
                      __func__,
                      index_ptr->nHeight);
                continue;
            }

            superblock_ptr = block.GetClaim().m_superblock;
            superblock_ptr.Rebind(index_ptr);
        } else {
            auto iter = unit_test_blocks->find(index_ptr->nHeight);

            if (iter != unit_test_blocks->end()) {
                superblock_ptr = iter->second.second;
            }
        }

        // Stop if superblocks less than version 3 are encountered while going backwards. This will happen until we are 40
        // superblocks past the 1st superblock after the height specified for the changeover to v3 superblocks.
        if (superblock_ptr->m_version < 3) {
            break;
        }

        for (const auto& iter : whitelist) {
            // This is guaranteed to succeed, because every whitelisted project was inserted as a new baseline entry above.
            auto greylist_entry = m_greylist_ptr->find(iter.m_name);

            auto project = superblock_ptr->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.find(iter.m_name);

                   // This record MUST be found, because for the record to be in the whitelist, it must have at least a first record.
            auto project_first_active = project_first_actives.find(iter.m_name);

                   // The purpose of this time comparison is to ONLY post greylist candidate entry (updates) for superblocks that are
                   // equal to or after the first entry date. Remember we are going backwards here. There cannot be entries held against
                   // a whitelisted project from before it was ever whitelisted. This check is required to ensure the greylist rules work
                   // correctly for newly whitelisted projects that are within the 40 day window for WAS and 20 day window for ZCD.
            if (project_first_active != project_first_actives.end()
                && superblock_ptr.m_timestamp >= project_first_active->second->m_timestamp) {
                if (project != superblock_ptr->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.end()) {
                    // Update greylist candidate entry with the total credit for each project present in superblock.
                    greylist_entry->second.UpdateGreylistCandidateEntry(project->second, superblock_count);
                } else {
                    // Record updated greylist candidate entry with nullopt total credit. This is for a project that is in the
                    // whitelist, but does not have a project entry in this superblock. This would be because the scrapers could
                    // not converge on the project for this superblock.
                    greylist_entry->second.UpdateGreylistCandidateEntry(std::optional<uint64_t>(std::nullopt), superblock_count);
                }
            }
        }

        ++superblock_count;

        index_ptr = index_ptr->pprev;
    }

    m_superblock_hash = superblock_ptr_in->GetHash();
}

void AutoGreylist::RefreshWithSuperblock(Superblock& superblock) EXCLUSIVE_LOCKS_REQUIRED (cs_main)
{
    SuperblockPtr superblock_ptr;

    // For the purposes of forming a superblock as the mining of a new block adding to the head of the chain, we will
    // form a superblock ptr referencing the current head of the chain. The actual superblock will be the next block if it
    // is added to the chain, for for the purposes here, this is what we want, because it is simply used to feed the
    // overloaded version which takes the superblock_ptr and follows the chain backwards to do the greylist calculations.
    superblock_ptr.Replace(superblock);

    superblock_ptr.Rebind(pindexBest);

    RefreshWithSuperblock(superblock_ptr);

    // Here we want to get the whitelist with the greylist override applied, but do NOT want to refresh the auto grey list, since
    // we are in a refresh method within the auto greylist class.
    const WhitelistSnapshot whitelist = GetWhitelist().Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED, false, true);

    // Update the superblock object with the project greylist status.
    for (const auto& project : whitelist) {
        if (project.m_status == ProjectEntryStatus::AUTO_GREYLISTED || project.m_status == ProjectEntryStatus::MAN_GREYLISTED) {
            superblock.m_project_status.m_project_status.insert(std::make_pair(project.m_name, project.m_status));
        }
    }
}

void AutoGreylist::Reset()
{
    if (m_greylist_ptr != nullptr) {
        m_greylist_ptr->clear();
    } else {
        m_greylist_ptr = std::make_shared<Greylist>();
    }

    m_superblock_hash = Superblock().GetHash(true);
}

// -----------------------------------------------------------------------------
// Class: Whitelist (Registry)
// -----------------------------------------------------------------------------

WhitelistSnapshot Whitelist::Snapshot(const ProjectEntry::ProjectFilterFlag& filter,
                                      const bool& refresh_greylist,
                                      const bool& include_override) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    ProjectList projects;

    LOCK(cs_lock);

    if (m_project_entries.empty()) {
        return WhitelistSnapshot(std::make_shared<ProjectList>(projects), filter);
    }

    if (refresh_greylist && m_auto_greylist != nullptr) {
        m_auto_greylist->Refresh();
    }

    // This contains the override for automatic greylisting integrated with the switch. If the AutoGreylist class refresh
    // determines that the project meets greylisting criteria, it will be in the AutoGreylist object pointed to
    // by the greylist_ptr. This will override the corresponding project entry from the project whitelist registry.
    //
    // We do NOT want to actually change the registry entries map with the auto greylist output, because the
    // greylist output is a dynamic override and when the override is removed, the underlying value of the
    // project status enforced by the project entries from addkey should return. This effectively means the greylist
    // override is applied on superblock boundaries, because the AutoGreylist global (singleton) cache is updated
    // when the superblock stakes.
    //
    // Make a copy of the registry project entries map for override purposes. This is not too bad because the number of
    // projects is relatively small.
    ProjectEntryMap project_entries = m_project_entries;

    for (auto iter : project_entries) {
        if (include_override) {
            // This is the actual override. The most important thing here is the greylist_ptr->Contains(iter.first) part. This
            // applies the current state of the greylist at the time of the construction of the whitelist snapshot, without
            // disturbing the underlying projects registry.

            bool in_greylist = m_auto_greylist != nullptr ? m_auto_greylist->Contains(iter.first) : false;

            // If the project does NOT have a status of auto greylist override, and it is either active or already manually
            // greylisted, then if it is in the greylist, mark with the status auto greylisted.
            if (!(iter.second->m_status == ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE)
                && (iter.second->m_status == ProjectEntryStatus::ACTIVE || iter.second->m_status == ProjectEntryStatus::MAN_GREYLISTED)
                && in_greylist) {
                iter.second->m_status = ProjectEntryStatus::AUTO_GREYLISTED;
            }
        }

        switch (filter) {
        case ProjectEntry::ProjectFilterFlag::REG_ACTIVE:
            if (iter.second->m_status == ProjectEntryStatus::ACTIVE) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::ACTIVE:
            if (iter.second->m_status == ProjectEntryStatus::ACTIVE
                || iter.second->m_status == ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::MAN_GREYLISTED:
            if (iter.second->m_status == ProjectEntryStatus::MAN_GREYLISTED) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::AUTO_GREYLISTED:
            if (iter.second->m_status == ProjectEntryStatus::AUTO_GREYLISTED) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::AUTO_GREYLIST_OVERRIDE:
            if (iter.second->m_status == ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::GREYLISTED:
            if (iter.second->m_status == ProjectEntryStatus::MAN_GREYLISTED
                || iter.second->m_status == ProjectEntryStatus::AUTO_GREYLISTED) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::DELETED:
            if (iter.second->m_status == ProjectEntryStatus::DELETED) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::NOT_ACTIVE:
            if (iter.second->m_status == ProjectEntryStatus::MAN_GREYLISTED
                || iter.second->m_status == ProjectEntryStatus::AUTO_GREYLISTED
                || iter.second->m_status == ProjectEntryStatus::DELETED) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED:
            if (iter.second->m_status == ProjectEntryStatus::ACTIVE
                || iter.second->m_status == ProjectEntryStatus::MAN_GREYLISTED
                || iter.second->m_status == ProjectEntryStatus::AUTO_GREYLISTED
                || iter.second->m_status == ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE) {
                projects.push_back(*iter.second);
            }
            break;
        case ProjectEntry::ProjectFilterFlag::ALL:
            projects.push_back(*iter.second);
            break;
        case ProjectEntry::ProjectFilterFlag::NONE:
            break;
        }
    }

    return WhitelistSnapshot(std::make_shared<ProjectList>(projects), filter);
}

void Whitelist::Reset()
{
    LOCK(cs_lock);

    m_project_entries.clear();
    m_pending_project_entries.clear();
    m_expired_project_entries.clear();
    m_project_first_actives.clear();
    m_project_db.clear();

    // If the whitelist registry is reset, the auto greylist cache should be reset as well.
    m_auto_greylist->Reset();
}

void Whitelist::AddDelete(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the LevelDB database
    int height = -1;

    if (ctx.m_pindex)
    {
        height = ctx.m_pindex->nHeight;
    }

    Project payload = ctx->CopyPayloadAs<Project>();

    // Fill this in from the transaction context because these are not done during payload
    // initialization.
    payload.m_hash = ctx.m_tx.GetHash();
    payload.m_timestamp = ctx.m_tx.nTime;

    // If the contract status is ADD, then ProjectEntryStatus will be ACTIVE. If contract status
    // is REMOVE then ProjectEntryStatus will be DELETED.
    if (ctx->m_action == ContractAction::ADD) {
        // Normal add/delete contracts are constructed with "unknown" status. The status is recorded on the local client
        // based on the add/delete action. The manual greylist status is specified in the contract as constructed, and will carry
        // through here for the contract add with the greylist flag set.
        if (payload.m_status == ProjectEntryStatus::UNKNOWN) {
        payload.m_status = ProjectEntryStatus::ACTIVE;
        }
    } else if (ctx->m_action == ContractAction::REMOVE) {
        payload.m_status = ProjectEntryStatus::DELETED;
    }

    LOCK(cs_lock);

    auto project_entry_pair_iter = m_project_entries.find(payload.m_name);

    ProjectEntry_ptr current_project_entry_ptr = nullptr;

    bool first_entry = false;

    // Is there an existing project entry in the map?
    bool current_project_entry_present = (project_entry_pair_iter != m_project_entries.end());

    // If so, then get a smart pointer to it. If not then set first_entry to true to place an entry in
    // the m_project_first_actives map later.
    if (current_project_entry_present) {
        current_project_entry_ptr = project_entry_pair_iter->second;

        // Set the payload m_entry's prev entry ctx hash = to the existing entry's hash.
        payload.m_previous_hash = current_project_entry_ptr->m_hash;
    } else { // Original entry for this project entry key
        payload.m_previous_hash = uint256 {};

        first_entry = true;
    }

    LogPrint(LogFlags::CONTRACT, "INFO: %s: project entry add/delete: contract m_version = %u, payload "
                                "m_version = %u, name = %s, url = %s, m_timestamp = %" PRId64 ", "
                                "m_hash = %s, m_previous_hash = %s, m_gdpr_contracts = %u, m_status = %s",
             __func__,
             ctx->m_version,
             payload.m_version,
             payload.m_name,
             payload.m_url,
             payload.m_timestamp,
             payload.m_hash.ToString(),
             payload.m_previous_hash.ToString(),
             payload.m_gdpr_controls,
             payload.StatusToString()
             );

    // This does an implicit cast of Project to ProjectEntry, which gets rid of the payload version and uses
    // the entry serialization for going into leveldb.
    ProjectEntry& historical = payload;

    if (!m_project_db.insert(ctx.m_tx.GetHash(), height, historical))
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: In recording of the project entry for name %s, url %s, hash %s, "
                                    "the project entry db record already exists. This can be expected on a restart "
                                    "of the wallet to ensure multiple contracts in the same block get stored/replayed.",
                 __func__,
                 historical.m_name,
                 historical.m_url,
                 historical.m_hash.GetHex());
    }

    auto project_iter = m_project_db.find(ctx.m_tx.GetHash());

    // Finally, insert the new project entry (payload) smart pointer into the m_project_entries map.
    m_project_entries[payload.m_name] = project_iter->second;

    // The downside of this is that this map will hold a reference to each project that was ever active and they will
    // be ineligible for passivation as a result. However, the memory use for this is minimal given the relatively
    // small number of projects. It is worth it given the alternative of having to walk each project chainlet to determine
    // first active for every update of the auto greylist.
    if (first_entry) {
        m_project_first_actives.insert(std::make_pair(payload.m_name, project_iter->second));
    }

    ChangeType status;

    if (project_iter->second->m_status == ProjectEntryStatus::DELETED) {
        status = CT_DELETED;
    } else if (current_project_entry_present) {
        status = CT_UPDATED;
    } else {
        status = CT_NEW;
    }

    NotifyProjectChanged(project_iter->second, status);

    // notify an external script when a project is added to the whitelist, or a project status changes.
    #if HAVE_SYSTEM
    std::string cmd = gArgs.GetArg("-projectnotify", "");

    if (!cmd.empty()) {
        std::thread t(runCommand, cmd);
        t.detach(); // thread runs free
    }
    #endif

    return;

}

void Whitelist::Add(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void Whitelist::Delete(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void Whitelist::Revert(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<Project>();

    // For project entries, both adds and removes will have records to revert in the m_project_entries map,
    // and also, if not the first entry for that project key, will have a historical record to
    // resurrect.
    LOCK(cs_lock);

    auto entry_to_revert = m_project_entries.find(payload->m_name);

    if (entry_to_revert == m_project_entries.end()) {
        error("%s: The project entry for key %s to revert was not found in the project entry map.",
              __func__,
              entry_to_revert->second->m_name);

        // If there is no record in the current m_project_entries map, then there is nothing to do here. This
        // should not occur.
        return;
    }

    // If this is not a null hash, then there will be a prior entry to resurrect.
    std::string key = entry_to_revert->second->m_name;
    uint256 resurrect_hash = entry_to_revert->second->m_previous_hash;

    // Revert the ADD or REMOVE action. Unlike the beacons, this is symmetric.
    if (ctx->m_action == ContractAction::ADD || ctx->m_action == ContractAction::REMOVE) {
        // Erase the record from m_project_entries.
        if (m_project_entries.erase(payload->m_name) == 0) {
            error("%s: The project entry to erase during a project entry revert for key %s was not found.",
                  __func__,
                  key);
            // If the record to revert is not found in the m_project_entries map, no point in continuing.
            return;
        }

        // Also erase the record from the db.
        if (!m_project_db.erase(ctx.m_tx.GetHash())) {
            error("%s: The db entry to erase during a project entry revert for key %s was not found.",
                  __func__,
                  key);

            // Unlike the above we will keep going even if this record is not found, because it is identical to the
            // m_project_entries record above. This should not happen, because during contract adds and removes,
            // entries are made simultaneously to the m_project_entries and m_project_db.
        }

        if (resurrect_hash.IsNull()) {
            // The reverted record was the first record, so we need to revert (remove) the entry in the
            // m_project_first_actives map.
            if (!m_project_first_actives.erase(key)) {
                error("%S: The project first active entry for project \"%s\" in the first actives map was not found "
                      "to erase during a revert. This condition should not occur.",
                      __func__,
                      key);
            }

            return;
        }

        auto resurrect_entry = m_project_db.find(resurrect_hash);

        if (resurrect_entry == m_project_db.end()) {
            error("%s: The prior entry to resurrect during a project entry ADD revert for key %s was not found.",
                  __func__,
                  key);
            return;
        }

        // Resurrect the entry prior to the reverted one. It is safe to use the bracket form here, because of the protection
        // of the logic above. There cannot be any entry in m_project_entries with that key value left if we made it here.
        m_project_entries[resurrect_entry->second->m_name] = resurrect_entry->second;
    }
}

bool Whitelist::Validate(const Contract& contract, const CTransaction& tx, int &DoS) const
{
    // No validation is done with contract versions of 2 or less.
    if (contract.m_version <= 2) {
        return true;
    }

    const auto payload = contract.SharePayloadAs<Project>();

    if (contract.m_version >= 3 && payload->m_version < 3) {
        DoS = 25;
        error("%s: Project entry contract in contract v3 is wrong version.", __func__);
        return false;
    }

    if (!payload->WellFormed(contract.m_action.Value())) {
        DoS = 25;
        error("%s: Malformed project entry contract", __func__);
        return false;
    }

    return true;
}

bool Whitelist::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

int Whitelist::Initialize()
{
    LOCK(cs_lock);

    int height = m_project_db.Initialize(m_project_entries,
                                         m_pending_project_entries,
                                         m_expired_project_entries,
                                         m_project_first_actives,
                                         true);

    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_project_db size after load: %u", __func__, m_project_db.size());
    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_project_entries size after load: %u", __func__, m_project_entries.size());

    return height;
}

void Whitelist::SetDBHeight(int& height)
{
    LOCK(cs_lock);

    m_project_db.StoreDBHeight(height);
}

int Whitelist::GetDBHeight()
{
    int height = 0;

    LOCK(cs_lock);

    m_project_db.LoadDBHeight(height);

    return height;
}

void Whitelist::ResetInMemoryOnly()
{
    LOCK(cs_lock);

    m_project_entries.clear();
    m_pending_project_entries.clear();
    m_expired_project_entries.clear();
    m_project_first_actives.clear();
    m_project_db.clear_in_memory_only();
}

uint64_t Whitelist::PassivateDB()
{
    LOCK(cs_lock);

    return m_project_db.passivate_db();
}

const Whitelist::ProjectEntryMap Whitelist::GetProjectsFirstActive() const
{
    LOCK(cs_lock);

    return m_project_first_actives;
}

std::shared_ptr<AutoGreylist> Whitelist::GetAutoGreylist()
{
    return m_auto_greylist;
}

Whitelist::ProjectEntryDB &Whitelist::GetProjectDB()
{
    return m_project_db;
}

template<> const std::string Whitelist::ProjectEntryDB::KeyType()
{
    return std::string("project");
}
