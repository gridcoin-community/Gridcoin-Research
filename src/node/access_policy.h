// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_ACCESS_POLICY_H
#define GRIDCOIN_NODE_ACCESS_POLICY_H

//! A wallet is loaded and pwalletMain->CanUseKeys(): unencrypted, fully unlocked, or a capability
//! active on this thread. False when no wallet is loaded.
bool HoldsElevation();

#endif // GRIDCOIN_NODE_ACCESS_POLICY_H
