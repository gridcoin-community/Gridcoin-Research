// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/coinselectionview.h"

#include "qt/coinselectionmodel.h"

#include <QKeyEvent>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>

CoinSelectionView::CoinSelectionView(QWidget* parent)
    : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // Any expansion change moves rows into/out of the viewport. The expanded
    // signal fires BEFORE QTreeView's deferred branch layout runs, so a
    // same-turn viewport walk sees pre-layout geometry and fetches nothing —
    // the freshly visible rows would sit blank until the first scroll (found
    // by the 500k synthetic acceptance run). Schedule a second report after
    // the layout has settled.
    connect(this, &QTreeView::expanded, this, [this](const QModelIndex& idx) {
        // Record the INTENT here, on the signal, not in fetchMore(): the
        // model cannot tell a user expansion from a programmatic fetchMore()
        // sweep, and a re-expand of a still-warm slot never calls fetchMore()
        // at all. Symmetric with the noteCollapsed() call below.
        if (auto* m = qobject_cast<CoinSelectionModel*>(model())) {
            if (m->isGroup(idx)) m->noteExpanded(m->addressAt(idx).toStdString());
        }
        reportViewport();
        QTimer::singleShot(150, this, [this]() { reportViewport(); });
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex& idx) {
        // Record the INTENT now, synchronously, and defer only the structural
        // un-realize below. A persistent index survives a re-slot's coordinate
        // change but NOT the removal half of it, so if a re-slot lands before
        // the continuation runs, the continuation returns at the isValid()
        // check and m_expanded keeps the address — and the insert half of that
        // same re-slot, seeing it still expanded, queues a re-expansion of the
        // branch the user just closed. noteCollapsed() opens no bracket, so it
        // is safe here where releaseGroup() is not.
        if (auto* m = qobject_cast<CoinSelectionModel*>(model())) {
            if (m->isGroup(idx)) m->noteCollapsed(m->addressAt(idx).toStdString());
        }

        // DEFER the un-realize: collapsed() is emitted from inside
        // QTreeView's collapse handling, and mutating the model
        // (beginRemoveRows) re-entrantly from that dispatch overlaps
        // structural-change windows — QAbstractItemModelTester aborts on
        // changeInFlight != None (found by the 2000:5 tester run). A
        // persistent index survives any intervening directory re-slots.
        const QPersistentModelIndex pidx(idx);
        // Pin the model the collapse belongs to, mirroring
        // restoreReslottedBranches: pidx refers to THIS model, and if
        // setModel() swaps models before the turn fires, handing the old
        // model's index to the new model would resolve a stale row number
        // against an unrelated directory.
        const QPointer<CoinSelectionModel> pinned(
            qobject_cast<CoinSelectionModel*>(model()));
        QTimer::singleShot(0, this, [this, pidx, pinned]() {
            if (!pinned || pinned.data() != model()) return;
            if (!pidx.isValid()) return;
            // Re-validate at fire time: a re-expand landing after the
            // collapse but before this turn (double-click, keyboard
            // toggling, expandAll) means the user wants the branch open --
            // un-realizing it now would leave a node the view shows as
            // expanded with zero rows underneath.
            if (isExpanded(pidx)) return;
            pinned->releaseGroup(pidx);
            reportViewport();
        });
    });
}

void CoinSelectionView::setModel(QAbstractItemModel* model)
{
    if (auto* previous = qobject_cast<CoinSelectionModel*>(this->model())) {
        disconnect(previous, &CoinSelectionModel::groupsReslotted, this, nullptr);
    }
    QTreeView::setModel(model);
    if (auto* m = qobject_cast<CoinSelectionModel*>(model)) {
        connect(m, &CoinSelectionModel::groupsReslotted,
                this, &CoinSelectionView::restoreReslottedBranches);
    }
}

void CoinSelectionView::restoreReslottedBranches(const QList<int>& group_ids)
{
    // DEFER, for the same reason the collapse path above defers. The model
    // emits this after its batch's brackets have closed, but QTreeView::expand()
    // calls fetchMore() synchronously and Qt can still be mid-dispatch of the
    // batch's own row signals here; a continuation runs with nothing open, so
    // the expansion actually realizes instead of being silently suppressed.
    //
    // The scroll offset is captured NOW and restored after: re-inserting the
    // parent at its new slot has already moved rows under the viewport, and
    // re-expanding a branch above it moves them again. This holds the
    // scrollbar still rather than reconstructing what was under the cursor --
    // enough to stop the jump to the top, which is the visible symptom.
    //
    // The ids are only meaningful against the registry that issued them, so
    // pin both the model and that registry's generation. Neither a model swap
    // nor a reseed can cancel an already-queued singleShot: setModel() only
    // disconnects the signal, and a reseed renumbers m_id_addr in place. An
    // unpinned continuation would then resolve these integers against whatever
    // registry is current and expand rows nobody asked for -- and a reseed has
    // just collapsed everything, so that is a branch the user watched close.
    auto* emitter = qobject_cast<CoinSelectionModel*>(model());
    if (!emitter) return;
    const QPointer<CoinSelectionModel> pinned(emitter);
    const quint64 generation = emitter->registryGeneration();

    const int offset = verticalScrollBar()->value();
    QTimer::singleShot(0, this, [this, group_ids, offset, pinned, generation]() {
        if (!pinned) return;
        if (qobject_cast<CoinSelectionModel*>(model()) != pinned.data()) return;
        if (pinned->registryGeneration() != generation) return;
        for (const int id : group_ids) {
            const QModelIndex idx = pinned->groupIndexForId(id);
            // Gone again between the emission and this turn (removed for real).
            if (idx.isValid() && !isExpanded(idx)) expand(idx);
        }
        verticalScrollBar()->setValue(offset);
        reportViewport();
    });
}

void CoinSelectionView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && currentIndex().isValid()) {
        // Toggle the CHECKBOX column of the current row (legacy
        // CoinControlTreeWidget::keyPressEvent parity, group rows included);
        // stock QTreeView would only toggle the current cell.
        const QModelIndex checkbox =
            currentIndex().siblingAtColumn(CoinSelectionModel::COLUMN_CHECKBOX);
        const Qt::CheckState state =
            checkbox.data(Qt::CheckStateRole).value<Qt::CheckState>();
        model()->setData(checkbox,
                         state == Qt::Checked ? Qt::Unchecked : Qt::Checked,
                         Qt::CheckStateRole);
        return;
    }
    QTreeView::keyPressEvent(event);
}

void CoinSelectionView::scrollContentsBy(int dx, int dy)
{
    QTreeView::scrollContentsBy(dx, dy);
    if (dy != 0) reportViewport();
}

void CoinSelectionView::resizeEvent(QResizeEvent* event)
{
    QTreeView::resizeEvent(event);
    reportViewport();
}

void CoinSelectionView::reportViewport()
{
    // Coalesce to one report per event-loop turn (scrolling emits streams of
    // scrollContentsBy). The walk itself is bounded by the viewport height.
    if (m_report_queued) return;
    m_report_queued = true;
    QTimer::singleShot(0, this, [this]() {
        m_report_queued = false;
        if (!model()) return;

        QList<QModelIndex> visible;
        QModelIndex idx = indexAt(viewport()->rect().topLeft());
        const QModelIndex last = indexAt(viewport()->rect().bottomLeft());
        int guard = viewport()->height() / std::max(1, rowHeight(idx.isValid() ? idx : QModelIndex())) + 4;
        // rowHeight(invalid) is 0 on an empty view; the guard math above
        // degrades to the +4 floor, and the loop below exits immediately.
        while (idx.isValid() && guard-- > 0) {
            visible.append(idx);
            if (idx == last) break;
            idx = indexBelow(idx);
        }
        if (!visible.isEmpty()) emit visibleIndexesChanged(visible);
    });
}
