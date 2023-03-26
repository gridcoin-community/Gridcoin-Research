#ifndef BITCOIN_QT_TRANSACTIONVIEW_H
#define BITCOIN_QT_TRANSACTIONVIEW_H

#include <QFrame>

class WalletModel;
class TransactionFilterProxy;

QT_BEGIN_NAMESPACE
class QTableView;
class QComboBox;
class QLineEdit;
class QModelIndex;
class QMenu;
class QFrame;
class QDateTimeEdit;

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

private:
    WalletModel *model;
    TransactionFilterProxy *transactionProxyModel;
    QTableView *transactionView;

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
