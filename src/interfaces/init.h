// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_INIT_H
#define GRIDCOIN_INTERFACES_INIT_H

#include <memory>

class CWallet;

namespace interfaces {

class MRC;
class Node;
class ResearcherContext;
class SideStakeManager;
class StakingStatus;
class VotingManager;
class Wallet;
class WalletTxSource;

//! Per-process bootstrap interface: hands out the other interfaces. In the
//! monolithic build MakeGridcoinInit() returns an implementation whose
//! factories construct the in-process wrappers; in the Stage 2 multiprocess
//! build the GUI process receives an Init proxy whose factories return IPC
//! proxies instead (after the authentication and matching handshake described
//! in doc/multiprocess_design.md).
//!
//! The default implementations return nullptr so each process type overrides
//! only what it supports.
class Init
{
public:
    virtual ~Init();

    //! The defaults return nullptr and are defined out of line (init.cpp):
    //! inline definitions would require every includer to see the complete
    //! interface types just to instantiate the unique_ptr deleters.
    virtual std::unique_ptr<Node> makeNode();

    virtual std::unique_ptr<StakingStatus> makeStakingStatus();

    //! Returns the Manual Research Claim interface over the node's single
    //! wallet. Returns nullptr for a null wallet; \p wallet must outlive the
    //! returned object.
    virtual std::unique_ptr<MRC> makeMRC(CWallet* wallet);

    //! Returns the sidestake registry interface (the unified mandatory + local
    //! sidestake table and local add/edit/delete commands). Over the global
    //! registry, so no wallet argument.
    virtual std::unique_ptr<SideStakeManager> makeSideStakeManager();

    //! Returns the voting interface (poll table over the core result cache, and
    //! poll/vote submission commands) over the global poll registry and the
    //! node's single wallet.
    virtual std::unique_ptr<VotingManager> makeVotingManager();

    //! Returns the researcher/beacon interface (identity/magnitude/accrual/beacon
    //! snapshot, the fused project table, and beacon/mode commands) over the
    //! global researcher registries and the node's single wallet. Returns nullptr
    //! for a null wallet; \p wallet must outlive the returned object.
    virtual std::unique_ptr<ResearcherContext> makeResearcherContext(CWallet* wallet);

    //! Returns the interface for the node's single wallet. May return nullptr
    //! before wallet startup completes.
    virtual std::unique_ptr<Wallet> makeWallet();

    //! Returns a transaction-table source over \p wallet (the windowed
    //! tx-table store, its worker, and the producer subscriptions). The
    //! returned object owns the node-side machinery, so a headless process
    //! that never calls this pays nothing. Returns nullptr for a null wallet.
    //! \p wallet must outlive the returned object.
    virtual std::shared_ptr<WalletTxSource> makeWalletTxSource(CWallet* wallet);
};

//! Return the monolithic-build Init implementation.
std::unique_ptr<Init> MakeGridcoinInit();

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_INIT_H
