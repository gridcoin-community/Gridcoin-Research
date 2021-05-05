#include "consolidateunspentwizardselectdestinationpage.h"
#include "ui_consolidateunspentwizardselectdestinationpage.h"

#include "util.h"

ConsolidateUnspentWizardSelectDestinationPage::ConsolidateUnspentWizardSelectDestinationPage(QWidget *parent) :
    QWizardPage(parent),
    ui(new Ui::ConsolidateUnspentWizardSelectDestinationPage)
{
    ui->setupUi(this);

    ui->addressTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // destination address selection
    connect(ui->addressTableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(addressSelectionChanged()));

    ui->isCompleteCheckBox->hide();

    // This is to provide a convenient way to populate the fields shown on the last page ("send" screen).
    registerField("selectedAddressLabelField", ui->selectedAddressLabel, "text", "updateFieldsSignal()");
    registerField("selectedAddressField", ui->selectedAddress, "text", "updateFieldsSignal()");

    //This is used to control the disable/enable of the next button on this page.
    registerField("isCompleteSelectDestination*", ui->isCompleteCheckBox);
}

ConsolidateUnspentWizardSelectDestinationPage::~ConsolidateUnspentWizardSelectDestinationPage()
{
    delete ui;
}

void ConsolidateUnspentWizardSelectDestinationPage::initializePage()
{
    // This fills the selected address fields out again if this page is reaccessed by back and then next
    // from the select inputs page without any changes.
    if (m_selectedDestinationAddress.first.size())
    {
        ui->selectedAddressLabel->setText(m_selectedDestinationAddress.first);
        ui->selectedAddress->setText(m_selectedDestinationAddress.second);

        ui->isCompleteCheckBox->setChecked(true);
    }
}

// ----------------------------------------------------------------------- address - label
void ConsolidateUnspentWizardSelectDestinationPage::SetAddressList(std::map<QString, QString> addressList)
{
    ui->addressTableWidget->setSortingEnabled(false);

    ui->addressTableWidget->setRowCount(addressList.size());

    int row = 0;
    for (const auto& iter : addressList)
    {
        QTableWidgetItem* label = new QTableWidgetItem(iter.second);
        QTableWidgetItem* address = new QTableWidgetItem(iter.first);

        if (label != nullptr) ui->addressTableWidget->setItem(row, 0, label);
        if (address != nullptr) ui->addressTableWidget->setItem(row, 1, address);

        ++row;
    }

    ui->addressTableWidget->setSortingEnabled(true);
}

void ConsolidateUnspentWizardSelectDestinationPage::setDefaultAddressSelection(QString address)
{
    if (!address.size())
    {
        ui->addressTableWidget->clearSelection();

        m_selectedDestinationAddress = {};

        LogPrint(BCLog::LogFlags::QT, "INFO: %s: Cleared (default) address selection", __func__);

        ui->isCompleteCheckBox->setChecked(false);

        return;
    }

    QList<QTableWidgetItem*> defaultAddress = ui->addressTableWidget->findItems(address, Qt::MatchExactly);

    defaultAddress[0]->setSelected(true);

    LogPrint(BCLog::LogFlags::QT, "INFO: %s: Set default address to %s, QTableWidgetItem %s",
              __func__,
              address.toStdString(),
              defaultAddress[0]->text().toStdString());

    ui->addressTableWidget->setCurrentItem(defaultAddress[0]);

    LogPrintf("INFO: %s: currentRow = %i", __func__, ui->addressTableWidget->currentRow());

    emit updateFieldsSignal();
}

void ConsolidateUnspentWizardSelectDestinationPage::addressSelectionChanged()
{

    if (!ui->addressTableWidget->selectedItems().size())
    {
        ui->selectedAddressLabel->setText(QString());
        ui->selectedAddress->setText(QString());

        ui->isCompleteCheckBox->setChecked(false);

        return;
    }

    ui->addressTableWidget->selectedItems()[0]->row();

    int selectedRow = ui->addressTableWidget->selectedItems()[0]->row();

    if (selectedRow < 0) return;

    QTableWidgetItem* selectedLabel = ui->addressTableWidget->item(selectedRow, 0);
    QTableWidgetItem* selectedAddress = ui->addressTableWidget->item(selectedRow, 1);

    ui->selectedAddressLabel->setText(selectedLabel->text());
    ui->selectedAddress->setText(selectedAddress->text());

    m_selectedDestinationAddress = std::make_pair(selectedLabel->text(), selectedAddress->text());

    LogPrint(BCLog::LogFlags::QT, "INFO: %s: Label %, Address %s selected.", __func__,
             m_selectedDestinationAddress.first.toStdString(),
             m_selectedDestinationAddress.second.toStdString());

    ui->isCompleteCheckBox->setChecked(true);

    emit updateFieldsSignal();
}
