// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "key.h"
#include "qt/forms/ui_researcherwizardpoolpage.h"
#include "qt/guiutil.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"
#include "qt/researcher/researcherwizardpoolpage.h"
#include "qt/walletmodel.h"

#include <QClipboard>
#include <QDesktopServices> // for opening URLs
#include <QInputDialog>
#include <QUrl>

// -----------------------------------------------------------------------------
// Class: ResearcherWizardPoolPage
// -----------------------------------------------------------------------------

ResearcherWizardPoolPage::ResearcherWizardPoolPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardPoolPage)
{
    ui->setupUi(this);
    ui->addressLabel->setFont(GUIUtil::bitcoinAddressFont());
    ui->copyToClipboardButton->setVisible(false);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->newAddressButton->setIcon(QIcon());
#endif
}

ResearcherWizardPoolPage::~ResearcherWizardPoolPage()
{
    delete ui;
}

void ResearcherWizardPoolPage::setModel(
    ResearcherModel* researcher_model,
    WalletModel* wallet_model)
{
    this->m_researcher_model = researcher_model;
    this->m_wallet_model = wallet_model;
}

void ResearcherWizardPoolPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    m_researcher_model->switchToPool();

    connect(ui->poolTableWidget, SIGNAL(cellClicked(int, int)),
            this, SLOT(openLink(int, int)));

    if (!m_wallet_model) {
        return;
    }

    connect(ui->newAddressButton, SIGNAL(clicked()), this, SLOT(getNewAddress()));
}

void ResearcherWizardPoolPage::openLink(int row, int column) const
{
    const QTableWidgetItem* item = ui->poolTableWidget->item(row, column);

    QDesktopServices::openUrl(QUrl(item->text()));
}

void ResearcherWizardPoolPage::getNewAddress()
{
    const WalletModel::UnlockContext unlock_context(m_wallet_model->requestUnlock());

    if (!unlock_context.isValid()) {
        // Unlock wallet was cancelled
        return;
    }

    bool ok;
    const QString label = QInputDialog::getText(
        this,
        tr("Address Label"),
        tr("Label:"),
        QLineEdit::Normal,
        tr("Pool Receiving Address"),
        &ok);

    if (!ok) {
        // Address label dialog was cancelled
        return;
    }

    CPubKey public_key;

    if (!m_wallet_model->getKeyFromPool(public_key, label.toStdString())) {
        ui->addressLabel->setText("Error: failed to generate a new address.");
        return;
    }

    ui->addressLabel->setText(
        QString::fromStdString(CBitcoinAddress(public_key.GetID()).ToString()));
    ui->copyToClipboardButton->setVisible(true);
}

void ResearcherWizardPoolPage::on_copyToClipboardButton_clicked()
{
    QApplication::clipboard()->setText(ui->addressLabel->text());
}
