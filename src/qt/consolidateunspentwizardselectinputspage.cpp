#include "qt/guilog.h"
#include "consolidateunspentwizardselectinputspage.h"
#include "ui_consolidateunspentwizardselectinputspage.h"

#include "bitcoinunits.h"
#include "key_io.h"
#include "optionsmodel.h"
#include "qt/coinselectionmodel.h"
#include "qt/coinselectionview.h"

#include <set>

using namespace std;

ConsolidateUnspentWizardSelectInputsPage::ConsolidateUnspentWizardSelectInputsPage(QWidget *parent) :
    QWizardPage(parent),
    ui(new Ui::ConsolidateUnspentWizardSelectInputsPage),
    sortColumn(CoinSelectionModel::COLUMN_AMOUNT),
    sortOrder(Qt::DescendingOrder)
{
    ui->setupUi(this);

    // toggle tree/list mode
    connect(ui->treeModeRadioButton, &QRadioButton::toggled, this, &ConsolidateUnspentWizardSelectInputsPage::treeModeRadioButton);
    connect(ui->listModeRadioButton, &QRadioButton::toggled, this, &ConsolidateUnspentWizardSelectInputsPage::listModeRadioButton);

    // click on header
    ui->treeView->header()->setSectionsClickable(true);
    connect(ui->treeView->header(), &QHeaderView::sectionClicked, this, &ConsolidateUnspentWizardSelectInputsPage::headerSectionClicked);

    // (un)select all
    connect(ui->selectAllPushButton, &QPushButton::clicked, this, &ConsolidateUnspentWizardSelectInputsPage::buttonSelectAllClicked);

    // filter/consolidate button interaction
    connect(ui->maxMinOutputValue, &BitcoinAmountField::textChanged, this, &ConsolidateUnspentWizardSelectInputsPage::maxMinOutputValueChanged);

    // filter mode
    connect(ui->filterModePushButton, &QPushButton::clicked, this, &ConsolidateUnspentWizardSelectInputsPage::buttonFilterModeClicked);

    // filter
    connect(ui->filterPushButton, &QPushButton::clicked, this, &ConsolidateUnspentWizardSelectInputsPage::buttonFilterClicked);

    // This is to provide a convenient way to populate the fields shown on the last page ("send" screen).
    registerField("quantityField", ui->quantityLabel, "text", "updateFieldsSignal()");
    registerField("feeField", ui->feeLabel, "text", "updateFieldsSignal()");
    registerField("afterFeeAmountField", ui->afterFeeLabel, "text", "updateFieldsSignal()");

    //This is used to control the disable/enable of the next button on this page.
    registerField("isCompleteSelectInputs*", ui->isCompleteCheckBox);

    // The tooltips that show m_InputSelectionLimit are set in setModel(), once
    // the value is available from the wallet interface (no model at construction).

    ui->outputLimitWarningIconLabel->setVisible(false);
    ui->outputLimitStopIconLabel->setVisible(false);

    ui->isCompleteCheckBox->hide();
}

ConsolidateUnspentWizardSelectInputsPage::~ConsolidateUnspentWizardSelectInputsPage()
{
    delete ui;
}

void ConsolidateUnspentWizardSelectInputsPage::setModel(WalletModel *model)
{
    this->model = model;

    if (model && model->getOptionsModel() && coinControl != nullptr)
    {
        // The consolidation input cap is a policy value from the wallet
        // interface; set it and the tooltips that show it here (no model exists
        // at construction).
        m_InputSelectionLimit = model->wallet().getMaxConsolidationInputs();
        ui->outputLimitWarningIconLabel->setToolTip(tr("Note: The number of inputs selected for consolidation has been "
                                                     "limited to %1 to prevent a transaction failure due to too many "
                                                     "inputs.").arg(m_InputSelectionLimit));
        ui->outputLimitStopIconLabel->setToolTip(tr("Note: The number of inputs selected for consolidation is currently more "
                                                     "than the limit of %1. Please use the filter or manual selection to reduce "
                                                    "the number of inputs to %1 or less to prevent a transaction failure due to "
                                                    "too many inputs.").arg(m_InputSelectionLimit));

        // The shared windowed selection model on the wizard's own view id:
        // registers node-side, reconciles the selection against the store,
        // and seeds from the (possibly still loading) snapshot.
        m_selection_model = new CoinSelectionModel(model, coinControl, GRC::VIEW_COIN_WIZARD, this);
        ui->treeView->setModel(m_selection_model);
        ui->treeView->setAlternatingRowColors(m_selection_model->displayMode() == GRC::CoinViewMode::Flat);

        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_CHECKBOX, 150);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_AMOUNT, 170);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_LABEL, 200);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_ADDRESS, 290);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_DATE, 110);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_CONFIRMATIONS, 100);

        connect(ui->treeView, &CoinSelectionView::visibleIndexesChanged,
                m_selection_model, &CoinSelectionModel::onVisibleIndexes);
        connect(m_selection_model, &CoinSelectionModel::selectionChanged,
                this, &ConsolidateUnspentWizardSelectInputsPage::modelSelectionChanged);
        connect(m_selection_model, &CoinSelectionModel::loadingFinished,
                this, &ConsolidateUnspentWizardSelectInputsPage::modelLoadingFinished);

        // default view is sorted by amount desc
        ui->treeView->header()->setSortIndicator(CoinSelectionModel::COLUMN_AMOUNT, Qt::DescendingOrder);

        if (m_selection_model->isLoading()) {
            ui->treeView->setEnabled(false);
        } else {
            modelLoadingFinished();
        }
    }
}

void ConsolidateUnspentWizardSelectInputsPage::setCoinControl(interfaces::WalletCoinControl *coinControl)
{
    this->coinControl = coinControl;
}

void ConsolidateUnspentWizardSelectInputsPage::setPayAmounts(QList<qint64> *payAmounts)
{
    this->payAmounts = payAmounts;
}

void ConsolidateUnspentWizardSelectInputsPage::modelLoadingFinished()
{
    ui->treeView->setEnabled(true);
    updateLabels();
}

void ConsolidateUnspentWizardSelectInputsPage::modelSelectionChanged()
{
    // Any USER-driven selection mutation invalidates the filter's cull state
    // (the WARNING boundary depends on it); the filter's own bulk mutation
    // must not clear the flag it just set.
    if (!m_ApplyingFilter) m_InputSelectionLimitedByFilter = false;

    updateLabels();
}

// (un)select all
void ConsolidateUnspentWizardSelectInputsPage::buttonSelectAllClicked()
{
    if (!m_selection_model) return;

    m_InputSelectionLimitedByFilter = false;

    m_selection_model->selectAll(m_ToState == Qt::Checked);

    if (m_ToState == Qt::Checked)
    {
        m_ToState = Qt::Unchecked;
    }
    else
    {
        m_ToState = Qt::Checked;
    }

    if (m_ToState == Qt::Checked)
    {
       ui->selectAllPushButton->setText(tr("Select All"));
    }
    else
    {
       ui->selectAllPushButton->setText(tr("Select None"));
    }
}

void ConsolidateUnspentWizardSelectInputsPage::maxMinOutputValueChanged()
{
    ui->maxMinOutputValue->value(&m_FilterValueValid);
}

void ConsolidateUnspentWizardSelectInputsPage::buttonFilterModeClicked()
{
    if (m_FilterMode)
    {
        m_FilterMode = false;
        ui->filterModePushButton->setText(">=");
    }
    else
    {
        m_FilterMode = true;
        ui->filterModePushButton->setText("<=");
    }
}

void ConsolidateUnspentWizardSelectInputsPage::buttonFilterClicked()
{
    if (!m_selection_model) return;

    m_ApplyingFilter = true;
    m_InputSelectionLimitedByFilter =
        m_selection_model->applyValueFilter(m_FilterMode, ui->maxMinOutputValue->value(),
                                            m_InputSelectionLimit);
    m_ApplyingFilter = false;

    updateLabels();
}

// treeview: clicked on header
void ConsolidateUnspentWizardSelectInputsPage::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == CoinSelectionModel::COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeView->header()->setSortIndicator(sortColumn, sortOrder);
        return;
    }

    if (sortColumn == logicalIndex)
    {
        sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder);
    }
    else
    {
        sortColumn = logicalIndex;

        // if amount,date,conf then default => desc, else default => asc
        sortOrder = ((sortColumn == CoinSelectionModel::COLUMN_AMOUNT
                      || sortColumn == CoinSelectionModel::COLUMN_DATE
                      || sortColumn == CoinSelectionModel::COLUMN_CONFIRMATIONS) ?
                         Qt::DescendingOrder : Qt::AscendingOrder);
    }

    m_selection_model->sort(sortColumn, sortOrder);
    ui->treeView->header()->setSortIndicator(sortColumn, sortOrder);
}

// toggle tree mode
void ConsolidateUnspentWizardSelectInputsPage::treeModeRadioButton(bool checked)
{
    if (checked && m_selection_model)
    {
        ui->treeView->setAlternatingRowColors(false);
        m_selection_model->setDisplayMode(GRC::CoinViewMode::Tree);
    }
}

// toggle list mode
void ConsolidateUnspentWizardSelectInputsPage::listModeRadioButton(bool checked)
{
    if (checked && m_selection_model)
    {
        ui->treeView->setAlternatingRowColors(true);
        m_selection_model->setDisplayMode(GRC::CoinViewMode::Flat);
    }
}

void ConsolidateUnspentWizardSelectInputsPage::updateLabels()
{
    if (!model || !m_selection_model) return;

    // Gather the recipient amounts; all fee/quantity math (byte sizing via
    // pubkey compression, nTransactionFee/GetMinFee, sub-CENT change absorption)
    // runs node-side in one call, so the wizard does no fee math and holds no
    // policy/consensus headers.
    std::vector<int64_t> recipientAmounts;
    recipientAmounts.reserve(payAmounts->size());
    for (const auto& amount : std::as_const(*payAmounts))
    {
        recipientAmounts.push_back(amount);
    }

    const interfaces::CoinControlSummary summary =
        model->wallet().computeCoinControlSummary(*coinControl, recipientAmounts, /*subtract_fee_from_amount=*/false);

    const unsigned int nQuantity = static_cast<unsigned int>(summary.quantity);

    // actually update labels
    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel()) nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    // stats
    ui->quantityLabel->setText(QString::number(summary.quantity));                        // Quantity
    ui->feeLabel->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.fee));       // Fee
    ui->afterFeeLabel->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.after_fee)); // After Fee

    // The candidate destinations and the default: served by the group
    // directory's aggregates (no realized children needed). A group qualifies
    // when it has at least one direct (non-change) output; it counts as
    // "checked" when any of its members is selected. The default address is
    // the LABEL (the destination page matches it against its label column)
    // when exactly one qualifying group has selections.
    std::map<QString, QString> addressList;
    QString defaultAddress;
    unsigned int numberAddressesWhereOutputsChecked = 0;

    for (const GRC::CoinGroupInfo& group : m_selection_model->groupDirectory())
    {
        if (group.direct_output_count <= 0) continue;

        const QString label = group.label.empty() ? tr("(no label)")
                                                  : QString::fromStdString(group.label);
        addressList[QString::fromStdString(group.address)] = label;

        if (group.selected_count > 0)
        {
            defaultAddress = label;
            ++numberAddressesWhereOutputsChecked;
        }
    }

    if (!addressList.empty()) emit setAddressListSignal(addressList);

    // This covers the 0 case too, where the default address will be an empty QString.
    if (numberAddressesWhereOutputsChecked < 2)
    {
        // This will be an empty QString if the numberAddressesWhereOutputsChecked equals 0. It will be
        // the above defaultAddress if numberAddressesWhereOutputsChecked equals 1.
        emit setDefaultAddressSignal(defaultAddress);
    }
    else
    {
        // If numberAddressesWhereOutputsChecked is 2 or greater, then clear the default address (i.e. set to
        // empty QString.
        emit setDefaultAddressSignal(QString());
    }

    // This provides the trigger to update the fields from the labels, since they are QLabels and don't have appropriate
    // internal signals.
    emit updateFieldsSignal();

    if (nQuantity < 2)
    {
        SetOutputWarningStop(InputStatus::INSUFFICIENT_OUTPUTS);
    }
    else if (nQuantity < m_InputSelectionLimit
             || (nQuantity == m_InputSelectionLimit && !m_InputSelectionLimitedByFilter))
    {
        SetOutputWarningStop(InputStatus::NORMAL);
    }
    else if (nQuantity == m_InputSelectionLimit && m_InputSelectionLimitedByFilter)
    {
        SetOutputWarningStop(InputStatus::WARNING);
    }
    else if (nQuantity > m_InputSelectionLimit)
    {
        SetOutputWarningStop(InputStatus::STOP);
    }
}

void ConsolidateUnspentWizardSelectInputsPage::SetOutputWarningStop(InputStatus input_status)
{
    switch (input_status)
    {
    case InputStatus::INSUFFICIENT_OUTPUTS:
        ui->outputLimitWarningIconLabel->setVisible(false);
        ui->outputLimitStopIconLabel->setVisible(false);
        ui->isCompleteCheckBox->setChecked(false);
        break;
    case InputStatus::NORMAL:
        ui->outputLimitWarningIconLabel->setVisible(false);
        ui->outputLimitStopIconLabel->setVisible(false);
        ui->isCompleteCheckBox->setChecked(true);
        break;
    case InputStatus::WARNING:
        ui->outputLimitWarningIconLabel->setVisible(true);
        ui->outputLimitStopIconLabel->setVisible(false);
        ui->isCompleteCheckBox->setChecked(true);
        break;
    case InputStatus::STOP:
        ui->outputLimitWarningIconLabel->setVisible(false);
        ui->outputLimitStopIconLabel->setVisible(true);
        ui->isCompleteCheckBox->setChecked(false);
    }
}
