// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINSELECTIONMODEL_H
#define BITCOIN_QT_COINSELECTIONMODEL_H

#include "interfaces/wallet_coin_channel.h"
#include "interfaces/wallet_coin_source.h"
#include "qt/windowcache.h"

#include <QAbstractItemModel>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class WalletModel;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace interfaces {
struct WalletCoinControl;
}

//!
//! \brief Windowed two-mode (flat + tree) consumer model for coin-control
//! selection (issue #3183) — the codebase's first tree QAbstractItemModel.
//!
//! Registers one view in the producer interfaces::WalletCoinSource and
//! renders:
//!  - FLAT mode: one virtual root-level row per unspent output, windowed
//!    through a GRC::WindowCache<CoinRecord> (the DetailedTxModel shape);
//!  - TREE mode: one root row per address group, with the group DIRECTORY
//!    fully materialized client-side (group counts are 10^2..10^3 — the
//!    pathological axis is children-per-group), and children realized
//!    lazily on expand into one WindowCache per expanded group. Collapsing
//!    un-realizes (bounded memory), bracketed by begin/endRemoveRows.
//!
//! INDEX SCHEME (the load-bearing invariant): internalId() encodes 0 for a
//! root-level index and (stable group id + 1) for a child index. The id is a
//! per-generation token from a registry (address <-> id, never reused within
//! a generation; a Reset renumbers, which is safe because a model reset
//! invalidates every persistent index). It is NEVER the directory row: Qt
//! preserves internalId verbatim while adjusting persistent-index rows, so a
//! row-encoding scheme corrupts held child indexes whenever a live directory
//! re-slot shifts group rows (which the default amount sort does on every
//! coin arrival).
//!
//! Selection: interfaces::WalletCoinControl remains authoritative. A child
//! toggle goes source.setSelected -> (validated) coinControl mutation; a
//! parent toggle goes source.selectGroup -> the returned outpoint delta is
//! applied to coinControl. Parent tristate renders from the directory's
//! server-computed aggregates — never from realized children. Removal events
//! prune coinControl unconditionally at application time, before and
//! independent of the seqno-gated cache routing.
//!
//! Events arrive via WalletModel's single coin drain (coinEventsDrained);
//! this model filters to its view id. data() is pure: off-window rows render
//! placeholders (and, critically, NO CheckStateRole and no UserCheckable
//! flag — a blank checkbox that cannot be mis-clicked, not a lying
//! "unchecked").
//!
class CoinSelectionModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum ColumnIndex {
        COLUMN_CHECKBOX = 0,
        COLUMN_AMOUNT,
        COLUMN_LABEL,
        COLUMN_ADDRESS,
        COLUMN_DATE,
        COLUMN_CONFIRMATIONS,
    };

    CoinSelectionModel(WalletModel* wallet_model,
                       interfaces::WalletCoinControl* coin_control,
                       int view_id,
                       QObject* parent = nullptr);

    //! TEST SEAM: the same model over a bare source, with no WalletModel.
    //! WalletModel supplies exactly two things -- the display unit and the
    //! drain pump -- so a test that injects batches through
    //! applyCoinEventBatch() itself needs neither. Registers the view and
    //! seeds from the source directly; never used by the GUI.
    CoinSelectionModel(interfaces::WalletCoinSource& source,
                       interfaces::WalletCoinControl* coin_control,
                       int view_id,
                       QObject* parent = nullptr);
    //! Unregisters the view node-side (must run while the source is alive —
    //! the dialog is modal and stack-scoped, so this always precedes the
    //! bitcoin.cpp source teardown).
    ~CoinSelectionModel() override;

    // QAbstractItemModel
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    //! Switch flat/tree display mode (the radio buttons). Reset semantics.
    void setDisplayMode(GRC::CoinViewMode mode);
    GRC::CoinViewMode displayMode() const { return m_mode; }

    //! True while the STORE's initial scan is still outstanding — the dialog
    //! renders a loading state meanwhile. Deliberately not "until the first
    //! Reset": on a cold store the registration Reset is published against a
    //! not-yet-scanned table and arrives long before the scan finishes, and a
    //! model attaching to an already-scanned store is not loading at all. The
    //! gate is WalletModel::isCoinSourceLoaded() / coinSourceLoadFinished().
    bool isLoading() const { return m_loading; }

    //! Un-realize a collapsed group's child cache (bounded memory). The view
    //! calls this from its collapsed() signal, on a DEFERRED turn.
    void releaseGroup(const QModelIndex& group_index);

    //! Record that the user collapsed \p address, without touching structure.
    //!
    //! Split out of releaseGroup() because the two halves need different
    //! timing. Un-realizing has to be deferred (beginRemoveRows re-entrantly
    //! from QTreeView's collapse dispatch overlaps structural-change windows),
    //! but the INTENT cannot be: releaseGroup resolves a QPersistentModelIndex
    //! that a re-slot arriving first invalidates, and it then returns before
    //! pruning m_expanded -- so the GroupInsert half of that same re-slot sees
    //! the address still expanded and queues a re-expansion of the branch the
    //! user just closed. Erasing from a std::set opens no bracket and is safe
    //! to call synchronously from the collapse dispatch.
    void noteCollapsed(const std::string& address);

    //! The directory index of the group with stable id \p group_id, or an
    //! invalid index when that id does not currently name a directory row.
    //! The view resolves a deferred re-expand through this: a stable id
    //! survives the directory re-slots a queued continuation can race with,
    //! where a row number would not.
    QModelIndex groupIndexForId(int group_id) const;

    // ---- selection operations (the dialog's buttons) --------------------

    //! Select/deselect everything, server-side; applies the returned outpoint
    //! delta to coinControl. Returns the number of outpoints that changed.
    int selectAll(bool selected);

    //! The relocated filterInputsByValue (prune-only). Returns true when the
    //! max_inputs cap culled anything (the consolidation warning).
    bool applyValueFilter(bool less_or_equal, qint64 value, unsigned int max_inputs);

    // ---- accessors for the dialog (context menu / consolidation) --------

    bool isGroup(const QModelIndex& index) const;
    //! The outpoint of a child/flat row; false for a group row or placeholder.
    bool outpointAt(const QModelIndex& index, COutPoint& out) const;
    QString addressAt(const QModelIndex& index) const;
    QString labelAt(const QModelIndex& index) const;
    QString amountTextAt(const QModelIndex& index) const;
    QString txHashTextAt(const QModelIndex& index) const;

    //! The full group directory (address-ordered, view-independent) with
    //! aggregates — the consolidation address pickers.
    std::vector<GRC::CoinGroupInfo> groupDirectory() const;

    //! How many children a directory row has, from the server-side aggregate
    //! — i.e. WITHOUT realizing the group (rowCount() reports 0 until the
    //! branch is realized). 0 for an out-of-range row or in flat mode.
    int groupOutputCount(int directory_row) const;

signals:
    //! A user-driven selection mutation went through (toggle / select-all /
    //! value filter) — the dialog refreshes the summary labels.
    void selectionChanged();
    //! The store's wallet scan completed and was applied (isLoading() flipped
    //! false).
    void loadingFinished();
    //! Groups the user had expanded were RE-SLOTTED within a drained batch and
    //! came back collapsed (#3228 item 3). A sort-key move is published as
    //! GroupRemove + GroupInsert -- an in-place GroupChange would desynchronize
    //! the consumer's positional coordinates -- and the removal takes the
    //! realized child cache and QTreeView's expansion with it, though the user
    //! never collapsed anything. Carries the stable ids of the branches to
    //! restore.
    //!
    //! Emitted ONCE at the end of the batch and never from inside a structural
    //! bracket: QTreeView::expand() calls fetchMore() synchronously, and
    //! canFetchMore() answers false while m_structure_locked is nonzero, so an
    //! expand issued mid-batch would mark the node expanded with the
    //! realization silently dropped -- and nothing would retry it, the node
    //! being already in QTreeView's expanded set. Consumers must defer their
    //! expand past the current event-loop turn.
    void groupsReslotted(const QList<int>& group_ids);

public slots:
    //! WalletModel fans each drained coin-event batch here; events for other
    //! view ids are ignored.
    void applyCoinEventBatch(const std::vector<GRC::WalletCoinEvent>& events);
    //! The view reports the visible indexes (scroll/resize/expand — an
    //! indexAt/indexBelow walk, bounded by the viewport row count). A visible
    //! span can mix scopes (tail of one group's children, headers, head of
    //! the next group), so the model buckets rows per scope and coalesces
    //! per-scope content fetches via the debounce timer.
    void onVisibleIndexes(const QList<QModelIndex>& visible);

private slots:
    void onFetchTimeout();
    //! WalletModel announcing a completed (and drained) coin-store scan.
    void onCoinSourceLoadFinished();

private:
    struct GroupSlot {
        GRC::WindowCache<GRC::CoinRecord> cache;
        std::unique_ptr<GRC::WindowCacheSink> sink;
    };

    //! Stable-id registry (one generation; renumbered on Reset).
    int idForAddress(const std::string& address);           //!< assigns if new
    int rowForAddress(const std::string& address) const;    //!< -1 if absent
    const std::string* addressForId(quintptr id_plus_one) const;

    //! Root/parent index helpers for the sink adapters.
    QModelIndex groupIndexByAddress(const std::string& address) const;

    //! The directory entry a group index names, or nullptr when the row is out
    //! of range. Indexes reach the accessors from places Qt does not validate
    //! for us — a context menu holds one across its own nested event loop, in
    //! which a drain can shrink the directory — so every directory read goes
    //! through this instead of subscripting m_directory directly.
    const GRC::CoinGroupInfo* groupAt(const QModelIndex& index) const;

    //! Reseed everything from the source (a Reset arrived, or first load).
    void reseedFromSource();

    //! Debounced per-scope content fetch (drain-then-fetch retry inside).
    void fetchScope(const std::string& scope, int first, int count);

    //! The record behind a child/flat index, or nullptr (placeholder).
    const GRC::CoinRecord* recordAt(const QModelIndex& index) const;

    //! Apply a bulk-op outpoint delta to coinControl.
    void applyDeltaToCoinControl(const GRC::CoinBulkSelectionResult& delta);

    //! Refresh directory aggregate values in place after a bulk selection op
    //! (positions cannot move: selection is not a directory sort key).
    void refreshDirectoryAggregates();

    //! Rebuild the address->row map for directory rows at/after \p from.
    void rebuildDirRows(int from);

    WalletModel* m_wallet_model;
    interfaces::WalletCoinSource& m_source;
    interfaces::WalletCoinControl* m_coin_control;
    const int m_view_id;
    GRC::CoinViewMode m_mode{GRC::CoinViewMode::Tree};
    //! A requested display mode, applied inside the next reset bracket (the
    //! mode decides the model's whole row structure — see setDisplayMode).
    std::optional<GRC::CoinViewMode> m_pending_mode;
    bool m_loading{true};

    //! TREE mode: the materialized directory (aggregates for parent rows) and
    //! the address <-> stable-id registry.
    std::vector<GRC::CoinGroupInfo> m_directory;
    std::unordered_map<std::string, int> m_dir_row;     //!< address -> current row
    std::unordered_map<std::string, int> m_addr_id;     //!< address -> stable id
    std::vector<std::string> m_id_addr;                 //!< stable id -> address
    //! Realized (expanded) groups, keyed by address.
    std::map<std::string, GroupSlot> m_groups;
    //! Addresses the USER has expanded -- deliberately not the same thing as
    //! m_groups. A re-slot drops the realized slot without the user having
    //! collapsed anything, and this set is what tells a re-slot apart from a
    //! real collapse. Grows only on expand, so it is bounded by user actions;
    //! pruned on a real collapse and cleared on Reset (which collapses
    //! everything anyway).
    std::set<std::string> m_expanded;
    //! Stable ids queued for re-expansion, accumulated over one drained batch
    //! and flushed by the groupsReslotted emission at its end.
    QList<int> m_pending_reexpand;

    //! FLAT mode: the single windowed row universe.
    GRC::WindowCache<GRC::CoinRecord> m_flat;
    std::unique_ptr<GRC::WindowCacheSink> m_flat_sink;

    QTimer* m_fetch_timer{nullptr};
    //! Pending viewport ranges per scope ("" = flat), coalesced by the timer.
    std::map<std::string, std::pair<int, int>> m_pending_fetch;
    bool m_applying_events{false};
    //! Nonzero while ANY of this model's structural brackets (reset,
    //! release-remove, fetchMore-insert, event-driven cache brackets) is
    //! open. Qt machinery (and QAbstractItemModelTester) can synchronously
    //! probe canFetchMore and invoke fetchMore from inside a bracket's
    //! signal dispatch; realizing a group there nests an insert bracket
    //! inside the open one — an aborting invariant violation (caught twice
    //! by the tester runs: collapse-remove and sort-reset). canFetchMore
    //! answers false while nonzero; the suppressed realization happens
    //! later via the normal expand/viewport paths.
    int m_structure_locked{0};

    struct StructureLock {
        int& counter;
        explicit StructureLock(int& c) : counter(c) { ++counter; }
        ~StructureLock() { --counter; }
    };
    //! Drained tip height: the confirmations column renders serve-time depth
    //! from block_height so a tip advance repaints without refetching rows.
    int m_tip_height{0};

    friend struct CoinCacheSink;
};

#endif // BITCOIN_QT_COINSELECTIONMODEL_H
