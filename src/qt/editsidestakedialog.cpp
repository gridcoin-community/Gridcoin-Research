// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "editsidestakedialog.h"
#include "ui_editsidestakedialog.h"
#include "sidestaketablemodel.h"
#include "guiutil.h"
#include "qt/decoration.h"

#include <QMessageBox>

EditSideStakeDialog::EditSideStakeDialog(Mode mode, QWidget* parent)
    : QDialog(parent)
      , ui(new Ui::EditSideStakeDialog)
      , mode(mode)
      , model(nullptr)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    GUIUtil::setupAddressWidget(ui->addressLineEdit, this);

    switch (mode)
    {
    case NewSideStake:
        setWindowTitle(tr("New SideStake"));
        ui->statusLineEdit->setEnabled(false);
        ui->statusLabel->setHidden(true);
        ui->statusLineEdit->setHidden(true);
        break;
    case EditSideStake:
        setWindowTitle(tr("Edit SideStake"));
        ui->addressLineEdit->setEnabled(false);
        ui->statusLabel->setHidden(false);
        ui->statusLineEdit->setHidden(false);
        ui->statusLineEdit->setEnabled(false);
        break;
    }

}

EditSideStakeDialog::~EditSideStakeDialog()
{
    delete ui;
}

void EditSideStakeDialog::setModel(SideStakeTableModel* model)
{
    this->model = model;
    if (!model) {
        return;
    }

}

void EditSideStakeDialog::loadRow(int row)
{
    m_row = row;

    ui->addressLineEdit->setText(model->index(row, SideStakeTableModel::Address, QModelIndex()).data(Qt::EditRole).toString());
    ui->allocationLineEdit->setText(model->index(row, SideStakeTableModel::Allocation, QModelIndex()).data(Qt::EditRole).toString());
    ui->descriptionLineEdit->setText(model->index(row, SideStakeTableModel::Description, QModelIndex()).data(Qt::EditRole).toString());
    ui->statusLineEdit->setText(model->index(row, SideStakeTableModel::Status, QModelIndex()).data(Qt::EditRole).toString());
}

bool EditSideStakeDialog::saveCurrentRow()
{
    if (!model) {
        return false;
    }

    bool success = true;

    switch (mode)
    {
    case NewSideStake:
        address = model->addRow(ui->addressLineEdit->text(),
                                ui->allocationLineEdit->text(),
                                ui->descriptionLineEdit->text());

        if (address.isEmpty()) {
            success = false;
        }

        break;
    case EditSideStake:
        QModelIndex index = model->index(m_row, SideStakeTableModel::Allocation, QModelIndex());
        model->setData(index, ui->allocationLineEdit->text(), Qt::EditRole);

        if (model->getEditStatus() == SideStakeTableModel::OK || model->getEditStatus() == SideStakeTableModel::NO_CHANGES) {
            index = model->index(m_row, SideStakeTableModel::Description, QModelIndex());
            model->setData(index, ui->descriptionLineEdit->text(), Qt::EditRole);

            if (model->getEditStatus() == SideStakeTableModel::OK || model->getEditStatus() == SideStakeTableModel::NO_CHANGES) {
                break;
            }
        }

        success = false;

        break;
    }

    return success;
}

void EditSideStakeDialog::accept()
{
    if (!model) {
        return;
    }

    if (!saveCurrentRow())
    {
        switch (model->getEditStatus())
        {
        case SideStakeTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case SideStakeTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case SideStakeTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                                 tr("The entered address \"%1\" is not "
                                    "a valid Gridcoin address.").arg(ui->addressLineEdit->text()),
                                 QMessageBox::Ok, QMessageBox::Ok);
            break;
        case SideStakeTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                                 tr("The entered address \"%1\" already "
                                    "has a local sidestake entry.").arg(ui->addressLineEdit->text()),
                                 QMessageBox::Ok, QMessageBox::Ok);
            break;
        case SideStakeTableModel::INVALID_ALLOCATION:
            QMessageBox::warning(this, windowTitle(),
                                 tr("The entered allocation is not valid. Check to make sure that the "
                                    "allocation is greater than zero and when added to the other allocations "
                                    "totals less than 100."),
                                 QMessageBox::Ok, QMessageBox::Ok);
            break;
        case SideStakeTableModel::INVALID_DESCRIPTION:
            QMessageBox::warning(this, windowTitle(),
                                 tr("The entered description is not valid. Check to make sure that the "
                                    "description only contains letters, numbers, spaces, periods, or "
                                    "underscores."),
                                 QMessageBox::Ok, QMessageBox::Ok);
        }

        return;
    }

    QDialog::accept();
}
