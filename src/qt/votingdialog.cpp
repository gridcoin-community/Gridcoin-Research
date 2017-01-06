#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	#include <QtCharts/QChartView>
	#include <QPieSeries>
#endif

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

using namespace QtCharts;

extern json_spirit::Array GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool bIncludeExpired);
extern std::vector<std::string> split(std::string s, std::string delim);
extern std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);

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
    Qt::AlignLeft|Qt::AlignVCenter, // Title
    Qt::AlignLeft|Qt::AlignVCenter, // Expiration
    Qt::AlignLeft|Qt::AlignVCenter, // ShareType
    Qt::AlignLeft|Qt::AlignVCenter, // Question
    Qt::AlignLeft|Qt::AlignVCenter, // Answers
    Qt::AlignRight|Qt::AlignVCenter, // TotalParticipants
    Qt::AlignRight|Qt::AlignVCenter, // TotalShares
    Qt::AlignLeft|Qt::AlignVCenter, // Url
    Qt::AlignLeft|Qt::AlignVCenter, // BestAnswer
    };

// VotingTableModel
//
VotingTableModel::VotingTableModel(void)
{
    columns_
        << tr("#")
        << tr("Title")
        << tr("Expiration")
        << tr("Share Type")
        << tr("Question")
        << tr("Answers")
        << tr("Total Participants")
        << tr("Total Shares")
        << tr("URL")
        << tr("Best Answer")
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
        case Expiration: {
            struct tm tm;
            localtime_r(&item->expiration_, &tm);
            char str[32];
            asctime_r(&tm, str);
            for(char *s=str; *s; s++)
                if (*s == '\n')
                    { *s = '\0'; break; }
            return QVariant(QString(str));
        }
        case ShareType:
            return item->shareType_;
        case Question:
            return item->question_;
        case Answers:
            return item->answers_;
        case TotalParticipants:
            return item->totalParticipants_;
        case TotalShares:
            return item->totalShares_;
        case Url:
            return item->url_;
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
            return QVariant((unsigned int)item->expiration_);
        case ShareType:
            return item->shareType_;
        case Question:
            return item->question_;
        case Answers:
            return item->answers_;
        case TotalParticipants:
            return item->totalParticipants_;
        case TotalShares:
            return item->totalShares_;
        case Url:
            return item->url_;
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
        return (unsigned int) item->expiration_;

    case ShareTypeRole:
        return item->shareType_;

    case QuestionRole:
        return item->question_;

    case AnswersRole:
        return item->answers_;

    case TotalParticipantsRole:
        return item->totalParticipants_;

    case TotalSharesRole:
        return item->totalShares_;

    case UrlRole:
        return item->url_;

    case BestAnswerRole:
        return item->bestAnswer_;

    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];

    default:
        ;
    }

    return QVariant();
}

QVariant VotingTableModel::headerData(int column, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
            return columns_[column];
        else
        if (role == Qt::TextAlignmentRole)
            return column_alignments[column];
        else
        if (role == Qt::ToolTipRole)
        {
            switch (column)
            {
            case RowNumber:
                return tr("Row Number.");
            case Title:
                return tr("Title.");
            case Expiration:
                return tr("Expiration.");
            case ShareType:
                return tr("Share Type.");
            case Question:
                return tr("Question.");
            case Answers:
                return tr("Answers");
            case TotalParticipants:
                return tr("Total Participants.");
            case TotalShares:
                return tr("Total Shares.");
            case Url:
                return tr("URL.");
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

void VotingTableModel::resetData(void)
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
    GetJSONPollsReport(true, "", sVotingPayload, true);

    time_t now = time(NULL);

    std::vector<std::string> vPolls = split(sVotingPayload, "<POLL>");
    for(size_t y=0; y < vPolls.size(); y++) {
        // replace underscores with spaces
        for(size_t nPos=0; (nPos=vPolls[y].find('_', nPos)) != std::string::npos; )
            vPolls[y][nPos] = ' ';

        std::string sTitle = ExtractXML(vPolls[y], "<TITLE>", "</TITLE>");
        std::string sId = GetFoundationGuid(sTitle);
        if (sTitle.size() && (sId.empty())) {
            std::string sExpiration = ExtractXML(vPolls[y], "<EXPIRATION>", "</EXPIRATION>");

            struct tm tm;
            memset(&tm, 0, sizeof(struct tm));
            strptime(sExpiration.c_str(), "%m-%d-%Y %H:%M:%S", &tm);
            time_t expiration = mktime(&tm);
            if (now > expiration + 7*24*60*60) // do not show items expired more than 7 days
                continue;

            std::string sShareType = ExtractXML(vPolls[y], "<SHARETYPE>", "</SHARETYPE>");
            std::string sQuestion = ExtractXML(vPolls[y], "<QUESTION>", "</QUESTION>");
            std::string sAnswers = ExtractXML(vPolls[y], "<ANSWERS>", "</ANSWERS>");
            std::string sArrayOfAnswers = ExtractXML(vPolls[y], "<ARRAYANSWERS>", "</ARRAYANSWERS>");
            std::string sTotalParticipants = ExtractXML(vPolls[y], "<TOTALPARTICIPANTS>", "</TOTALPARTICIPANTS>");
            std::string sTotalShares = ExtractXML(vPolls[y], "<TOTALSHARES>", "</TOTALSHARES>");
            std::string sUrl = ExtractXML(vPolls[y], "<URL>", "</URL>");
            std::string sBestAnswer = ExtractXML(vPolls[y], "<BESTANSWER>", "</BESTANSWER>");

            // Dim lDateDiff As Long = DateDiff(DateInterval.Day, Now, GlobalCDate(sExpiration))
            // If Len(sTitle) > 0 And lDateDiff > -7 Then
            // If lDateDiff < 0 Then dgv.Rows(iRow).Cells(2).Style.BackColor = Drawing.Color.Red
            // If Len(sAnswers) > 81 Then sAnswers = Mid(sAnswers, 1, 81) + "..."

            VotingItem *item = new VotingItem;
            item->rowNumber_ = items.size() + 1;
            item->title_ = QString::fromStdString(sTitle);
            item->expiration_ = expiration;
            item->shareType_ = QString::fromStdString(sShareType);
            item->question_ = QString::fromStdString(sQuestion);
            item->answers_ = QString::fromStdString(sAnswers);
            item->arrayOfAnswers_ = QString::fromStdString(sArrayOfAnswers);
            item->totalParticipants_ = QString::fromStdString(sTotalParticipants);
            item->totalShares_ = QString::fromStdString(sTotalShares);
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

void VotingProxyModel::setFilterTitle(const QString &str)
{
    filterTitle_ = (str.size() >= 2)? str: "";
    invalidateFilter();
}

void VotingProxyModel::setFilterQuestion(const QString &str)
{
    filterQuestion_ = (str.size() >= 2)? str: "";
    invalidateFilter();
}

void VotingProxyModel::setFilterAnswers(const QString &str)
{
    filterAnswers_ = (str.size() >= 2)? str: "";
    invalidateFilter();
}

void VotingProxyModel::setFilterUrl(const QString &str)
{
    filterUrl_ = (str.size() >= 2)? str: "";
    invalidateFilter();
}

bool VotingProxyModel::filterAcceptsRow(int row, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(row, 0, sourceParent);

    QString title = index.data(VotingTableModel::TitleRole).toString();
    if (!title.contains(filterTitle_, Qt::CaseInsensitive))
        return false;

    QString question = index.data(VotingTableModel::QuestionRole).toString();
    if (!title.contains(filterQuestion_, Qt::CaseInsensitive))
        return false;

    QString answers = index.data(VotingTableModel::AnswersRole).toString();
    if (!answers.contains(filterAnswers_, Qt::CaseInsensitive))
        return false;

    QString url = index.data(VotingTableModel::UrlRole).toString();
    if (!url.contains(filterUrl_, Qt::CaseInsensitive))
        return false;

    return true;
}

// VotingDialog
//
VotingDialog::VotingDialog(QWidget *parent)
    : QDialog(parent),
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

    // view
    setWindowTitle(tr("Gridcoin Voting System 1.1"));
    resize(1100, 320);

    QVBoxLayout *vlayout = new QVBoxLayout(this);

    QGroupBox *groupBox = new QGroupBox(tr("Active Polls (Right Click to Vote)"));
    vlayout->addWidget(groupBox);

    QVBoxLayout *groupboxvlayout = new QVBoxLayout();
    groupBox->setLayout(groupboxvlayout);

    QGridLayout *glayout = new QGridLayout();
    glayout->setHorizontalSpacing(0);
    glayout->setVerticalSpacing(0);
    glayout->setColumnStretch(0, 1);
    glayout->setColumnStretch(1, 3);
    glayout->setColumnStretch(2, 5);

    groupboxvlayout->addLayout(glayout);

    QLabel *filterByTitleLabel = new QLabel(tr("Filter By Title: "));
    filterByTitleLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    filterByTitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(filterByTitleLabel, 0, 0);
    filterTitle = new QLineEdit();
    glayout->addWidget(filterTitle, 0, 1);
    glayout->addWidget(new QLabel(""), 0, 2); // for stretch
    connect(filterTitle, SIGNAL(textChanged(QString)), this, SLOT(filterTitleChanged(QString)));

    QLabel *filterByQuestionLabel = new QLabel(tr("Filter By Question: "));
    filterByQuestionLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    filterByQuestionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(filterByQuestionLabel, 1, 0);
    filterQuestion = new QLineEdit();
    glayout->addWidget(filterQuestion, 1, 1);
    connect(filterQuestion, SIGNAL(textChanged(QString)), this, SLOT(filterQuestionChanged(QString)));

    QLabel *filterByAnswersLabel = new QLabel(tr("Filter By Answers: "));
    filterByAnswersLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    filterByAnswersLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(filterByAnswersLabel, 2, 0);
    filterAnswers = new QLineEdit();
    glayout->addWidget(filterAnswers, 2, 1);
    connect(filterAnswers, SIGNAL(textChanged(QString)), this, SLOT(filterAnswersChanged(QString)));

    QLabel *filterByUrlLabel = new QLabel(tr("Filter By Url: "));
    filterByUrlLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    filterByUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    glayout->addWidget(filterByUrlLabel, 3, 0);
    filterUrl = new QLineEdit();
    glayout->addWidget(filterUrl, 3, 1);
    connect(filterUrl, SIGNAL(textChanged(QString)), this, SLOT(filterUrlChanged(QString)));

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

    tableView_->setColumnWidth(VotingTableModel::RowNumber, VOTINGDIALOG_WIDTH_RowNumber);
    tableView_->setColumnWidth(VotingTableModel::Title, VOTINGDIALOG_WIDTH_Title);
    tableView_->setColumnWidth(VotingTableModel::Expiration, VOTINGDIALOG_WIDTH_Expiration);
    tableView_->setColumnWidth(VotingTableModel::ShareType, VOTINGDIALOG_WIDTH_ShareType);
    tableView_->setColumnWidth(VotingTableModel::Question, VOTINGDIALOG_WIDTH_Question);
    tableView_->setColumnWidth(VotingTableModel::Answers, VOTINGDIALOG_WIDTH_Answers);
    tableView_->setColumnWidth(VotingTableModel::TotalParticipants, VOTINGDIALOG_WIDTH_TotalParticipants);
    tableView_->setColumnWidth(VotingTableModel::TotalShares, VOTINGDIALOG_WIDTH_TotalShares);
    tableView_->setColumnWidth(VotingTableModel::Url, VOTINGDIALOG_WIDTH_Url);
    tableView_->setColumnWidth(VotingTableModel::BestAnswer, VOTINGDIALOG_WIDTH_BestAnswer);
    tableView_->setModel(proxyModel_);
    tableView_->setFont(QFont("Arial", 8));

    groupboxvlayout->addWidget(tableView_);

    chartDialog_ = new VotingChartDialog(this);
    voteDialog_ = new VotingVoteDialog(this);
}

void VotingDialog::resetData(void)
{
    if (tableModel_)
        tableModel_->resetData();
}

void VotingDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    tableView_->setColumnWidth(VotingTableModel::RowNumber, VOTINGDIALOG_WIDTH_RowNumber);
    tableView_->setColumnWidth(VotingTableModel::Title, VOTINGDIALOG_WIDTH_Title);
    tableView_->setColumnWidth(VotingTableModel::Expiration, VOTINGDIALOG_WIDTH_Expiration);
    tableView_->setColumnWidth(VotingTableModel::ShareType, VOTINGDIALOG_WIDTH_ShareType);
    tableView_->setColumnWidth(VotingTableModel::Question, VOTINGDIALOG_WIDTH_Question);
    tableView_->setColumnWidth(VotingTableModel::Answers, VOTINGDIALOG_WIDTH_Answers);
    tableView_->setColumnWidth(VotingTableModel::TotalParticipants, VOTINGDIALOG_WIDTH_TotalParticipants);
    tableView_->setColumnWidth(VotingTableModel::TotalShares, VOTINGDIALOG_WIDTH_TotalShares);
    tableView_->setColumnWidth(VotingTableModel::Url, VOTINGDIALOG_WIDTH_Url);
    tableView_->setColumnWidth(VotingTableModel::BestAnswer, VOTINGDIALOG_WIDTH_BestAnswer);
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

void VotingDialog::filterTitleChanged(const QString &str)
{
    if (proxyModel_)
        proxyModel_->setFilterTitle(str);
}

void VotingDialog::filterQuestionChanged(const QString &str)
{
    if (proxyModel_)
        proxyModel_->setFilterQuestion(str);
}

void VotingDialog::filterAnswersChanged(const QString &str)
{
    if (proxyModel_)
        proxyModel_->setFilterAnswers(str);
}

void VotingDialog::filterUrlChanged(const QString &str)
{
    if (proxyModel_)
        proxyModel_->setFilterUrl(str);
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
    menu.addAction("Chart", this, SLOT(showChartDialog()));
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
    int row = indexes.at(0).row();
    const VotingItem *item = tableModel_->index(row);

    // reset the dialog's data
    voteDialog_->resetData(item);

    voteDialog_->show();
    voteDialog_->raise();
    voteDialog_->setFocus();
}

// VotingChartDialog
//
VotingChartDialog::VotingChartDialog(QWidget *parent)
    : QDialog(parent),
    chart_(0)
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

    chart_ = new QChart;
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignRight);
    QChartView *m_chartView = new QChartView(chart_);
    vlayout->addWidget(m_chartView);

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

    chart_->removeAllSeries();

    question_->setText(QString(tr("Q: ")) + item->question_);
    url_->setText(QString(tr("Discussion URL: ")) + item->url_);
    answer_->setText(QString(tr("Best Answer: ")) + item->bestAnswer_);

    QPieSeries *series = new QPieSeries();
    std::string arrayOfAnswers = item->arrayOfAnswers_.toUtf8().constData();
    std::vector<std::string> vAnswers = split(arrayOfAnswers, "<RESERVED>");
    for(size_t y=0; y < vAnswers.size(); y++) {
        std::string sAnswerName = ExtractXML(vAnswers[y], "<ANSWERNAME>", "</ANSWERNAME>");
        std::string sShares = ExtractXML(vAnswers[y], "<SHARES>", "</SHARES>");
        QPieSlice *slice = new QPieSlice(QString::fromStdString(sAnswerName), atoi(sShares.c_str()));
        unsigned int num = rand();
        int r = (num >>  0) % 0xFF;
        int g = (num >>  8) % 0xFF;
        int b = (num >> 16) % 0xFF;
        slice->setColor(QColor(r, g, b));
        series->append(slice);
    }
    chart_->addSeries(series);
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
}

void VotingVoteDialog::resetData(const VotingItem *item)
{
    if (!item)
        return;

    question_->setText(QString(tr("Q: ")) + item->question_);
    url_->setText(QString(tr("Discussion URL: ")) + item->url_);
    answer_->setText(QString(tr("Best Answer: ")) + item->bestAnswer_);
}

