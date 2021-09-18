// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_STAKING_EXCEPTIONS_H
#define GRIDCOIN_STAKING_EXCEPTIONS_H

#include <set>
#include "uint256.h"

namespace GRC {
//!
//! \brief Get bad block list.
//!
//! Gets a list of blocks which under the current rules are considered invalid
//! but for various reasons still made it into the chain. These are known
//! blocks which can bypass some validation parts.
//!
//! \return A list of currently known bad blocks.
//!
const std::set<uint256>& GetBadBlocks();
}

#endif // GRIDCOIN_STAKING_EXCEPTIONS_H
