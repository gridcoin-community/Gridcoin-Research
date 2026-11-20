// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_ACCESS_POLICY_H
#define GRIDCOIN_NODE_ACCESS_POLICY_H

#include <string>

//! A wallet is loaded and pwalletMain->CanUseKeys(): unencrypted, fully unlocked, or a capability
//! active on this thread. False when no wallet is loaded.
bool HoldsElevation();

//! The settings the GUI reads through interfaces::Node: contractchangetoinputaddress, disableupdatecheck,
//! enablesidestaking, enablestakesplit, minstakesplitvalue, proxy, reservebalance, showorphans, staking,
//! stakingefficiency, upnp. The getters answer every other name with its default (isSettingSet: false).
bool IsGuiReadableSetting(const std::string& name);

#endif // GRIDCOIN_NODE_ACCESS_POLICY_H
