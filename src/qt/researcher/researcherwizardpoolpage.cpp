// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

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
#include <QStringList>
#include <QTableWidgetItem>
#include <QUrl>

#include <utility>
#include <vector>

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

    populatePoolTable();

    connect(ui->poolTableWidget, &QTableWidget::cellClicked,
            this, &ResearcherWizardPoolPage::openLink);

    if (!m_wallet_model) {
        return;
    }

    connect(ui->newAddressButton, &QPushButton::clicked, this, &ResearcherWizardPoolPage::getNewAddress);
}

void ResearcherWizardPoolPage::populatePoolTable()
{
    // The rows come from the pool registry rather than the .ui file. The
    // registry is seeded with the grandfathered pools in its constructor and
    // again on Reset(), so it is never empty and needs no static fallback.
    const std::vector<std::pair<QString, QString>> pools = m_researcher_model->buildPoolList();

    ui->poolTableWidget->clearContents();
    ui->poolTableWidget->setRowCount(static_cast<int>(pools.size()));

    QStringList names;

    for (int row = 0; row < static_cast<int>(pools.size()); ++row) {
        names << pools[row].first;

        // Not translated: a pool's website address is data, like the addresses
        // the .ui rows carried with notr="true".
        auto* url_item = new QTableWidgetItem(pools[row].second);
        url_item->setFlags(url_item->flags() & ~Qt::ItemIsEditable);
        ui->poolTableWidget->setItem(row, 0, url_item);
    }

    // The pool name is the row's vertical header, as it was in the .ui.
    ui->poolTableWidget->setVerticalHeaderLabels(names);
}

void ResearcherWizardPoolPage::openLink(int row, int column) const
{
    const QTableWidgetItem* item = ui->poolTableWidget->item(row, column);

    if (!item) {
        return;
    }

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

    const QString address = m_wallet_model->getNewReceiveAddress(label);

    if (address.isEmpty()) {
        ui->addressLabel->setText(tr("Error: failed to generate a new address."));
        return;
    }

    ui->addressLabel->setText(address);
    ui->copyToClipboardButton->setVisible(true);
}

void ResearcherWizardPoolPage::on_copyToClipboardButton_clicked()
{
    QApplication::clipboard()->setText(ui->addressLabel->text());
}
