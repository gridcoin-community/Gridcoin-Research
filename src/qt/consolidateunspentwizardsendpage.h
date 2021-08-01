#ifndef CONSOLIDATEUNSPENTWIZARDSENDPAGE_H
#define CONSOLIDATEUNSPENTWIZARDSENDPAGE_H

#include "walletmodel.h"
#include "amount.h"

#include <QWizard>

namespace Ui {
    class ConsolidateUnspentWizardSendPage;
}

class ConsolidateUnspentWizardSendPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ConsolidateUnspentWizardSendPage(QWidget *parent = nullptr);
    ~ConsolidateUnspentWizardSendPage();

    void initializePage();

    void setModel(WalletModel*);

public slots:
    void onFinishButtonClicked();

signals:
    void selectedConsolidationRecipientSignal(SendCoinsRecipient consolidationRecipient);

private:
    Ui::ConsolidateUnspentWizardSendPage *ui;
    WalletModel *model;
    SendCoinsRecipient m_recipient;

    size_t m_inputSelectionLimit;
};

#endif // CONSOLIDATEUNSPENTWIZARDSENDPAGE_H
