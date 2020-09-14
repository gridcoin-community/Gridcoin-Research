#pragma once

#include <set>
#include "uint256.h"

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
