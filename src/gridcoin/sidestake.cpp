// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "sidestake.h"
#include "node/ui_interface.h"
#include "univalue.h"

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
// Class: Allocation
// -----------------------------------------------------------------------------
Allocation::Allocation()
    : Fraction()
{}

Allocation::Allocation(const double& allocation)
    : Fraction(static_cast<int64_t>(std::round(allocation * static_cast<double>(10000.0))), static_cast<int64_t>(10000), true)
{}

Allocation::Allocation(const Fraction& f)
    : Fraction(f)
{}

Allocation::Allocation(const int64_t& numerator, const int64_t& denominator)
    : Fraction(numerator, denominator)
{}

Allocation::Allocation(const int64_t& numerator, const int64_t& denominator, const bool& simplify)
    : Fraction(numerator, denominator, simplify)
{}

CAmount Allocation::ToCAmount() const
{
    return GetNumerator() / GetDenominator();
}

double Allocation::ToPercent() const
{
    return ToDouble() * 100.0;
}

Allocation Allocation::operator+(const Allocation& rhs) const
{
    return static_cast<Allocation>(Fraction::operator+(rhs));
}

Allocation Allocation::operator+(const int64_t& rhs) const
{
    return static_cast<Allocation>(Fraction::operator+(rhs));
}

Allocation Allocation::operator-(const Allocation& rhs) const
{
    return static_cast<Allocation>(Fraction::operator-(rhs));
}

Allocation Allocation::operator-(const int64_t& rhs) const
{
    return static_cast<Allocation>(Fraction::operator-(rhs));
}

Allocation Allocation::operator*(const Allocation& rhs) const
{
    return static_cast<Allocation>(Fraction::operator*(rhs));
}

Allocation Allocation::operator*(const int64_t& rhs) const
{
    return static_cast<Allocation>(Fraction::operator*(rhs));
}

Allocation Allocation::operator/(const Allocation& rhs) const
{
    return static_cast<Allocation>(Fraction::operator/(rhs));
}

Allocation Allocation::operator/(const int64_t& rhs) const
{
    return static_cast<Allocation>(Fraction::operator/(rhs));
}

Allocation Allocation::operator+=(const Allocation& rhs)
{
    return static_cast<Allocation>(Fraction::operator+=(rhs));
}

Allocation Allocation::operator+=(const int64_t& rhs)
{
    return static_cast<Allocation>(Fraction::operator+=(rhs));
}

Allocation Allocation::operator-=(const Allocation& rhs)
{
    return static_cast<Allocation>(Fraction::operator-=(rhs));
}

Allocation Allocation::operator-=(const int64_t& rhs)
{
    return static_cast<Allocation>(Fraction::operator-=(rhs));
}

Allocation Allocation::operator*=(const Allocation& rhs)
{
    return static_cast<Allocation>(Fraction::operator*=(rhs));
}

Allocation Allocation::operator*=(const int64_t& rhs)
{
    return static_cast<Allocation>(Fraction::operator*=(rhs));
}

Allocation Allocation::operator/=(const Allocation& rhs)
{
    return static_cast<Allocation>(Fraction::operator/=(rhs));
}

Allocation Allocation::operator/=(const int64_t& rhs)
{
    return static_cast<Allocation>(Fraction::operator/=(rhs));
}

bool Allocation::operator==(const Allocation& rhs) const
{
    return Fraction::operator==(rhs);
}

bool Allocation::operator!=(const Allocation& rhs) const
{
    return Fraction::operator!=(rhs);
}

bool Allocation::operator<=(const Allocation& rhs) const
{
    return Fraction::operator<=(rhs);
}

bool Allocation::operator>=(const Allocation& rhs) const
{
    return Fraction::operator>=(rhs);
}

bool Allocation::operator<(const Allocation& rhs) const
{
    return Fraction::operator<(rhs);
}

bool Allocation::operator>(const Allocation& rhs) const
{
    return Fraction::operator>(rhs);
}

bool Allocation::operator==(const int64_t& rhs) const
{
    return Fraction::operator==(rhs);
}

bool Allocation::operator!=(const int64_t& rhs) const
{
    return Fraction::operator!=(rhs);
}

bool Allocation::operator<=(const int64_t& rhs) const
{
    return Fraction::operator<=(rhs);
}

bool Allocation::operator>=(const int64_t& rhs) const
{
    return Fraction::operator>=(rhs);
}

bool Allocation::operator<(const int64_t& rhs) const
{
    return Fraction::operator<(rhs);
}

bool Allocation::operator>(const int64_t& rhs) const
{
    return Fraction::operator>(rhs);
}

// -----------------------------------------------------------------------------
// Class: LocalSideStake
// -----------------------------------------------------------------------------
LocalSideStake::LocalSideStake()
    : m_destination()
      , m_allocation()
      , m_description()
      , m_status(LocalSideStakeStatus::UNKNOWN)
{}

LocalSideStake::LocalSideStake(CTxDestination destination, Allocation allocation, std::string description)
    : m_destination(destination)
      , m_allocation(allocation)
      , m_description(description)
      , m_status(LocalSideStakeStatus::UNKNOWN)
{}

LocalSideStake::LocalSideStake(CTxDestination destination,
                               Allocation allocation,
                               std::string description,
                               LocalSideStakeStatus status)
    : m_destination(destination)
      , m_allocation(allocation)
      , m_description(description)
      , m_status(status)
{}

bool LocalSideStake::WellFormed() const
{
    return CBitcoinAddress(m_destination).IsValid() && m_allocation >= 0 && m_allocation <= 1;
}

std::string LocalSideStake::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string LocalSideStake::StatusToString(const LocalSideStakeStatus& status, const bool& translated) const
{
    if (translated) {
        switch(status) {
        case LocalSideStakeStatus::UNKNOWN:         return _("Unknown");
        case LocalSideStakeStatus::ACTIVE:          return _("Active");
        case LocalSideStakeStatus::INACTIVE:        return _("Inactive");
        case LocalSideStakeStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case LocalSideStakeStatus::UNKNOWN:         return "Unknown";
        case LocalSideStakeStatus::ACTIVE:          return "Active";
        case LocalSideStakeStatus::INACTIVE:        return "Inactive";
        case LocalSideStakeStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    }

           // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
           // from some compiler versions.
    return std::string{};
}

bool LocalSideStake::operator==(LocalSideStake b)
{
    bool result = true;

    result &= (m_destination == b.m_destination);
    result &= (m_allocation == b.m_allocation);
    result &= (m_description == b.m_description);
    result &= (m_status == b.m_status);

    return result;
}

bool LocalSideStake::operator!=(LocalSideStake b)
{
    return !(*this == b);
}

// -----------------------------------------------------------------------------
// Class: MandatorySideStake
// -----------------------------------------------------------------------------
MandatorySideStake::MandatorySideStake()
    : m_destination()
      , m_allocation()
      , m_description()
      , m_timestamp(0)
      , m_hash()
      , m_previous_hash()
      , m_status(MandatorySideStakeStatus::UNKNOWN)
{}

MandatorySideStake::MandatorySideStake(CTxDestination destination, Allocation allocation, std::string description)
    : m_destination(destination)
      , m_allocation(allocation)
      , m_description(description)
      , m_timestamp(0)
      , m_hash()
      , m_previous_hash()
      , m_status(MandatorySideStakeStatus::UNKNOWN)
{}

MandatorySideStake::MandatorySideStake(CTxDestination destination,
                                       Allocation allocation,
                                       std::string description,
                                       int64_t timestamp,
                                       uint256 hash,
                                       MandatorySideStakeStatus status)
    : m_destination(destination)
      , m_allocation(allocation)
      , m_description(description)
      , m_timestamp(timestamp)
      , m_hash(hash)
      , m_previous_hash()
      , m_status(status)
{}

bool MandatorySideStake::WellFormed() const
{
    return CBitcoinAddress(m_destination).IsValid() && m_allocation >= 0 && m_allocation <= 1;
}

CTxDestination MandatorySideStake::Key() const
{
    return m_destination;
}

std::pair<std::string, std::string> MandatorySideStake::KeyValueToString() const
{
    return std::make_pair(CBitcoinAddress(m_destination).ToString(), StatusToString());
}

std::string MandatorySideStake::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string MandatorySideStake::StatusToString(const MandatorySideStakeStatus& status, const bool& translated) const
{
    if (translated) {
        switch(status) {
        case MandatorySideStakeStatus::UNKNOWN:         return _("Unknown");
        case MandatorySideStakeStatus::DELETED:         return _("Deleted");
        case MandatorySideStakeStatus::MANDATORY:       return _("Mandatory");
        case MandatorySideStakeStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case MandatorySideStakeStatus::UNKNOWN:         return "Unknown";
        case MandatorySideStakeStatus::DELETED:         return "Deleted";
        case MandatorySideStakeStatus::MANDATORY:       return "Mandatory";
        case MandatorySideStakeStatus::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    }

           // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
           // from some compiler versions.
    return std::string{};
}

bool MandatorySideStake::operator==(MandatorySideStake b)
{
    bool result = true;

    result &= (m_destination == b.m_destination);
    result &= (m_allocation == b.m_allocation);
    result &= (m_description == b.m_description);
    result &= (m_timestamp == b.m_timestamp);
    result &= (m_hash == b.m_hash);
    result &= (m_previous_hash == b.m_previous_hash);
    result &= (m_status == b.m_status);

    return result;
}

bool MandatorySideStake::operator!=(MandatorySideStake b)
{
    return !(*this == b);
}

// -----------------------------------------------------------------------------
// Class: SideStake
// -----------------------------------------------------------------------------
SideStake::SideStake()
    : m_local_sidestake_ptr(nullptr)
      , m_mandatory_sidestake_ptr(nullptr)
      , m_type(Type::UNKNOWN)
{}

SideStake::SideStake(LocalSideStake_ptr sidestake_ptr)
    : m_local_sidestake_ptr(sidestake_ptr)
      , m_mandatory_sidestake_ptr(nullptr)
      , m_type(Type::LOCAL)
{}

SideStake::SideStake(MandatorySideStake_ptr sidestake_ptr)
    : m_local_sidestake_ptr(nullptr)
      , m_mandatory_sidestake_ptr(sidestake_ptr)
      , m_type(Type::MANDATORY)
{}

bool SideStake::IsMandatory() const
{
    return (m_type == Type::MANDATORY) ? true : false;
}

CTxDestination SideStake::GetDestination() const
{
    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->m_destination;
    } else if (m_type == Type::LOCAL && m_local_sidestake_ptr != nullptr) {
        return m_local_sidestake_ptr->m_destination;
    }

    return CNoDestination();
}

Allocation SideStake::GetAllocation() const
{
    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->m_allocation;
    } else if (m_type == Type::LOCAL && m_local_sidestake_ptr != nullptr) {
        return m_local_sidestake_ptr->m_allocation;
    }

    return Allocation(Fraction());
}

std::string SideStake::GetDescription() const
{
    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->m_description;
    } else if (m_type == Type::LOCAL && m_local_sidestake_ptr != nullptr) {
        return m_local_sidestake_ptr->m_description;
    }

    return std::string {};
}

int64_t SideStake::GetTimeStamp() const
{
    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->m_timestamp;
    }

    return int64_t {0};
}

uint256 SideStake::GetHash() const
{
    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->m_hash;
    }

    return uint256 {};
}

uint256 SideStake::GetPreviousHash() const
{
    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->m_previous_hash;
    }

    return uint256 {};
}

SideStake::Status SideStake::GetStatus() const
{
    // For trivial initializer case
    if (m_mandatory_sidestake_ptr == nullptr && m_local_sidestake_ptr == nullptr) {
        return {};
    }

    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->m_status;
    } else if (m_type == Type::LOCAL && m_local_sidestake_ptr != nullptr) {
        return m_local_sidestake_ptr->m_status;
    }

    return {};
}

std::string SideStake::StatusToString() const
{
    // For trivial initializer case
    if (m_mandatory_sidestake_ptr == nullptr && m_local_sidestake_ptr == nullptr) {
        return {};
    }

    if (m_type == Type::MANDATORY && m_mandatory_sidestake_ptr != nullptr) {
        return m_mandatory_sidestake_ptr->StatusToString();
    } else if (m_type == Type::LOCAL && m_local_sidestake_ptr != nullptr){
        return m_local_sidestake_ptr->StatusToString();
    }

    return std::string {};
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
                                   CTxDestination destination,
                                   Allocation allocation,
                                   std::string description,
                                   MandatorySideStake::MandatorySideStakeStatus status)
    : IContractPayload()
      , m_version(version)
      , m_entry(MandatorySideStake(destination, allocation, description, 0, uint256{}, status))
{
}

SideStakePayload::SideStakePayload(const uint32_t version, MandatorySideStake entry)
    : IContractPayload()
      , m_version(version)
      , m_entry(std::move(entry))
{
}

SideStakePayload::SideStakePayload(MandatorySideStake entry)
    : SideStakePayload(CURRENT_VERSION, std::move(entry))
{
}

// -----------------------------------------------------------------------------
// Class: SideStakeRegistry
// -----------------------------------------------------------------------------
const std::vector<SideStake_ptr> SideStakeRegistry::SideStakeEntries() const
{
    std::vector<SideStake_ptr> sidestakes;

    LOCK(cs_lock);

    for (const auto& entry : m_mandatory_sidestake_entries) {
        sidestakes.push_back(std::make_shared<SideStake>(entry.second));
    }

    for (const auto& entry : m_local_sidestake_entries) {
        sidestakes.push_back(std::make_shared<SideStake>(entry.second));
    }

    return sidestakes;
}

const std::vector<SideStake_ptr> SideStakeRegistry::ActiveSideStakeEntries(const SideStake::FilterFlag& filter,
                                                                           const bool& include_zero_alloc) const
{
    std::vector<SideStake_ptr> sidestakes;
    Allocation allocation_sum;

    // Note that LoadLocalSideStakesFromConfig is called upon a receipt of the core signal RwSettingsUpdated, which
    // occurs immediately after the settings r-w file is updated.

    // The loops below prevent sidestakes from being added that cause a total allocation above 1.0 (100%).

    LOCK(cs_lock);

    if (filter & SideStake::FilterFlag::MANDATORY) {
        for (const auto& entry : m_mandatory_sidestake_entries)
        {
            if (entry.second->m_status == MandatorySideStake::MandatorySideStakeStatus::MANDATORY
                && allocation_sum + entry.second->m_allocation <= Params().GetConsensus().MaxMandatorySideStakeTotalAlloc) {
                if ((include_zero_alloc && entry.second->m_allocation == 0) || entry.second->m_allocation > 0) {
                    sidestakes.push_back(std::make_shared<SideStake>(entry.second));
                    allocation_sum += entry.second->m_allocation;
                }
            }
        }
    }

    if (filter & SideStake::FilterFlag::LOCAL) {
        // Followed by local active sidestakes if sidestaking is enabled. Note that mandatory sidestaking cannot be disabled.
        bool fEnableSideStaking = gArgs.GetBoolArg("-enablesidestaking");

        if (fEnableSideStaking) {
            LogPrint(BCLog::LogFlags::MINER, "INFO: %s: fEnableSideStaking = %u", __func__, fEnableSideStaking);

            for (const auto& entry : m_local_sidestake_entries)
            {
                if (entry.second->m_status == LocalSideStake::LocalSideStakeStatus::ACTIVE
                    && allocation_sum + entry.second->m_allocation <= 1) {
                    if ((include_zero_alloc && entry.second->m_allocation == 0) || entry.second->m_allocation > 0) {
                        sidestakes.push_back(std::make_shared<SideStake>(entry.second));
                        allocation_sum += entry.second->m_allocation;
                    }
                }
            }
        }
    }

    return sidestakes;
}

std::vector<SideStake_ptr> SideStakeRegistry::Try(const CTxDestination& key, const SideStake::FilterFlag& filter) const
{
    LOCK(cs_lock);

    std::vector<SideStake_ptr> result;

    if (filter & SideStake::FilterFlag::MANDATORY) {
        const auto mandatory_entry = m_mandatory_sidestake_entries.find(key);

        if (mandatory_entry != m_mandatory_sidestake_entries.end()) {
            result.push_back(std::make_shared<SideStake>(mandatory_entry->second));
        }
    }

    if (filter & SideStake::FilterFlag::LOCAL) {
        const auto local_entry = m_local_sidestake_entries.find(key);

        if (local_entry != m_local_sidestake_entries.end()) {
            result.push_back(std::make_shared<SideStake>(local_entry->second));
        }
    }

    return result;
}

std::vector<SideStake_ptr> SideStakeRegistry::TryActive(const CTxDestination& key, const SideStake::FilterFlag& filter) const
{
    LOCK(cs_lock);

    std::vector<SideStake_ptr> result;

    for (const auto& iter : Try(key, filter)) {
        if (iter->IsMandatory()) {
            if (std::get<MandatorySideStake::Status>(iter->GetStatus()) == MandatorySideStake::MandatorySideStakeStatus::MANDATORY) {
                result.push_back(iter);
            }
        } else {
            if (std::get<LocalSideStake::Status>(iter->GetStatus()) == LocalSideStake::LocalSideStakeStatus::ACTIVE) {
                result.push_back(iter);
            }
        }
    }

    return result;
}

void SideStakeRegistry::Reset()
{
    LOCK(cs_lock);

    m_mandatory_sidestake_entries.clear();
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
        payload.m_entry.m_status = MandatorySideStake::MandatorySideStakeStatus::DELETED;
    }

    LOCK(cs_lock);

    auto sidestake_entry_pair_iter = m_mandatory_sidestake_entries.find(payload.m_entry.m_destination);

    MandatorySideStake_ptr current_sidestake_entry_ptr = nullptr;

    // Is there an existing SideStake entry in the map?
    bool current_sidestake_entry_present = (sidestake_entry_pair_iter != m_mandatory_sidestake_entries.end());

    // If so, then get a smart pointer to it.
    if (current_sidestake_entry_present) {
        current_sidestake_entry_ptr = sidestake_entry_pair_iter->second;

    // Set the payload m_entry's prev entry ctx hash = to the existing entry's hash.
        payload.m_entry.m_previous_hash = current_sidestake_entry_ptr->m_hash;
    } else { // Original entry for this SideStake entry key
        payload.m_entry.m_previous_hash = uint256 {};
    }

    LogPrint(LogFlags::CONTRACT, "INFO: %s: SideStake entry add/delete: contract m_version = %u, payload "
                                 "m_version = %u, address = %s, allocation = %f, m_timestamp = %" PRId64 ", "
                                 "m_hash = %s, m_previous_hash = %s, m_status = %s",
             __func__,
             ctx->m_version,
             payload.m_version,
             CBitcoinAddress(payload.m_entry.m_destination).ToString(),
             payload.m_entry.m_allocation.ToPercent(),
             payload.m_entry.m_timestamp,
             payload.m_entry.m_hash.ToString(),
             payload.m_entry.m_previous_hash.ToString(),
             payload.m_entry.StatusToString()
             );

    MandatorySideStake& historical = payload.m_entry;

    if (!m_sidestake_db.insert(ctx.m_tx.GetHash(), height, historical))
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: In recording of the SideStake entry for key %s, value %f, hash %s, "
                                     "the SideStake entry db record already exists. This can be expected on a restart "
                                     "of the wallet to ensure multiple contracts in the same block get stored/replayed.",
                 __func__,
                 CBitcoinAddress(historical.m_destination).ToString(),
                 historical.m_allocation.ToPercent(),
                 historical.m_hash.GetHex());
    }

    // Finally, insert the new SideStake entry (payload) smart pointer into the m_sidestake_entries map.
    m_mandatory_sidestake_entries[payload.m_entry.m_destination] = m_sidestake_db.find(ctx.m_tx.GetHash())->second;

    return;
}

void SideStakeRegistry::Add(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void SideStakeRegistry::Delete(const ContractContext& ctx)
{
    AddDelete(ctx);
}

void SideStakeRegistry::NonContractAdd(const LocalSideStake& sidestake, const bool& save_to_file)
{
    LOCK(cs_lock);

    // Using this form of insert because we want the latest record with the same key to override any previous one.
    m_local_sidestake_entries[sidestake.m_destination] = std::make_shared<LocalSideStake>(sidestake);

    if (save_to_file) {
        SaveLocalSideStakesToConfig();
    }
}

void SideStakeRegistry::NonContractDelete(const CTxDestination& destination, const bool& save_to_file)
{
    LOCK(cs_lock);

    auto sidestake_entry_pair_iter = m_local_sidestake_entries.find(destination);

    if (sidestake_entry_pair_iter != m_local_sidestake_entries.end()) {
        m_local_sidestake_entries.erase(sidestake_entry_pair_iter);
    }

    if (save_to_file) {
        SaveLocalSideStakesToConfig();
    }
}

void SideStakeRegistry::Revert(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<SideStakePayload>();

    // For SideStake entries, both adds and removes will have records to revert in the m_sidestake_entries map,
    // and also, if not the first entry for that SideStake key, will have a historical record to
    // resurrect.
    LOCK(cs_lock);

    auto entry_to_revert = m_mandatory_sidestake_entries.find(payload->m_entry.m_destination);

    if (entry_to_revert == m_mandatory_sidestake_entries.end()) {
        error("%s: The SideStake entry for key %s to revert was not found in the SideStake entry map.",
              __func__,
              CBitcoinAddress(entry_to_revert->second->m_destination).ToString());

        // If there is no record in the current m_sidestake_entries map, then there is nothing to do here. This
        // should not occur.
        return;
    }

           // If this is not a null hash, then there will be a prior entry to resurrect.
    CTxDestination key = entry_to_revert->second->m_destination;
    uint256 resurrect_hash = entry_to_revert->second->m_previous_hash;

           // Revert the ADD or REMOVE action. Unlike the beacons, this is symmetric.
    if (ctx->m_action == ContractAction::ADD || ctx->m_action == ContractAction::REMOVE) {
        // Erase the record from m_sidestake_entries.
        if (m_mandatory_sidestake_entries.erase(payload->m_entry.m_destination) == 0) {
            error("%s: The SideStake entry to erase during a SideStake entry revert for key %s was not found.",
                  __func__,
                  CBitcoinAddress(key).ToString());
            // If the record to revert is not found in the m_sidestake_entries map, no point in continuing.
            return;
        }

               // Also erase the record from the db.
        if (!m_sidestake_db.erase(ctx.m_tx.GetHash())) {
            error("%s: The db entry to erase during a SideStake entry revert for key %s was not found.",
                  __func__,
                  CBitcoinAddress(key).ToString());

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
                  CBitcoinAddress(key).ToString());
            return;
        }

        // Resurrect the entry prior to the reverted one. It is safe to use the bracket form here, because of the protection
        // of the logic above. There cannot be any entry in m_sidestake_entries with that key value left if we made it here.
        m_mandatory_sidestake_entries[resurrect_entry->second->m_destination] = resurrect_entry->second;
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

    Allocation allocation = payload->m_entry.m_allocation;

    // Contracts that would result in a total active mandatory sidestake allocation greater than the maximum allowed by consensus
    // protocol must be rejected. Note that this is not a perfect validation, because there could be more than one sidestake
    // contract transaction in the memory pool, and this is using already committed sidestake contracts (i.e. in blocks already
    // accepted) as a basis.
    if (GetMandatoryAllocationsTotal() + allocation > Params().GetConsensus().MaxMandatorySideStakeTotalAlloc) {
        DoS = 25;
        return false;
    }

    return true;
}

bool SideStakeRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    return (IsV13Enabled(ctx.m_pindex->nHeight) && Validate(ctx.m_contract, ctx.m_tx, DoS));
}

int SideStakeRegistry::Initialize()
{
    LOCK(cs_lock);

    int height = m_sidestake_db.Initialize(m_mandatory_sidestake_entries, m_pending_sidestake_entries, m_expired_sidestake_entries);

    SubscribeToCoreSignals();

    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_sidestake_db size after load: %u", __func__, m_sidestake_db.size());
    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_sidestake_entries size after load: %u", __func__, m_mandatory_sidestake_entries.size());

    // Add the local sidestakes specified in the config file(s) to the local sidestakes map.
    LoadLocalSideStakesFromConfig();

    m_local_entry_already_saved_to_config = false;

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

    m_local_sidestake_entries.clear();
    m_mandatory_sidestake_entries.clear();
    m_sidestake_db.clear_in_memory_only();
}

uint64_t SideStakeRegistry::PassivateDB()
{
    LOCK(cs_lock);

    return m_sidestake_db.passivate_db();
}

void SideStakeRegistry::LoadLocalSideStakesFromConfig()
{
    // If the m_local_entry_already_saved_to_config is set, then SaveLocalSideStakeToConfig was just called,
    // and we want to then ignore the update signal from the r-w file change that calls this function for
    // that action (only) and then reset the flag to be responsive to any changes on the core r-w file side
    // through changesettings, for example.
    if (m_local_entry_already_saved_to_config) {
        m_local_entry_already_saved_to_config = false;

        return;
    }

    std::vector<LocalSideStake> vLocalSideStakes;
    std::vector<std::tuple<std::string, std::string, std::string>> raw_vSideStakeAlloc;
    Allocation sum_allocation;

    // Parse destinations and allocations. We don't need to worry about any that are rejected other than a warning
    // message, because any unallocated rewards will go back into the coinstake output(s).

    // If -sidestakeaddresses and -sidestakeallocations is set in either the config file or the r-w settings file
    // and the settings are not empty and they are the same size, this will take precedence over the multiple entry
    // -sidestake format. Note that -descriptions is optional; however, if descriptions is used, the size must
    // match the other two if present.
    std::vector<std::string> addresses;
    std::vector<std::string> allocations;
    std::vector<std::string> descriptions;

    ParseString(gArgs.GetArg("-sidestakeaddresses", ""), ',', addresses);
    ParseString(gArgs.GetArg("-sidestakeallocations", ""), ',', allocations);
    ParseString(gArgs.GetArg("-sidestakedescriptions", ""), ',', descriptions);

    bool new_format_valid = false;

    if (!addresses.empty()) {
        if (addresses.size() != allocations.size() || (!descriptions.empty() && addresses.size() != descriptions.size())) {
            LogPrintf("WARN: %s: Malformed new style sidestaking configuration entries. "
                      "Reverting to original format in read only gridcoinresearch.conf file.",
                      __func__);
        } else {
            new_format_valid = true;

            for (unsigned int i = 0; i < addresses.size(); ++i)
            {
                if (descriptions.empty()) {
                    raw_vSideStakeAlloc.push_back(std::make_tuple(addresses[i], allocations[i], ""));
                } else {
                    raw_vSideStakeAlloc.push_back(std::make_tuple(addresses[i], allocations[i], descriptions[i]));
                }
            }
        }
    }

    if (new_format_valid == false && gArgs.GetArgs("-sidestake").size())
    {
        for (auto const& sSubParam : gArgs.GetArgs("-sidestake"))
        {
            std::vector<std::string> vSubParam;

            ParseString(sSubParam, ',', vSubParam);
            if (vSubParam.size() < 2)
            {
                LogPrintf("WARN: %s: Incomplete SideStake Allocation specified. Skipping SideStake entry.", __func__);
                continue;
            }

            // Deal with optional description.
            if (vSubParam.size() == 3) {
                raw_vSideStakeAlloc.push_back(std::make_tuple(vSubParam[0], vSubParam[1], vSubParam[2]));
            } else {
                raw_vSideStakeAlloc.push_back(std::make_tuple(vSubParam[0], vSubParam[1], ""));
            }
        }
    }

    // First, add the allocation already taken by mandatory sidestakes, because they must be allocated first.
    sum_allocation += GetMandatoryAllocationsTotal();

    LOCK(cs_lock);

    for (const auto& entry : raw_vSideStakeAlloc)
    {
        std::string sAddress = std::get<0>(entry);
        std::string sAllocation = std::get<1>(entry);
        std::string sDescription = std::get<2>(entry);

        CBitcoinAddress address(sAddress);
        if (!address.IsValid())
        {
            LogPrintf("WARN: %s: ignoring sidestake invalid address %s.", __func__, sAddress);
            continue;
        }

        double read_allocation = 0.0;
        if (!ParseDouble(sAllocation, &read_allocation))
        {
            LogPrintf("WARN: %s: Invalid allocation %s provided. Skipping allocation.", __func__, sAllocation);
            continue;
        }

        LogPrintf("INFO: %s: allocation = %f", __func__, read_allocation);

        //int64_t numerator = read_allocation * 100.0;
        //Allocation allocation(Fraction(numerator, 10000, true));

        Allocation allocation(read_allocation / 100.0);

        if (allocation < 0)
        {
            LogPrintf("WARN: %s: Negative allocation provided. Skipping allocation.", __func__);
            continue;
        }

        // The below will stop allocations if someone has made a mistake and the total adds up to more than 100%.
        // Note this same check is also done in SplitCoinStakeOutput, but it needs to be done here for two reasons:
        // 1. Early alertment in the debug log, rather than when the first kernel is found, and 2. When the UI is
        // hooked up, the SideStakeAlloc vector will be filled in by other than reading the config file and will
        // skip the above code.
        sum_allocation += allocation;
        if (sum_allocation > 1)
        {
            LogPrintf("WARN: %s: allocation percentage over 100 percent, ending sidestake allocations.", __func__);
            break;
        }

        LocalSideStake sidestake(address.Get(),
                                 allocation,
                                 sDescription,
                                 LocalSideStake::LocalSideStakeStatus::ACTIVE);

        // This will add or update (replace) a non-contract entry in the registry for the local sidestake.
        NonContractAdd(sidestake, false);

        // This is needed because we need to detect entries in the registry map that are no longer in the config file to mark
        // them deleted.
        vLocalSideStakes.push_back(sidestake);

        LogPrint(BCLog::LogFlags::MINER, "INFO: %s: SideStakeAlloc Address %s, Allocation %f",
                 __func__, sAddress, allocation.ToPercent());
    }

    for (auto& entry : m_local_sidestake_entries)
    {
        // Only look at active entries. The others are NA for this alignment.
        if (entry.second->m_status == LocalSideStake::LocalSideStakeStatus::ACTIVE) {
            auto iter = std::find(vLocalSideStakes.begin(), vLocalSideStakes.end(), *entry.second);

            if (iter == vLocalSideStakes.end()) {
                // Entry in map is no longer found in config files, so mark map entry inactive.

                entry.second->m_status = LocalSideStake::LocalSideStakeStatus::INACTIVE;
            }
        }
    }

    // If we get here and dSumAllocation is zero then the enablesidestaking flag was set, but no VALID distribution
    // was provided in the config file, so warn in the debug log.
    if (!sum_allocation)
        LogPrintf("WARN: %s: enablesidestaking was set in config but nothing has been allocated for"
                  " distribution!", __func__);
}

bool SideStakeRegistry::SaveLocalSideStakesToConfig()
{
    bool status = false;

    std::string addresses;
    std::string allocations;
    std::string descriptions;

    std::string separator;

    std::vector<std::pair<std::string, util::SettingsValue>> settings;

    LOCK(cs_lock);

    unsigned int i = 0;
    for (const auto& iter : m_local_sidestake_entries) {
        if (i) {
            separator = ",";
        }

        addresses += separator + CBitcoinAddress(iter.second->m_destination).ToString();
        allocations += separator + ToString(iter.second->m_allocation.ToPercent());
        descriptions += separator + iter.second->m_description;

        ++i;
    }

    settings.push_back(std::make_pair("sidestakeaddresses", addresses));
    settings.push_back(std::make_pair("sidestakeallocations", allocations));
    settings.push_back(std::make_pair("sidestakedescriptions", descriptions));

    status = updateRwSettings(settings);

    m_local_entry_already_saved_to_config = true;

    return status;
}

Allocation SideStakeRegistry::GetMandatoryAllocationsTotal() const
{
    std::vector<SideStake_ptr> sidestakes = ActiveSideStakeEntries(SideStake::FilterFlag::MANDATORY, false);
    Allocation allocation_total;

    for (const auto& entry : sidestakes) {
        allocation_total += entry->GetAllocation();
    }

    return allocation_total;
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

template<> const std::string SideStakeRegistry::SideStakeDB::KeyType()
{
    return std::string("SideStake");
}

