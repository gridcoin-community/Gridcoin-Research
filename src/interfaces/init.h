// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_INIT_H
#define GRIDCOIN_INTERFACES_INIT_H

#include "interfaces/marshal.h"

#include <cstdint>
#include <memory>
#include <string>

class CWallet;

namespace interfaces {

class MRC;
class Node;
class PSGTPoolContext;
class ResearcherContext;
class SideStakeManager;
class StakingStatus;
class VotingManager;
class Wallet;
class WalletTxSource;

//! The node's compile-time build fingerprint. After authentication the GUI
//! compares its own embedded fingerprint against the node's, per the matching
//! policy in doc/multiprocess_design.md section 4.2 (schema_major mismatch is a
//! hard fail, git_commit mismatch a dismissible soft-warn banner, etc.). Value
//! snapshot; crosses the IPC boundary.
struct BuildInfo
{
    std::string git_commit;      //!< BUILD_GIT_COMMIT (may carry a -dirty suffix).
    std::string built_at;        //!< Build timestamp (informational, About dialog).
    uint32_t schema_major = 0;   //!< Cap'n Proto schema major (wire compatibility).
    uint32_t schema_minor = 0;
    uint32_t protocol_version = 0;
};

//! The node's per-datadir identity. The GUI persists the expected node_id +
//! network on first successful connect and hard-fails on every later launch when
//! a different node_id answers at the same socket path (a moved datadir, a
//! -reindex rotation, or a foreign node squatting the socket). See
//! doc/multiprocess_design.md section 4.2. Value snapshot; crosses IPC.
struct NodeIdentity
{
    std::string node_id;      //!< Random per-datadir UUID (from node_identity.json).
    std::string network;      //!< Chain name ("main" / "test" / "regtest").
    std::string datadir;      //!< Canonical datadir path.
    std::string genesis_hash; //!< Genesis block hash, hex.
};

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

    //! Whether core initialization (ThreadAppInit2 / AppInit2) has completed. The
    //! GUI polls this while pumping its event loop, instead of reading the raw
    //! bGridcoinCoreInitComplete global; a Stage-2 IPC readiness handshake later
    //! replaces the poll. The default (no core to wait on) reports ready.
    virtual bool isCoreReady();

    //! First IPC call of the connect handshake (doc/multiprocess_design.md
    //! section 4.3, step 5): the GUI presents the 256-bit cookie it read from
    //! <datadir>/<network>/ipc.cookie; the node compares it constant-time against
    //! the cookie it wrote at startup and returns whether it matches. Build and
    //! identity information are never served to an unauthenticated peer, so the
    //! GUI hard-fails and disconnects on a false result. Authentication precedes
    //! every other exchange. The base default denies (returns false); only the
    //! serving Init wrapper grants on a valid cookie, so a base Init served
    //! directly never leaks access. The monolithic build never calls this.
    virtual bool authenticate(const std::string& cookie);

    //! The node's build fingerprint, for the schema/protocol/commit comparison in
    //! the handshake (section 4.2). Only meaningful after a successful
    //! authenticate(). The base default returns an empty snapshot; the node's
    //! serving Init populates it from the process's embedded build constants.
    virtual BuildInfo getBuildInfo();

    //! The node's per-datadir identity, compared against the GUI's stored
    //! expectation (section 4.2). Only meaningful after authenticate(). The base
    //! default returns an empty snapshot; the node's serving Init populates it.
    virtual NodeIdentity getIdentity();

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

    //! Returns the PSGT pool + multisig-workbench interface (pool table reads/
    //! commands and the PSGT sign/combine/submit/finalize operations) over the
    //! global PSGT pool and the node's single wallet. Returns nullptr for a null
    //! wallet; \p wallet must outlive the returned object.
    virtual std::unique_ptr<PSGTPoolContext> makePSGTPoolContext(CWallet* wallet);

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

// Marshalability pins (interfaces/marshal.h): the handshake DTOs cross the IPC
// boundary and must stay copyable value types.
INTERFACES_ASSERT_MARSHALABLE(BuildInfo);
INTERFACES_ASSERT_MARSHALABLE(NodeIdentity);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_INIT_H
