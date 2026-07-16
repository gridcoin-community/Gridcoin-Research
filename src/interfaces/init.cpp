// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/init.h"

#include "interfaces/node.h"
#include "interfaces/staking.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_tx_source.h"

namespace interfaces {

Init::~Init() = default;

std::unique_ptr<Node> Init::makeNode() { return nullptr; }

std::unique_ptr<StakingStatus> Init::makeStakingStatus() { return nullptr; }

std::unique_ptr<Wallet> Init::makeWallet() { return nullptr; }

std::shared_ptr<WalletTxSource> Init::makeWalletTxSource(CWallet* /*wallet*/) { return nullptr; }

} // namespace interfaces
