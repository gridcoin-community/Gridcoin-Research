// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "qspinbox.h"
#include "sync.h"
#include "mrcrequestpage.h"
#include "ui_mrcrequestpage.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "qt/decoration.h"
#include "bitcoinunits.h"

MRCRequestPage::MRCRequestPage(
    QWidget *parent,
    MRCModel* mrc_model)
    : QDialog(parent)
    , ui(new Ui::MRCRequestPage)
    , m_mrc_model(mrc_model)
{
    if (m_mrc_model) {
        m_wallet_model = m_mrc_model->getWalletModel();
    }

    ui->setupUi(this);

    m_orig_geometry = this->geometry();
    m_gridLayout_orig_geometry = ui->gridLayout->geometry();

    m_scaled_size = GRC::ScaleSize(this, width(), height());

    resize(m_scaled_size);

    ui->SubmittedIconLabel->setPixmap(GRC::ScaleIcon(this, ":/icons/round_green_check", 32));
    ui->ErrorIconLabel->setPixmap(GRC::ScaleIcon(this, ":/icons/warning", 32));
    ui->waitForBlockUpdate->setPixmap(GRC::ScaleIcon(this, ":/icons/no_result", 32));

    connect(m_mrc_model, &MRCModel::mrcChanged, this, &MRCRequestPage::updateMRCStatus);
    connect(m_mrc_model, &MRCModel::walletStatusChangedSignal, this, &MRCRequestPage::updateMRCStatus);

    connect(ui->mrcRequestButtonBox, &QDialogButtonBox::clicked, this, &MRCRequestPage::buttonBoxClicked);
    connect(ui->mrcUpdateButton, &QAbstractButton::clicked, this, &MRCRequestPage::updateMRCStatus);
    connect(ui->mrcFeeBoostSpinBox, &BitcoinAmountField::textChanged, this, &MRCRequestPage::setMRCProvidedFee);
    connect(ui->mrcSubmitButton, &QAbstractButton::clicked, this, &MRCRequestPage::submitMRC);

    ui->mrcFeeBoostSpinBox->setValue(m_mrc_model->getMRCFeeBoost());

    ui->ErrorIconLabel->hide();
    ui->SubmittedIconLabel->hide();

    updateMRCStatus();
}

MRCRequestPage::~MRCRequestPage()
{
    delete ui;
}

void MRCRequestPage::updateMRCModel()
{
    if (!m_mrc_model) return;

    m_mrc_model->refresh();
}

void MRCRequestPage::setMRCProvidedFee()
{
    if (!m_mrc_model) return;

    CAmount fee_boost = ui->mrcFeeBoostSpinBox->value();

    m_mrc_model->setMRCFeeBoost(fee_boost);

    updateMRCStatus();
}

void MRCRequestPage::buttonBoxClicked(QAbstractButton* button)
{
    if (ui->mrcRequestButtonBox->buttonRole(button) == QDialogButtonBox::AcceptRole) {
        done(QDialog::Accepted);
    } else {
        done(QDialog::Rejected);
    }
}

void MRCRequestPage::updateMRCStatus()
{
    if (!m_mrc_model) return;

    updateMRCModel();

    int display_unit = BitcoinUnits::BTC;
    if (m_wallet_model && m_wallet_model->getOptionsModel()) {
        display_unit = m_wallet_model->getOptionsModel()->getDisplayUnit();
    }

    ui->mrcQueueLimit->setText(QString::number(m_mrc_model->getMRCOutputLimit()));
    ui->numMRCInQueue->setText(QString::number(m_mrc_model->getMRCQueueLength()));

    if (m_mrc_model->getMRCQueueLength() > 0) {
        ui->mrcQueueHeadFee->setText(BitcoinUnits::formatWithUnit(display_unit, m_mrc_model->getMRCQueueHeadFee()));
    } else {
        ui->mrcQueueHeadFee->setText(tr("N/A"));
    }

    if (m_mrc_model->getMRCQueueLength() >= m_mrc_model->getMRCOutputLimit()) {
        ui->mrcQueuePayLimitFee->setText(BitcoinUnits::formatWithUnit(display_unit, m_mrc_model->getMRCQueuePayLimitFee()));
    } else {
        ui->mrcQueuePayLimitFee->setText(tr("N/A"));
    }

    if (m_mrc_model->getMRCQueueLength() > 0) {
        ui->mrcQueueTailFee->setText(BitcoinUnits::formatWithUnit(display_unit, m_mrc_model->getMRCQueueTailFee()));
    } else {
        ui->mrcQueueTailFee->setText(tr("N/A"));
    }

    MRCRequestStatus s;
    std::string e;
    QString message;

    // Note MRCError treats the PENDING status as an error from a handling point of view, because it blocks the
    // submission of a new MRC while there is one already in progress.
    if (m_mrc_model->isMRCError(s, e)) {
        message = QString::fromStdString(e) + " MRC request cannot be submitted.";

        ui->mrcSubmitButton->setEnabled(false);
        ui->mrcSubmitButton->setToolTip(message);

        if (s == MRCRequestStatus::PENDING) {
            ui->mrcQueuePosition->setText(QString::number(m_mrc_model->getMRCPos() + 1));
            ui->mrcQueuePositionLabel->setText(tr("Your Submitted MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(tr("N/A"));

            ui->SubmittedIconLabel->show();
            ui->ErrorIconLabel->hide();
            ui->ErrorIconLabel->setToolTip("");
        } else if (s == MRCRequestStatus::QUEUE_FULL) {
            ui->mrcQueuePosition->setText(tr("N/A"));
            ui->mrcQueuePositionLabel->setText(tr("Your Projected MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(BitcoinUnits::formatWithUnit(display_unit, m_mrc_model->getMRCMinimumSubmitFee()));

            ui->SubmittedIconLabel->hide();
            ui->ErrorIconLabel->show();
            ui->ErrorIconLabel->setToolTip(message);
        } else {
            ui->mrcQueuePosition->setText(tr("N/A"));
            ui->mrcQueuePositionLabel->setText(tr("Your Projected MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(tr("N/A"));

            ui->SubmittedIconLabel->hide();
            ui->ErrorIconLabel->show();
            ui->ErrorIconLabel->setToolTip(message);
        }
    } else {
        message = "Submits the MRC request.";

        ui->mrcQueuePosition->setText(QString::number(m_mrc_model->getMRCPos() + 1));
        ui->mrcQueuePositionLabel->setText(tr("Your Projected MRC Request Position in Queue"));

        ui->mrcMinimumSubmitFee->setText(BitcoinUnits::formatWithUnit(display_unit, m_mrc_model->getMRCMinimumSubmitFee()));

        ui->mrcSubmitButton->setEnabled(true);
        ui->mrcSubmitButton->setToolTip(message);
        ui->SubmittedIconLabel->hide();
        ui->ErrorIconLabel->hide();
        ui->ErrorIconLabel->setToolTip("");
    }

    LogPrintf("INFO: %s: getMRCModelStatus = %i",
              __func__,
              (int) m_mrc_model->getMRCModelStatus());

    showMRCStatus(m_mrc_model->getMRCModelStatus());
}

void MRCRequestPage::showMRCStatus(MRCModel::ModelStatus status) {
    switch (status) {
    case MRCModel::ModelStatus::NOT_VALID_RESEARCHER:
        ui->waitForBlockUpdateLabel->setText(tr("You must have an active beacon and the wallet must be in solo mode to "
                                                "submit MRCs."));
        ui->waitForNextBlockUpdateFrame->show();
        ui->mrcStatusSubmitFrame->hide();
        return;
    case MRCModel::ModelStatus::INVALID_BLOCK_VERSION:
        ui->waitForBlockUpdateLabel->setText(tr("The block version must be v12 or higher to submit MRCs."));
        ui->waitForNextBlockUpdateFrame->show();
        ui->mrcStatusSubmitFrame->hide();
        return;
    case MRCModel::ModelStatus::OUT_OF_SYNC:
        ui->waitForBlockUpdateLabel->setText(tr("The wallet must be in sync to submit MRCs."));
        ui->waitForNextBlockUpdateFrame->show();
        ui->mrcStatusSubmitFrame->hide();
        return;
    case MRCModel::ModelStatus::NO_BLOCK_UPDATE_FROM_INIT:
        ui->waitForBlockUpdateLabel->setText(tr("A block update must have occurred since the wallet start to submit MRCs."));
        ui->waitForNextBlockUpdateFrame->show();
        ui->mrcStatusSubmitFrame->hide();
        return;
    case MRCModel::ModelStatus::VALID:
        ui->waitForBlockUpdateLabel->setText("");
        ui->waitForNextBlockUpdateFrame->hide();
        ui->mrcStatusSubmitFrame->show();
        return;
    }
    assert(false);
}

void MRCRequestPage::submitMRC()
{
    MRCRequestStatus s;
    std::string e;
    QString message;

    if (!m_mrc_model) return;

    if (!m_mrc_model->submitMRC(s, e)) {
        message = QString::fromStdString(e) + " MRC request cannot be submitted.";

        ui->mrcSubmitButton->setToolTip(message);
    } else {
        // Since MRC was successfully submitted, reset the fee boost to zero.
        CAmount fee_boost = 0;

        ui->mrcFeeBoostSpinBox->clear();
        m_mrc_model->setMRCFeeBoost(fee_boost);
    }
}
