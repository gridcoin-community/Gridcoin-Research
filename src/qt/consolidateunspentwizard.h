#ifndef CONSOLIDATEUNSPENTWIZARD_H
#define CONSOLIDATEUNSPENTWIZARD_H

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
                                      CCoinControl *coinControl = nullptr,
                                      QList<qint64> *payAmounts = nullptr);
    ~ConsolidateUnspentWizard();

    void setModel(WalletModel *model);

    void accept() override;

signals:
    void passCoinControlSignal(CCoinControl*);
    void selectedConsolidationRecipientSignal(SendCoinsRecipient);
    void sendConsolidationTransactionSignal();

private:
    Ui::ConsolidateUnspentWizard *ui;
    CCoinControl *coinControl;
    QList<qint64> *payAmounts;
    WalletModel *model;
};

#endif // CONSOLIDATEUNSPENTWIZARD_H
