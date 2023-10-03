// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "sidestake.h"
#include "node/ui_interface.h"

//!
//! \brief Model callback bound to the \c RwSettingsUpdated core signal.
//!
void RwSettingsUpdated(GRC::SideStakeRegistry* registry)
{
    LogPrint(BCLog::LogFlags::MISC, "INFO: %s: received RwSettingsUpdated() core signal", __func__);

    registry->LoadLocalSideStakesFromConfig();
}


using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
SideStakeRegistry g_sidestake_entries;
} // anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

SideStakeRegistry& GRC::GetSideStakeRegistry()
{
    return g_sidestake_entries;
}

// -----------------------------------------------------------------------------
// Class: CBitcoinAddressForStorage
// -----------------------------------------------------------------------------
CBitcoinAddressForStorage::CBitcoinAddressForStorage()
    : CBitcoinAddress()
{}

CBitcoinAddressForStorage::CBitcoinAddressForStorage(CBitcoinAddress address)
    : CBitcoinAddress(address)
{}

// -----------------------------------------------------------------------------
// Class: SideStake
// -----------------------------------------------------------------------------
SideStake::SideStake()
    : m_key()
      , m_allocation()
      , m_description()
      , m_timestamp(0)
      , m_hash()
      , m_previous_hash()
      , m_status(SideStakeStatus::UNKNOWN)
{}

SideStake::SideStake(CBitcoinAddressForStorage address, double allocation, std::string description)
    : m_key(address)
      , m_allocation(allocation)
      , m_description(description)
      , m_timestamp(0)
      , m_hash()
      , m_previous_hash()
      , m_status(SideStakeStatus::UNKNOWN)
{}

SideStake::SideStake(CBitcoinAddressForStorage address,
                     double allocation,
                     std::string description,
                     int64_t timestamp,
                     uint256 hash,
                     SideStakeStatus status)
    : m_key(address)
      , m_allocation(allocation)
      , m_description(description)
      , m_timestamp(timestamp)
      , m_hash(hash)
      , m_previous_hash()
      , m_status(status)
{}

bool SideStake::WellFormed() const
{
    return m_key.IsValid() && m_allocation >= 0.0 && m_allocation <= 1.0;
}

CBitcoinAddressForStorage SideStake::Key() const
{
    return m_key;
}

std::pair<std::string, std::string> SideStake::KeyValueToString() const
{
    return std::make_pair(m_key.ToString(), StatusToString());
}

std::string SideStake::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string SideStake::StatusToString(const SideStakeStatus& status, const bool& translated) const
{
    if (translated) {
        switch(status) {
        case SideStakeStatus::UNKNOWN:         return _("Unknown");
        case SideStakeStatus::ACTIVE:          return _("Active");
        case SideStakeStatus::INACTIVE:        return _("Inactive");
        case SideStakeStatus::DELETED:         return _("Deleted");
        case SideStakeStatus::MANDATORY:       return _("Mandatory");
        case SideStakeStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case SideStakeStatus::UNKNOWN:         return "Unknown";
        case SideStakeStatus::ACTIVE:          return "Active";
        case SideStakeStatus::INACTIVE:        return "Inactive";
        case SideStakeStatus::DELETED:         return "Deleted";
        case SideStakeStatus::MANDATORY:       return "Mandatory";
        case SideStakeStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    }

           // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
           // from some compiler versions.
    return std::string{};
}

bool SideStake::operator==(SideStake b)
{
    bool result = true;

    result &= (m_key == b.m_key);
    result &= (m_allocation == b.m_allocation);
    result &= (m_timestamp == b.m_timestamp);
    result &= (m_hash == b.m_hash);
    result &= (m_previous_hash == b.m_previous_hash);
    result &= (m_status == b.m_status);

    return result;
}

bool SideStake::operator!=(SideStake b)
{
    return !(*this == b);
}

// -----------------------------------------------------------------------------
// Class: SideStakePayload
// -----------------------------------------------------------------------------

constexpr uint32_t SideStakePayload::CURRENT_VERSION; // For clang

SideStakePayload::SideStakePayload(uint32_t version)
    : IContractPayload()
      , m_version(version)
{
}

SideStakePayload::SideStakePayload(const uint32_t version,
                                   CBitcoinAddressForStorage key,
                                   double value,
                                   std::string description,
                                   SideStakeStatus status)
    : IContractPayload()
      , m_version(version)
      , m_entry(SideStake(key, value, description, 0, uint256{}, status))
{
}

SideStakePayload::SideStakePayload(const uint32_t version, SideStake entry)
    : IContractPayload()
      , m_version(version)
      , m_entry(std::move(entry))
{
}

SideStakePayload::SideStakePayload(SideStake entry)
    : SideStakePayload(CURRENT_VERSION, std::move(entry))
{
}

// -----------------------------------------------------------------------------
// Class: SideStakeRegistry
// -----------------------------------------------------------------------------
const SideStakeRegistry::SideStakeMap& SideStakeRegistry::SideStakeEntries() const
{
    return m_sidestake_entries;
}

const std::vector<SideStake_ptr> SideStakeRegistry::ActiveSideStakeEntries()
{
    std::vector<SideStake_ptr> sidestakes;
    double allocation_sum = 0.0;

    // Note that LoadLocalSideStakesFromConfig is called upon a receipt of the core signal RwSettingsUpdated, which
    // occurs immediately after the settings r-w file is updated.

    // The loops below prevent sidestakes from being added that cause a total allocation above 1.0 (100%).

    LOCK(cs_lock);

    // Do mandatory sidestakes first.
    for (const auto& entry : m_sidestake_entries)
    {
        if (entry.second->m_status == SideStakeStatus::MANDATORY && allocation_sum + entry.second->m_allocation <= 1.0) {
            sidestakes.push_back(entry.second);
            allocation_sum += entry.second->m_allocation;
        }
    }

    // Followed by local active sidestakes if sidestaking is enabled. Note that mandatory sidestaking cannot be disabled.
    bool fEnableSideStaking = gArgs.GetBoolArg("-enablesidestaking");

    if (fEnableSideStaking) {
        LogPrint(BCLog::LogFlags::MINER, "INFO: %s: fEnableSideStaking = %u", __func__, fEnableSideStaking);

        for (const auto& entry : m_sidestake_entries)
        {
            if (entry.second->m_status == SideStakeStatus::ACTIVE && allocation_sum + entry.second->m_allocation <= 1.0) {
                sidestakes.push_back(entry.second);
                allocation_sum += entry.second->m_allocation;
            }
        }
    }

    return sidestakes;
}

SideStakeOption SideStakeRegistry::Try(const CBitcoinAddressForStorage& key) const
{
    LOCK(cs_lock);

    const auto iter = m_sidestake_entries.find(key);

    if (iter == m_sidestake_entries.end()) {
        return nullptr;
    }

    return iter->second;
}

SideStakeOption SideStakeRegistry::TryActive(const CBitcoinAddressForStorage& key) const
{
    LOCK(cs_lock);

    if (const SideStakeOption SideStake_entry = Try(key)) {
        if (SideStake_entry->m_status == SideStakeStatus::ACTIVE || SideStake_entry->m_status == SideStakeStatus::MANDATORY) {
            return SideStake_entry;
        }
    }

    return nullptr;
}

void SideStakeRegistry::Reset()
{
    LOCK(cs_lock);

    m_sidestake_entries.clear();
    m_sidestake_db.clear();
}

void SideStakeRegistry::AddDelete(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the LevelDB database
    int height = -1;

    if (ctx.m_pindex)
    {
        height = ctx.m_pindex->nHeight;
    }

    SideStakePayload payload = ctx->CopyPayloadAs<SideStakePayload>();

    // Fill this in from the transaction context because these are not done during payload
    // initialization.
    payload.m_entry.m_hash = ctx.m_tx.GetHash();
    payload.m_entry.m_timestamp = ctx.m_tx.nTime;

    // Ensure status is DELETED if the contract action was REMOVE, regardless of what was actually
    // specified.
    if (ctx->m_action == ContractAction::REMOVE) {
        payload.m_entry.m_status = SideStakeStatus::DELETED;
    }

    LOCK(cs_lock);

    auto sidestake_entry_pair_iter = m_sidestake_entries.find(payload.m_entry.m_key);

    SideStake_ptr current_sidestake_entry_ptr = nullptr;

    // Is there an existing SideStake entry in the map?
    bool current_sidestake_entry_present = (sidestake_entry_pair_iter != m_sidestake_entries.end());

    // If so, then get a smart pointer to it.
    if (current_sidestake_entry_present) {
        current_sidestake_entry_ptr = sidestake_entry_pair_iter->second;

    // Set the payload m_entry's prev entry ctx hash = to the existing entry's hash.
        payload.m_entry.m_previous_hash = current_sidestake_entry_ptr->m_hash;
    } else { // Original entry for this SideStake entry key
        payload.m_entry.m_previous_hash = uint256 {};
    }

    LogPrint(LogFlags::CONTRACT, "INFO: %s: SideStake entry add/delete: contract m_version = %u, payload "
                                 "m_version = %u, key = %s, value = %f, m_timestamp = %" PRId64 ", "
                                 "m_hash = %s, m_previous_hash = %s, m_status = %s",
             __func__,
             ctx->m_version,
             payload.m_version,
             payload.m_entry.m_key.ToString(),
             payload.m_entry.m_allocation,
             payload.m_entry.m_timestamp,
             payload.m_entry.m_hash.ToString(),
             payload.m_entry.m_previous_hash.ToString(),
             payload.m_entry.StatusToString()
             );

    SideStake& historical = payload.m_entry;

    if (!m_sidestake_db.insert(ctx.m_tx.GetHash(), height, historical))
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: In recording of the SideStake entry for key %s, value %f, hash %s, "
                                     "the SideStake entry db record already exists. This can be expected on a restart "
                                     "of the wallet to ensure multiple contracts in the same block get stored/replayed.",
                 __func__,
                 historical.m_key.ToString(),
                 historical.m_allocation,
                 historical.m_hash.GetHex());
    }

    // Finally, insert the new SideStake entry (payload) smart pointer into the m_sidestake_entries map.
    m_sidestake_entries[payload.m_entry.m_key] = m_sidestake_db.find(ctx.m_tx.GetHash())->second;

    return;
}

void SideStakeRegistry::NonContractAdd(SideStake& sidestake)
{
    // Using this form of insert because we want the latest record with the same key to override any previous one.
    m_sidestake_entries[sidestake.m_key] = std::make_shared<SideStake>(sidestake);
}

void SideStakeRegistry::Add(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void SideStakeRegistry::NonContractDelete(CBitcoinAddressForStorage& address)
{
    auto sidestake_entry_pair_iter = m_sidestake_entries.find(address);

    if (sidestake_entry_pair_iter != m_sidestake_entries.end()) {
        m_sidestake_entries.erase(sidestake_entry_pair_iter);
    }
}

void SideStakeRegistry::Delete(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void SideStakeRegistry::Revert(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<SideStakePayload>();

    // For SideStake entries, both adds and removes will have records to revert in the m_sidestake_entries map,
    // and also, if not the first entry for that SideStake key, will have a historical record to
    // resurrect.
    LOCK(cs_lock);

    auto entry_to_revert = m_sidestake_entries.find(payload->m_entry.m_key);

    if (entry_to_revert == m_sidestake_entries.end()) {
        error("%s: The SideStake entry for key %s to revert was not found in the SideStake entry map.",
              __func__,
              entry_to_revert->second->m_key.ToString());

        // If there is no record in the current m_sidestake_entries map, then there is nothing to do here. This
        // should not occur.
        return;
    }

           // If this is not a null hash, then there will be a prior entry to resurrect.
    CBitcoinAddressForStorage key = entry_to_revert->second->m_key;
    uint256 resurrect_hash = entry_to_revert->second->m_previous_hash;

           // Revert the ADD or REMOVE action. Unlike the beacons, this is symmetric.
    if (ctx->m_action == ContractAction::ADD || ctx->m_action == ContractAction::REMOVE) {
        // Erase the record from m_sidestake_entries.
        if (m_sidestake_entries.erase(payload->m_entry.m_key) == 0) {
            error("%s: The SideStake entry to erase during a SideStake entry revert for key %s was not found.",
                  __func__,
                  key.ToString());
            // If the record to revert is not found in the m_sidestake_entries map, no point in continuing.
            return;
        }

               // Also erase the record from the db.
        if (!m_sidestake_db.erase(ctx.m_tx.GetHash())) {
            error("%s: The db entry to erase during a SideStake entry revert for key %s was not found.",
                  __func__,
                  key.ToString());

            // Unlike the above we will keep going even if this record is not found, because it is identical to the
            // m_sidestake_entries record above. This should not happen, because during contract adds and removes,
            // entries are made simultaneously to the m_sidestake_entries and m_sidestake_db.
        }

        if (resurrect_hash.IsNull()) {
            return;
        }

        auto resurrect_entry = m_sidestake_db.find(resurrect_hash);

        if (resurrect_entry == m_sidestake_db.end()) {
            error("%s: The prior entry to resurrect during a SideStake entry ADD revert for key %s was not found.",
                  __func__,
                  key.ToString());
            return;
        }

        // Resurrect the entry prior to the reverted one. It is safe to use the bracket form here, because of the protection
        // of the logic above. There cannot be any entry in m_sidestake_entries with that key value left if we made it here.
        m_sidestake_entries[resurrect_entry->second->m_key] = resurrect_entry->second;
    }
}

bool SideStakeRegistry::Validate(const Contract& contract, const CTransaction& tx, int &DoS) const
{
    const auto payload = contract.SharePayloadAs<SideStakePayload>();

    if (contract.m_version < 3) {
        DoS = 25;
        error("%s: Sidestake entries only valid in contract v3 and above", __func__);
        return false;
    }

    if (!payload->WellFormed(contract.m_action.Value())) {
        DoS = 25;
        error("%s: Malformed SideStake entry contract", __func__);
        return false;
    }

    return true;
}

bool SideStakeRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

int SideStakeRegistry::Initialize()
{
    LOCK(cs_lock);

    int height = m_sidestake_db.Initialize(m_sidestake_entries, m_pending_sidestake_entries);

    SubscribeToCoreSignals();

    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_sidestake_db size after load: %u", __func__, m_sidestake_db.size());
    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_sidestake_entries size after load: %u", __func__, m_sidestake_entries.size());

    // Add the local sidestakes specified in the config file(s) to the mandatory sidestakes.
    LoadLocalSideStakesFromConfig();

    return height;
}

void SideStakeRegistry::SetDBHeight(int& height)
{
    LOCK(cs_lock);

    m_sidestake_db.StoreDBHeight(height);
}

int SideStakeRegistry::GetDBHeight()
{
    int height = 0;

    LOCK(cs_lock);

    m_sidestake_db.LoadDBHeight(height);

    return height;
}

void SideStakeRegistry::ResetInMemoryOnly()
{
    LOCK(cs_lock);

    m_sidestake_entries.clear();
    m_sidestake_db.clear_in_memory_only();
}

uint64_t SideStakeRegistry::PassivateDB()
{
    LOCK(cs_lock);

    return m_sidestake_db.passivate_db();
}

void SideStakeRegistry::LoadLocalSideStakesFromConfig()
{
    std::vector<SideStake> vSideStakes;
    std::vector<std::pair<std::string, std::string>> raw_vSideStakeAlloc;
    double dSumAllocation = 0.0;

    // Parse destinations and allocations. We don't need to worry about any that are rejected other than a warning
    // message, because any unallocated rewards will go back into the coinstake output(s).

    // If -sidestakeaddresses and -sidestakeallocations is set in either the config file or the r-w settings file
    // and the settings are not empty and they are the same size, this will take precedence over the multiple entry
    // -sidestake format.
    std::vector<std::string> addresses;
    std::vector<std::string> allocations;

    ParseString(gArgs.GetArg("-sidestakeaddresses", ""), ',', addresses);
    ParseString(gArgs.GetArg("-sidestakeallocations", ""), ',', allocations);

    if (addresses.size() != allocations.size())
    {
        LogPrintf("WARN: %s: Malformed new style sidestaking configuration entries. Reverting to original format.",
                  __func__);
    }

    if (addresses.size() && addresses.size() == allocations.size())
    {
        for (unsigned int i = 0; i < addresses.size(); ++i)
        {
            raw_vSideStakeAlloc.push_back(std::make_pair(addresses[i], allocations[i]));
        }
    }
    else if (gArgs.GetArgs("-sidestake").size())
    {
        for (auto const& sSubParam : gArgs.GetArgs("-sidestake"))
        {
            std::vector<std::string> vSubParam;

            ParseString(sSubParam, ',', vSubParam);
            if (vSubParam.size() != 2)
            {
                LogPrintf("WARN: %s: Incomplete SideStake Allocation specified. Skipping SideStake entry.", __func__);
                continue;
            }

            raw_vSideStakeAlloc.push_back(std::make_pair(vSubParam[0], vSubParam[1]));
        }
    }

    // First, determine allocation already taken by mandatory sidestakes, because they must be allocated first.
    for (const auto& entry : SideStakeEntries()) {
        if (entry.second->m_status == SideStakeStatus::MANDATORY) {
            dSumAllocation += entry.second->m_allocation;
        }
    }

    LOCK(cs_lock);

    for (const auto& entry : raw_vSideStakeAlloc)
    {
        std::string sAddress;

        double dAllocation = 0.0;

        sAddress = entry.first;

        CBitcoinAddress address(sAddress);
        if (!address.IsValid())
        {
            LogPrintf("WARN: %s: ignoring sidestake invalid address %s.", __func__, sAddress);
            continue;
        }

        if (!ParseDouble(entry.second, &dAllocation))
        {
            LogPrintf("WARN: %s: Invalid allocation %s provided. Skipping allocation.", __func__, entry.second);
            continue;
        }

        dAllocation /= 100.0;

        if (dAllocation <= 0)
        {
            LogPrintf("WARN: %s: Negative or zero allocation provided. Skipping allocation.", __func__);
            continue;
        }

        // The below will stop allocations if someone has made a mistake and the total adds up to more than 100%.
        // Note this same check is also done in SplitCoinStakeOutput, but it needs to be done here for two reasons:
        // 1. Early alertment in the debug log, rather than when the first kernel is found, and 2. When the UI is
        // hooked up, the SideStakeAlloc vector will be filled in by other than reading the config file and will
        // skip the above code.
        dSumAllocation += dAllocation;
        if (dSumAllocation > 1.0)
        {
            LogPrintf("WARN: %s: allocation percentage over 100 percent, ending sidestake allocations.", __func__);
            break;
        }

        SideStake sidestake(static_cast<CBitcoinAddressForStorage>(address),
                            dAllocation,
                            std::string {},
                            0,
                            uint256{},
                            SideStakeStatus::ACTIVE);

        // This will add or update (replace) a non-contract entry in the registry for the local sidestake.
        NonContractAdd(sidestake);

        // This is needed because we need to detect entries in the registry map that are no longer in the config file to mark
        // them deleted.
        vSideStakes.push_back(sidestake);

        LogPrint(BCLog::LogFlags::MINER, "INFO: %s: SideStakeAlloc Address %s, Allocation %f",
                 __func__, sAddress, dAllocation);
    }

    for (auto& entry : m_sidestake_entries)
    {
        // Only look at active entries. The others are NA for this alignment.
        if (entry.second->m_status == SideStakeStatus::ACTIVE) {
            auto iter = std::find(vSideStakes.begin(), vSideStakes.end(), *entry.second);

            if (iter == vSideStakes.end()) {
                // Entry in map is no longer found in config files, so mark map entry inactive.

                entry.second->m_status = SideStakeStatus::INACTIVE;
            }
        }
    }

    // If we get here and dSumAllocation is zero then the enablesidestaking flag was set, but no VALID distribution
    // was provided in the config file, so warn in the debug log.
    if (!dSumAllocation)
        LogPrintf("WARN: %s: enablesidestaking was set in config but nothing has been allocated for"
                  " distribution!", __func__);
}

void SideStakeRegistry::SubscribeToCoreSignals()
{
    uiInterface.RwSettingsUpdated_connect(std::bind(RwSettingsUpdated, this));
}

void SideStakeRegistry::UnsubscribeFromCoreSignals()
{
  // Disconnect signals from client (no-op currently)
}

SideStakeRegistry::SideStakeDB &SideStakeRegistry::GetSideStakeDB()
{
    LOCK(cs_lock);

    return m_sidestake_db;
}

// This is static and called by the scheduler.
void SideStakeRegistry::RunDBPassivation()
{
    TRY_LOCK(cs_main, locked_main);

    if (!locked_main)
    {
        return;
    }

    SideStakeRegistry& SideStake_entries = GetSideStakeRegistry();

    SideStake_entries.PassivateDB();
}

template<> const std::string SideStakeRegistry::SideStakeDB::KeyType()
{
    return std::string("SideStake");
}

