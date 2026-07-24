#ifndef BITCOIN_QT_COINCONTROLDIALOG_H
#define BITCOIN_QT_COINCONTROLDIALOG_H

#include "walletmodel.h"
#include "amount.h"

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QList>
#include <QMenu>
#include <QModelIndex>
#include <QPoint>
#include <QString>

namespace Ui {
    class CoinControlDialog;
}
class CoinSelectionModel;
class WalletModel;

//!
//! \brief Coin-control selection dialog over the windowed CoinSelectionModel
//! (#3183). The QTreeWidget that materialized every UTXO is replaced by a
//! CoinSelectionView + virtual model: parent rows render from server-side
//! aggregates, children realize lazily on expand, and only the viewport
//! window of records is ever cached GUI-side. Selection state remains
//! authoritative in interfaces::WalletCoinControl (Phase 1e); the summary
//! labels (updateLabels/computeCoinControlSummary) are unchanged.
//!
class CoinControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlDialog(QWidget* parent = nullptr,
                               interfaces::WalletCoinControl* coinControl = nullptr,
                               QList<qint64>* payAmounts = nullptr,
                               bool fSubtractFeeFromAmount = false);
    ~CoinControlDialog();

    void setModel(WalletModel *model);

    // static because also called from sendcoinsdialog
    static void updateLabels(WalletModel*, interfaces::WalletCoinControl*, QList<qint64>*, QDialog*,
                             bool fSubtractFeeFromAmount = false);

    // This is based on what will guarantee a successful transaction. Set from
    // the wallet interface in setModel() (no model exists at construction).
    size_t m_inputSelectionLimit{0};

signals:
    void selectedConsolidationRecipientSignal(SendCoinsRecipient consolidationRecipient);

public slots:
    //! The legacy signature, now a thin forward to the model's server-side
    //! applyValueFilter (prune-only; returns true when the input cap culled).
    bool filterInputsByValue(const bool& less, const CAmount& inputFilterValue, const unsigned int& inputSelectionLimit);

private:
    Ui::CoinControlDialog *ui;
    interfaces::WalletCoinControl *coinControl;
    QList<qint64> *payAmounts;
    WalletModel *model;
    CoinSelectionModel *m_selection_model{nullptr};
    int sortColumn;
    Qt::SortOrder sortOrder;

    QMenu *contextMenu;
    QModelIndex contextMenuIndex;
    QAction *copyTransactionHashAction;

    std::pair<QString, QString> m_consolidationAddress;
    Qt::CheckState m_ToState = Qt::Checked;
    bool m_FilterMode = true;
    bool m_fSubtractFeeFromAmount = false;

    void showHideConsolidationReadyToSend();
    //! Expand tree groups that are partially selected (bounded — the legacy
    //! auto-expand parity, capped so a pathological wallet cannot realize
    //! half a million rows in one shot).
    void expandPartiallySelected();

private slots:
    void showMenu(const QPoint &);
    void copyAmount();
    void copyLabel();
    void copyAddress();
    void copyTransactionHash();
    void clipboardQuantity();
    void clipboardAmount();
    void clipboardFee();
    void clipboardAfterFee();
    void clipboardBytes();
    void clipboardLowOutput();
    void clipboardChange();
    void treeModeRadioButton(bool);
    void listModeRadioButton(bool);
    void headerSectionClicked(int);
    void buttonBoxClicked(QAbstractButton*);
    void buttonSelectAllClicked();
    void maxMinOutputValueChanged();
    void buttonFilterModeClicked();
    void buttonFilterClicked();
    void buttonConsolidateClicked();
    void selectedConsolidationAddressSlot(std::pair<QString, QString> address);
    void modelSelectionChanged();
    void modelLoadingFinished();
};

#endif // BITCOIN_QT_COINCONTROLDIALOG_H
