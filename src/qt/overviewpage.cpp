#include <QWidget>
#include <QListView>

#include "overviewpage.h"
#include "ui_overviewpage.h"

#ifndef Q_MOC_RUN
#include "main.h"
#endif
#include "researcher/researchermodel.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "gridcoin/voting/fwd.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 48

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(QObject *parent=nullptr, int scaledDecorationSize = DECORATION_SIZE):
        QAbstractItemDelegate(parent), unit(BitcoinUnits::BTC), scaledDecorationSize(scaledDecorationSize)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(scaledDecorationSize, scaledDecorationSize));
        int xspace = scaledDecorationSize + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();

		QColor foreground = QColor(200, 0, 0);
        QVariant value = index.data(Qt::ForegroundRole);
        if(value.canConvert<QColor>())
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(scaledDecorationSize, scaledDecorationSize);
    }

    int unit;

private:

    int scaledDecorationSize;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    researcherModel(nullptr),
    walletModel(nullptr),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1)
{
    scaledDecorationSize = DECORATION_SIZE * this->logicalDpiX() / 96;

    txdelegate = new TxViewDelegate(this, scaledDecorationSize);

    ui->setupUi(this);

    // Override .ui default spacing to deal with various dpi displays.
    int verticalSpacing = 7 * this->logicalDpiY() / 96;
    ui->verticalLayout_10->setMargin(verticalSpacing);
    ui->formLayout->setVerticalSpacing(verticalSpacing);
    ui->formLayout_2->setVerticalSpacing(verticalSpacing);
    ui->researcherFormLayout->setVerticalSpacing(verticalSpacing);

    QRect verticalSpacerSpacing(0, 0, 20, 20 * this->logicalDpiY() / 96);
    ui->verticalSpacer->setGeometry(verticalSpacerSpacing);
    ui->researcherSectionVerticalSpacer->setGeometry(verticalSpacerSpacing);
    ui->verticalSpacer_2->setGeometry(verticalSpacerSpacing);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(scaledDecorationSize, scaledDecorationSize));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    updateTransactions();

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    connect(ui->pollLabel, SIGNAL(clicked()), this, SLOT(handlePollLabelClicked()));

    // init "out of sync" warning labels
    ui->walletStatusLabel->setText("(" + tr("out of sync") + ")");
    ui->transactionsStatusLabel->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
	OverviewPage::UpdateBoincUtilization();

    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handlePollLabelClicked()
{
    emit pollLabelClicked();
}


void OverviewPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateTransactions();
}

void OverviewPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateTransactions();
}

int OverviewPage::getNumTransactionsForView()
{
    // Compute the maximum number of transactions the transaction list widget
    // can hold without overflowing.
    const size_t itemHeight = scaledDecorationSize + ui->listTransactions->spacing();
    const size_t contentsHeight = ui->listTransactions->height();
    const int numItems = contentsHeight / itemHeight;

    return numItems;
}


void OverviewPage::updateTransactions()
{
    if (filter)
    {
        int numItems = getNumTransactionsForView();

        LogPrint(BCLog::LogFlags::QT, "OverviewPage::updateTransactions(): numItems = %d, getLimit() = %d", numItems, filter->getLimit());

        // This is a "stairstep" approach, using x3 to x6 factors to size the setLimit.
        // Based on testing with a wallet with a large number of transactions (40k+)
        // Using a factor of three is a good balance between the setRowHidden loop
        // and the very high expense of the getLimit call, which invalidates the filter
        // and sort, and implicitly redoes the sort, which can take seconds for a large
        // wallet.

        // Most main window resizes will be done without an actual call to setLimit.
        if (filter->getLimit() < numItems)
        {
            filter->setLimit(numItems * 3);
            LogPrint(BCLog::LogFlags::QT, "OverviewPage::updateTransactions(), setLimit(%d)", numItems * 3);
        }
        else if (filter->getLimit() > numItems * 6)
        {
            filter->setLimit(numItems * 3);
            LogPrint(BCLog::LogFlags::QT, "OverviewPage::updateTransactions(), setLimit(%d)", numItems * 3);
        }

        for (int i = 0; i <= filter->getLimit(); ++i)
        {
            ui->listTransactions->setRowHidden(i, i >= numItems);
        }

        ui->listTransactions->update();
        LogPrint(BCLog::LogFlags::QT, "OverviewPage::updateTransactions(), end update");
    }
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->balanceLabel->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->stakeLabel->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->unconfirmedLabel->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->immatureLabel->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));
    ui->totalLabel->setText(BitcoinUnits::formatWithUnit(unit, balance + stake + unconfirmedBalance + immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->immatureLabel->setVisible(showImmature);
    ui->immatureTextLabel->setVisible(showImmature);
	OverviewPage::UpdateBoincUtilization();

}

void OverviewPage::UpdateBoincUtilization()
{
    {
        LogPrint(BCLog::MISC, "OverviewPage::UpdateBoincUtilization()");

        if (miner_first_pass_complete) g_GlobalStatus.SetGlobalStatus(true);

        const GlobalStatus::globalStatusStringType& globalStatusStrings = g_GlobalStatus.GetGlobalStatusStrings();

        ui->blocksLabel->setText(QString::fromUtf8(globalStatusStrings.blocks.c_str()));
        ui->difficultyLabel->setText(QString::fromUtf8(globalStatusStrings.difficulty.c_str()));
        ui->netWeightLabel->setText(QString::fromUtf8(globalStatusStrings.netWeight.c_str()));
        ui->coinWeightLabel->setText(QString::fromUtf8(globalStatusStrings.coinWeight.c_str()));
        ui->errorsLabel->setText(QString::fromUtf8(globalStatusStrings.errors.c_str()));
    }

    // GetCurrentPollTitle() locks cs_main:
    ui->pollLabel->setText(QString::fromStdString(GRC::GetCurrentPollTitle())
        .left(80)
        .replace(QChar('_'), QChar(' '), Qt::CaseSensitive));
}

void OverviewPage::setResearcherModel(ResearcherModel *researcherModel)
{
    this->researcherModel = researcherModel;

    if (!researcherModel) {
        return;
    }

    updateResearcherStatus();
    connect(researcherModel, SIGNAL(researcherChanged()), this, SLOT(updateResearcherStatus()));
    connect(researcherModel, SIGNAL(magnitudeChanged()), this, SLOT(updateMagnitude()));
    connect(researcherModel, SIGNAL(accrualChanged()), this, SLOT(updatePendingAccrual()));
    connect(researcherModel, SIGNAL(beaconChanged()), this, SLOT(updateResearcherAlert()));
    connect(ui->beaconButton, SIGNAL(clicked()), this, SLOT(onBeaconButtonClicked()));
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->setLimit(getNumTransactionsForView());
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);
        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        connect(model->getOptionsModel(), SIGNAL(LimitTxnDisplayChanged(bool)), this, SLOT(updateTransactions()));
        connect(model, SIGNAL(transactionUpdated()), this, SLOT(updateTransactions()));

        UpdateBoincUtilization();
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, walletModel->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
        updatePendingAccrual();
    }
}

void OverviewPage::updateResearcherStatus()
{
    if (!researcherModel) {
        return;
    }

    ui->statusLabel->setText(researcherModel->formatStatus());
    ui->cpidLabel->setText(researcherModel->formatCpid());

    updateMagnitude();
    updatePendingAccrual();
    updateResearcherAlert();
}

void OverviewPage::updateMagnitude()
{
    if (!researcherModel) {
        return;
    }

    ui->magnitudeLabel->setText(researcherModel->formatMagnitude());
}

void OverviewPage::updatePendingAccrual()
{
    if (!researcherModel) {
        return;
    }

    int unit = BitcoinUnits::BTC;

    if (walletModel) {
        unit = walletModel->getOptionsModel()->getDisplayUnit();
    }

    ui->accrualLabel->setText(researcherModel->formatAccrual(unit));
}

void OverviewPage::updateResearcherAlert()
{
    if (!researcherModel) {
        return;
    }

    ui->researcherAlertWrapper->setVisible(researcherModel->actionNeeded());
}

void OverviewPage::onBeaconButtonClicked()
{
    if (!researcherModel || !walletModel) {
        return;
    }

    researcherModel->showWizard(walletModel);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->walletStatusLabel->setVisible(fShow);
    ui->transactionsStatusLabel->setVisible(fShow);
	OverviewPage::UpdateBoincUtilization();
}

void OverviewPage::updateGlobalStatus()
{
	OverviewPage::UpdateBoincUtilization();
}
