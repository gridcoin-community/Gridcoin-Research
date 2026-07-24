// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/init.h"

#include "interfaces/mrc.h"
#include "interfaces/node.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/staking.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_tx_source.h"

namespace interfaces {

Init::~Init() = default;

bool Init::isCoreReady() { return true; }

// Handshake surface (doc/multiprocess_design.md section 4.3). The base default
// denies: authentication is only meaningful for an Init served over IPC, and the
// serving wrapper (ipc::MakeServeInit) overrides this to grant on a valid cookie.
// Denying by default means a base Init accidentally served directly never grants
// unauthenticated access. (The monolithic build never calls authenticate() -- it
// uses the in-process Init without a handshake.) The build/identity snapshots are
// empty for the same reason; the serving Init overrides them.
bool Init::authenticate(const std::string& /*cookie*/) { return false; }

BuildInfo Init::getBuildInfo() { return BuildInfo{}; }

NodeIdentity Init::getIdentity() { return NodeIdentity{}; }

std::unique_ptr<Node> Init::makeNode() { return nullptr; }

std::unique_ptr<StakingStatus> Init::makeStakingStatus() { return nullptr; }

std::unique_ptr<MRC> Init::makeMRC() { return nullptr; }

std::unique_ptr<SideStakeManager> Init::makeSideStakeManager() { return nullptr; }

std::unique_ptr<VotingManager> Init::makeVotingManager() { return nullptr; }

std::unique_ptr<ResearcherContext> Init::makeResearcherContext() { return nullptr; }

std::unique_ptr<PSGTPoolContext> Init::makePSGTPoolContext() { return nullptr; }

std::unique_ptr<Wallet> Init::makeWallet() { return nullptr; }

std::shared_ptr<WalletTxSource> Init::makeWalletTxSource() { return nullptr; }

} // namespace interfaces
