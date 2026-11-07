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

#include <cstdint>
#include <string>

namespace interfaces {

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

// Marshalability pin (interfaces/marshal.h).
INTERFACES_ASSERT_MARSHALABLE(GuiStateSnapshot);
} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_GUI_STATE_H
