// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/access_policy.h"

#include "init.h"
#include "wallet/wallet.h"

bool HoldsElevation()
{
    // Takes no lock: CanUseKeys() reads the unlock scope and this thread's grant.
    return pwalletMain != nullptr && pwalletMain->CanUseKeys();
}
