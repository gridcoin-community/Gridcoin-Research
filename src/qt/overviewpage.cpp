#include <QWidget>
#include <QListView>

#include "overviewpage.h"
#include "ui_overviewpage.h"

#ifndef Q_MOC_RUN
#include "qt/decoration.h"
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

#define DECORATION_SIZE 40

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(QObject *parent = nullptr) : QAbstractItemDelegate(parent), unit(BitcoinUnits::BTC)
    {
        QPaintDevice *paintDevice = dynamic_cast<QPaintDevice *>(parent);

        m_decoration_size = GRC::ScalePx(paintDevice, DECORATION_SIZE);
        m_padding_y = GRC::ScalePx(paintDevice, 6);
        m_offset_x = m_decoration_size + GRC::ScalePx(paintDevice, 8);
        m_height = m_decoration_size + (m_padding_y * 2);
        m_half_height = (m_height - (m_padding_y * 2)) / 2;
    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        // Paint the theme's background color for the hover state:
        const QStyle* const style = option.widget->style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.left(), mainRect.top() + m_padding_y, m_decoration_size, m_decoration_size);
        QRect amountRect(mainRect.left() + m_offset_x, mainRect.top() + m_padding_y, mainRect.width() - m_offset_x, m_half_height);
        QRect addressRect(mainRect.left() + m_offset_x, mainRect.top() + m_padding_y + m_half_height, mainRect.width() - m_offset_x, m_half_height);
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
        return QSize(m_decoration_size, m_height);
    }

    int height() const
    {
        return m_height;
    }

    int unit;

private:
    int m_decoration_size;
    int m_padding_y;
    int m_offset_x;
    int m_height;
    int m_half_height;
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
    txdelegate = new TxViewDelegate(this);

    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->headerTitleLabel, 15);
    GRC::ScaleFontPointSize(ui->cpidLabel, 9);
    GRC::ScaleFontPointSize(ui->headerBalanceLabel, 14);
    GRC::ScaleFontPointSize(ui->headerBalanceCaptionLabel, 8);
    GRC::ScaleFontPointSize(ui->headerMagnitudeLabel, 14);
    GRC::ScaleFontPointSize(ui->headerMagnitudeCaptionLabel, 8);
    GRC::ScaleFontPointSize(ui->overviewWalletLabel, 11);
    GRC::ScaleFontPointSize(ui->researcherHeaderLabel, 11);
    GRC::ScaleFontPointSize(ui->stakingHeaderLabel, 11);
    GRC::ScaleFontPointSize(ui->currentPollsHeaderLabel, 11);
    GRC::ScaleFontPointSize(ui->recentTransLabel, 11);
    GRC::ScaleFontPointSize(ui->walletStatusLabel, 8);
    GRC::ScaleFontPointSize(ui->transactionsStatusLabel, 8);
    GRC::ScaleFontPointSize(ui->researcherAlertLabel, 8);

    // Override .ui default spacing to deal with various dpi displays.
    int verticalSpacing = GRC::ScalePx(this, 7);
    ui->walletGridLayout->setVerticalSpacing(verticalSpacing);
    ui->stakingGridLayout->setVerticalSpacing(verticalSpacing);
    ui->researcherGridLayout->setVerticalSpacing(verticalSpacing);

    // Recent Transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    updateTransactions();

    connect(ui->listTransactions, &QListView::clicked, this, &OverviewPage::handleTransactionClicked);
    connect(ui->currentPollsTitleLabel, &ClickLabel::clicked, this, &OverviewPage::handlePollLabelClicked);

    // init "out of sync" warning labels
    ui->walletStatusLabel->setText(tr("Out of Sync"));
    ui->transactionsStatusLabel->setText(tr("Out of Sync"));

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
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
    const size_t itemHeight = txdelegate->height() + ui->listTransactions->spacing();

    // We have to use the frame here because when the listTransactions is hidden, the recentTransactionsNoResult
    // takes up the space and would cause the calculation to be off.
    const size_t contentsHeight = std::max(ui->recentTransactionsFrame->height() - ui->recentTransLabel->height(), 0);

    LogPrint(BCLog::LogFlags::QT, "INFO: %s: contentsHeight = %u, itemHeight = %u",
             __func__, contentsHeight, itemHeight);

    // take one off so that there is not a "half-visible one" there, ensure not below 0.
    const int numItems = std::max((int) (contentsHeight / itemHeight) - 1, 0);

    return numItems;
}


void OverviewPage::updateTransactions()
{
    if (filter)
    {
        int numItems = getNumTransactionsForView();

        LogPrint(BCLog::LogFlags::QT, "OverviewPage::updateTransactions(): numItems = %d, getLimit() = %d",
                 numItems, filter->getLimit());

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

        int transaction_count = filter->rowCount();

        // This needs to be both here and in SetPrivacy because the trigger could come from either.
        ui->recentTransactionsNoResult->setVisible(m_privacy || !transaction_count);
        ui->listTransactions->setVisible(!m_privacy && transaction_count);

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
    ui->headerBalanceLabel->setText(BitcoinUnits::formatOverviewRounded(balance, m_privacy));
    ui->balanceLabel->setText(BitcoinUnits::formatWithPrivacy(unit, balance, m_privacy));
    ui->stakeLabel->setText(BitcoinUnits::formatWithPrivacy(unit, stake, m_privacy));
    ui->unconfirmedLabel->setText(BitcoinUnits::formatWithPrivacy(unit, unconfirmedBalance, m_privacy));
    ui->immatureLabel->setText(BitcoinUnits::formatWithPrivacy(unit, immatureBalance, m_privacy));
    ui->totalLabel->setText(BitcoinUnits::formatWithPrivacy(unit, balance + stake + unconfirmedBalance + immatureBalance,
                                                            m_privacy));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-staking users
    bool showImmature = immatureBalance != 0;
    ui->immatureLabel->setVisible(showImmature);
    ui->immatureTextLabel->setVisible(showImmature);
}

void OverviewPage::setHeight(int height, int height_of_peers, bool in_sync)
{
    QString text = QString::number(height);
    unsigned int percent_progress = 0;

    if (!in_sync && height_of_peers > 0 && height < height_of_peers) {
        percent_progress = height * 100 / height_of_peers;

        text += QString(" of %1 (%2\%)")
                .arg(QString::number(height_of_peers))
                .arg(QString::number(percent_progress));
    }

    ui->blocksLabel->setText(text);
}

void OverviewPage::setDifficulty(double difficulty, double net_weight)
{
    ui->difficultyLabel->setText(QString::number(difficulty, 'f', 3));
    ui->netWeightLabel->setText(QString::number(net_weight, 'f', 3));
}

void OverviewPage::setCoinWeight(double coin_weight)
{
    QString text;

    if (m_privacy) {
        text = QString("#.##");
    } else {
        text = QString::number(coin_weight, 'f', 2);
    }

    ui->coinWeightLabel->setText(text);
}

void OverviewPage::setCurrentPollTitle(const QString& title)
{
    ui->currentPollsTitleLabel->setText(title);
}

void OverviewPage::setPrivacy(bool privacy)
{
    m_privacy = privacy;
    int transaction_count = filter->rowCount();

    if (currentBalance != -1) {
        setBalance(currentBalance, currentStake, currentUnconfirmedBalance, currentImmatureBalance);
    }

    if (m_privacy) {
        ui->recentTransactionsNoResult->showPrivacyEnabledTitle();
    } else {
        ui->recentTransactionsNoResult->showDefaultNothingHereTitle();
    }

    ui->recentTransactionsNoResult->setVisible(m_privacy || !transaction_count);
    ui->listTransactions->setVisible(!m_privacy && transaction_count);
    if (researcherModel) researcherModel->setMaskAccrualAndMagnitude(m_privacy);

    LogPrint(BCLog::LogFlags::QT, "INFO: %s: m_privacy = %u", __func__, m_privacy);

    updateTransactions();
    updatePendingAccrual();
}

void OverviewPage::setResearcherModel(ResearcherModel *researcherModel)
{
    this->researcherModel = researcherModel;

    if (!researcherModel) {
        return;
    }

    updateResearcherStatus();
    connect(researcherModel, &ResearcherModel::researcherChanged, this, &OverviewPage::updateResearcherStatus);
    connect(researcherModel, &ResearcherModel::magnitudeChanged, this, &OverviewPage::updateMagnitude);
    connect(researcherModel, &ResearcherModel::accrualChanged, this, &OverviewPage::updatePendingAccrual);
    connect(researcherModel, &ResearcherModel::beaconChanged, this, &OverviewPage::updateResearcherAlert);
    connect(ui->researcherConfigToolButton, &QAbstractButton::clicked, this, &OverviewPage::onBeaconButtonClicked);
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

        int num_transactions_for_view = getNumTransactionsForView();
        filter->setLimit(num_transactions_for_view);

        LogPrint(BCLog::LogFlags::QT, "INFO: %s: num_transactions_for_view = %i, getLimit() = %i",
                 __func__, num_transactions_for_view, filter->getLimit());

        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);
        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, &WalletModel::balanceChanged, this, &OverviewPage::setBalance);

        connect(model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &OverviewPage::updateDisplayUnit);

        connect(model->getOptionsModel(), &OptionsModel::LimitTxnDisplayChanged, this, &OverviewPage::updateTransactions);
        connect(model, &WalletModel::transactionUpdated, this, &OverviewPage::updateTransactions);

        // Set the privacy state for the overview screen from the optionsModel for init.
        setPrivacy(model->getOptionsModel()->getMaskValues());

        // Connect the privacy mode setting to the options dialog.
        connect(walletModel->getOptionsModel(), &OptionsModel::MaskValuesChanged, this, &OverviewPage::setPrivacy);
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

    if (researcherModel->hasEligibleProjects()) {
        ui->cpidTextLabel->setText("CPID");
        ui->cpidLabel->setText(researcherModel->formatCpid());
        ui->cpidLabel->setVisible(true);
        ui->headerMagnitudeWrapper->setVisible(true);
        ui->headerMagnitudeVLine->setVisible(true);
    } else if (researcherModel->hasPoolProjects()) {
        ui->cpidTextLabel->setText(tr("Pool"));
        ui->cpidLabel->setVisible(false);
        ui->headerMagnitudeWrapper->setVisible(false);
        ui->headerMagnitudeVLine->setVisible(false);
    } else {
        ui->cpidTextLabel->setText(tr("Staking Only"));
        ui->cpidLabel->setVisible(false);
        ui->headerMagnitudeWrapper->setVisible(false);
        ui->headerMagnitudeVLine->setVisible(false);
    }

    updateMagnitude();
    updatePendingAccrual();
    updateResearcherAlert();
}

void OverviewPage::updateMagnitude()
{
    if (!researcherModel) {
        return;
    }

    const QString magnitude = researcherModel->formatMagnitude();

    ui->headerMagnitudeLabel->setText(magnitude);
    ui->magnitudeLabel->setText(magnitude);
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

    const bool action_needed = researcherModel->actionNeeded();

    ui->researcherAlertLabel->setVisible(action_needed);
    ui->researcherConfigToolButton->setProperty("actionNeeded", action_needed);
    ui->researcherConfigToolButton->style()->unpolish(ui->researcherConfigToolButton);
    ui->researcherConfigToolButton->style()->polish(ui->researcherConfigToolButton);
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
}
