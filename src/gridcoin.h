#pragma once

#include <string>
#include <set>
#include "uint256.h"

static const std::string BoincHashMerkleRootNew = "ElimZa7b8c9ateXr9kgueTheJ2HackersExa192";
static const std::string BoincHashWindowsMerkleRootNew = "yG3uv41o6n7apYOVVszTMQ==";

//!
//! \brief Get bad block list.
//!
//! Gets a list of blocks which under the current rules are considered invalid
//! but for various reasons still made it into the chain. These are known
//! blocks which can bypass some validation parts.
//!
//! \return A list of currently known bad blocks.
//!
std::set<uint256> GetBadBlocks();
