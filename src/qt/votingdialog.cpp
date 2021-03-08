// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>

#ifdef QT_CHARTS_LIB
	#include <QtCharts/QChartView>
	#include <QtCharts/QPieSeries>
#endif

#include <QtConcurrentRun>
#include <QClipboard>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QModelIndexList>
#include <QPainter>
#include <QPaintEvent>

#include <QRectF>
#include <QString>
#include <QVBoxLayout>

#include "util.h"
#include "gridcoin/voting/builders.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "qt/walletmodel.h"
#include "votingdialog.h"
#include "rpc/protocol.h"
#include "sync.h"

using namespace GRC;

extern CCriticalSection cs_main;

static int column_alignments[] = {
    Qt::AlignRight|Qt::AlignVCenter, // RowNumber
    Qt::AlignLeft|Qt::AlignVCenter, // Expiration
    Qt::AlignLeft|Qt::AlignVCenter, // Title
    Qt::AlignLeft|Qt::AlignVCenter, // BestAnswer
    Qt::AlignRight|Qt::AlignVCenter, // TotalParticipants
    Qt::AlignRight|Qt::AlignVCenter, // TotalShares
    Qt::AlignLeft|Qt::AlignVCenter, // ShareType
    };

// VotingTableModel
//
VotingTableModel::VotingTableModel(void)
{
    columns_
        << tr("#")
        << tr("Expiration")
        << tr("Title")
        << tr("Best Answer")
        << tr("# Voters")  // Total Participants
        << tr("Total Shares")
        << tr("Share Type")
        ;
}

VotingTableModel::~VotingTableModel(void)
{
    for(ssize_t i=0; i < data_.size(); i++)
        if (data_.at(i))
            delete data_.at(i);
}

int VotingTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return data_.size();
}

int VotingTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns_.length();
}

QVariant VotingTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const VotingItem *item = (const VotingItem *) index.internalPointer();

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column())
        {
        case RowNumber:
            return QVariant(item->rowNumber_);
        case Title:
            return item->title_;
        case Expiration:
            return item->expiration_.toString();
        case ShareType:
            return item->shareType_;
        case TotalParticipants:
            return item->totalParticipants_;
        case TotalShares:
            return item->totalShares_;
        case BestAnswer:
            return item->bestAnswer_;
        default:
            ;
        }
        break;

    case SortRole:
        switch (index.column())
        {
        case RowNumber:
            return QVariant(item->rowNumber_);
        case Title:
            return item->title_;
        case Expiration:
            return item->expiration_;
        case ShareType:
            return item->shareType_;
        case TotalParticipants:
            return item->totalParticipants_;
        case TotalShares:
            return item->totalShares_;
        case BestAnswer:
            return item->bestAnswer_;
        default:
            ;
        }
        break;

    case RowNumberRole:
        return QVariant(item->rowNumber_);

    case TitleRole:
        return item->title_;

    case ExpirationRole:
        return item->expiration_;

    case ShareTypeRole:
        return item->shareType_;

    case TotalParticipantsRole:
        return item->totalParticipants_;

    case TotalSharesRole:
        return item->totalShares_;

    case BestAnswerRole:
        return item->bestAnswer_;

    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];

    default:
        ;
    }

    return QVariant();
}

// section corresponds to column number for horizontal headers
QVariant VotingTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && section >= 0)
    {
        if (role == Qt::DisplayRole)
            return columns_[section];
        else
        if (role == Qt::TextAlignmentRole)
            return column_alignments[section];
        else
        if (role == Qt::ToolTipRole)
        {
            switch (section)
            {
            case RowNumber:
                return tr("Row Number.");
            case Title:
                return tr("Title.");
            case Expiration:
                return tr("Expiration.");
            case ShareType:
                return tr("Share Type.");
            case TotalParticipants:
                return tr("Total Participants.");
            case TotalShares:
                return tr("Total Shares.");
            case BestAnswer:
                return tr("Best Answer.");
            }
        }
    }
    return QVariant();
}

const VotingItem *VotingTableModel::index(int row) const
{
    if ((row >= 0) && (row < data_.size()))
        return data_[row];
    return 0;
}

QModelIndex VotingTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    const VotingItem *item = index(row);
    if (item)
        return createIndex(row, column, (void *)item);
    return QModelIndex();
}

Qt::ItemFlags VotingTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

namespace {
VotingItem* BuildPollItem(const PollRegistry::Sequence::Iterator& iter)
{
    const PollResultOption result = PollResult::BuildFor(iter->Ref());

    if (!result) {
        return nullptr;
    }

    const Poll& poll = result->m_poll;

    VotingItem *item = new VotingItem;
    item->pollTxid_ = iter->Ref().Txid();
    item->expiration_ = QDateTime::fromMSecsSinceEpoch(poll.Expiration() * 1000);
    item->shareType_ = QString::fromStdString(poll.WeightTypeToString());
    item->responseType_ = QString::fromStdString(poll.ResponseTypeToString());
    item->totalParticipants_ = result->m_votes.size();
    item->totalShares_ = result->m_total_weight / (double)COIN;

    item->title_ = QString::fromStdString(poll.m_title).replace("_"," ");
    item->question_ = QString::fromStdString(poll.m_question).replace("_"," ");
    item->url_ = QString::fromStdString(poll.m_url).trimmed();

    if (!item->url_.startsWith("http://") && !item->url_.startsWith("https://")) {
        item->url_.prepend("http://");
    }

    for (size_t i = 0; i < result->m_responses.size(); ++i) {
        item->vectorOfAnswers_.emplace_back(
            poll.Choices().At(i)->m_label,
            result->m_responses[i].m_weight / (double)COIN,
            result->m_responses[i].m_votes);
    }

    if (!result->m_votes.empty()) {
        item->bestAnswer_ = QString::fromStdString(result->WinnerLabel()).replace("_"," ");
    }

    return item;
}
} // Anonymous namespace

void VotingTableModel::resetData(bool history)
{
    std::string function = __func__;
    function += ": ";

    // data: erase
    if (data_.size()) {
        beginRemoveRows(QModelIndex(), 0, data_.size() - 1);
        for(ssize_t i=0; i < data_.size(); i++)
            if (data_.at(i))
                delete data_.at(i);
        data_.clear();
        endRemoveRows();
    }

    g_timer.GetTimes(function + "erase data", "votedialog");

    // retrieve data
    std::vector<VotingItem *> items;

    {
        LOCK(cs_main);

        for (const auto iter : GetPollRegistry().Polls().OnlyActive(!history)) {
            if (VotingItem* item = BuildPollItem(iter)) {
                item->rowNumber_ = items.size() + 1;
                items.push_back(item);

            }
        }

        g_timer.GetTimes(function + "populate poll results (cs_main lock)", "votedialog");
    }

    // data: populate
    if (items.size()) {
        beginInsertRows(QModelIndex(), 0, items.size() - 1);
        for(size_t i=0; i < items.size(); i++)
            data_.append(items[i]);
        endInsertRows();

        g_timer.GetTimes(function + "insert data in display table", "votedialog");
    }
}

// VotingProxyModel
//

VotingProxyModel::VotingProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(VotingTableModel::SortRole);
}

void VotingProxyModel::setFilterTQAU(const QString &str)
{
    filterTQAU_ = str;
    invalidateFilter();
}

bool VotingProxyModel::filterAcceptsRow(int row, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(row, 0, sourceParent);

    QString title = index.data(VotingTableModel::TitleRole).toString();

    if (!title.contains(filterTQAU_, Qt::CaseInsensitive)){
        return false;
    }
    return true;
}

// VotingDialog
//
VotingDialog::VotingDialog(QWidget *parent)
    : QWidget(parent),
    tableView_(0),
    tableModel_(0),
    proxyModel_(0),
    chartDialog_(0),
    voteDialog_(0)
{
    // data
    tableModel_ = new VotingTableModel();
    proxyModel_ = new VotingProxyModel(this);
    proxyModel_->setSourceModel(tableModel_);

    QVBoxLayout *vlayout = new QVBoxLayout(this);

    QGroupBox *groupBox = new QGroupBox(tr("Active Polls (Right Click to Vote)"));
    vlayout->addWidget(groupBox);

    QVBoxLayout *groupboxvlayout = new QVBoxLayout();
    groupBox->setLayout(groupboxvlayout);

    QHBoxLayout *filterhlayout = new QHBoxLayout();
    groupboxvlayout->addLayout(filterhlayout);

    // Filter by Title with one QLineEdit
    QLabel *filterByTQAULabel = new QLabel(tr("Filter: "));
    filterByTQAULabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    filterByTQAULabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    filterByTQAULabel->setMaximumWidth(150);
    filterhlayout->addWidget(filterByTQAULabel);
    filterTQAU = new QLineEdit();
    filterTQAU->setMaximumWidth(350);
    filterhlayout->addWidget(filterTQAU);
    connect(filterTQAU, SIGNAL(textChanged(QString)), this, SLOT(filterTQAUChanged(QString)));
    filterhlayout->addStretch();

    // buttons in horizontal layout
    QHBoxLayout *groupboxhlayout = new QHBoxLayout();
    groupboxvlayout->addLayout(groupboxhlayout);

    QPushButton *resetButton = new QPushButton();
    resetButton->setText(tr("Reload Polls"));
    resetButton->setMaximumWidth(150);
    groupboxhlayout->addWidget(resetButton);
    connect(resetButton, SIGNAL(clicked()), this, SLOT(resetData()));

    QPushButton *histButton = new QPushButton();
    histButton->setText(tr("Load History"));
    histButton->setMaximumWidth(150);
    groupboxhlayout->addWidget(histButton);
    connect(histButton, SIGNAL(clicked()), this, SLOT(loadHistory()));

    QPushButton *newPollButton = new QPushButton();
    newPollButton->setText(tr("Create Poll"));
    newPollButton->setMaximumWidth(150);
    groupboxhlayout->addWidget(newPollButton);
    connect(newPollButton, SIGNAL(clicked()), this, SLOT(showNewPollDialog()));

    groupboxhlayout->addStretch();

    tableView_ = new QTableView();
    tableView_->installEventFilter(this);
    tableView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    // tableView_->setTabKeyNavigation(false);
    tableView_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tableView_, SIGNAL(customContextMenuRequested(const QPoint &)),
        this, SLOT(showContextMenu(const QPoint &)));
    tableView_->setAlternatingRowColors(true);
    tableView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView_->setSortingEnabled(true);
    tableView_->sortByColumn(VotingTableModel::RowNumber, Qt::DescendingOrder);
    tableView_->verticalHeader()->hide();

    tableView_->setModel(proxyModel_);
    tableView_->setFont(QFont("Arial", 10));
    tableView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView_->horizontalHeader()->setMinimumWidth(VOTINGDIALOG_WIDTH_RowNumber + VOTINGDIALOG_WIDTH_Title + VOTINGDIALOG_WIDTH_Expiration + VOTINGDIALOG_WIDTH_ShareType + VOTINGDIALOG_WIDTH_TotalParticipants + VOTINGDIALOG_WIDTH_TotalShares + VOTINGDIALOG_WIDTH_BestAnswer);
    tableView_->verticalHeader()->setDefaultSectionSize(40);

    groupboxvlayout->addWidget(tableView_);

    // loading overlay. Due to a bug in QFutureWatcher for Qt <5.6.0 we
    // have to track the running state ourselves. See
    // https://bugreports.qt.io/browse/QTBUG-12358
    watcher.setProperty("running", false);
    connect(&watcher, SIGNAL(finished()), this, SLOT(onLoadingFinished()));
    loadingIndicator = new QLabel(this);
    loadingIndicator->setWordWrap(true);

    groupboxvlayout->addWidget(loadingIndicator);

    chartDialog_ = new VotingChartDialog(this);
    voteDialog_ = new VotingVoteDialog(this);
    pollDialog_ = new NewPollDialog(this);

    loadingIndicator->setText(tr("Press reload to load polls... This can take several minutes, and the wallet may not respond until finished."));
    tableView_->hide();
    loadingIndicator->show();

    QObject::connect(vote_update_age_timer, SIGNAL(timeout()), this, SLOT(setStale()));
}

void VotingDialog::setModel(WalletModel *wallet_model)
{
    if (!wallet_model) {
        return;
    }

    voteDialog_->setModel(wallet_model);
    pollDialog_->setModel(wallet_model);
}

void VotingDialog::loadPolls(bool history)
{
    std::string function = __func__;
    function += ": ";

    bool isRunning = watcher.property("running").toBool();
    if (tableModel_&& !isRunning)
    {
        loadingIndicator->setText(tr("Recalculating voting weights... This can take several minutes, and the wallet may not respond until finished."));
        tableView_->hide();
        loadingIndicator->show();

        g_timer.InitTimer("votedialog", LogInstance().WillLogCategory(BCLog::LogFlags::MISC));
        vote_update_age_timer->start(STALE);

        QFuture<void> future = QtConcurrent::run(tableModel_, &VotingTableModel::resetData, history);

        g_timer.GetTimes(function + "Post future assignment", "votedialog");

        watcher.setProperty("running", true);
        watcher.setFuture(future);
    }
}

void VotingDialog::resetData(void)
{
    loadPolls(false);
}

void VotingDialog::loadHistory(void)
{
    loadPolls(true);
}

void VotingDialog::setStale(void)
{
    LogPrint(BCLog::LogFlags::MISC, "INFO: %s called.", __func__);

    // If the stale flag is not set, but this function is called, it is from the timeout
    // trigger of the vote_update_age_timer. Therefore set the loading indicator to stale
    // and set the stale flag to true. The stale flag will be reset and the timer restarted
    // when the loadPolls is called to refresh.
    if (!stale)
    {
        loadingIndicator->setText(tr("Poll data is more than one hour old. Press reload to update... "
                                     "This can take several minutes, and the wallet may not respond "
                                     "until finished."));
        tableView_->hide();
        loadingIndicator->show();

        stale = true;
    }
}

void VotingDialog::onLoadingFinished(void)
{
    watcher.setProperty("running", false);

    int rowsCount = tableView_->verticalHeader()->count();
    if (rowsCount > 0) {
        loadingIndicator->hide();
        tableView_->show();
    } else {
        loadingIndicator->setText(tr("No polls !"));
    }

    stale = false;
}

void VotingDialog::tableColResize(void)
{
    tableView_->setColumnWidth(VotingTableModel::RowNumber, VOTINGDIALOG_WIDTH_RowNumber);
    tableView_->setColumnWidth(VotingTableModel::Expiration, VOTINGDIALOG_WIDTH_Expiration);
    tableView_->setColumnWidth(VotingTableModel::ShareType, VOTINGDIALOG_WIDTH_ShareType);
    tableView_->setColumnWidth(VotingTableModel::TotalParticipants, VOTINGDIALOG_WIDTH_TotalParticipants);
    tableView_->setColumnWidth(VotingTableModel::TotalShares, VOTINGDIALOG_WIDTH_TotalShares);

    int fixedColWidth = VOTINGDIALOG_WIDTH_RowNumber + VOTINGDIALOG_WIDTH_Expiration + VOTINGDIALOG_WIDTH_ShareType + VOTINGDIALOG_WIDTH_TotalParticipants + VOTINGDIALOG_WIDTH_TotalShares;

    int dynamicWidth = tableView_->horizontalHeader()->width() - fixedColWidth;
    int nColumns = 2; // 2 dynamic columns
    int columns[] = {VotingTableModel::Title,VotingTableModel::BestAnswer};
    int remainingWidth = dynamicWidth % nColumns;
    for(int cNum = 0; cNum < nColumns; cNum++) {
        if(remainingWidth > 0)
        {
            tableView_->setColumnWidth(columns[cNum], (dynamicWidth/nColumns) + 1);
            remainingWidth -= 1;
        }
        else
        {
            tableView_->setColumnWidth(columns[cNum], dynamicWidth/nColumns);
        }
    }
}

//customize resize event to allow automatic as well as interactive resizing
void VotingDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    tableColResize();
}

//customize show event for instant table resize
void VotingDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (! event->spontaneous())
        tableColResize();
}

bool VotingDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == tableView_)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            if ((ke->key() == Qt::Key_C)
                && (ke->modifiers().testFlag(Qt::ControlModifier)))
            {
                /* Ctrl-C: copy the selected cells in TableModel */
                QString selected_text;
                QItemSelectionModel *selection = tableView_->selectionModel();
                QModelIndexList indexes = selection->selectedIndexes();
                std::sort(indexes.begin(), indexes.end());
                int prev_row = -1;
                for(int i=0; i < indexes.size(); i++) {
                    QModelIndex index = indexes.at(i);
                    if (i) {
                        char c = (index.row() != prev_row)? '\n': '\t';
                        selected_text.append(c);
                    }
                    QVariant data = tableView_->model()->data(index);
                    selected_text.append( data.toString() );
                    prev_row = index.row();
                }
                QApplication::clipboard()->setText(selected_text);
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void VotingDialog::filterTQAUChanged(const QString &str)
{
    if (proxyModel_)
        proxyModel_->setFilterTQAU(str);
}

void VotingDialog::showChartDialog(void)
{
    if (!proxyModel_ || !tableModel_ || !tableView_ || !chartDialog_)
        return;

    QItemSelectionModel *selection = tableView_->selectionModel();
    QModelIndexList indexes = selection->selectedIndexes();
    if (!indexes.size())
        return;

    // take the row of the top selected cell
    std::sort(indexes.begin(), indexes.end());
    int row = proxyModel_->mapToSource(indexes.at(0)).row();

    // reset the dialog's data
    const VotingItem *item = tableModel_->index(row);
    chartDialog_->resetData(item);

    chartDialog_->show();
    chartDialog_->raise();
    chartDialog_->setFocus();
}

void VotingDialog::showContextMenu(const QPoint &pos)
{
    QPoint globalPos = tableView_->viewport()->mapToGlobal(pos);

    QMenu menu;
    menu.addAction("Show Results", this, SLOT(showChartDialog()));
    menu.addAction("Vote", this, SLOT(showVoteDialog()));
    menu.exec(globalPos);
}

void VotingDialog::showVoteDialog(void)
{
    if (!proxyModel_ || !tableModel_ || !tableView_ || !voteDialog_)
        return;

    QItemSelectionModel *selection = tableView_->selectionModel();
    QModelIndexList indexes = selection->selectedIndexes();
    if (!indexes.size())
        return;

    // take the row of the top selected cell
    std::sort(indexes.begin(), indexes.end());
    int row = proxyModel_->mapToSource(indexes.at(0)).row();

    // reset the dialog's data
    const VotingItem *item = tableModel_->index(row);
    voteDialog_->resetData(item);

    voteDialog_->show();
    voteDialog_->raise();
    voteDialog_->setFocus();
}

void VotingDialog::showNewPollDialog(void)
{
    if (!proxyModel_ || !tableModel_ || !tableView_ || !voteDialog_)
        return;

    // reset the dialog's data
    pollDialog_->resetData();

    pollDialog_->show();
    pollDialog_->raise();
    pollDialog_->setFocus();
}

// VotingChartDialog
//
VotingChartDialog::VotingChartDialog(QWidget *parent)
    : QDialog(parent)
#ifdef QT_CHARTS_LIB
    ,chart_(0)
#endif
    ,answerTable_(NULL)
{
    setWindowTitle(tr("Poll Results"));
    resize(QDesktopWidget().availableGeometry(this).size() * 0.4);

    QVBoxLayout *vlayout = new QVBoxLayout(this);

    QGridLayout *glayout = new QGridLayout();
    glayout->setHorizontalSpacing(0);
    glayout->setVerticalSpacing(0);
    glayout->setColumnStretch(0, 1);
    glayout->setColumnStretch(1, 3);
    glayout->setColumnStretch(2, 5);

    vlayout->addLayout(glayout);

    QLabel *question = new QLabel(tr("Q: "));
    question->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    question->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(question, 0, 0);

    question_ = new QLabel();
    question_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    question_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(question_, 0, 1);

    QLabel *discussionLabel = new QLabel(tr("Discussion URL: "));
    discussionLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    discussionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(discussionLabel, 1, 0);

    url_ = new QLabel();
    url_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    url_->setTextFormat(Qt::RichText);
    url_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    url_->setOpenExternalLinks(true);
    glayout->addWidget(url_, 1, 1);

    QLabel *bestAnswer = new QLabel(tr("Best Answer: "));
    bestAnswer->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    bestAnswer->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(bestAnswer, 3, 0);

    answer_ = new QLabel();
    answer_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    answer_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(answer_, 3, 1);

    QTabWidget *resTabWidget = new QTabWidget;

#ifdef QT_CHARTS_LIB
    chart_ = new QtCharts::QChart;
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignRight);
    QtCharts::QChartView *m_chartView = new QtCharts::QChartView(chart_);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    resTabWidget->addTab(m_chartView, tr("Chart"));
#endif

    answerModel_ = new QStandardItemModel();
    answerModel_->setColumnCount(3);
    answerModel_->setRowCount(0);
    answerModel_->setHeaderData(0, Qt::Horizontal, tr("Answer"));
    answerModel_->setHeaderData(1, Qt::Horizontal, tr("Shares"));
    answerModel_->setHeaderData(2, Qt::Horizontal, "%");

    answerTable_ = new QTableView(this);
    answerTable_->setModel(answerModel_);
    answerTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    answerTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
    answerTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeMode::ResizeToContents);
    answerTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    answerTable_->setEditTriggers( QAbstractItemView::NoEditTriggers );
    resTabWidget->addTab(answerTable_, tr("List"));
    vlayout->addWidget(resTabWidget);
}

void VotingChartDialog::resetData(const VotingItem *item)
{
    if (!item)
        return;

    answerModel_->setRowCount(0);
    answerTable_->sortByColumn(-1, Qt::AscendingOrder);
    answerTable_->setSortingEnabled(false);

#ifdef QT_CHARTS_LIB
    QList<QtCharts::QAbstractSeries *> oldSeriesList = chart_->series();
    foreach (QtCharts::QAbstractSeries *oldSeries, oldSeriesList)
        chart_->removeSeries(oldSeries);

    QtCharts::QPieSeries *series = new QtCharts::QPieSeries();
#endif

    question_->setText(item->question_);
    url_->setText("<a href=\""+item->url_+"\">"+item->url_+"</a>");
    answer_->setText(item->bestAnswer_);
    answer_->setVisible(!item->bestAnswer_.isEmpty());
    answerModel_->setRowCount(item->vectorOfAnswers_.size());

    for (size_t y = 0; y < item->vectorOfAnswers_.size(); y++)
    {
        const auto& responses = item->vectorOfAnswers_;
        const QString answer = QString::fromStdString(responses[y].answer);

        QStandardItem *answerItem = new QStandardItem(answer);
        answerItem->setData(answer);
        answerModel_->setItem(y, 0, answerItem);
        QStandardItem *iSharesItem = new QStandardItem(QString::number(responses[y].shares, 'f', 0));
        iSharesItem->setData(responses[y].shares);
        iSharesItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        answerModel_->setItem(y, 1, iSharesItem);
        QStandardItem *percentItem = new QStandardItem();

        if (item->totalShares_ > 0) {
            const double ratio = responses[y].shares / (double)item->totalShares_;
            percentItem->setText(QString::number(ratio * 100, 'f', 2));
            percentItem->setData(ratio * 100);
            percentItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }

        answerModel_->setItem(y, 2, percentItem);

#ifdef QT_CHARTS_LIB
        QtCharts::QPieSlice *slice = new QtCharts::QPieSlice(answer, responses[y].shares);
        series->append(slice);
        chart_->addSeries(series);
#endif
    }

    answerModel_->setSortRole(Qt::UserRole+1);
    answerTable_->setSortingEnabled(true);
}

// VotingVoteDialog
//
VotingVoteDialog::VotingVoteDialog(QWidget *parent)
    : QDialog(parent)
    , m_wallet_model(nullptr)
{
    setWindowTitle(tr("PlaceVote"));
    resize(QDesktopWidget().availableGeometry(this).size() * 0.4);

    QVBoxLayout *vlayout = new QVBoxLayout(this);

    QGridLayout *glayout = new QGridLayout();
    glayout->setHorizontalSpacing(0);
    glayout->setVerticalSpacing(0);
    glayout->setColumnStretch(0, 1);
    glayout->setColumnStretch(1, 3);
    glayout->setColumnStretch(2, 5);

    vlayout->addLayout(glayout);

    QLabel *question = new QLabel(tr("Q: "));
    question->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    question->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(question, 0, 0);

    question_ = new QLabel();
    question_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    question_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(question_, 0, 1);

    QLabel *discussionLabel = new QLabel(tr("Discussion URL: "));
    discussionLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    discussionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(discussionLabel, 1, 0);

    url_ = new QLabel();
    url_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    url_->setTextFormat(Qt::RichText);
    url_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    url_->setOpenExternalLinks(true);
    glayout->addWidget(url_, 1, 1);

    QLabel *responseTypeLabel = new QLabel(tr("Response Type: "));
    responseTypeLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    responseTypeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(responseTypeLabel, 3, 0);

    responseType_ = new QLabel();
    responseType_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    responseType_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(responseType_, 3, 1);

    QLabel *bestAnswer = new QLabel(tr("Best Answer: "));
    bestAnswer->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    bestAnswer->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(bestAnswer, 4, 0);

    answer_ = new QLabel();
    answer_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    answer_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(answer_, 4, 1);

    answerList_ = new QListWidget(this);
    vlayout->addWidget(answerList_);

    QHBoxLayout *hlayout = new QHBoxLayout();
    vlayout->addLayout(hlayout);

    QPushButton *voteButton = new QPushButton();
    voteButton->setText(tr("Vote"));
    hlayout->addWidget(voteButton);
    connect(voteButton, SIGNAL(clicked()), this, SLOT(vote()));

    voteNote_ = new QLabel();
    voteNote_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    voteNote_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    voteNote_->setWordWrap(true);
    hlayout->addWidget(voteNote_);

}

void VotingVoteDialog::setModel(WalletModel *wallet_model)
{
    if (!wallet_model) {
        return;
    }

    m_wallet_model = wallet_model;
}

void VotingVoteDialog::resetData(const VotingItem *item)
{
    if (!item)
        return;

    answerList_->clear();
    voteNote_->clear();
    question_->setText(item->question_);
    url_->setText("<a href=\""+item->url_+"\">"+item->url_+"</a>");
    responseType_->setText(item->responseType_);
    answer_->setText(item->bestAnswer_);
    pollTxid_ = item->pollTxid_;

    for (const auto& choice : item->vectorOfAnswers_) {
        QListWidgetItem *answerItem = new QListWidgetItem(QString::fromStdString(choice.answer).replace("_", " "), answerList_);
        answerItem->setCheckState(Qt::Unchecked);
    }
}

void VotingVoteDialog::vote(void)
{
    // This overall try-catch is needed to properly catch the VoteBuilder builder move constructor and assignment,
    // otherwise an expired poll bubbles up all the way to the app level and ends execution with the exception handler
    // in bitcoin.cpp, which is not what is intended here. It also catchs any thrown VotingError exceptions in
    // builder.AddResponse() and SendVoteContract().
    try {
        voteNote_->setStyleSheet("QLabel { color : red; }");

        LOCK(cs_main);

        const PollReference* ref = GetPollRegistry().TryByTxid(pollTxid_);

        if (!ref) {
            voteNote_->setText(tr("Poll not found."));
            return;
        }

        const PollOption poll = ref->TryReadFromDisk();

        if (!poll) {
            voteNote_->setText(tr("Failed to load poll from disk"));
            return;
        }

        VoteBuilder builder = VoteBuilder::ForPoll(*poll, ref->Txid());

        for (int row = 0; row < answerList_->count(); ++row) {
            if (answerList_->item(row)->checkState() == Qt::Checked) {
                builder = builder.AddResponse(row);
            }
        }

        const WalletModel::UnlockContext unlock_context(m_wallet_model->requestUnlock());

        if (!unlock_context.isValid()) {
            voteNote_->setText(tr("Please unlock the wallet."));
            return;
        }

        SendVoteContract(std::move(builder));

        voteNote_->setStyleSheet("QLabel { color : green; }");
        voteNote_->setText(tr("Success. Vote will activate with the next block."));
    } catch (const VotingError& e){
        voteNote_->setText(e.what());
        return;
    }
}

NewPollDialog::NewPollDialog(QWidget *parent)
    : QDialog(parent)
    , m_wallet_model(nullptr)
{
    setWindowTitle(tr("Create Poll"));
    resize(QDesktopWidget().availableGeometry(this).size() * 0.4);

    QVBoxLayout *vlayout = new QVBoxLayout(this);

    QGridLayout *glayout = new QGridLayout();
    glayout->setHorizontalSpacing(0);
    glayout->setVerticalSpacing(0);
    glayout->setColumnStretch(0, 1);
    glayout->setColumnStretch(1, 3);
    glayout->setColumnStretch(2, 5);

    vlayout->addLayout(glayout);

    //title
    QLabel *title = new QLabel(tr("Title: "));
    title->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(title, 0, 0);

    title_ = new QLineEdit();
    title_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    glayout->addWidget(title_, 0, 1);

    //days
    QLabel *days = new QLabel(tr("Days: "));
    days->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    days->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(days, 1, 0);

    days_ = new QLineEdit();
    days_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    glayout->addWidget(days_, 1, 1);

    //question
    QLabel *question = new QLabel(tr("Question: "));
    question->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    question->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(question, 2, 0);

    question_ = new QLineEdit();
    question_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    glayout->addWidget(question_, 2, 1);

    //url
    QLabel *discussionLabel = new QLabel(tr("Discussion URL: "));
    discussionLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    discussionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(discussionLabel, 3, 0);

    url_ = new QLineEdit();
    url_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    glayout->addWidget(url_, 3, 1);

    //share type
    QLabel *shareType = new QLabel(tr("Share Type: "));
    shareType->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    shareType->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(shareType, 4, 0);

    shareTypeBox_ = new QComboBox(this);
    QStringList shareTypeBoxItems;
    shareTypeBoxItems << tr("Balance") << tr("Magnitude+Balance");
    shareTypeBox_->addItems(shareTypeBoxItems);
    glayout->addWidget(shareTypeBox_, 4, 1);

    // response type
    QLabel *responseTypeLabel = new QLabel(tr("Response Type: "));
    responseTypeLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    responseTypeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(responseTypeLabel, 5, 0);

    responseTypeBox_ = new QComboBox(this);
    QStringList responseTypeBoxItems;
    responseTypeBoxItems
        << tr("Yes/No/Abstain")
        << tr("Single Choice")
        << tr("Multiple Choice");
    responseTypeBox_->addItems(responseTypeBoxItems);
    glayout->addWidget(responseTypeBox_, 5, 1);

    // cost
    QLabel *costLabelLabel = new QLabel(tr("Cost:"));
    costLabelLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    costLabelLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(costLabelLabel, 6, 0);

    // TODO: make this dynamic when rewriting the voting GUI:
    QLabel *costLabel = new QLabel(tr("50 GRC + transaction fee"));
    costLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    costLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(costLabel, 6, 1);

    //answers
    answerList_ = new QListWidget(this);
    answerList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(answerList_, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    vlayout->addWidget(answerList_);
    connect (answerList_, SIGNAL (itemDoubleClicked (QListWidgetItem *)), this, SLOT (editItem (QListWidgetItem *)));

    QHBoxLayout *hlayoutTools = new QHBoxLayout();
    vlayout->addLayout(hlayoutTools);

    QPushButton *addItemButton = new QPushButton();
    addItemButton->setText(tr("Add Item"));
    hlayoutTools->addWidget(addItemButton);
    connect(addItemButton, SIGNAL(clicked()), this, SLOT(addItem()));

    QPushButton *removeItemButton = new QPushButton();
    removeItemButton->setText(tr("Remove Item"));
    hlayoutTools->addWidget(removeItemButton);
    connect(removeItemButton, SIGNAL(clicked()), this, SLOT(removeItem()));

    QPushButton *clearAllButton = new QPushButton();
    clearAllButton->setText(tr("Clear All"));
    hlayoutTools->addWidget(clearAllButton);
    connect(clearAllButton, SIGNAL(clicked()), this, SLOT(resetData()));

    QHBoxLayout *hlayoutBottom = new QHBoxLayout();
    vlayout->addLayout(hlayoutBottom);

    QPushButton *pollButton = new QPushButton();
    pollButton->setText(tr("Create Poll"));
    hlayoutBottom->addWidget(pollButton);
    connect(pollButton, SIGNAL(clicked()), this, SLOT(createPoll()));

    pollNote_ = new QLabel();
    pollNote_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    pollNote_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pollNote_->setWordWrap(true);
    hlayoutBottom->addWidget(pollNote_);

}

void NewPollDialog::setModel(WalletModel *wallet_model)
{
    if (!wallet_model) {
        return;
    }

    m_wallet_model = wallet_model;
}

void NewPollDialog::resetData()
{
    answerList_->clear();
    pollNote_->clear();
    title_->clear();
    days_->clear();
    question_->clear();
    url_->clear();
}

void NewPollDialog::createPoll(void)
{
    pollNote_->setStyleSheet("QLabel { color : red; }");
    PollBuilder builder = PollBuilder();

    try {
        builder = builder
            .SetType(PollType::SURVEY)
            .SetTitle(title_->text().toStdString())
            .SetDuration(days_->text().toInt())
            .SetQuestion(question_->text().toStdString())
            // The dropdown list only contains non-deprecated weight type
            // options which start from offset 2:
            .SetWeightType(shareTypeBox_->currentIndex() + 2)
            .SetResponseType(responseTypeBox_->currentIndex() + 1)
            .SetUrl(url_->text().toStdString());

        for (int row = 0; row < answerList_->count(); ++row) {
            const QListWidgetItem* const item = answerList_->item(row);
            builder = builder.AddChoice(item->text().toStdString());
        }
    } catch (const VotingError& e) {
        pollNote_->setText(e.what());
        return;
    }

    const WalletModel::UnlockContext unlock_context(m_wallet_model->requestUnlock());

    if (!unlock_context.isValid()) {
        pollNote_->setText(tr("Please unlock the wallet."));
        return;
    }

    try {
        SendPollContract(std::move(builder));
    } catch (const VotingError& e) {
        pollNote_->setText(e.what());
        return;
    }

    pollNote_->setStyleSheet("QLabel { color : green; }");
    pollNote_->setText("Success. The poll will activate with the next block.");
}

void NewPollDialog::addItem (void)
{
    QListWidgetItem *answerItem = new QListWidgetItem("New Item",answerList_);
    answerItem->setFlags (answerItem->flags() | Qt::ItemIsEditable);
}

void NewPollDialog::editItem (QListWidgetItem *item)
{
    answerList_->editItem(item);
}

void NewPollDialog::removeItem(void)
{
    QList<QListWidgetItem*> items = answerList_->selectedItems();
    foreach(QListWidgetItem * item, items)
    {
        delete answerList_->takeItem(answerList_->row(item));
    }

}

void NewPollDialog::showContextMenu(const QPoint &pos)
{
    QPoint globalPos = answerList_->viewport()->mapToGlobal(pos);

    QMenu menu;
    menu.addAction("Add Item", this, SLOT(addItem()));
    menu.addAction("Remove Item", this, SLOT(removeItem()));
    menu.exec(globalPos);
}
