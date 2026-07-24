// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/coinselectionview.h"

#include "qt/coinselectionmodel.h"

#include <QKeyEvent>
#include <QTimer>

#include <algorithm>

CoinSelectionView::CoinSelectionView(QWidget* parent)
    : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // Any expansion change moves rows into/out of the viewport.
    connect(this, &QTreeView::expanded, this, [this](const QModelIndex&) { reportViewport(); });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex& idx) {
        if (auto* m = qobject_cast<CoinSelectionModel*>(model())) {
            m->releaseGroup(idx);
        }
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
