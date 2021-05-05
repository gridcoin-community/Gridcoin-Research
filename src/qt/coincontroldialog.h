#ifndef COINCONTROLDIALOG_H
#define COINCONTROLDIALOG_H

#include "walletmodel.h"
#include "amount.h"

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QTreeWidgetItem>

namespace Ui {
    class CoinControlDialog;
}
class WalletModel;
class CCoinControl;

class CoinControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlDialog(QWidget *parent = 0,
                               CCoinControl *coinControl = nullptr,
                               QList<qint64> *payAmounts = nullptr);
    ~CoinControlDialog();

    void setModel(WalletModel *model);

    // static because also called from sendcoinsdialog
    static void updateLabels(WalletModel*, CCoinControl*, QList<qint64>*, QDialog*);
    static QString getPriorityLabel(double);

    // This is based on what will guarantee a successful transaction.
    const size_t m_inputSelectionLimit;

signals:
    void selectedConsolidationRecipientSignal(SendCoinsRecipient consolidationRecipient);

public slots:
    bool filterInputsByValue(const bool& less, const CAmount& inputFilterValue, const unsigned int& inputSelectionLimit);

private:
    Ui::CoinControlDialog *ui;
    CCoinControl *coinControl;
    QList<qint64> *payAmounts;
    WalletModel *model;
    int sortColumn;
    Qt::SortOrder sortOrder;

    QMenu *contextMenu;
    QTreeWidgetItem *contextMenuItem;
    QAction *copyTransactionHashAction;
    //QAction *lockAction;
    //QAction *unlockAction;

    std::pair<QString, QString> m_consolidationAddress;
    Qt::CheckState m_ToState = Qt::Checked;
    bool m_FilterMode = true;

    QString strPad(QString, int, QString);
    void sortView(int, Qt::SortOrder);
    void updateView();
    void showHideConsolidationReadyToSend();

    enum
    {
        COLUMN_CHECKBOX,
        COLUMN_AMOUNT,
        COLUMN_LABEL,
        COLUMN_ADDRESS,
        COLUMN_DATE,
        COLUMN_CONFIRMATIONS,
        COLUMN_PRIORITY,
        COLUMN_TXHASH,
        COLUMN_VOUT_INDEX,
        COLUMN_AMOUNT_INT64,
        COLUMN_PRIORITY_INT64,
        COLUMN_CHANGE_BOOL
    };

private slots:
    void showMenu(const QPoint &);
    void copyAmount();
    void copyLabel();
    void copyAddress();
    void copyTransactionHash();
    //void lockCoin();
    //void unlockCoin();
    void clipboardQuantity();
    void clipboardAmount();
    void clipboardFee();
    void clipboardAfterFee();
    void clipboardBytes();
    void clipboardPriority();
    void clipboardLowOutput();
    void clipboardChange();
    void treeModeRadioButton(bool);
    void listModeRadioButton(bool);
    void viewItemChanged(QTreeWidgetItem*, int);
    void headerSectionClicked(int);
    void buttonBoxClicked(QAbstractButton*);
    void buttonSelectAllClicked();
    void maxMinOutputValueChanged();
    void buttonFilterModeClicked();
    void buttonFilterClicked();
    void buttonConsolidateClicked();
    void selectedConsolidationAddressSlot(std::pair<QString, QString> address);
    //void updateLabelLocked();
};

#endif // COINCONTROLDIALOG_H
