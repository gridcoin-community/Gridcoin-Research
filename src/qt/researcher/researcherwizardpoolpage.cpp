// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include "key.h"
#include "qt/decoration.h"
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

    GRC::ScaleFontPointSize(ui->headerLabel, 11);

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

    // TODO (issue #1783 follow-up): populate poolTableWidget from
    // GetPoolRegistry().ActivePools() instead of the static rows defined in
    // researcherwizardpoolpage.ui. With the V15 rework grandfathering the
    // 5 legacy pools into PoolRegistry's constructor, the registry is never
    // empty, so the dynamic path can switch over at any time — the
    // pre-activation fallback this comment used to mention is no longer
    // needed.

    connect(ui->poolTableWidget, &QTableWidget::cellClicked,
            this, &ResearcherWizardPoolPage::openLink);

    if (!m_wallet_model) {
        return;
    }

    connect(ui->newAddressButton, &QPushButton::clicked, this, &ResearcherWizardPoolPage::getNewAddress);
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
        ui->addressLabel->setText(tr("Error: failed to generate a new address."));
        return;
    }

    ui->addressLabel->setText(
        QString::fromStdString(EncodeDestination(public_key.GetID())));
    ui->copyToClipboardButton->setVisible(true);
}

void ResearcherWizardPoolPage::on_copyToClipboardButton_clicked()
{
    QApplication::clipboard()->setText(ui->addressLabel->text());
}
