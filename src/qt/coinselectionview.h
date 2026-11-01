// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINSELECTIONVIEW_H
#define BITCOIN_QT_COINSELECTIONVIEW_H

#include <QTreeView>

//!
//! \brief QTreeView for the windowed coin-selection model (#3183), replacing
//! the item-based CoinControlTreeWidget. Three duties beyond stock QTreeView:
//!
//!  - SPACE toggles the checkbox column of the current ROW regardless of
//!    which column is current — including group rows (stock QTreeView only
//!    toggles the current cell, which with row selection is usually not the
//!    checkbox column). The legacy widget's behavior, routed through
//!    model->setData so the validated selection path runs.
//!
//!  - Viewport reporting: on scroll/resize/expand, walk the visible indexes
//!    (indexAt + indexBelow, bounded by the viewport row count) and hand
//!    them to the model, which buckets per scope and windows its content
//!    fetches.
//!
//!  - uniformRowHeights is mandatory at this scale (QTreeView materializes a
//!    view item per expanded row; per-item height bookkeeping would degrade
//!    500k-row branches badly). Set here so every consumer gets it.
//!
//! The legacy Escape-closes-with-Accepted quirk is dropped: the dialogs
//! ignore exec()'s return (selection state lives in WalletCoinControl and is
//! applied immediately), so stock Escape/reject behaves identically for the
//! user. Documented deviation.
//!
class CoinSelectionView : public QTreeView
{
    Q_OBJECT

public:
    explicit CoinSelectionView(QWidget* parent = nullptr);

    //! Wires the re-slot restoration (see restoreReslottedBranches) so every
    //! consumer of this view gets it, the way uniformRowHeights is set in the
    //! constructor rather than per dialog.
    void setModel(QAbstractItemModel* model) override;

signals:
    //! The visible index set changed (scroll / resize / expand / collapse).
    void visibleIndexesChanged(const QList<QModelIndex>& visible);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void reportViewport();
    //! Re-expand branches the model reports as re-slotted, restoring the
    //! scroll offset (#3228 item 3).
    void restoreReslottedBranches(const QList<int>& group_ids);

private:
    bool m_report_queued{false};
};

#endif // BITCOIN_QT_COINSELECTIONVIEW_H
