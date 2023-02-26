// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "scraper_registry.h"
#include "wallet/wallet.h"

using namespace GRC;
using LogFlags = BCLog::LogFlags;

extern int64_t g_v11_timestamp;

namespace {
ScraperRegistry g_scrapers;
} // anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

ScraperRegistry& GRC::GetScraperRegistry()
{
    return g_scrapers;
}

// -----------------------------------------------------------------------------
// Class: ScraperEntry
// -----------------------------------------------------------------------------

ScraperEntry::ScraperEntry()
    : m_keyid()
    , m_timestamp(0)
    , m_hash()
    , m_previous_hash()
    , m_status(ScraperEntryStatus::UNKNOWN)
{
}

ScraperEntry::ScraperEntry(CKeyID key_id, Status status)
    : ScraperEntry(std::move(key_id), std::move(status), 0, uint256 {})
{
}

ScraperEntry::ScraperEntry(CKeyID key_id, Status status, int64_t tx_timestamp, uint256 hash)
    : m_keyid(key_id)
    , m_timestamp(tx_timestamp)
    , m_hash(hash)
    , m_previous_hash()
    , m_status(status)
{
}

bool ScraperEntry::WellFormed() const
{
    return (CBitcoinAddress(m_keyid).IsValid()
            && m_status != ScraperEntryStatus::UNKNOWN
            && m_status != ScraperEntryStatus::OUT_OF_BOUND);
}

CKeyID ScraperEntry::GetId() const
{
    return m_keyid;
}

CBitcoinAddress ScraperEntry::GetAddress() const
{
    return CBitcoinAddress(m_keyid);
}

std::string ScraperEntry::ScraperStatusToString() const
{
    return ScraperStatusToString(m_status.Value());
}

std::string ScraperEntry::ScraperStatusToString(const ScraperEntryStatus& status, const bool& translated) const
{
    if (translated) {
        switch(status) {
        case ScraperEntryStatus::UNKNOWN:         return _("Unknown");
        case ScraperEntryStatus::DELETED:         return _("Deleted");
        case ScraperEntryStatus::NOT_AUTHORIZED:  return _("Not authorized");
        case ScraperEntryStatus::AUTHORIZED:      return _("Authorized");
        case ScraperEntryStatus::EXPLORER:        return _("Explorer");
        case ScraperEntryStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case ScraperEntryStatus::UNKNOWN:         return "unknown";
        case ScraperEntryStatus::DELETED:         return "deleted";
        case ScraperEntryStatus::NOT_AUTHORIZED:  return "not_authorized";
        case ScraperEntryStatus::AUTHORIZED:      return "authorized";
        case ScraperEntryStatus::EXPLORER:        return "explorer";
        case ScraperEntryStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    }

    // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
    // from some compiler versions.
    return std::string{};
}

bool ScraperEntry::WalletHasPrivateKey(const CWallet* const wallet) const
{
    LOCK(wallet->cs_wallet);

    return wallet->HaveKey(m_keyid);
}

AppCacheEntryExt ScraperEntry::GetLegacyScraperEntry()
{
    AppCacheEntryExt entry;

    entry.value = CBitcoinAddress(m_keyid).ToString();
    entry.timestamp = m_timestamp;
    entry.deleted = (m_status == ScraperEntryStatus::DELETED);

    return entry;
}

bool ScraperEntry::operator==(ScraperEntry b)
{
    bool result = true;

    result &= (m_keyid == b.m_keyid);
    result &= (m_timestamp == b.m_timestamp);
    result &= (m_hash == b.m_hash);
    result &= (m_previous_hash == b.m_previous_hash);
    result &= (m_status == b.m_status);

    return result;
}

bool ScraperEntry::operator!=(ScraperEntry b)
{
    return !(*this == b);
}

// -----------------------------------------------------------------------------
// Class: ScraperEntryPayload
// -----------------------------------------------------------------------------

constexpr uint32_t ScraperEntryPayload::CURRENT_VERSION; // For clang

ScraperEntryPayload::ScraperEntryPayload()
{
}

ScraperEntryPayload::ScraperEntryPayload(const uint32_t version, CKeyID key_id, ScraperEntryStatus status)
    : LegacyPayload()
    , m_version(version)
    , m_scraper_entry(ScraperEntry(key_id, status))
{
}

ScraperEntryPayload::ScraperEntryPayload(const uint32_t version, ScraperEntry scraper_entry)
    : LegacyPayload()
    , m_version(version)
    , m_scraper_entry(std::move(scraper_entry))
{
}

ScraperEntryPayload::ScraperEntryPayload(ScraperEntry scraper_entry)
    : ScraperEntryPayload(CURRENT_VERSION, std::move(scraper_entry))
{
}

ScraperEntryPayload::ScraperEntryPayload(const std::string& key, const std::string& value)
    : LegacyPayload(key, value)
{
    // Ensure m_version = 1. It should be already from the default value of m_version in the class definition.
    m_version = 1;

    CBitcoinAddress address;

    address.SetString(m_key);

    if (!address.IsValid()) {
        error("%s: Error during initialization of ScraperEntryPayload from legacy format: key = %s, value = %s",
              __func__,
              key,
              value);
        return;
    }

    address.GetKeyID(m_scraper_entry.m_keyid);

    if (ToLower(m_value) == "true") {
        m_scraper_entry.m_status = ScraperEntryStatus::AUTHORIZED;
    } else {
        // any other value than "true" in legacy scraper contract is interpreted as NOT_AUTHORIZED.
        m_scraper_entry.m_status = ScraperEntryStatus::NOT_AUTHORIZED;
    }
}

ScraperEntryPayload ScraperEntryPayload::Parse(const std::string& key, const std::string& value)
{
    CBitcoinAddress address;

    address.SetString(key);

    if (!address.IsValid()) {
        return ScraperEntryPayload();
    }

    CKeyID key_id;
    address.GetKeyID(key_id);

    if (ToLower(value) == "true") {
        return ScraperEntryPayload(1, key_id, ScraperEntryStatus::AUTHORIZED);
    }

    // any other value than "true" in legacy scraper contract is interpreted as NOT_AUTHORIZED.
    return ScraperEntryPayload(1, key_id, ScraperEntryStatus::NOT_AUTHORIZED);
}

// -----------------------------------------------------------------------------
// Class: ScraperRegistry
// -----------------------------------------------------------------------------
const ScraperRegistry::ScraperMap& ScraperRegistry::Scrapers() const
{
    return m_scrapers;
}

const AppCacheSection ScraperRegistry::GetScrapersLegacy() const
{
    AppCacheSection scrapers;

    // Only includes authorized scrapers.
    for (const auto& iter : GetScrapersLegacyExt(true)) {
        AppCacheEntry entry;

        entry.timestamp = iter.second.timestamp;
        entry.value = iter.second.value;

        scrapers[iter.first] = entry;
    }

    return scrapers;
}

const AppCacheSectionExt ScraperRegistry::GetScrapersLegacyExt(const bool& authorized_only) const
{
    AppCacheSectionExt scrapers_ext;

    LOCK(cs_lock);

    for (const auto& entry : m_scrapers) {

        std::string key = CBitcoinAddress(entry.first).ToString();

        switch (entry.second->m_status.Value()) {
        case ScraperEntryStatus::DELETED:
            // Mark entry in scrapers_ext as deleted at the timestamp of the deletion. The value is changed
            // to false, because if it is deleted, it is also not authorized.
            if (!authorized_only) {
                scrapers_ext[key] = AppCacheEntryExt {"false", entry.second->m_timestamp, true};
            }
            break;

        case ScraperEntryStatus::NOT_AUTHORIZED:
            scrapers_ext[key] = AppCacheEntryExt {"false", entry.second->m_timestamp, false};
            break;

        case ScraperEntryStatus::AUTHORIZED:
            [[fallthrough]];
            // For the legacy AppCacheEntryExt, this case really doesn't exist, but treat the same as AUTHORIZED.
        case ScraperEntryStatus::EXPLORER:
            scrapers_ext[key] = AppCacheEntryExt {"true", entry.second->m_timestamp, false};
            break;

            // Ignore UNKNOWN and OUT_OF_BOUND.
        case ScraperEntryStatus::UNKNOWN:
        case ScraperEntryStatus::OUT_OF_BOUND:
            break;
        }
    }

    return scrapers_ext;
}

ScraperEntryOption ScraperRegistry::Try(const CKeyID& key_id) const
{
    LOCK(cs_lock);

    const auto iter = m_scrapers.find(key_id);

    if (iter == m_scrapers.end()) {
        return nullptr;
    }

    return iter->second;
}

ScraperEntryOption ScraperRegistry::TryAuthorized(const CKeyID& key_id) const
{
    LOCK(cs_lock);

    if (const ScraperEntryOption scraper_entry = Try(key_id)) {
        if (scraper_entry->m_status == ScraperEntryStatus::AUTHORIZED
                || scraper_entry->m_status == ScraperEntryStatus::EXPLORER) {
            return scraper_entry;
        }
    }

    return nullptr;
}

void ScraperRegistry::Reset()
{
    LOCK(cs_lock);

    m_scrapers.clear();
    m_scraper_db.clear();
}

void ScraperRegistry::AddDelete(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the LevelDB database
    int height = -1;

    if (ctx.m_pindex)
    {
        height = ctx.m_pindex->nHeight;
    }

    ScraperEntryPayload payload = ctx->CopyPayloadAs<ScraperEntryPayload>();

    // If the payload m_version is less than two, then the payload is initialized from the legacy key value. Therefore
    // we must fill in the hash and timestamp from the transaction context, and also mark the status deleted if the
    // contract action was REMOVE, because that action resulted in an implicit deletion with the legacy K-V entries and
    // the appcache. The new status makes it explicit.
    if (payload.m_version < 2) {
        payload.m_scraper_entry.m_hash = ctx.m_tx.GetHash();
        payload.m_scraper_entry.m_timestamp = ctx.m_tx.nTime;

        if (ctx->m_action == ContractAction::REMOVE) {
            payload.m_scraper_entry.m_status = ScraperEntryStatus::DELETED;
        }
    }

    LOCK(cs_lock);

    auto scraper_entry_pair_iter = m_scrapers.find(payload.m_scraper_entry.m_keyid);

    ScraperEntry_ptr current_scraper_entry_ptr = nullptr;

    // Make sure the payload m_scraper has the correct time and transaction hash.
    //payload.m_scraper_entry.m_timestamp = ctx.m_tx.nTime;
    //payload.m_scraper_entry.m_hash = ctx.m_tx.GetHash();

    // Is there an existing scraper entry in the map?
    bool current_scraper_entry_present = (scraper_entry_pair_iter != m_scrapers.end());

    // If so, then get a smart pointer to it.
    if (current_scraper_entry_present) {
        current_scraper_entry_ptr = scraper_entry_pair_iter->second;

        // Set the payload m_scraper_entry's prev scraper entry ctx hash = to the existing scraper entry's hash.
        payload.m_scraper_entry.m_previous_hash = current_scraper_entry_ptr->m_hash;
    } else { // Original entry for this scraper keyid
        payload.m_scraper_entry.m_previous_hash = uint256 {};
    }

    CBitcoinAddress address;
    address.Set(payload.m_scraper_entry.m_keyid);

    LogPrint(LogFlags::SCRAPER, "INFO: %s: scraper entry add/delete: contract m_version = %u, payload "
                                "m_version = %u, address for m_keyid = %s, m_timestamp = %" PRId64 ", "
                                "m_hash = %s, m_previous_hash = %s, m_status = %i",
             __func__,
             ctx->m_version,
             payload.m_version,
             address.ToString(),
             payload.m_scraper_entry.m_timestamp,
             payload.m_scraper_entry.m_hash.ToString(),
             payload.m_scraper_entry.m_previous_hash.ToString(),
             payload.m_scraper_entry.m_status.Raw()
             );

    ScraperEntry& historical = payload.m_scraper_entry;

    if (!m_scraper_db.insert(ctx.m_tx.GetHash(), height, historical))
    {
        LogPrint(LogFlags::SCRAPER, "INFO: %s: In recording of the scraper entry for address %s, hash %s, the scraper entry "
                                    "db record already exists. This can be expected on a restart of the wallet to ensure "
                                    "multiple contracts in the same block get stored/replayed.",
                 __func__,
                 historical.GetAddress().ToString(),
                 historical.m_hash.GetHex());
    }

    // Finally, insert the new scraper entry (payload) smart pointer into the m_scrapers map.
    m_scrapers[payload.m_scraper_entry.m_keyid] = m_scraper_db.find(ctx.m_tx.GetHash())->second;

    return;
}

void ScraperRegistry::Add(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void ScraperRegistry::Delete(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void ScraperRegistry::Revert(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<ScraperEntryPayload>();

    // For scraper entries, both adds and removes will have records to revert in the m_scrapers map,
    // and also, if not the first entry for that scraper keyid, will have a historical record to
    // resurrect.
    LOCK(cs_lock);

    auto entry_to_revert = m_scrapers.find(payload->m_scraper_entry.m_keyid);

    if (entry_to_revert == m_scrapers.end()) {
        error("%s: The scraper entry for address %s to revert was not found in the scraper entry map.",
              __func__,
              entry_to_revert->second->GetAddress().ToString());

        // If there is no record in the current m_scrapers map, then there is nothing to do here. This
        // should not occur.
        return;
    }

    CBitcoinAddress address = entry_to_revert->second->GetAddress();

    // If this is not a null hash, then there will be a prior entry to resurrect.
    uint256 resurrect_hash = entry_to_revert->second->m_previous_hash;

    // Revert the ADD or REMOVE action. Unlike the beacons, this is symmetric.
    if (ctx->m_action == ContractAction::ADD || ctx->m_action == ContractAction::REMOVE) {
        // Erase the record from m_scrapers.
        if (m_scrapers.erase(payload->m_scraper_entry.m_keyid) == 0) {
            error("%s: The scraper entry to erase during a scraper entry revert for address %s was not found.",
                  __func__,
                  address.ToString());
            // If the record to revert is not found in the m_scrapers map, no point in continuing.
            return;
        }

        // Also erase the record from the db.
        if (!m_scraper_db.erase(ctx.m_tx.GetHash())) {
            error("%s: The db entry to erase during a scraper entry revert for address %s was not found.",
                  __func__,
                  address.ToString());

            // Unlike the above we will keep going even if this record is not found, because it is identical to the
            // m_scrapers record above. This should not happen, because during contract adds and removes, entries are
            // made simultaneously to be the m_scrapers and m_scraper_db.
        }

        if (resurrect_hash.IsNull()) {
            return;
        }

        auto resurrect_entry = m_scraper_db.find(resurrect_hash);

        if (resurrect_entry == m_scraper_db.end()) {
            error("%s: The prior entry to resurrect during a scraper entry ADD revert for address %s was not found.",
                  __func__,
                  address.ToString());
            return;
        }

        // Resurrect the entry prior to the reverted one. It is safe to use the bracket form here, because of the protection
        // of the logic above. There cannot be any entry in m_scrapers with that keyid value left if we made it here.
        m_scrapers[resurrect_entry->second->m_keyid] = resurrect_entry->second;
    }
}

bool ScraperRegistry::Validate(const Contract& contract, const CTransaction& tx, int &DoS) const
{
    if (contract.m_version < 1) {
        return true;
    }

    const auto payload = contract.SharePayloadAs<ScraperEntryPayload>();

    // TODO review if this is correct for scraper entries.
    if (contract.m_version >= 3 && payload->m_version < 2) {
        DoS = 25;
        error("%s: Legacy scraper contract in contract v3", __func__);
        return false;
    }

    if (!payload->WellFormed(contract.m_action.Value())) {
        DoS = 25;
        error("%s: Malformed scraper contract", __func__);
        return false;
    }

    return true;
}

bool ScraperRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

int ScraperRegistry::Initialize()
{
    LOCK(cs_lock);

    int height = m_scraper_db.Initialize(m_scrapers);

    LogPrint(LogFlags::SCRAPER, "INFO %s: m_scraper_db size after load: %u", __func__, m_scraper_db.size());
    LogPrint(LogFlags::SCRAPER, "INFO %s: m_scrapers size after load: %u", __func__, m_scrapers.size());

    return height;
}

void ScraperRegistry::SetDBHeight(int& height)
{
    LOCK(cs_lock);

    m_scraper_db.StoreDBHeight(height);
}

int ScraperRegistry::GetDBHeight()
{
    int height = 0;

    LOCK(cs_lock);

    m_scraper_db.LoadDBHeight(height);

    return height;
}

void ScraperRegistry::ResetInMemoryOnly()
{
    LOCK(cs_lock);

    m_scrapers.clear();
    m_scraper_db.clear_in_memory_only();
}

uint64_t ScraperRegistry::PassivateDB()
{
    LOCK(cs_lock);

    return m_scraper_db.passivate_db();
}

// This is static and called by the scheduler.
void ScraperRegistry::RunScraperDBPassivation()
{
    TRY_LOCK(cs_main, locked_main);

    if (!locked_main)
    {
        return;
    }

    ScraperRegistry& scraper_entries = GetScraperRegistry();

    scraper_entries.PassivateDB();
}


// -----------------------------------------------------------------------------
// Class: ScraperRegistry::ScraperEntryDB
// -----------------------------------------------------------------------------
// Required to make the linker happy.
constexpr uint32_t ScraperRegistry::ScraperEntryDB::CURRENT_VERSION;

int ScraperRegistry::ScraperEntryDB::Initialize(ScraperMap& scrapers)
{
    bool status = true;
    int height = 0;
    uint32_t version = 0;

    // First load the scraper db version from LevelDB and check it against the constant in the class.
    {
        CTxDB txdb("r");

        std::pair<std::string, std::string> key = std::make_pair("scraper_db", "version");

        bool status = txdb.ReadGenericSerializable(key, version);

        if (!status) version = 0;
    }

    if (version != CURRENT_VERSION) {
        LogPrint(LogFlags::SCRAPER, "WARNING: %s: Version level of the scraper entry db stored in LevelDB, %u, does not "
                                    "match that required in this code level, version %u. Clearing the LevelDB scraper entry "
                                    "storage and setting version level to match this code level.",
                 __func__,
                 version,
                 CURRENT_VERSION);

        clear_leveldb();

        LogPrint(LogFlags::SCRAPER, "INFO: %s: LevelDB scraper area cleared. Version level set to %u.",
                 __func__,
                 CURRENT_VERSION);
    }

    // If LoadDBHeight not successful or height is zero then LevelDB has not been initialized before.
    // LoadDBHeight will also set the private member variable m_height_stored from LevelDB for this first call.
    if (!LoadDBHeight(height) || !height) {
        return height;
    } else { // LevelDB already initialized from a prior run.

        // Set m_database_init to true. This will cause LoadDBHeight hereinafter to simply report
        // the value of m_height_stored rather than loading the stored height from LevelDB.
        m_database_init = true;
    }

    LogPrint(LogFlags::SCRAPER, "INFO: %s: db stored height at block %i.",
             __func__,
             height);

    // Now load the scraper entries from LevelDB.

    std::string key_type = "scraper";

    // This temporary map is keyed by record number, which insures the replay down below occurs in the right order.
    StorageScraperMapByRecordNum storage_by_record_num;

    // Code block to scope the txdb object.
    {
        CTxDB txdb("r");

        uint256 hash_hint = uint256();

        // Load the temporary which is similar to m_historical, except the key is by record number not hash.
        status = txdb.ReadGenericSerializablesToMapWithForeignKey(key_type, storage_by_record_num, hash_hint);
    }

    if (!status) {
        if (height > 0){
            // For the height be greater than zero from the height K-V, but the read into the map to fail
            // means the storage in LevelDB must be messed up in the scraper area and not be in concordance with
            // the scraper_db K-V's. Therefore clear the whole thing.
            clear();
        }

        // Return height of zero.
        return 0;
    }

    uint64_t recnum_high_watermark = 0;
    uint64_t number_passivated = 0;

    for (const auto& iter : storage_by_record_num) {
        const uint64_t& recnum = iter.first;
        const ScraperEntry& scraper_entry = iter.second;

        recnum_high_watermark = std::max(recnum_high_watermark, recnum);

        LogPrint(LogFlags::SCRAPER, "INFO: %s: scraper entry m_historical insert: address %s, timestamp %" PRId64 ", hash %s, "
                                    "previous_hash %s, scraper entry status = %u, recnum = %" PRId64 ".",
                 __func__,
                 scraper_entry.GetAddress().ToString(), // address
                 scraper_entry.m_timestamp, // timestamp
                 scraper_entry.m_hash.GetHex(), // transaction hash
                 scraper_entry.m_previous_hash.GetHex(), // prev scraper entry transaction hash
                 scraper_entry.m_status.Raw(), // status
                 recnum
                 );

        // Insert the entry into the historical map.
        m_historical[iter.second.m_hash] = std::make_shared<ScraperEntry>(scraper_entry);
        ScraperEntry_ptr& historical_scraper_entry_ptr = m_historical[iter.second.m_hash];

        ScraperRegistry::HistoricalScraperMap::iterator prev_historical_iter = m_historical.end();

        // prev_historical_iter here is for purposes of passivation later. If insertion of records by recnum results in a
        // second or succeeding record for the same key, then m_previous_hash will not be null. If the prior record
        // pointed to by that hash is found, then it can be removed from memory, since only the current record by recnum
        // needs to be retained.
        if (!historical_scraper_entry_ptr->m_previous_hash.IsNull()) {
            prev_historical_iter = m_historical.find(historical_scraper_entry_ptr->m_previous_hash);
        }

        // The unknown or out of bound status conditions should have never made it into leveldb to begin with, since
        // the scraper entry contract will fail validation, but to be thorough, include the filter condition anyway.
        // Unlike beacons, this is a straight replay.
        if (scraper_entry.m_status != ScraperEntryStatus::UNKNOWN && scraper_entry.m_status != ScraperEntryStatus::OUT_OF_BOUND) {
            LogPrint(LogFlags::SCRAPER, "INFO: %s: m_scrapers insert: address %s; timestamp %" PRId64 "; hash %s; "
                                        "previous_hash %s; status = %u - (1) DELETED, (2) NOT_AUTHORIZED, (3) AUTHORIZED, "
                                        "(4) EXPLORER; recnum = %" PRId64 ".",
                     __func__,
                     scraper_entry.GetAddress().ToString(), // address
                     scraper_entry.m_timestamp, // timestamp
                     scraper_entry.m_hash.GetHex(), // transaction hash
                     scraper_entry.m_previous_hash.GetHex(), // prev scraper entry transaction hash
                     scraper_entry.m_status.Raw(), // status
                     recnum
                     );

            // Insert or replace the existing map entry for the cpid with the latest active or renewed for that CPID.
            scrapers[scraper_entry.m_keyid] = historical_scraper_entry_ptr;
        }

        if (prev_historical_iter != m_historical.end()) {
            // Note that passivation is not expected to be successful for every call. See the comments
            // in the passivate() function.
            std::pair<ScraperRegistry::HistoricalScraperMap::iterator, bool> passivation_result
                    = passivate(prev_historical_iter);

            number_passivated += passivation_result.second;
        }
    } // storage_by_record_num iteration

    LogPrint(LogFlags::SCRAPER, "INFO: %s: number of historical records passivated: %" PRId64 ".",
             __func__,
             number_passivated);

    // Set the in-memory record number stored variable to the highest recnum encountered during the replay above.
    m_recnum_stored = recnum_high_watermark;

    // Set the needs passivation flag to true, because the one-by-one passivation done above may not catch everything.
    m_needs_passivation = true;

    return height;
}


void ScraperRegistry::ScraperEntryDB::clear_in_memory_only()
{
    m_historical.clear();
    m_database_init = false;
    m_height_stored = 0;
    m_recnum_stored = 0;
    m_needs_passivation = false;
}

bool ScraperRegistry::ScraperEntryDB::clear_leveldb()
{
    bool status = true;

    CTxDB txdb("rw");

    std::string key_type = "scraper";
    uint256 start_key_hint_scraper = uint256();

    status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint_scraper);

    key_type = "scraper_db";
    std::string start_key_hint_scraper_db {};

    status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint_scraper_db);

    // We want to write back into LevelDB the revision level of the db in the running code.
    std::pair<std::string, std::string> key = std::make_pair(key_type, "version");
    status &= txdb.WriteGenericSerializable(key, CURRENT_VERSION);

    m_height_stored = 0;
    m_recnum_stored = 0;
    m_database_init = false;
    m_needs_passivation = false;

    return status;
}

uint64_t ScraperRegistry::ScraperEntryDB::passivate_db()
{
    uint64_t number_passivated = 0;

    // Don't bother to go through the historical scraper entry map unless the needs passivation flag is set. This makes
    // this function extremely light for most calls from the periodic schedule.
    if (m_needs_passivation) {
        for (auto iter = m_historical.begin(); iter != m_historical.end(); /*no-op*/) {
            // The passivate function increments the iterator.
            std::pair<ScraperRegistry::HistoricalScraperMap::iterator, bool> result = passivate(iter);

            iter = result.first;
            number_passivated += result.second;

        }
    }

    LogPrint(BCLog::LogFlags::SCRAPER, "INFO %s: Passivated %" PRId64 " elements from scraper db.",
             __func__,
             number_passivated);

    // Set needs passivation flag to false after passivating the db.
    m_needs_passivation = false;

    return number_passivated;
}

bool ScraperRegistry::ScraperEntryDB::clear()
{
    clear_in_memory_only();

    return clear_leveldb();
}

size_t ScraperRegistry::ScraperEntryDB::size()
{
    return m_historical.size();
}

bool ScraperRegistry::ScraperEntryDB::StoreDBHeight(const int& height_stored)
{
    // Update the in-memory bookmark variable.
    m_height_stored = height_stored;

    // Update LevelDB.
    CTxDB txdb("rw");

    std::pair<std::string, std::string> key = std::make_pair("scraper_db", "height_stored");

    return txdb.WriteGenericSerializable(key, height_stored);
}

bool ScraperRegistry::ScraperEntryDB::LoadDBHeight(int& height_stored)
{
    bool status = true;

    // If the database has already been initialized (which includes loading the height to what the
    // scraper entry storage was updated), then just report the value of m_height_stored, otherwise
    // pull the value from LevelDB.
    if (m_database_init) {
        height_stored = m_height_stored;
    } else {
        CTxDB txdb("r");

        std::pair<std::string, std::string> key = std::make_pair("scraper_db", "height_stored");

        bool status = txdb.ReadGenericSerializable(key, height_stored);

        if (!status) height_stored = 0;

        m_height_stored = height_stored;
    }

    return status;
}

bool ScraperRegistry::ScraperEntryDB::insert(const uint256 &hash, const int& height, const ScraperEntry& scraper_entry)
{
    bool status = false;

    if (m_historical.find(hash) != m_historical.end()) {
        return status;
    } else {
        LogPrint(LogFlags::SCRAPER, "INFO %s - store scraper entry: address %s, height %i, timestamp %" PRId64
                 ", hash %s, previous_hash %s, status = %u.",
                 __func__,
                 scraper_entry.GetAddress().ToString(), // address
                 height, // height
                 scraper_entry.m_timestamp, // timestamp
                 scraper_entry.m_hash.GetHex(), // transaction hash
                 scraper_entry.m_previous_hash.GetHex(), // prev scraper entry transaction hash
                 scraper_entry.m_status.Raw() // status
                 );

        m_historical.insert(std::make_pair(hash, std::make_shared<ScraperEntry>(scraper_entry)));

        status = Store(hash, scraper_entry);

        if (height) {
            status &= StoreDBHeight(height);
        }

        // Set needs passivation flag to true to allow the scheduled passivation to remove unnecessary records from
        // memory.
        m_needs_passivation = true;

        return status;
    }
}

bool ScraperRegistry::ScraperEntryDB::erase(const uint256& hash)
{
    auto iter = m_historical.find(hash);

    if (iter != m_historical.end()) {
        m_historical.erase(hash);
    }

    return Delete(hash);
}

// Note that this function uses the shared pointer use_count() to determine whether an element in
// m_historical is referenced by either the m_scraper map and if not, erases it, leaving the backing
// state in LevelDB untouched. Note that the use of use_count() in multithreaded environments must be carefully
// considered because it is only approximate. In this case it is exact. Access to the entire ScraperRegistry class
// and everything in it is protected by the cs_main lock and is therefore single threaded. This method of passivating
// is MUCH faster than searching through m_scrapers for each element, because they are not keyed by hash.
//
// Note that this function acts very similarly to the map erase function with an iterator argument, but with a standard
// pair returned. The first part of the pair a boolean as to whether the element was passivated, and the
// second is an iterator to the next element. This is designed to be traversed in a for loop just like map erase.
std::pair<ScraperRegistry::HistoricalScraperMap::iterator, bool>
ScraperRegistry::ScraperEntryDB::passivate(ScraperRegistry::HistoricalScraperMap::iterator& iter)
{
    // m_historical itself holds one reference, additional references can be held by m_scraper.
    // If there is only one reference then remove the shared_pointer from m_historical, which will implicitly destroy
    // the shared_pointer object.
    if (iter->second.use_count() == 1) {
        iter = m_historical.erase(iter);
        return std::make_pair(iter, true);
    } else {
        LogPrint(BCLog::LogFlags::SCRAPER, "INFO: %s: Passivate called for historical scraper entry record with hash %s that "
                                           "has existing reference count %li. This is expected under certain situations.",
                 __func__,
                 iter->second->m_hash.GetHex(),
                 iter->second.use_count());

        ++iter;
        return std::make_pair(iter, false);
    }
}

ScraperRegistry::HistoricalScraperMap::iterator ScraperRegistry::ScraperEntryDB::begin()
{
    return m_historical.begin();
}

ScraperRegistry::HistoricalScraperMap::iterator ScraperRegistry::ScraperEntryDB::end()
{
    return m_historical.end();
}

ScraperRegistry::HistoricalScraperMap::iterator ScraperRegistry::ScraperEntryDB::find(const uint256& hash)
{
    // See if scraper entry from that ctx_hash is already in the historical map. If so, get iterator.
    auto iter = m_historical.find(hash);

    // If it isn't, attempt to load the scraper entry from LevelDB into the map.
    if (iter == m_historical.end()) {
        ScraperEntry scraper_entry;

        // If the load from LevelDB is successful, insert into the historical map and return the iterator.
        if (Load(hash, scraper_entry)) {
            iter = m_historical.insert(std::make_pair(hash, std::make_shared<ScraperEntry>(scraper_entry))).first;

            // Set the needs passivation flag to true
            m_needs_passivation = true;
        }
    }

    // Note that if there is no entry in m_historical, and also there is no K-V in LevelDB, then an
    // iterator at end() will be returned.
    return iter;
}

// TODO: A poor man's forward iterator. Implement a full wrapper iterator. Maybe don't need it though.
ScraperRegistry::HistoricalScraperMap::iterator ScraperRegistry::ScraperEntryDB::advance(HistoricalScraperMap::iterator iter)
{
    return ++iter;
}

bool ScraperRegistry::ScraperEntryDB::Store(const uint256& hash, const ScraperEntry& scraper_entry)
{
    CTxDB txdb("rw");

    ++m_recnum_stored;

    std::pair<std::string, uint256> key = std::make_pair("scraper", hash);

    return txdb.WriteGenericSerializable(key, std::make_pair(m_recnum_stored, scraper_entry));
}

bool ScraperRegistry::ScraperEntryDB::Load(const uint256& hash, ScraperEntry& scraper_entry)
{
    CTxDB txdb("r");

    std::pair<std::string, uint256> key = std::make_pair("scraper", hash);

    std::pair<uint64_t, ScraperEntry> scraper_entry_pair;

    bool status = txdb.ReadGenericSerializable(key, scraper_entry_pair);

    scraper_entry = scraper_entry_pair.second;

    return status;
}

bool ScraperRegistry::ScraperEntryDB::Delete(const uint256& hash)
{
    CTxDB txdb("rw");

    std::pair<std::string, uint256> key = std::make_pair("scraper", hash);

    return txdb.EraseGenericSerializable(key);
}

ScraperRegistry::ScraperEntryDB &ScraperRegistry::GetScraperDB()
{
    return m_scraper_db;
}
