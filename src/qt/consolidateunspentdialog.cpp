#include "consolidateunspentdialog.h"
#include "qt/decoration.h"
#include <QAbstractButton>
#include "ui_consolidateunspentdialog.h"

#include "util.h"

using namespace std;

ConsolidateUnspentDialog::ConsolidateUnspentDialog(QWidget *parent, size_t inputSelectionLimit) :
    QDialog(parent),
    ui(new Ui::ConsolidateUnspentDialog),
    m_inputSelectionLimit(inputSelectionLimit)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    ui->addressTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // ok button
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &ConsolidateUnspentDialog::buttonBoxClicked);

    // destination address selection
    connect(ui->addressTableWidget, &QTableWidget::itemSelectionChanged, this, &ConsolidateUnspentDialog::addressSelectionChanged);

    ui->outputLimitWarningLabel->setText(tr("Note: The number of inputs selected for consolidation has been "
                                                 "limited to %1 to prevent a transaction failure due to too many "
                                                 "inputs.").arg(m_inputSelectionLimit));
    SetOutputWarningVisible(false);
}

ConsolidateUnspentDialog::~ConsolidateUnspentDialog()
{
    delete ui;
}

// --------------------------------------------------------- address - label
void ConsolidateUnspentDialog::SetAddressList(const std::map<QString, QString>& addressList)
{
    ui->addressTableWidget->setSortingEnabled(false);

    int row = 0;
    for (const auto& iter : addressList)
    {
        ui->addressTableWidget->insertRow(row);

        QTableWidgetItem* label = new QTableWidgetItem(iter.second);
        QTableWidgetItem* address = new QTableWidgetItem(iter.first);

        if (label != nullptr) ui->addressTableWidget->setItem(row, 0, label);
        if (address != nullptr) ui->addressTableWidget->setItem(row, 1, address);

        ++row;
    }

    ui->addressTableWidget->setCurrentItem(ui->addressTableWidget->item(0, 1));
}

void ConsolidateUnspentDialog::SetOutputWarningVisible(bool status)
{
    ui->outputLimitWarningIconLabel->setVisible(status);
    ui->outputLimitWarningLabel->setVisible(status);
}

// ok button
void ConsolidateUnspentDialog::buttonBoxClicked(QAbstractButton* button)
{
    emit selectedConsolidationAddressSignal(m_selectedDestinationAddress);

    // closes the dialog
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole) done(QDialog::Accepted);
}

void ConsolidateUnspentDialog::addressSelectionChanged()
{
    int currentRow = ui->addressTableWidget->currentRow();

    QTableWidgetItem* selectedLabel = ui->addressTableWidget->item(currentRow, 0);
    QTableWidgetItem* selectedAddress = ui->addressTableWidget->item(currentRow, 1);

    m_selectedDestinationAddress = std::make_pair(selectedLabel->text(), selectedAddress->text());

    LogPrint(BCLog::LogFlags::MISC, "INFO: %s: Label %, Address %s selected.", __func__,
             m_selectedDestinationAddress.first.toStdString(),
             m_selectedDestinationAddress.second.toStdString());
}
