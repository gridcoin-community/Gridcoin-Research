#ifndef BITCOIN_QT_TRANSACTIONDESCDIALOG_H
#define BITCOIN_QT_TRANSACTIONDESCDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
    class TransactionDescDialog;
}

/** Dialog showing transaction details. */
class TransactionDescDialog : public QDialog
{
    Q_OBJECT

public:
    //! \p html is the pre-rendered transaction-detail HTML produced
    //! producer-side by WalletTxStore::getRowDetail (windowed-model PR5-C). The
    //! dialog no longer reaches back through a model role to format detail, so it
    //! has no dependency on TransactionTableModel.
    explicit TransactionDescDialog(const QString& html, QWidget* parent = nullptr);
    ~TransactionDescDialog();

private:
    Ui::TransactionDescDialog *ui;
};

#endif // BITCOIN_QT_TRANSACTIONDESCDIALOG_H
