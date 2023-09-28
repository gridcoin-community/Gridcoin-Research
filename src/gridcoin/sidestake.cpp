// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "sidestake.h"
#include "node/ui_interface.h"

using namespace GRC;

// -----------------------------------------------------------------------------
// Class: SideStake
// -----------------------------------------------------------------------------
SideStake::SideStake()
    : m_address()
      , m_allocation()
      , m_timestamp(0)
      , m_hash()
      , m_previous_hash()
      , m_status(SideStakeStatus::UNKNOWN)
{}

SideStake::SideStake(CBitcoinAddress address, double allocation)
    : m_address(address)
      , m_allocation(allocation)
      , m_timestamp(0)
      , m_hash()
      , m_previous_hash()
      , m_status(SideStakeStatus::UNKNOWN)
{}

SideStake::SideStake(CBitcoinAddress address, double allocation, int64_t timestamp, uint256 hash)
    : m_address(address)
      , m_allocation(allocation)
      , m_timestamp(timestamp)
      , m_hash(hash)
      , m_previous_hash()
      , m_status(SideStakeStatus::UNKNOWN)
{}

bool SideStake::WellFormed() const
{
    return m_address.IsValid() && m_allocation >= 0.0 && m_allocation <= 1.0;
}

std::pair<std::string, std::string> SideStake::KeyValueToString() const
{
    return std::make_pair(m_address.ToString(), StatusToString());
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

    result &= (m_address == b.m_address);
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
