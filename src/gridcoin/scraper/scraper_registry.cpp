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
    : m_key()
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
    : m_key(key_id)
    , m_timestamp(tx_timestamp)
    , m_hash(hash)
    , m_previous_hash()
    , m_status(status)
{
}

bool ScraperEntry::WellFormed() const
{
    return (CBitcoinAddress(m_key).IsValid()
            && m_status != ScraperEntryStatus::UNKNOWN
            && m_status != ScraperEntryStatus::OUT_OF_BOUND);
}

CKeyID ScraperEntry::Key() const
{
    return m_key;
}

std::pair<std::string, std::string> ScraperEntry::KeyValueToString() const
{
    return std::make_pair(CBitcoinAddress(m_key).ToString(), StatusToString());
}

CKeyID ScraperEntry::GetId() const
{
    return m_key;
}

CBitcoinAddress ScraperEntry::GetAddress() const
{
    return CBitcoinAddress(m_key);
}

std::string ScraperEntry::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string ScraperEntry::StatusToString(const ScraperEntryStatus& status, const bool& translated) const
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

    return wallet->HaveKey(m_key);
}

AppCacheEntryExt ScraperEntry::GetLegacyScraperEntry()
{
    AppCacheEntryExt entry;

    entry.value = CBitcoinAddress(m_key).ToString();
    entry.timestamp = m_timestamp;
    entry.deleted = (m_status == ScraperEntryStatus::DELETED);

    return entry;
}

bool ScraperEntry::operator==(ScraperEntry b)
{
    bool result = true;

    result &= (m_key == b.m_key);
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

ScraperEntryPayload::ScraperEntryPayload(const uint32_t version)
    : m_version(version)
{
}

ScraperEntryPayload::ScraperEntryPayload(const uint32_t version, CKeyID key_id, ScraperEntryStatus status)
    : LegacyPayload()
    , m_version(version)
    , m_scraper_entry(ScraperEntry(key_id, status))
{
    assert(version > 1);
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

    address.GetKeyID(m_scraper_entry.m_key);

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
    ScraperEntryStatus scraper_entry_status = ScraperEntryStatus::UNKNOWN;

    if (ToLower(value) == "true") {
        scraper_entry_status = ScraperEntryStatus::AUTHORIZED;
    } else {
        // any other value than "true" in legacy scraper contract is interpreted as NOT_AUTHORIZED.
        scraper_entry_status = ScraperEntryStatus::NOT_AUTHORIZED;
    }

    ScraperEntryPayload payload(1, ScraperEntry(key_id, scraper_entry_status));
    // The above constructor doesn't carry over the legacy K-V which we need.
    payload.m_key = key;
    payload.m_value = value;

     return payload;
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
            [[fallthrough]];
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

    // Fill in the hash and time from the transaction context, because this is not done during payload initialization.
    payload.m_scraper_entry.m_hash = ctx.m_tx.GetHash();
    payload.m_scraper_entry.m_timestamp = ctx.m_tx.nTime;

    // If the contract action is to remove a scraper entry, then the record added must have a status of deleted,
    // regardless of what was specified in the status.
    if (ctx->m_action == ContractAction::REMOVE) {
        payload.m_scraper_entry.m_status = ScraperEntryStatus::DELETED;
    }

    LOCK(cs_lock);

    auto scraper_entry_pair_iter = m_scrapers.find(payload.m_scraper_entry.m_key);

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
    address.Set(payload.m_scraper_entry.m_key);

    LogPrint(LogFlags::SCRAPER, "INFO: %s: scraper entry add/delete: contract m_version = %u, payload "
                                "m_version = %u, address for m_key = %s, m_timestamp = %" PRId64 ", "
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
    m_scrapers[payload.m_scraper_entry.m_key] = m_scraper_db.find(ctx.m_tx.GetHash())->second;

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

    auto entry_to_revert = m_scrapers.find(payload->m_scraper_entry.m_key);

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
        if (m_scrapers.erase(payload->m_scraper_entry.m_key) == 0) {
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
        m_scrapers[resurrect_entry->second->m_key] = resurrect_entry->second;
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

    int height = m_scraper_db.Initialize(m_scrapers, m_pending_scrapers, m_expired_scraper_entries);

    LogPrint(LogFlags::SCRAPER, "INFO: %s: m_scraper_db size after load: %u", __func__, m_scraper_db.size());
    LogPrint(LogFlags::SCRAPER, "INFO: %s: m_scrapers size after load: %u", __func__, m_scrapers.size());

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

ScraperRegistry::ScraperEntryDB &ScraperRegistry::GetScraperDB()
{
    return m_scraper_db;
}

template<> const std::string ScraperRegistry::ScraperEntryDB::KeyType()
{
    return std::string("scraper");
}
