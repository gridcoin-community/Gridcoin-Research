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

    resize(GRC::ScaleSize(this, width(), height()));

    ui->SubmittedIconLabel->setPixmap(GRC::ScaleIcon(this, ":/icons/round_green_check", 32));
    ui->ErrorIconLabel->setPixmap(GRC::ScaleIcon(this, ":/icons/warning", 32));
    ui->waitForBlockUpdate->setPixmap(GRC::ScaleIcon(this, ":/icons/no_result", 32));

    connect(m_mrc_model, &MRCModel::mrcChanged, this, &MRCRequestPage::updateMRCStatus);
    connect(m_mrc_model, &MRCModel::walletStatusChangedSignal, this, &MRCRequestPage::updateMRCStatus);

    connect(ui->mrcRequestButtonBox, &QDialogButtonBox::clicked, this, &MRCRequestPage::buttonBoxClicked);
    connect(ui->mrcUpdateButton, &QAbstractButton::clicked, this, &MRCRequestPage::updateMRCStatus);
    connect(ui->mrcFeeBoostSpinBox, &BitcoinAmountField::textChanged, this, &MRCRequestPage::setMRCFeeBoost);
    connect(ui->mrcSubmitButton, &QAbstractButton::clicked, this, &MRCRequestPage::submitMRC);
    connect(ui->mrcFeeBoostRaiseToMinimumButton, &QAbstractButton::clicked,
            this, &MRCRequestPage::setMRCFeeBoostToSubmitMinimum);

    ui->mrcFeeBoostRaiseToMinimumButton->hide();

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

void MRCRequestPage::setMRCFeeBoost()
{
    if (!m_mrc_model) return;

    CAmount fee_boost = ui->mrcFeeBoostSpinBox->value();

    m_mrc_model->setMRCFeeBoost(fee_boost);

    updateMRCStatus();
}

void MRCRequestPage::setMRCFeeBoostToSubmitMinimum()
{
    // Note that the button must be enabled for this function to be called, because it is connected to the button press
    // and is a private slot, so therefore we are already in the situation where
    // m_mrc_model->getMRCQueueLength() >= m_mrc_model->getMRCOutputLimit().

    // This condition should not happen because of the checks in updateMRCStatus, but protect against it anyway.
    if (m_mrc_model->getMRCReward() <= m_mrc_model->getMRCQueuePayLimitFee()) return;

    // Set the boost to the pay limit plus 1 Halford minus the mrc minimum fee to submit calculated from the MRC trail run.
    CAmount minimum_boost_needed = m_mrc_model->getMRCQueuePayLimitFee() + 1 - m_mrc_model->getMRCMinimumSubmitFee();

    ui->mrcFeeBoostSpinBox->setValue(minimum_boost_needed);
    m_mrc_model->setMRCFeeBoost(minimum_boost_needed);
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
    QString e;
    QString message;

    // Note MRCError treats the PENDING status as an error from a handling point of view, because it blocks the
    // submission of a new MRC while there is one already in progress.
    if (m_mrc_model->isMRCError(s, e)) {
        ui->mrcSubmitButton->setEnabled(false);

        switch (s) {
        case MRCRequestStatus::PENDING:
            message = e + " MRC request cannot be submitted.";

            ui->mrcQueuePosition->setText(QString::number(m_mrc_model->getMRCPos() + 1));
            ui->mrcQueuePositionLabel->setText(tr("Your Submitted MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(tr("N/A"));

            ui->mrcFeeBoostRaiseToMinimumButton->setEnabled(false);
            ui->mrcFeeBoostRaiseToMinimumButton->hide();

            ui->SubmittedIconLabel->show();
            ui->ErrorIconLabel->hide();
            ui->ErrorIconLabel->setToolTip("");

            break;
        case MRCRequestStatus::PENDING_CANCEL:
            message = e;

            ui->mrcQueuePosition->setText(QString::number(m_mrc_model->getMRCPos() + 1));
            ui->mrcQueuePositionLabel->setText(tr("Your Submitted MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(tr("N/A"));

            ui->mrcFeeBoostRaiseToMinimumButton->setEnabled(false);
            ui->mrcFeeBoostRaiseToMinimumButton->hide();

            ui->SubmittedIconLabel->hide();
            ui->ErrorIconLabel->show();
            ui->ErrorIconLabel->setToolTip(message);

            break;
        case MRCRequestStatus::STALE_CANCEL:
            message = e;

            ui->mrcQueuePosition->setText(QString::number(m_mrc_model->getMRCPos() + 1));
            ui->mrcQueuePositionLabel->setText(tr("Your Submitted MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(tr("N/A"));

            ui->mrcFeeBoostRaiseToMinimumButton->setEnabled(false);
            ui->mrcFeeBoostRaiseToMinimumButton->hide();

            ui->SubmittedIconLabel->hide();
            ui->ErrorIconLabel->show();
            ui->ErrorIconLabel->setToolTip(message);

            break;
        case MRCRequestStatus::QUEUE_FULL:
            message = e + " MRC request cannot be submitted.";

            ui->mrcQueuePosition->setText(tr("N/A"));
            ui->mrcQueuePositionLabel->setText(tr("Your Projected MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(BitcoinUnits::formatWithUnit(display_unit, m_mrc_model->getMRCMinimumSubmitFee()));

            if (m_mrc_model->getMRCReward() > m_mrc_model->getMRCQueuePayLimitFee()) {
                ui->mrcFeeBoostRaiseToMinimumButton->setEnabled(true);
                ui->mrcFeeBoostRaiseToMinimumButton->show();
            } else {
                ui->mrcFeeBoostRaiseToMinimumButton->setEnabled(false);
                ui->mrcFeeBoostRaiseToMinimumButton->hide();
            }

            ui->SubmittedIconLabel->hide();
            ui->ErrorIconLabel->show();
            ui->ErrorIconLabel->setToolTip(message);

            break;
        default:
            message = e + " MRC request cannot be submitted.";

            ui->mrcQueuePosition->setText(tr("N/A"));
            ui->mrcQueuePositionLabel->setText(tr("Your Projected MRC Request Position in Queue"));

            ui->mrcMinimumSubmitFee->setText(tr("N/A"));

            ui->SubmittedIconLabel->hide();
            ui->ErrorIconLabel->show();
            ui->ErrorIconLabel->setToolTip(message);
        } // switch for error conditions

        ui->mrcSubmitButton->setToolTip(message);
    } else {
        message = "Submits the MRC request.";

        ui->mrcQueuePosition->setText(QString::number(m_mrc_model->getMRCPos() + 1));
        ui->mrcQueuePositionLabel->setText(tr("Your Projected MRC Request Position in Queue"));

        ui->mrcMinimumSubmitFee->setText(BitcoinUnits::formatWithUnit(display_unit, m_mrc_model->getMRCMinimumSubmitFee()));

        ui->mrcFeeBoostRaiseToMinimumButton->setEnabled(false);
        ui->mrcFeeBoostRaiseToMinimumButton->hide();

        ui->mrcSubmitButton->setEnabled(true);
        ui->mrcSubmitButton->setToolTip(message);
        ui->SubmittedIconLabel->hide();
        ui->ErrorIconLabel->hide();
        ui->ErrorIconLabel->setToolTip("");
    }

    showMRCStatus(m_mrc_model->getMRCModelStatus());
}

void MRCRequestPage::showMRCStatus(const MRCModel::ModelStatus& status) {
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
    QString e;
    QString message;

    if (!m_mrc_model) return;

    if (!m_mrc_model->submitMRC(s, e)) {
        message = e + " MRC request cannot be submitted.";

        ui->mrcSubmitButton->setToolTip(message);
    } else {
        // Since MRC was successfully submitted, reset the fee boost to zero.
        CAmount fee_boost = 0;

        ui->mrcFeeBoostSpinBox->setValue(fee_boost);
        m_mrc_model->setMRCFeeBoost(fee_boost);
    }
}
