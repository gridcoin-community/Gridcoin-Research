// Copyright (c) 2014-2024 The Gridcoin developers
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
#include <atomic>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
    Whitelist g_whitelist;
} // anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

Whitelist& GRC::GetWhitelist()
{
    return g_whitelist;
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
    , m_public_key(CPubKey {})
    , m_status(ProjectEntryStatus::UNKNOWN)
{
}

ProjectEntry::ProjectEntry(uint32_t version, std::string name, std::string url)
    : ProjectEntry(version, name, url, false, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

ProjectEntry::ProjectEntry(uint32_t version, std::string name, std::string url, bool gdpr_controls)
    : ProjectEntry(version, name, url, gdpr_controls, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

ProjectEntry::ProjectEntry(uint32_t version, std::string name, std::string url,
                           bool gdpr_controls, Status status, int64_t timestamp)
    : m_version(version)
    , m_name(name)
    , m_url(url)
    , m_timestamp(timestamp)
    , m_hash()
    , m_previous_hash()
    , m_gdpr_controls(gdpr_controls)
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
        case ProjectEntryStatus::UNKNOWN:         return _("Unknown");
        case ProjectEntryStatus::DELETED:         return _("Deleted");
        case ProjectEntryStatus::MAN_GREYLISTED:  return _("Manually Greylisted");
        case ProjectEntryStatus::AUTO_GREYLISTED: return _("Automatically Greylisted");
        case ProjectEntryStatus::ACTIVE:          return _("Active");
        case ProjectEntryStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case ProjectEntryStatus::UNKNOWN:         return "Unknown";
        case ProjectEntryStatus::DELETED:         return "Deleted";
        case ProjectEntryStatus::MAN_GREYLISTED:  return "Manually Greylisted";
        case ProjectEntryStatus::AUTO_GREYLISTED: return "Automatically Greylisted";
        case ProjectEntryStatus::ACTIVE:          return "Active";
        case ProjectEntryStatus::OUT_OF_BOUND:    break;
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
    : ProjectEntry(1, name, url, false, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

Project::Project(uint32_t version, std::string name, std::string url)
    : ProjectEntry(version, name, url, false, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

Project::Project(std::string name, std::string url, int64_t timestamp, uint32_t version)
    : ProjectEntry(version, name, url, false, ProjectEntryStatus::UNKNOWN, timestamp)
{
}

Project::Project(uint32_t version, std::string name, std::string url, bool gdpr_controls)
    : ProjectEntry(version, name, url, gdpr_controls, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
}

Project::Project(uint32_t version, std::string name, std::string url, bool gdpr_controls, bool manual_greylist)
    : ProjectEntry(version, name, url, gdpr_controls, ProjectEntryStatus::UNKNOWN, int64_t {0})
{
    if (manual_greylist) {
        m_status = ProjectEntryStatus::MAN_GREYLISTED;
    }
}

Project::Project(uint32_t version, std::string name, std::string url, bool gdpr_controls, int64_t timestamp)
    : ProjectEntry(version, name, url, gdpr_controls, ProjectEntryStatus::UNKNOWN, timestamp)
{
}

Project::Project(std::string name, std::string url, int64_t timestamp, uint32_t version, bool gdpr_controls)
    : ProjectEntry(version, name, url, gdpr_controls, ProjectEntryStatus::UNKNOWN, timestamp)
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
    if (name.empty()) {
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
    , m_superblock_hash(uint256 {})
{
    Refresh();
}

AutoGreylist::Greylist::const_iterator AutoGreylist::begin() const
{
    return m_greylist_ptr->begin();
}

AutoGreylist::Greylist::const_iterator AutoGreylist::end() const
{
    return m_greylist_ptr->end();
}

AutoGreylist::Greylist::size_type AutoGreylist::size() const
{
    return m_greylist_ptr->size();
}

bool AutoGreylist::Contains(const std::string& name, const bool& only_auto_greylisted) const
{
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

void AutoGreylist::Refresh()
{
    LOCK(cs_main);

    // If the current superblock has not changed, then no need to do anything.
    if (!m_superblock_hash.IsNull() && Quorum::CurrentSuperblock()->GetHash() == m_superblock_hash) {
        return;
    }

    SuperblockPtr superblock_ptr = Quorum::CurrentSuperblock();

    if (!superblock_ptr.IsEmpty()) {
        RefreshWithSuperblock(superblock_ptr);
    }
}

void AutoGreylist::RefreshWithSuperblock(SuperblockPtr superblock_ptr_in)
{
    LOCK(lock);

    // We need the current whitelist, including all records except deleted. This will include greylisted projects,
    // whether currently marked as manually greylisted from protocol or overridden to auto greylisted by the auto greylist class,
    // based on the current state of the autogreylist. NOTE that the refresh_greylist is set to false here and MUST be this
    // when called in the AutoGreylist class itself, to avoid an infinite loop.
    const WhitelistSnapshot whitelist = GetWhitelist().Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED, false);

    m_greylist_ptr->clear();

    unsigned int v3_superblock_count = 0;

    if (superblock_ptr_in->m_version > 2) {
        ++v3_superblock_count;
    }

    // Notice the superblock_ptr_in m_projects_all_cpid_total_credits MUST ALEADY BE POPULATED to record the TC state into
    // the auto greylist.
    for (const auto& iter : whitelist) {
        auto project = superblock_ptr_in->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.find(iter.m_name);

        if (project != superblock_ptr_in->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.end()) {
            // Record new greylist candidate entry baseline with the total credit for each project present in superblock.
            m_greylist_ptr->insert(std::make_pair(iter.m_name,
                                                  GreylistCandidateEntry(iter.m_name, std::nearbyint(project->second))));
        } else {
            // Record new greylist candidate entry with nullopt total credit. This is for a project that is in the whitelist,
            // but does not have a project entry in the superblock. This would be because the scrapers could not converge on the
            // project.
            m_greylist_ptr->insert(std::make_pair(iter.m_name, GreylistCandidateEntry(iter.m_name,
                                                                                      std::optional<uint64_t>(std::nullopt))));
        }
    }

    CBlockIndex* index_ptr;
    {
        // Find the block index entry for the block before the provided superblock_ptr.
        index_ptr = GRC::BlockFinder::FindByHeight(superblock_ptr_in.m_height - 1);
    }

    unsigned int superblock_count = 1; // The 0 (baseline) superblock was processed above. Here we start with 1 and go up to 40

    while (index_ptr != nullptr && index_ptr->pprev != nullptr && superblock_count <= 40) {

        if (!index_ptr->IsSuperblock()) {
            index_ptr = index_ptr->pprev;
            continue;
        }

        // For some reason this is not working.
        //superblock_ptr.ReadFromDisk(index_ptr);

        CBlock block;
        if (!ReadBlockFromDisk(block, index_ptr, Params().GetConsensus())) {
            error("%s: Failed to read block from disk with requested height %u",
                  __func__,
                  index_ptr->nHeight);
            continue;
        }

        SuperblockPtr superblock_ptr = block.GetClaim().m_superblock;

        if (superblock_ptr->m_version > 2) {
            for (const auto& iter : whitelist) {
                // This is guaranteed to succeed, because every whitelisted project was inserted as a new baseline entry above.
                auto greylist_entry = m_greylist_ptr->find(iter.m_name);

                auto project = superblock_ptr->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.find(iter.m_name);

                if (project != superblock_ptr->m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits.end()) {
                    // Update greylist candidate entry with the total credit for each project present in superblock.
                    greylist_entry->second.UpdateGreylistCandidateEntry(project->second, superblock_count);
                } else {
                    // Record updated greylist candidate entry with nullopt total credit. This is for a project that is in the whitelist,
                    // but does not have a project entry in this superblock. This would be because the scrapers could not converge on the
                    // project for this superblock.
                    greylist_entry->second.UpdateGreylistCandidateEntry(std::optional<uint64_t>(std::nullopt), superblock_count);
                }
            }

            ++v3_superblock_count;
        }

        ++superblock_count;

        index_ptr = index_ptr->pprev;
    }

    // Mark elements with whether they meet greylist criteria.
    for (auto iter = m_greylist_ptr->begin(); iter != m_greylist_ptr->end(); ++iter) {
        // the v3_superblock_count >= 2 test is to ensure there are at least two v3 superblocks with the total credits
        // filled in to sample for the auto greylist determination. A two sb sample with positive change in TC will
        // cause the ZCD and WAS rules to pass.
        if (iter->second.GetZCD() < 7 && iter->second.GetWAS() >= Fraction(1, 10) && v3_superblock_count >= 2) {
            iter->second.m_meets_greylisting_crit = true;
         }
    }
}

void AutoGreylist::RefreshWithSuperblock(Superblock& superblock)
{
    SuperblockPtr superblock_ptr;

    // For the purposes of forming a superblock as the mining of a new block adding to the head of the chain, we will
    // form a superblock ptr referencing the current head of the chain. The actual superblock will be the next block if it
    // is added to the chain, for for the purposes here, this is what we want, because it is simply used to feed the
    // overloaded version which takes the superblock_ptr and follows the chain backwards to do the greylist calculations.
    superblock_ptr.Replace(superblock);

    {
        LOCK(cs_main);

        superblock_ptr.Rebind(pindexBest);
    }

    RefreshWithSuperblock(superblock_ptr);

    const WhitelistSnapshot whitelist = GetWhitelist().Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED, false);

    // Update the superblock object with the project greylist status.
    for (const auto& project : whitelist) {
        if (project.m_status == ProjectEntryStatus::AUTO_GREYLISTED || project.m_status == ProjectEntryStatus::MAN_GREYLISTED) {
            superblock.m_project_status.m_project_status.insert(std::make_pair(project.m_name, project.m_status));
        }
    }
}

// This is the global cached (singleton) for the auto greylist.
std::shared_ptr<AutoGreylist> g_autogreylist_ptr = std::make_shared<AutoGreylist>();

std::shared_ptr<AutoGreylist> AutoGreylist::GetAutoGreylistCache()
{
    return g_autogreylist_ptr;
}

// -----------------------------------------------------------------------------
// Class: Whitelist (Registry)
// -----------------------------------------------------------------------------

WhitelistSnapshot Whitelist::Snapshot(const ProjectEntry::ProjectFilterFlag& filter, const bool& refresh_greylist) const
{
    LOCK(cs_lock);

    std::shared_ptr<GRC::AutoGreylist> greylist_ptr = GRC::AutoGreylist::GetAutoGreylistCache();

    if (refresh_greylist) {
        greylist_ptr->Refresh();
    }

    ProjectList projects;

    // This is the override for automatic greylisting. If the AutoGreylist class refresh determines
    // that the project meets greylisting criteria, it will be in the AutoGreylist object pointed to
    // by the greylist_ptr. This will override the whitelist entries from the project whitelist registry.
    for (auto iter : m_project_entries) {
        if ((iter.second->m_status == ProjectEntryStatus::ACTIVE || iter.second->m_status == ProjectEntryStatus::MAN_GREYLISTED)
            && greylist_ptr->Contains(iter.first)) {
            iter.second->m_status = ProjectEntryStatus::AUTO_GREYLISTED;
        }

        switch (filter) {
        case ProjectEntry::ProjectFilterFlag::ACTIVE:
            if (iter.second->m_status == ProjectEntryStatus::ACTIVE) {
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
                || iter.second->m_status == ProjectEntryStatus::AUTO_GREYLISTED) {
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
    m_project_db.clear();
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

    // Is there an existing project entry in the map?
    bool current_project_entry_present = (project_entry_pair_iter != m_project_entries.end());

    // If so, then get a smart pointer to it.
    if (current_project_entry_present) {
        current_project_entry_ptr = project_entry_pair_iter->second;

        // Set the payload m_entry's prev entry ctx hash = to the existing entry's hash.
        payload.m_previous_hash = current_project_entry_ptr->m_hash;
    } else { // Original entry for this project entry key
        payload.m_previous_hash = uint256 {};
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

    int height = m_project_db.Initialize(m_project_entries, m_pending_project_entries, m_expired_project_entries);

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
    m_project_db.clear_in_memory_only();
}

uint64_t Whitelist::PassivateDB()
{
    LOCK(cs_lock);

    return m_project_db.passivate_db();
}

Whitelist::ProjectEntryDB &Whitelist::GetProjectDB()
{
    return m_project_db;
}

template<> const std::string Whitelist::ProjectEntryDB::KeyType()
{
    return std::string("project");
}
