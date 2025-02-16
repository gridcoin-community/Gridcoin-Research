// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_FWD_H
#define GRIDCOIN_FWD_H

namespace GRC
{
//!
//! \brief Enumeration of project entry status. Unlike beacons this is for both storage
//! and memory.
//!
//! UNKNOWN status is only encountered in trivially constructed empty
//! project entries and should never be seen on the blockchain.
//!
//! DELETED status corresponds to a removed entry.
//!
//! ACTIVE corresponds to an active entry.
//!
//! GREYLISTED means that the project temporarily does not meet the whitelist qualification criteria.
//!
//! OUT_OF_BOUND must go at the end and be retained for the EnumBytes wrapper.
//!
enum class ProjectEntryStatus
{
    UNKNOWN,
    DELETED,
    ACTIVE,
    MAN_GREYLISTED,
    AUTO_GREYLISTED,
    AUTO_GREYLIST_OVERRIDE,
    OUT_OF_BOUND
};
} // namespace GRC

#endif // GRIDCOIN_FWD_H

