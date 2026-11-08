// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_GUI_STATE_H
#define GRIDCOIN_INTERFACES_GUI_STATE_H

#include "interfaces/marshal.h"
#include "interfaces/mrc.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/wallet.h"

#include "amount.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace interfaces {

class Handler;

//! The state the GUI shows that needs cs_main to compute, computed by the node
//! and pushed (the node's GUI state cache, node/gui_cache.h). The GUI keeps its
//! last copy and reads that; it never takes cs_main to refresh it.
//!
//! Two kinds of part. The ones above the line are computed always, because they
//! have a meaning while the node is syncing. The ones below it mean nothing
//! until the node is synced (OutOfSyncByAge), so they are computed only then,
//! and each carries a present flag: absent means "unavailable until the wallet
//! is synced". A part's own gates (the MRC version gate inside MRCSnapshot) stay
//! inside its fields. All fields are pointer-free values.
struct GuiStateSnapshot
{
    //! Increases by one with every update, whatever changed, so a reader that
    //! missed one can tell. 0 means not yet computed: the GUI then shows the
    //! state it shows at startup.
    uint64_t sequence = 0;

    // Computed always.

    int height = -1;              //!< Chain tip height.
    int64_t tip_time = 0;         //!< Chain tip block time (Unix seconds).
    double difficulty = 0;        //!< GRC::GetCurrentDifficulty().
    bool initial_download = true; //!< IsInitialBlockDownload().
    bool out_of_sync = true;      //!< OutOfSyncByAge(): the gate of the synced parts below.
    int peers_height = -1;        //!< GetNumBlocksOfPeers().
    double net_weight = 0;        //!< GRC::GetEstimatedNetworkWeight(), in stake-weight units as StakingStatus reports it.
    bool staking = false;         //!< The miner is staking (MinerStatus::StakingActive()).
    double coin_weight = 0;       //!< The miner's last search: the wallet's coin weight.
    std::string miner_errors;     //!< MinerStatus::FormatErrors(): why the miner is not staking.
    bool has_balances = false;    //!< False when the node runs without a wallet.
    WalletBalances balances;
    //! Whether the PSGT pool is available, and why not. Pushed always, although
    //! it is "until synced" state: its out-of-sync reason is what tells the pool
    //! page that it is syncing.
    PSGTPoolStatus psgt_status;

    // Computed only when synced (out_of_sync false); each with its present flag.

    //! The unboosted MRC quote: an unsigned trial claim at the tip, with the
    //! wallet's mature balance. A boosted quote is a GUI request.
    bool has_mrc = false;
    MRCSnapshot mrc;
    //! GRC::GetEstimatedTimetoStake(), in seconds; 0 when the miner is not staking.
    bool has_etts = false;
    double etts_seconds = 0;
    bool has_researcher = false;
    ResearcherSnapshot researcher;
};

//!
//! \brief The GUI's view of the node's GUI state cache (node/gui_cache.h): the
//! snapshot the node pushes, and the requests a GUI may make of it.
//!
//! No method takes cs_main or waits for the node's work: a read takes the
//! cache's leaf lock alone, and a request only records itself for the cache's
//! timer, which takes cs_main on its own thread and answers through a
//! registered fn. In the multiprocess build each call is an IPC round trip to
//! the node, which answers at once, and the fns are called back from it.
//!
//! The cache computes nothing while no GUI is subscribed (handleChanged): a node
//! with no GUI attached pays nothing for it.
//!
class GuiState
{
public:
    virtual ~GuiState() = default;

    //! The cache's current snapshot, copied under its leaf lock, never cs_main.
    //! For a GUI that attaches, or misses a push; its sequence is 0 before the
    //! first update.
    virtual GuiStateSnapshot current() = 0;

    //! Receives every snapshot the cache publishes, on the cache's timer thread,
    //! holding no lock. The sequences never decrease but may repeat, so keep a
    //! snapshot only when its sequence is higher than the last one's. The fn must
    //! only hand the snapshot off (interfaces/handler.h, THE HANDLER CONTRACT).
    using ChangedFn = std::function<void(const GuiStateSnapshot&)>;

    //! Subscribe to the cache's snapshots (GuardNotify-wrapped). The cache
    //! computes only while at least one subscription is alive. The first one
    //! (none was alive) also asks for a forced update, so a GUI that attaches
    //! gets a computed snapshot within one timer cycle. Disconnecting or
    //! destroying the Handler ends the subscription.
    virtual std::unique_ptr<Handler> handleChanged(ChangedFn fn) = 0;

    //! Ask for a fresh snapshot: the cache's timer makes a forced update, not
    //! held to the cache's interval, and delivers it to the handleChanged fns.
    //! Returns at once. With no subscription alive there is no one to answer,
    //! and nothing is computed.
    virtual void requestUpdate() = 0;

    //! Ask for the MRC figures at \p fee_boost (the boosted trial claim, as
    //! interfaces::MRC::snapshot computes it, unsigned). Returns at once. The
    //! cache's timer computes it under cs_main, not held to the cache's interval,
    //! and answers the handleMRCQuote fns with \p request_id. One quote is
    //! pending at a time: a request made before the timer takes the last one
    //! replaces it, so only the latest is answered.
    virtual void requestMRCQuote(CAmount fee_boost, uint64_t request_id) = 0;

    //! Receives each quote answer, on the cache's timer thread, holding no lock,
    //! with the id of the request it answers.
    using MRCQuoteFn = std::function<void(uint64_t request_id, const MRCSnapshot&)>;

    //! Subscribe to the quote answers (GuardNotify-wrapped).
    virtual std::unique_ptr<Handler> handleMRCQuote(MRCQuoteFn fn) = 0;
};

//! Return a GuiState over the node's GUI state cache, or null if the cache is
//! not running (node::GetGuiCache()). The cache outlives it: the node starts one
//! per process and keeps it until exit.
std::unique_ptr<GuiState> MakeGuiState();

// Marshalability pin (interfaces/marshal.h).
INTERFACES_ASSERT_MARSHALABLE(GuiStateSnapshot);
} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_GUI_STATE_H
