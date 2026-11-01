#ifndef BITCOIN_QT_CONSOLIDATEUNSPENTWIZARD_H
#define BITCOIN_QT_CONSOLIDATEUNSPENTWIZARD_H

#include "walletmodel.h"

#include <QDialogButtonBox>
#include <QDialog>
#include <QWizard>
#include <QString>
#include <QLabel>

namespace Ui {
    class ConsolidateUnspentWizard;
}

class CoinControlDialog;

class ConsolidateUnspentWizard : public QWizard
{
    Q_OBJECT

public:
    enum Pages
    {
        SelectInputsPage,
        SelectDestinationPage,
        SendPage
    };

    explicit ConsolidateUnspentWizard(QWidget *parent = nullptr,
                                      interfaces::WalletCoinControl *coinControl = nullptr,
                                      QList<qint64> *payAmounts = nullptr);
    ~ConsolidateUnspentWizard();

    void setModel(WalletModel *model);

    void accept() override;

signals:
    void passCoinControlSignal(interfaces::WalletCoinControl*);
    void selectedConsolidationRecipientSignal(SendCoinsRecipient);
    void sendConsolidationTransactionSignal();

private:
    Ui::ConsolidateUnspentWizard *ui;
    interfaces::WalletCoinControl *coinControl;
    QList<qint64> *payAmounts;
    WalletModel *model;
};

#endif // BITCOIN_QT_CONSOLIDATEUNSPENTWIZARD_H
