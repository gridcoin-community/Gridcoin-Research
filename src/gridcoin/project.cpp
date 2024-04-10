// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/project.h"
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
        case ProjectEntryStatus::ACTIVE:          return _("Active");
        case ProjectEntryStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case ProjectEntryStatus::UNKNOWN:         return "Unknown";
        case ProjectEntryStatus::DELETED:         return "Deleted";
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

WhitelistSnapshot::WhitelistSnapshot(ProjectListPtr projects)
    : m_projects(std::move(projects))
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

    return WhitelistSnapshot(std::make_shared<ProjectList>(sorted));
}

// -----------------------------------------------------------------------------
// Class: Whitelist (Registry)
// -----------------------------------------------------------------------------

WhitelistSnapshot Whitelist::Snapshot() const
{
    LOCK(cs_lock);

    ProjectList projects;

    for (const auto& iter : m_project_entries) {
        if (iter.second->m_status == ProjectEntryStatus::ACTIVE) {
            projects.push_back(*iter.second);
        }
    }

    return WhitelistSnapshot(std::make_shared<ProjectList>(projects));
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
        payload.m_status = ProjectEntryStatus::ACTIVE;
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

    // Finally, insert the new project entry (payload) smart pointer into the m_project_entries map.
    m_project_entries[payload.m_name] = m_project_db.find(ctx.m_tx.GetHash())->second;

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

