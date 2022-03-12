// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_FILTER_H
#define GRIDCOIN_VOTING_FILTER_H

namespace GRC {
//!
//! \brief Bit flags that represents attributes to filter polls by.
//!
enum PollFilterFlag
{
    NO_FILTER = 0, //!< No active filter. Include all results.
    ACTIVE = 1,    //!< Include unfinished polls.
    FINISHED = 2,  //!< Include finished polls.
};
} // namespace GRC

#endif // GRIDCOIN_VOTING_FILTER_H
