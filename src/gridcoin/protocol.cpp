// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/protocol.h"
#include "node/ui_interface.h"

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
ProtocolRegistry g_protocol_entries;
} // anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

ProtocolRegistry& GRC::GetProtocolRegistry()
{
    return g_protocol_entries;
}

// -----------------------------------------------------------------------------
// Class: ProtocolEntry
// -----------------------------------------------------------------------------

ProtocolEntry::ProtocolEntry()
    : m_key()
    , m_value()
    , m_timestamp(0)
    , m_hash()
    , m_previous_hash()
    , m_status(ProtocolEntryStatus::UNKNOWN)
{
}

ProtocolEntry::ProtocolEntry(std::string key, std::string value)
    : ProtocolEntry(key, value, ProtocolEntryStatus::ACTIVE)
{
}


ProtocolEntry::ProtocolEntry(std::string key, std::string value, Status status)
    : ProtocolEntry(std::move(key), std::move(value), std::move(status), 0, uint256 {})
{
}

ProtocolEntry::ProtocolEntry(std::string key, std::string value, Status status, int64_t tx_timestamp, uint256 hash)
    : m_key(key)
    , m_value(value)
    , m_timestamp(tx_timestamp)
    , m_hash(hash)
    , m_previous_hash()
    , m_status(status)
{
}

bool ProtocolEntry::WellFormed() const
{
    return (!m_key.empty()
            && !m_value.empty()
            && m_status != ProtocolEntryStatus::UNKNOWN
            && m_status != ProtocolEntryStatus::OUT_OF_BOUND);
}

std::string ProtocolEntry::Key() const
{
    return m_key;
}

std::pair<std::string, std::string> ProtocolEntry::KeyValueToString() const
{
    return std::make_pair(m_key, m_value);
}

std::string ProtocolEntry::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string ProtocolEntry::StatusToString(const ProtocolEntryStatus& status, const bool& translated) const
{
    if (translated) {
        switch(status) {
        case ProtocolEntryStatus::UNKNOWN:         return _("Unknown");
        case ProtocolEntryStatus::DELETED:         return _("Deleted");
        case ProtocolEntryStatus::ACTIVE:          return _("Active");
        case ProtocolEntryStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case ProtocolEntryStatus::UNKNOWN:         return "Unknown";
        case ProtocolEntryStatus::DELETED:         return "Deleted";
        case ProtocolEntryStatus::ACTIVE:          return "Active";
        case ProtocolEntryStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    }

    // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
    // from some compiler versions.
    return std::string{};
}

AppCacheEntryExt ProtocolEntry::GetLegacyProtocolEntry()
{
    AppCacheEntryExt entry;

    entry.value = m_value;
    entry.timestamp = m_timestamp;
    entry.deleted = (m_status == ProtocolEntryStatus::DELETED);

    return entry;
}

bool ProtocolEntry::operator==(ProtocolEntry b)
{
    bool result = true;

    result &= (m_key == b.m_key);
    result &= (m_value == m_value);
    result &= (m_timestamp == b.m_timestamp);
    result &= (m_hash == b.m_hash);
    result &= (m_previous_hash == b.m_previous_hash);
    result &= (m_status == b.m_status);

    return result;
}

bool ProtocolEntry::operator!=(ProtocolEntry b)
{
    return !(*this == b);
}

// -----------------------------------------------------------------------------
// Class: ProtocolEntryPayload
// -----------------------------------------------------------------------------

constexpr uint32_t ProtocolEntryPayload::CURRENT_VERSION; // For clang

ProtocolEntryPayload::ProtocolEntryPayload(uint32_t version)
    : LegacyPayload()
    , m_version(version)
{
}

ProtocolEntryPayload::ProtocolEntryPayload(const uint32_t version, std::string key, std::string value, ProtocolEntryStatus status)
    : LegacyPayload()
    , m_version(version)
    , m_entry(ProtocolEntry(key, value, status))
{
    assert(version > 1);
}

ProtocolEntryPayload::ProtocolEntryPayload(const uint32_t version, ProtocolEntry entry)
    : LegacyPayload()
    , m_version(version)
    , m_entry(std::move(entry))
{
}

ProtocolEntryPayload::ProtocolEntryPayload(ProtocolEntry entry)
    : ProtocolEntryPayload(CURRENT_VERSION, std::move(entry))
{
}

ProtocolEntryPayload::ProtocolEntryPayload(const std::string& key, const std::string& value)
    : LegacyPayload(key, value)
    , m_version(1)
    , m_entry(key, value)
{
}

ProtocolEntryPayload ProtocolEntryPayload::Parse(const std::string& key, const std::string& value)
{
    // This constructor assigns the entry with a status of active and also fills out the legacy payload.
    ProtocolEntryPayload payload(key, value);

     return payload;
}

// -----------------------------------------------------------------------------
// Class: ProtocolRegistry
// -----------------------------------------------------------------------------
const ProtocolRegistry::ProtocolEntryMap& ProtocolRegistry::ProtocolEntries() const
{
    return m_protocol_entries;
}

const AppCacheSection ProtocolRegistry::GetProtocolEntriesLegacy() const
{
    AppCacheSection protocol_entries;

    // Only includes active protocol entries.
    for (const auto& iter : GetProtocolEntriesLegacyExt(true)) {
        AppCacheEntry entry;

        entry.timestamp = iter.second.timestamp;
        entry.value = iter.second.value;

        protocol_entries[iter.first] = entry;
    }

    return protocol_entries;
}

const AppCacheSectionExt ProtocolRegistry::GetProtocolEntriesLegacyExt(const bool& active_only) const
{
    AppCacheSectionExt protocol_entries_ext;

    LOCK(cs_lock);

    for (const auto& entry : m_protocol_entries) {

        const std::string& key = entry.first;
        const std::string& value = entry.second->m_value;

        switch (entry.second->m_status.Value()) {
        case ProtocolEntryStatus::DELETED:
            // Mark entry in protocol_entries_ext as deleted at the timestamp of the deletion.
            if (!active_only) {
                protocol_entries_ext[key] = AppCacheEntryExt {value, entry.second->m_timestamp, true};
            }
            break;

        case ProtocolEntryStatus::ACTIVE:
            protocol_entries_ext[key] = AppCacheEntryExt {value, entry.second->m_timestamp, false};
            break;

            // Ignore UNKNOWN and OUT_OF_BOUND.
        case ProtocolEntryStatus::UNKNOWN:
            [[fallthrough]];
        case ProtocolEntryStatus::OUT_OF_BOUND:
            break;
        }
    }

    return protocol_entries_ext;
}

const AppCacheEntry ProtocolRegistry::GetProtocolEntryByKeyLegacy(std::string key) const
{
    AppCacheEntry entry;

    entry.value = std::string {};
    entry.timestamp = 0;

    LOCK(cs_lock);

    auto iter = m_protocol_entries.find(key);

    // Only want to return entries that exist and are active. If not return an empty AppCacheEntry.
    if (iter == m_protocol_entries.end() || iter->second->m_status != ProtocolEntryStatus::ACTIVE) {
        return entry;
    }

    entry.value = iter->second->m_value;
    entry.timestamp = iter->second->m_timestamp;

    return entry;
}

ProtocolEntryOption ProtocolRegistry::Try(const std::string& key) const
{
    LOCK(cs_lock);

    const auto iter = m_protocol_entries.find(key);

    if (iter == m_protocol_entries.end()) {
        return nullptr;
    }

    return iter->second;
}

ProtocolEntryOption ProtocolRegistry::TryActive(const std::string& key) const
{
    LOCK(cs_lock);

    if (const ProtocolEntryOption protocol_entry = Try(key)) {
        if (protocol_entry->m_status == ProtocolEntryStatus::ACTIVE) {
            return protocol_entry;
        }
    }

    return nullptr;
}

void ProtocolRegistry::Reset()
{
    LOCK(cs_lock);

    m_protocol_entries.clear();
    m_protocol_db.clear();
}

void ProtocolRegistry::AddDelete(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the LevelDB database
    int height = -1;

    if (ctx.m_pindex)
    {
        height = ctx.m_pindex->nHeight;
    }

    ProtocolEntryPayload payload = ctx->CopyPayloadAs<ProtocolEntryPayload>();

    // Fill this in from the transaction context because these are not done during payload
    // initialization.
    payload.m_entry.m_hash = ctx.m_tx.GetHash();
    payload.m_entry.m_timestamp = ctx.m_tx.nTime;

    // Ensure status is DELETED if the contract action was REMOVE, regardless of what was actually
    // specified.
    if (ctx->m_action == ContractAction::REMOVE) {
        payload.m_entry.m_status = ProtocolEntryStatus::DELETED;
    }

    LOCK(cs_lock);

    auto protocol_entry_pair_iter = m_protocol_entries.find(payload.m_entry.m_key);

    ProtocolEntry_ptr current_protocol_entry_ptr = nullptr;

    // Is there an existing protocol entry in the map?
    bool current_protocol_entry_present = (protocol_entry_pair_iter != m_protocol_entries.end());

    // If so, then get a smart pointer to it.
    if (current_protocol_entry_present) {
        current_protocol_entry_ptr = protocol_entry_pair_iter->second;

        // Set the payload m_entry's prev entry ctx hash = to the existing entry's hash.
        payload.m_entry.m_previous_hash = current_protocol_entry_ptr->m_hash;
    } else { // Original entry for this protocol entry key
        payload.m_entry.m_previous_hash = uint256 {};
    }

    LogPrint(LogFlags::CONTRACT, "INFO: %s: protocol entry add/delete: contract m_version = %u, payload "
                                "m_version = %u, key = %s, value = %s, m_timestamp = %" PRId64 ", "
                                "m_hash = %s, m_previous_hash = %s, m_status = %s",
             __func__,
             ctx->m_version,
             payload.m_version,
             payload.m_entry.m_key,
             payload.m_entry.m_value,
             payload.m_entry.m_timestamp,
             payload.m_entry.m_hash.ToString(),
             payload.m_entry.m_previous_hash.ToString(),
             payload.m_entry.StatusToString()
             );

    ProtocolEntry& historical = payload.m_entry;

    if (!m_protocol_db.insert(ctx.m_tx.GetHash(), height, historical))
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: In recording of the protocol entry for key %s, value %s, hash %s, "
                                    "the protocol entry db record already exists. This can be expected on a restart "
                                    "of the wallet to ensure multiple contracts in the same block get stored/replayed.",
                 __func__,
                 historical.m_key,
                 historical.m_value,
                 historical.m_hash.GetHex());
    }

    // Finally, insert the new protocol entry (payload) smart pointer into the m_protocol_entries map.
    m_protocol_entries[payload.m_entry.m_key] = m_protocol_db.find(ctx.m_tx.GetHash())->second;

    return;
}

void ProtocolRegistry::Add(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void ProtocolRegistry::Delete(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void ProtocolRegistry::Revert(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<ProtocolEntryPayload>();

    // For protocol entries, both adds and removes will have records to revert in the m_protocol_entries map,
    // and also, if not the first entry for that protocol key, will have a historical record to
    // resurrect.
    LOCK(cs_lock);

    auto entry_to_revert = m_protocol_entries.find(payload->m_entry.m_key);

    if (entry_to_revert == m_protocol_entries.end()) {
        error("%s: The protocol entry for key %s to revert was not found in the protocol entry map.",
              __func__,
              entry_to_revert->second->m_key);

        // If there is no record in the current m_protocol_entries map, then there is nothing to do here. This
        // should not occur.
        return;
    }

    // If this is not a null hash, then there will be a prior entry to resurrect.
    std::string key = entry_to_revert->second->m_key;
    uint256 resurrect_hash = entry_to_revert->second->m_previous_hash;

    // Revert the ADD or REMOVE action. Unlike the beacons, this is symmetric.
    if (ctx->m_action == ContractAction::ADD || ctx->m_action == ContractAction::REMOVE) {
        // Erase the record from m_protocol_entries.
        if (m_protocol_entries.erase(payload->m_entry.m_key) == 0) {
            error("%s: The protocol entry to erase during a protocol entry revert for key %s was not found.",
                  __func__,
                  key);
            // If the record to revert is not found in the m_protocol_entries map, no point in continuing.
            return;
        }

        // Also erase the record from the db.
        if (!m_protocol_db.erase(ctx.m_tx.GetHash())) {
            error("%s: The db entry to erase during a protocol entry revert for key %s was not found.",
                  __func__,
                  key);

            // Unlike the above we will keep going even if this record is not found, because it is identical to the
            // m_protocol_entries record above. This should not happen, because during contract adds and removes,
            // entries are made simultaneously to the m_protocol_entries and m_protocol_db.
        }

        if (resurrect_hash.IsNull()) {
            return;
        }

        auto resurrect_entry = m_protocol_db.find(resurrect_hash);

        if (resurrect_entry == m_protocol_db.end()) {
            error("%s: The prior entry to resurrect during a protocol entry ADD revert for key %s was not found.",
                  __func__,
                  key);
            return;
        }

        // Resurrect the entry prior to the reverted one. It is safe to use the bracket form here, because of the protection
        // of the logic above. There cannot be any entry in m_protocol_entries with that key value left if we made it here.
        m_protocol_entries[resurrect_entry->second->m_key] = resurrect_entry->second;
    }
}

bool ProtocolRegistry::Validate(const Contract& contract, const CTransaction& tx, int &DoS) const
{
    if (contract.m_version < 1) {
        return true;
    }

    const auto payload = contract.SharePayloadAs<ProtocolEntryPayload>();

    if (contract.m_version >= 3 && payload->m_version < 2) {
        DoS = 25;
        error("%s: Legacy protocol entry contract in contract v3", __func__);
        return false;
    }

    if (!payload->WellFormed(contract.m_action.Value())) {
        DoS = 25;
        error("%s: Malformed protocol entry contract", __func__);
        return false;
    }

    return true;
}

bool ProtocolRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

int ProtocolRegistry::Initialize()
{
    LOCK(cs_lock);

    int height = m_protocol_db.Initialize(m_protocol_entries, m_pending_protocol_entries, m_expired_protocol_entries);

    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_protocol_db size after load: %u", __func__, m_protocol_db.size());
    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_protocol_entries size after load: %u", __func__, m_protocol_entries.size());

    return height;
}

void ProtocolRegistry::SetDBHeight(int& height)
{
    LOCK(cs_lock);

    m_protocol_db.StoreDBHeight(height);
}

int ProtocolRegistry::GetDBHeight()
{
    int height = 0;

    LOCK(cs_lock);

    m_protocol_db.LoadDBHeight(height);

    return height;
}

void ProtocolRegistry::ResetInMemoryOnly()
{
    LOCK(cs_lock);

    m_protocol_entries.clear();
    m_protocol_db.clear_in_memory_only();
}

uint64_t ProtocolRegistry::PassivateDB()
{
    LOCK(cs_lock);

    return m_protocol_db.passivate_db();
}

ProtocolRegistry::ProtocolEntryDB &ProtocolRegistry::GetProtocolEntryDB()
{
    return m_protocol_db;
}

template<> const std::string ProtocolRegistry::ProtocolEntryDB::KeyType()
{
    return std::string("protocol");
}
