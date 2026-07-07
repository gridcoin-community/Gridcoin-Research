// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "psgtpoolpage.h"

#include "clientmodel.h"
#include "multisigndialog.h"
#include "psgtpooltablemodel.h"
#include "walletmodel.h"

#include <chainparams.h>
#include <main.h>
#include <node/psgt_pool.h>
#include <sync.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

PSGTPoolPage::PSGTPoolPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    m_banner = new QLabel(this);
    m_banner->setWordWrap(true);
    m_banner->setVisible(false);
    layout->addWidget(m_banner);

    m_table = new QTableView(this);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_table);

    auto* buttons = new QHBoxLayout();
    m_sign_button = new QPushButton(tr("&Sign"), this);
    m_sign_button->setToolTip(tr("Sign the selected PSGT with this wallet and, if it "
                                 "completes the multisig, broadcast the transaction"));
    m_remove_button = new QPushButton(tr("&Remove"), this);
    m_remove_button->setToolTip(tr("Remove the selected PSGT from this node's pool "
                                   "(local only; other nodes keep their copies)"));
    m_details_button = new QPushButton(tr("&Details..."), this);
    m_details_button->setToolTip(tr("Open the selected PSGT in the Multisign dialog"));
    m_refresh_button = new QPushButton(tr("Re&fresh"), this);

    buttons->addWidget(m_sign_button);
    buttons->addWidget(m_remove_button);
    buttons->addWidget(m_details_button);
    buttons->addStretch();
    buttons->addWidget(m_refresh_button);
    layout->addLayout(buttons);

    connect(m_sign_button, &QPushButton::clicked, this, &PSGTPoolPage::sign);
    connect(m_remove_button, &QPushButton::clicked, this, &PSGTPoolPage::remove);
    connect(m_details_button, &QPushButton::clicked, this, &PSGTPoolPage::details);

    updateActiveState();
    updateButtons();
}

void PSGTPoolPage::setWalletModel(WalletModel* wallet_model)
{
    m_wallet_model = wallet_model;
    if (!wallet_model) return;

    m_table_model = new PSGTPoolTableModel(wallet_model, this);
    m_table->setModel(m_table_model);
    m_table->horizontalHeader()->setSectionResizeMode(
        PSGTPoolTableModel::Image, QHeaderView::Stretch);

    connect(m_refresh_button, &QPushButton::clicked, m_table_model, &PSGTPoolTableModel::refresh);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &PSGTPoolPage::updateButtons);
    // The core PSGTPoolChanged signal is delivered to the model by BitcoinGUI
    // (which owns the uiInterface subscription) via handlePoolChanged; refresh
    // the button enablement whenever the rows change.
    connect(m_table_model, &QAbstractItemModel::modelReset, this, &PSGTPoolPage::updateButtons);

    updateButtons();
}

void PSGTPoolPage::setClientModel(ClientModel* client_model)
{
    m_client_model = client_model;
    if (!client_model) return;

    // Keep ages and the pool-active banner current as blocks arrive.
    connect(client_model, &ClientModel::numBlocksChanged, this, [this]() {
        updateActiveState();
        if (m_table_model) m_table_model->refresh();
    });

    // Deliver the core PSGTPoolChanged signal (marshalled onto the GUI thread
    // by ClientModel) to the table model so its rows track the pool live.
    connect(client_model, &ClientModel::psgtPoolChanged, this,
            [this](const QString&, quint8 change_type) {
                if (m_table_model) m_table_model->handlePoolChanged(change_type);
                updateButtons();
            });

    updateActiveState();
}

void PSGTPoolPage::setMultisignDialog(MultisignPSGTDialog* dialog)
{
    m_multisign_dialog = dialog;
    updateButtons();
}

void PSGTPoolPage::updateActiveState()
{
    const bool active = WITH_LOCK(cs_main, return IsV15Enabled(nBestHeight));
    if (active) {
        m_banner->setVisible(false);
    } else {
        m_banner->setText(tr("The PSGT pool is not active yet: it activates with the "
                             "block v15 network upgrade. Pending multisig transactions "
                             "will appear here once it does."));
        m_banner->setVisible(true);
    }
}

bool PSGTPoolPage::selectedImageHex(std::string& image_hex_out) const
{
    if (!m_table_model || !m_table->selectionModel()) return false;
    const QModelIndexList selected = m_table->selectionModel()->selectedRows();
    if (selected.isEmpty()) return false;

    const PSGTPoolTableModel::Row* row = m_table_model->rowAt(selected.first().row());
    if (!row || !row->in_pool) return false;

    image_hex_out = row->image.ToString();
    return true;
}

void PSGTPoolPage::updateButtons()
{
    std::string image_hex;
    const bool have_pool_selection = selectedImageHex(image_hex);

    const PSGTPoolTableModel::Row* row = nullptr;
    if (have_pool_selection && m_table->selectionModel()) {
        row = m_table_model->rowAt(m_table->selectionModel()->selectedRows().first().row());
    }

    m_sign_button->setEnabled(have_pool_selection && row
                              && row->status == PSGTPoolTableModel::RowStatus::AwaitingYourSignature);
    m_remove_button->setEnabled(have_pool_selection);
    m_details_button->setEnabled(have_pool_selection && m_multisign_dialog);
}

void PSGTPoolPage::sign()
{
    std::string image_hex;
    if (!selectedImageHex(image_hex) || !m_wallet_model) return;

    // Resolve the pooled entry by image before touching the wallet.
    uint160 image_bits;
    image_bits.SetHex(image_hex);
    const CScriptID image(image_bits);

    const auto entry = g_psgt_pool.Get(image);
    if (!entry) {
        QMessageBox::warning(this, tr("Sign PSGT"),
                             tr("This PSGT is no longer in the pool."));
        return;
    }

    WalletModel::UnlockContext ctx(m_wallet_model->requestUnlock());
    if (!ctx.isValid()) return; // unlock cancelled

    std::string error;
    uint256 txid;
    const PSGTSignResult result = SignAndAdvancePSGT(image, error, &txid);

    switch (result) {
    case PSGTSignResult::COMPLETED_AND_BROADCAST:
        QMessageBox::information(this, tr("Sign PSGT"),
                                 tr("The multisig is complete. The transaction was "
                                    "broadcast:\n%1").arg(QString::fromStdString(txid.ToString())));
        break;
    case PSGTSignResult::SIGNED_AND_RELAYED:
        QMessageBox::information(this, tr("Sign PSGT"),
                                 tr("Your signature was added and relayed to the other "
                                    "co-signers."));
        break;
    case PSGTSignResult::ALREADY_KNOWN:
        QMessageBox::information(this, tr("Sign PSGT"),
                                 tr("Your signature was added, but this exact revision is "
                                    "already known to the network (a co-signer may have "
                                    "signed it first). Nothing more to do."));
        break;
    case PSGTSignResult::NO_NEW_SIGNATURES:
        QMessageBox::warning(this, tr("Sign PSGT"),
                             tr("This wallet holds no key that can add a signature to "
                                "this PSGT."));
        break;
    case PSGTSignResult::FAILED:
        QMessageBox::critical(this, tr("Sign PSGT"),
                              tr("Signing failed: %1").arg(QString::fromStdString(error)));
        break;
    }

    if (m_table_model) m_table_model->refresh();
}

void PSGTPoolPage::remove()
{
    std::string image_hex;
    if (!selectedImageHex(image_hex)) return;

    if (QMessageBox::question(this, tr("Remove PSGT"),
                              tr("Remove this PSGT from your node's pool? Other nodes "
                                 "keep their copies, and a new revision from the network "
                                 "would be accepted again."))
        != QMessageBox::Yes) {
        return;
    }

    uint160 image_bits;
    image_bits.SetHex(image_hex);
    g_psgt_pool.Remove(CScriptID(image_bits), PSGTRemovalReason::LOCAL_REMOVE);

    if (m_table_model) m_table_model->refresh();
}

void PSGTPoolPage::details()
{
    std::string image_hex;
    if (!selectedImageHex(image_hex) || !m_multisign_dialog) return;

    uint160 image_bits;
    image_bits.SetHex(image_hex);
    const auto entry = g_psgt_pool.Get(CScriptID(image_bits));
    if (!entry) {
        QMessageBox::warning(this, tr("PSGT details"),
                             tr("This PSGT is no longer in the pool."));
        return;
    }

    // Hand the pooled PSGT to the dialog without a base64 round-trip -- the
    // seam #3054 built m_working / setWorking for.
    m_multisign_dialog->setWorking(entry->psgt);
    m_multisign_dialog->show();
    m_multisign_dialog->raise();
    m_multisign_dialog->activateWindow();
}
