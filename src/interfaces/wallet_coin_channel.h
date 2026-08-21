// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_WALLET_COIN_CHANNEL_H
#define GRIDCOIN_INTERFACES_WALLET_COIN_CHANNEL_H

#include "interfaces/marshal.h"

#include "primitives/transaction.h"
#include "uint256.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <variant>
#include <vector>

//! Boundary types for the wallet coin channel: the windowed coin-control
//! selection stack (issue #3183), the coin-side sibling of the wallet
//! transaction channel (interfaces/wallet_tx_channel.h,
//! doc/multiprocess_design.md §6). The abstract interfaces::WalletCoinSource
//! (interfaces/wallet_coin_source.h) owns the store behind these types; the
//! GUI drives it through that handle and only ever sees the value types
//! declared here — pointer-free, Qt-free, marshalable.
//!
//! The coin channel differs from the tx channel in one structural way: reads
//! and events are keyed by SCOPE as well as view. A scope is one independently
//! windowed row universe:
//!
//!   - the FLAT scope (scope string ""): the single flat sequence of all
//!     unspent outputs (list display mode);
//!   - one GROUP scope per address (scope string == the group address): that
//!     address's child outputs (tree display mode);
//!   - the group DIRECTORY: the sequence of group rows itself. It has its own
//!     event payload types (CoinGroups*) rather than a scope string.
//!
//! Each scope carries its own event high-water so a consumer can hold one
//! reconciliation cache per scope (one per expanded tree group) without
//! events for other scopes invalidating its content fetches. See the
//! high_water contract on CoinRowsResult.

namespace GRC {

//!
//! \brief View identifiers stamped on coin-channel events. This is the coin
//! channel's own id space, unrelated to the tx channel's GRC::VIEW_* values.
//!
constexpr int VIEW_COIN_CONTROL = 1;  //!< CoinControlDialog
constexpr int VIEW_COIN_WIZARD  = 2;  //!< ConsolidateUnspentWizardSelectInputsPage

//!
//! \brief The two display modes of the coin-control selection view. Flat is a
//! single windowed sequence of outputs; Tree groups outputs under address
//! rows with lazily realized, independently windowed children.
//!
enum class CoinViewMode {
    Flat,
    Tree,
};

//!
//! \brief Qt-free mirror of the coin-selection view's sortable columns, the
//! sort vocabulary pushed down through setViewSort. The Qt model's column enum
//! static_asserts against these so the two cannot drift (the tx channel's
//! TxSortColumn precedent).
//!
//! Child rows sort by the named key with the outpoint as the deterministic
//! tiebreak. Group (parent) rows have no date/confirmations of their own and
//! fall back to the address for those columns; they sort by total amount for
//! COINCOL_AMOUNT.
//!
enum CoinSortColumn {
    COINCOL_AMOUNT  = 0,
    COINCOL_LABEL   = 1,
    COINCOL_ADDRESS = 2,
    COINCOL_DATE    = 3,
    COINCOL_CONFS   = 4,
};

//!
//! \brief One unspent output row (the coin analogue of TransactionRecord).
//!
//! group_address is the change-walked grouping key: for a change output it is
//! the address of the ancestor output that funded the chain (the walk the
//! legacy listCoins() performed), otherwise it equals address. label is the
//! address-book label of group_address ("" when unlabeled).
//!
//! depth is filled at serve time from the store's cached tip height (the
//! store keeps only the static block_height), so a chain-tip advance never
//! reorders a confirmations-sorted view — height order IS confirmations
//! order. block_height is -1 for an unconfirmed output.
//!
struct CoinRecord
{
    COutPoint   outpoint;
    int64_t     amount{0};
    std::string address;
    std::string group_address;
    std::string label;
    int64_t     time{0};
    int         block_height{-1};
    int         depth{0};
    bool        is_change{false};
};

//!
//! \brief One tree-mode group (parent) row. All aggregates are computed
//! server-side so the consumer renders parent rows — including the selection
//! tristate — without materializing any children.
//!
//! direct_output_count counts the group's non-change members. The
//! consolidation flows need it: their address pickers historically excluded
//! pure-change entries, which output_count alone cannot express (a group can
//! consist entirely of change walked back to a spent address).
//!
//! The tristate denominator is output_count: selected_count == 0 renders
//! unchecked, == output_count checked, anything else partial.
//!
struct CoinGroupInfo
{
    std::string address;
    std::string label;
    int64_t     total_amount{0};
    int         output_count{0};
    int         direct_output_count{0};
    int         selected_count{0};
    int64_t     selected_amount{0};
};

//!
//! \brief Atomic result of a windowed coin read (getRows / getGroupRows): the
//! requested slice plus the scope's metadata, all sampled under the SAME
//! store-lock hold (the tx channel's GRC::RowsResult generalized to scopes).
//!
//! high_water is SCOPE-LOCAL WITH A PER-VIEW FLOOR: the store tracks, per
//! scope, the seqno of the last event emitted for that scope, plus a per-view
//! floor seqno advanced by every broadcast event (CoinReset, resync). A read
//! returns max(scope_seqno, view_floor). The consumer contract is exactly the
//! tx channel's: adopt a content fetch only when its epoch matches AND its
//! high_water equals the scope cache's structural seqno; never advance the
//! structural seqno from a fetch. The floor guarantees a cache reseeded after
//! a broadcast event observes a high-water at least the broadcast's seqno, so
//! a quiescent wallet can never leave a scope's gate permanently mismatched.
//! Global seqnos are monotonic across all scopes; a scope's own stream
//! therefore has gaps, which the ≤-based structural skip gate tolerates.
//!
struct CoinRowsResult
{
    std::vector<CoinRecord> records; //!< the [first, first+count) slice (a copy)
    int      total_accepted{0};      //!< the scope's full row count (virtual rowCount)
    uint64_t epoch{0};               //!< view sort/mode generation at the read instant
    uint64_t high_water{0};          //!< max(scope high-water, view floor) at the read
};

//!
//! \brief Atomic result of a group-directory read (getGroups): the group-row
//! slice plus the directory's metadata under one store-lock hold. Same
//! epoch / high_water contract as CoinRowsResult, with the directory as the
//! scope.
//!
struct CoinGroupsResult
{
    std::vector<CoinGroupInfo> groups;
    int      total_groups{0};
    uint64_t epoch{0};
    uint64_t high_water{0};
};

//!
//! \brief Result of a single-outpoint selection toggle (setSelected).
//!
//! applied is false when the outpoint is unknown to the store (e.g. the coin
//! was spent and removed between the render and the click) — the GUI must not
//! apply the toggle to its own selection set in that case, or the two sides
//! diverge (a "phantom selection" the event stream can never repair, because
//! the outpoint's removal event was emitted before the phantom insert).
//! group carries the refreshed aggregates of the toggled coin's group so the
//! consumer updates the parent tristate row synchronously.
//!
struct CoinSelectionUpdate
{
    bool applied{false};
    CoinGroupInfo group;
};

//!
//! \brief Result of a server-side bulk selection mutation (selectGroup /
//! selectAll / applyValueFilter): the exact outpoint delta the store applied
//! to its selection mirror. The GUI applies added/removed — and ONLY these —
//! to its authoritative interfaces::WalletCoinControl selection, keeping the
//! two sides consistent by construction. culled is true when applyValueFilter
//! trimmed the selection to its max_inputs cap (the consolidation flows
//! surface this to the user).
//!
struct CoinBulkSelectionResult
{
    std::vector<COutPoint> added;
    std::vector<COutPoint> removed;
    bool culled{false};
};

//!
//! \brief Producer payload: rows were inserted into \p scope of \p view_id at
//! a store-computed position. One payload == one contiguous insert bracket in
//! that scope's cache. A child insert also changes its group's directory row
//! (count/amount aggregates), which arrives as a separate CoinGroupsChanged —
//! or a CoinGroupsRemoved + CoinGroupsInserted pair when the aggregate change
//! moves the group under the directory's current sort.
//!
struct CoinRowsInsertedPayload
{
    int view_id;
    uint64_t epoch;
    std::string scope;
    int position;
    std::vector<CoinRecord> records;
};

//!
//! \brief Producer payload: rows [position, position+count) of \p scope were
//! removed. Carries the removed outpoints so the drain path can prune them
//! from the GUI-side selection UNCONDITIONALLY — before, and independent of,
//! the seqno-gated cache application (a cache that was reseeded past this
//! event skips the structural delta, but the selection prune must still
//! happen; pruning is idempotent).
//!
struct CoinRowsRemovedPayload
{
    int view_id;
    uint64_t epoch;
    std::string scope;
    int position;
    int count;
    std::vector<COutPoint> outpoints;
};

//!
//! \brief Producer payload: rows [first, first+count) of \p scope changed in
//! place without moving (e.g. a label re-snapshot that is not the active sort
//! key). The consumer re-reads them (dataChanged / windowed refetch).
//!
struct CoinRowsChangedPayload
{
    int view_id;
    uint64_t epoch;
    std::string scope;
    int first;
    int count;
};

//!
//! \brief Producer payload: group rows were inserted into the directory at a
//! store-computed position.
//!
struct CoinGroupsInsertedPayload
{
    int view_id;
    uint64_t epoch;
    int position;
    std::vector<CoinGroupInfo> groups;
};

//!
//! \brief Producer payload: directory rows [position, position+count) were
//! removed. A group whose aggregate change MOVES it under the current
//! directory sort is emitted as Remove + Insert (never an in-place change),
//! so the consumer's directory order tracks the store's positionally.
//!
struct CoinGroupsRemovedPayload
{
    int view_id;
    uint64_t epoch;
    int position;
    int count;
};

//!
//! \brief Producer payload: directory rows [first, first+count) changed in
//! place (aggregates updated, position unchanged under the current sort).
//!
struct CoinGroupsChangedPayload
{
    int view_id;
    uint64_t epoch;
    int first;
    int count;
};

//!
//! \brief Producer payload: the view's cursor was rebuilt wholesale (sort or
//! mode change, or a full store resync). The consumer drops every scope cache
//! it holds for \p view_id and reseeds each from fresh fetches. This is a
//! BROADCAST event: it advances the view's floor seqno (see CoinRowsResult),
//! never any single scope's high-water.
//!
struct CoinResetPayload
{
    int view_id;
    uint64_t epoch;
};

//!
//! \brief Producer payload: the chain tip advanced (the coin channel's
//! analogue of the tx channel's ChainTipChangedPayload). Depth is computed at
//! serve time from the store's cached tip height, so no rows move and no
//! per-row events are needed — the consumer repaints the confirmations column
//! of its cached rows. This payload never touches scope high-waters or view
//! floors.
//!
struct CoinDepthRefreshPayload
{
    int height;
};

using WalletCoinEventPayload = std::variant<
    CoinRowsInsertedPayload,
    CoinRowsRemovedPayload,
    CoinRowsChangedPayload,
    CoinGroupsInsertedPayload,
    CoinGroupsRemovedPayload,
    CoinGroupsChangedPayload,
    CoinResetPayload,
    CoinDepthRefreshPayload>;

//!
//! \brief A single event in the wallet→GUI coin event channel. seqno is
//! assigned by the source's event queue under its mutex — unique and
//! monotonic across all producer threads and ALL scopes (per-scope streams
//! therefore contain gaps; see CoinRowsResult).
//!
struct WalletCoinEvent
{
    uint64_t              seqno;
    int64_t               emit_time_us;
    WalletCoinEventPayload payload;
};

//!
//! \brief Hasher for COutPoint-keyed unordered containers (the store's
//! by-outpoint index, tests' synthetic tables). The tx hash is high-entropy;
//! folding in n separates the outputs of one transaction.
//!
struct OutPointHasher
{
    std::size_t operator()(const COutPoint& out) const noexcept
    {
        return static_cast<std::size_t>(out.hash.GetUint64(0)) ^
               (static_cast<std::size_t>(out.n) << 1);
    }
};

// Marshalability pins (interfaces/marshal.h): each DTO above must stay a
// copyable value type the node side can fill and ship across the boundary.
INTERFACES_ASSERT_MARSHALABLE(CoinRecord);
INTERFACES_ASSERT_MARSHALABLE(CoinGroupInfo);
INTERFACES_ASSERT_MARSHALABLE(CoinRowsResult);
INTERFACES_ASSERT_MARSHALABLE(CoinGroupsResult);
INTERFACES_ASSERT_MARSHALABLE(CoinSelectionUpdate);
INTERFACES_ASSERT_MARSHALABLE(CoinBulkSelectionResult);
INTERFACES_ASSERT_MARSHALABLE(CoinRowsInsertedPayload);
INTERFACES_ASSERT_MARSHALABLE(CoinRowsRemovedPayload);
INTERFACES_ASSERT_MARSHALABLE(CoinRowsChangedPayload);
INTERFACES_ASSERT_MARSHALABLE(CoinGroupsInsertedPayload);
INTERFACES_ASSERT_MARSHALABLE(CoinGroupsRemovedPayload);
INTERFACES_ASSERT_MARSHALABLE(CoinGroupsChangedPayload);
INTERFACES_ASSERT_MARSHALABLE(CoinResetPayload);
INTERFACES_ASSERT_MARSHALABLE(CoinDepthRefreshPayload);
INTERFACES_ASSERT_MARSHALABLE(WalletCoinEvent);

} // namespace GRC

#endif // GRIDCOIN_INTERFACES_WALLET_COIN_CHANNEL_H
