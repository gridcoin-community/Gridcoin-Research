// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/access_policy.h"

#include "init.h"
#include "wallet/wallet.h"

#include <set>

bool HoldsElevation()
{
    // Takes no lock: CanUseKeys() reads the unlock scope and this thread's grant.
    return pwalletMain != nullptr && pwalletMain->CanUseKeys();
}

bool IsGuiReadableSetting(const std::string& name)
{
    static const std::set<std::string> gui_readable{
        "contractchangetoinputaddress",
        "disableupdatecheck",
        "enablesidestaking",
        "enablestakesplit",
        "minstakesplitvalue",
        "proxy",
        "reservebalance",
        "showorphans",
        "staking",
        "stakingefficiency",
        "upnp",
    };

    return gui_readable.count(name) != 0;
}
