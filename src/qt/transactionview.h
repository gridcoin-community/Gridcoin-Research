#ifndef BITCOIN_QT_TRANSACTIONVIEW_H
#define BITCOIN_QT_TRANSACTIONVIEW_H

#include "qt/txfilter.h"
#include "uint256.h"

#include <QFrame>

class WalletModel;
class DetailedTxModel;

QT_BEGIN_NAMESPACE
class QTableView;
class QComboBox;
class QLineEdit;
class QModelIndex;
class QMenu;
class QFrame;
class QDateTimeEdit;
class QShowEvent;

QT_END_NAMESPACE

/** Widget showing the transaction list for a wallet, including a filter row.
    Using the filter row, the user can view or export a subset of the transactions.
  */
class TransactionView : public QFrame
{
    Q_OBJECT
public:
    explicit TransactionView(QWidget* parent = nullptr);

    void setModel(WalletModel *model);

    // Date ranges for filter
    enum DateEnum
    {
        All,
        Today,
        ThisWeek,
        ThisMonth,
        LastMonth,
        ThisYear,
        Range
    };

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    WalletModel *model;
    DetailedTxModel *m_detailedModel;
    QTableView *transactionView;

    //! The accumulated filter state the four filter widgets mutate; pushed to the
    //! server-side cursor by applyFilter() whenever one of them changes.
    GRC::FilterSpec m_filterSpec;

    //! Push m_filterSpec to the cursor (the date/type/address/amount handlers
    //! mutate the relevant field, then call this). Captures the resort anchor first.
    void applyFilter();

    //! Ensure the currently selected row is cached before a copy / Show details so
    //! it is never a placeholder under the windowed model (PR5-B GAP #3).
    void ensureSelectedRowCached();

    //! The resort anchor: the EXACT (hash, idx) of the (current, else
    //! topmost-visible) row captured before a sort/filter Reset, re-found +
    //! re-selected by restoreAnchor() after the model's viewReset, so the list
    //! reorders around the user's row instead of jumping to the top (PR5-B). idx
    //! (the decomposed-part index) is essential: a tx's parts share the hash but
    //! scatter under an Amount/Address sort, so a hash-only anchor would restore to
    //! the wrong part. Multi-row selection is not preserved across a resort (the
    //! Reset invalidates persistent indexes) — a minor regression vs the old proxy,
    //! but strictly better than PR4 which lost all selection + scrolled to the top.
    uint256 m_anchor_hash;
    int m_anchor_idx = -1;
    bool m_have_anchor = false;

    QComboBox *dateWidget;
    QComboBox *typeWidget;
    QLineEdit *searchWidget;
    QAction *searchWidgetIconAction;
    QLineEdit *amountWidget;

    QMenu *contextMenu;

    QFrame *dateRangeWidget;
    QDateTimeEdit *dateFrom;
    QDateTimeEdit *dateTo;

    QWidget *createDateRangeWidget();

    std::vector<int> m_table_column_sizes;
    bool m_init_column_sizes_set;
    bool m_resize_columns_in_progress;

private slots:
    void contextualMenu(const QPoint &);
    void dateRangeChanged();
    void copyAddress();
    void editLabel();
    void copyLabel();
    void copyAmount();
    void copyTxID();
    void updateIcons(const QString& theme);
    void txnViewSectionResized(int index, int old_size, int new_size);

    //! Report the visible row range [rowAt(0), rowAt(bottom)] to the windowed model
    //! so it fetches that slice (PR5-B). Bails if the table is not yet laid out
    //! (rowAt returns -1), so a pre-layout report can never request the whole table.
    void reportViewport();
    //! Capture / restore the resort anchor around a sort/filter Reset.
    void captureAnchor();
    void restoreAnchor();

signals:
    void doubleClicked(const QModelIndex&);

public slots:
    void showDetails();
    void chooseDate(int idx);
    void chooseType(int idx);
    void changedPrefix(const QString &prefix);
    void changedAmount(const QString &amount);
    void exportClicked();
    void focusTransaction(const QModelIndex&);
    void resizeTableColumns(const bool& neighbor_pair_adjust = false, const int& index = 0,
                            const int& old_size = 0, const int& new_size = 0);
};

#endif // BITCOIN_QT_TRANSACTIONVIEW_H
