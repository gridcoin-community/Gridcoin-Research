// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "txorder.h"

namespace GRC {

bool TxOrderLess(const TxOrderKey& a, const TxOrderKey& b)
{
    if (a.time != b.time) return a.time > b.time;   // time DESCENDING
    if (a.hash != b.hash) return a.hash < b.hash;   // hash ASCENDING
    return a.idx < b.idx;                           // idx ASCENDING
}

} // namespace GRC
