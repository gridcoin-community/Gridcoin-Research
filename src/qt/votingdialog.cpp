#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
	#include <QtCharts/QChartView>
	#include <QtCharts/QPieSeries>
#endif

#include <QtConcurrentRun>
#include <QClipboard>
#include <QEvent>
#include <QFont>
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

#include "json/json_spirit.h"
#include "votingdialog.h"
#include "util.h"


extern json_spirit::Array GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool bIncludeExpired);
extern std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);
extern std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2);
extern std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5, std::string arg6);

static std::string GetFoundationGuid(const std::string &sTitle)
{
    const std::string foundation("[foundation ");
    size_t nPos1 = sTitle.find(foundation);
    if (nPos1 == std::string::npos)
        return std::string();
    nPos1 += foundation.size();
    size_t nPos2 = sTitle.find("]", nPos1);
    if (nPos2 == std::string::npos)
        return std::string();
    if (nPos2 != nPos1 + 36)
        return std::string();
    return sTitle.substr(nPos1, 36);
}

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

void VotingTableModel::resetData(bool history)
{
    // data: erase
    if (data_.size()) {
        beginRemoveRows(QModelIndex(), 0, data_.size() - 1);
        for(ssize_t i=0; i < data_.size(); i++)
            if (data_.at(i))
                delete data_.at(i);
        data_.clear();
        endRemoveRows();
    }

    // retrieve data
    std::vector<VotingItem *> items;
    std::string sVotingPayload;
    GetJSONPollsReport(true, "", sVotingPayload, history);

    //time_t now = time(NULL); // needed if history should be limited

    std::vector<std::string> vPolls = split(sVotingPayload, "<POLL>");
    for(size_t y=0; y < vPolls.size(); y++) {
        // replace underscores with spaces
        for(size_t nPos=0; (nPos=vPolls[y].find('_', nPos)) != std::string::npos; )
            vPolls[y][nPos] = ' ';

        std::string sTitle = ExtractXML(vPolls[y], "<TITLE>", "</TITLE>");
        std::string sId = GetFoundationGuid(sTitle);
        if (sTitle.size() && (sId.empty())) {
            QString sExpiration = QString::fromStdString(ExtractXML(vPolls[y], "<EXPIRATION>", "</EXPIRATION>"));
            std::string sShareType = ExtractXML(vPolls[y], "<SHARETYPE>", "</SHARETYPE>");
            std::string sQuestion = ExtractXML(vPolls[y], "<QUESTION>", "</QUESTION>");
            std::string sAnswers = ExtractXML(vPolls[y], "<ANSWERS>", "</ANSWERS>");
            std::string sArrayOfAnswers = ExtractXML(vPolls[y], "<ARRAYANSWERS>", "</ARRAYANSWERS>");
            std::string sTotalParticipants = ExtractXML(vPolls[y], "<TOTALPARTICIPANTS>", "</TOTALPARTICIPANTS>");
            std::string sTotalShares = ExtractXML(vPolls[y], "<TOTALSHARES>", "</TOTALSHARES>");
            std::string sUrl = ExtractXML(vPolls[y], "<URL>", "</URL>");
            std::string sBestAnswer = ExtractXML(vPolls[y], "<BESTANSWER>", "</BESTANSWER>");

            VotingItem *item = new VotingItem;
            item->rowNumber_ = items.size() + 1;
            item->title_ = QString::fromStdString(sTitle);
            item->expiration_ = QDateTime::fromString(sExpiration, "M-d-yyyy HH:mm:ss");
            item->shareType_ = QString::fromStdString(sShareType);
            item->question_ = QString::fromStdString(sQuestion);
            item->answers_ = QString::fromStdString(sAnswers);
            item->arrayOfAnswers_ = QString::fromStdString(sArrayOfAnswers);
            item->totalParticipants_ = std::stoul(sTotalParticipants);
            item->totalShares_ = std::stoul(sTotalShares);
            item->url_ = QString::fromStdString(sUrl);
            item->bestAnswer_ = QString::fromStdString(sBestAnswer);
            items.push_back(item);
        }
    }

    // data: populate
    if (items.size()) {
        beginInsertRows(QModelIndex(), 0, items.size() - 1);
        for(size_t i=0; i < items.size(); i++)
            data_.append(items[i]);
        endInsertRows();
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
    tableView_->setFont(QFont("Arial", 8));
    tableView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView_->horizontalHeader()->setMinimumWidth(VOTINGDIALOG_WIDTH_RowNumber + VOTINGDIALOG_WIDTH_Title + VOTINGDIALOG_WIDTH_Expiration + VOTINGDIALOG_WIDTH_ShareType + VOTINGDIALOG_WIDTH_TotalParticipants + VOTINGDIALOG_WIDTH_TotalShares + VOTINGDIALOG_WIDTH_BestAnswer);

    groupboxvlayout->addWidget(tableView_);

    // loading overlay. Due to a bug in QFutureWatcher for Qt <5.6.0 we
    // have to track the running state ourselves. See
    // https://bugreports.qt.io/browse/QTBUG-12358
    watcher.setProperty("running", false);
    connect(&watcher, SIGNAL(finished()), this, SLOT(onLoadingFinished()));
    loadingIndicator = new QLabel(this);
    loadingIndicator->move(50,170);

    chartDialog_ = new VotingChartDialog(this);
    voteDialog_ = new VotingVoteDialog(this);
    pollDialog_ = new NewPollDialog(this);
}

void VotingDialog::loadPolls(bool history)
{
    bool isRunning = watcher.property("running").toBool();
    if (tableModel_&& !isRunning)
    {
        loadingIndicator->setText(tr("...loading data!"));
        loadingIndicator->show();
        QFuture<void> future = QtConcurrent::run(tableModel_, &VotingTableModel::resetData, history);
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

void VotingDialog::onLoadingFinished(void)
{
    watcher.setProperty("running", false);

    int rowsCount = tableView_->verticalHeader()->count();
    if (rowsCount > 0) {
        loadingIndicator->hide();
    } else {
        loadingIndicator->setText(tr("No polls !"));
    }
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    ,chart_(0)
#endif
    ,answerTable_(NULL)
{
    setWindowTitle(tr("Poll Results"));
    resize(800, 320);

    QVBoxLayout *vlayout = new QVBoxLayout(this);

    question_ = new QLabel();
    question_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    question_->setText(tr("Q: "));
    question_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vlayout->addWidget(question_);

    url_ = new QLabel();
    url_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    url_->setText(tr("Discussion URL: "));
    url_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vlayout->addWidget(url_);

    QTabWidget *resTabWidget = new QTabWidget;

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    chart_ = new QtCharts::QChart;
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignRight);
    QtCharts::QChartView *m_chartView = new QtCharts::QChartView(chart_);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    resTabWidget->addTab(m_chartView, tr("Chart"));
#endif

    answerTable_ = new QTableWidget(this);
    answerTable_->setColumnCount(3);
    answerTable_->setRowCount(0);
    answerTableHeader<<"Answer"<<"Shares"<<"Percentage";
    answerTable_->setHorizontalHeaderLabels(answerTableHeader);
    answerTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    answerTable_->setEditTriggers( QAbstractItemView::NoEditTriggers );
    resTabWidget->addTab(answerTable_, tr("List"));
    vlayout->addWidget(resTabWidget);

    answer_ = new QLabel();
    answer_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    answer_->setText(tr("Best Answer: "));
    answer_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vlayout->addWidget(answer_);
}

void VotingChartDialog::resetData(const VotingItem *item)
{
    if (!item)
        return;

    answerTable_->setRowCount(0);
    answerTable_->setSortingEnabled(false);

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    //chart_->removeAllSeries();
    QList<QtCharts::QAbstractSeries *> oldSeriesList = chart_->series();
    foreach (QtCharts::QAbstractSeries *oldSeries, oldSeriesList) {
        chart_->removeSeries(oldSeries);
    }

    QtCharts::QPieSeries *series = new QtCharts::QPieSeries();
#endif

    question_->setText(QString(tr("Q: ")) + item->question_);
    url_->setText(QString(tr("Discussion URL: ")) + item->url_);
    answer_->setText(QString(tr("Best Answer: ")) + item->bestAnswer_);

    std::string arrayOfAnswers = item->arrayOfAnswers_.toUtf8().constData();
    std::vector<std::string> vAnswers = split(arrayOfAnswers, "<RESERVED>"); // the first entry is empty
    answerTable_->setRowCount(vAnswers.size()-1);
    std::vector<int> iShares;
    std::vector<QString> sAnswerNames;
    int sharesSum = 0;
    for(size_t y=1; y < vAnswers.size(); y++) {
        sAnswerNames.push_back(QString::fromStdString(ExtractXML(vAnswers[y], "<ANSWERNAME>", "</ANSWERNAME>")));
        int iShare = atoi(ExtractXML(vAnswers[y], "<SHARES>", "</SHARES>").c_str());
        iShares.push_back(iShare);
        sharesSum += iShare;
    }
    for(size_t y=0; y < sAnswerNames.size(); y++) {
        answerTable_->setItem(y, 0, new QTableWidgetItem(sAnswerNames[y]));
        QTableWidgetItem *iSharesItem = new QTableWidgetItem();
        iSharesItem->setData(Qt::DisplayRole,iShares[y]);
        answerTable_->setItem(y, 1, iSharesItem);
        QTableWidgetItem *percentItem = new QTableWidgetItem();
        percentItem->setData(Qt::DisplayRole,(float)iShares[y]/(float)sharesSum*100);
        answerTable_->setItem(y, 2, percentItem);

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
        QtCharts::QPieSlice *slice = new QtCharts::QPieSlice(sAnswerNames[y], iShares[y]);
        unsigned int num = rand();
        int r = (num >>  0) % 0xFF;
        int g = (num >>  8) % 0xFF;
        int b = (num >> 16) % 0xFF;
        slice->setColor(QColor(r, g, b));
        series->append(slice);
#endif
    }
    answerTable_->setSortingEnabled(true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    chart_->addSeries(series);
#endif
}

// VotingVoteDialog
//
VotingVoteDialog::VotingVoteDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("PlaceVote"));
    resize(800, 320);

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
    url_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(url_, 1, 1);

    QLabel *bestAnswer = new QLabel(tr("Best Answer: "));
    bestAnswer->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    bestAnswer->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(bestAnswer, 3, 0);

    answer_ = new QLabel();
    answer_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    answer_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(answer_, 3, 1);
    
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

void VotingVoteDialog::resetData(const VotingItem *item)
{
    if (!item)
        return;
    
    answerList_->clear();
    voteNote_->clear();
    question_->setText(item->question_);
    url_->setText(item->url_);
    answer_->setText(item->bestAnswer_);
    sVoteTitle=item->title_;
    std::string listOfAnswers = item->answers_.toUtf8().constData();
    std::vector<std::string> vAnswers = split(listOfAnswers, ";");
    for(size_t y=0; y < vAnswers.size(); y++) {
        QListWidgetItem *answerItem = new QListWidgetItem(QString::fromStdString(vAnswers[y]),answerList_);
        answerItem->setCheckState(Qt::Unchecked);
    }
}

void VotingVoteDialog::vote(void)
{
    QString sVoteValue = GetVoteValue();
    voteNote_->setStyleSheet("QLabel { color : red; }");
    
    if(sVoteValue.isEmpty()){
        voteNote_->setText(tr("Vote failed! Select one or more items to vote."));
        return;
    }
    
    // replace spaces with underscores
    sVoteValue.replace(" ","_");
    sVoteTitle.replace(" ","_");
    
    const std::string &sResult = ExecuteRPCCommand("vote", sVoteTitle.toStdString(), sVoteValue.toStdString());
    
    if (sResult.find("Success") != std::string::npos) {
        voteNote_->setStyleSheet("QLabel { color : green; }");
    }
    voteNote_->setText(QString::fromStdString(sResult)); 
}

QString VotingVoteDialog::GetVoteValue(void)
{
    QString sVote = "";
    for(int row = 0; row < answerList_->count(); row++) {
        QListWidgetItem *item = answerList_->item(row);
        if(item->checkState() == Qt::Checked)
            sVote += item->text() + ";";
    }
    sVote.chop(1);
    return sVote;
}

// NewPollDialog
//
NewPollDialog::NewPollDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Create Poll"));
    resize(800, 320);

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
    shareTypeBoxItems << "Magnitude" << "Balance" << "Both" << "CPIDCount" << "ParticipantCount";
    shareTypeBox_->addItems(shareTypeBoxItems);
    shareTypeBox_->setCurrentIndex(2);
    glayout->addWidget(shareTypeBox_, 4, 1);
    
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
    GetPollValues();
    pollNote_->setStyleSheet("QLabel { color : red; }");

    if(sPollTitle.isEmpty()){
        pollNote_->setText(tr("Creating poll failed! Title is missing."));
        return;
    }
    if(sPollDays.isEmpty()){
        pollNote_->setText(tr("Creating poll failed! Days value is missing."));
        return;
    }
    if(sPollDays.toInt() > 180){
        pollNote_->setText(tr("Creating poll failed! Polls can not last longer than 180 days."));
        return;
    }
    if(sPollQuestion.isEmpty()){
        pollNote_->setText(tr("Creating poll failed! Question is missing."));
        return;
    }
    if(sPollUrl.isEmpty()){
        pollNote_->setText(tr("Creating poll failed! URL is missing."));
        return;
    }
    if(sPollAnswers.isEmpty()){
        pollNote_->setText(tr("Creating poll failed! Answer is missing."));
        return;
    }

    // replace spaces with underscores
    sPollTitle.replace(" ","_");
    sPollDays.replace(" ","_");
    sPollQuestion.replace(" ","_");
    sPollUrl.replace(" ","_");
    sPollAnswers.replace(" ","_");

    const std::string &sResult = ExecuteRPCCommand("addpoll", sPollTitle.toStdString(), sPollDays.toStdString(),sPollQuestion.toStdString(),sPollAnswers.toStdString(),sPollShareType.toStdString(),sPollUrl.toStdString());

    if (sResult.find("Success") != std::string::npos) {
        pollNote_->setStyleSheet("QLabel { color : green; }");
    }
    pollNote_->setText(QString::fromStdString(sResult));
}
 
void NewPollDialog::GetPollValues(void)
{

    sPollTitle = title_->text();
    sPollDays = days_->text();
    sPollQuestion = question_->text();
    sPollUrl = url_->text();
    sPollShareType = QString::number(shareTypeBox_->currentIndex() + 1);

    sPollAnswers = "";
    for(int row = 0; row < answerList_->count(); row++) {
        QListWidgetItem *item = answerList_->item(row);
        sPollAnswers += item->text() + ";";
    }
    sPollAnswers.chop(1);
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



