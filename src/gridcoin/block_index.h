// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/cpid.h"

namespace GRC {
//!
//! \brief Block index fields specific to research reward claims.
//!
//! Non-researcher nodes produce nearly 75% of Gridcoin's block chain. By
//! allocating the researcher context only for blocks staked with a CPID,
//! we conserve 24 bytes for each non-research entry in the block index.
//!
//! Testnet exhibits the opposite behavior pattern--researchers stake the
//! majority of blocks. The memory optimization provides no benefit for a
//! testnet node, but we prefer to tune performance for mainnet here.
//!
class ResearcherContext
{
public:
    Cpid m_cpid;
    int64_t m_research_subsidy;
    double m_magnitude;

    ResearcherContext(
        const Cpid cpid,
        const int64_t research_subsidy,
        const double magnitude)
        : m_cpid(cpid)
        , m_research_subsidy(research_subsidy)
        , m_magnitude(magnitude)
    {
    }
};
} // namespace GRC
