#pragma once

#include "fwd.h"

//!
//! \brief Find previous tally trigger from block.
//!
//! Scans the chain back from \p block to find the block which should be used
//! as the starting point for a tally. \p block must not be NULL.
//!
//! \param block Block to start from.
//! \return Tally trigger point from \p block.
//!
CBlockIndex* FindTallyTrigger(CBlockIndex* block);

//!
//! \brief Check if a block is a tally trigger.
//!
//! Checks the height of \p block to see if it matches the tally granularity.
//! If this returns \c false then no tally should be made on that block.
//!
//! \param block Block to check.
//! \return
//!
bool IsTallyTrigger(const CBlockIndex* block);
