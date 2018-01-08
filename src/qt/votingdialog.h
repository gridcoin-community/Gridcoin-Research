#ifndef VOTINGDIALOG_H
#define VOTINGDIALOG_H

#include <time.h>
#include <QAbstractTableModel>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QPoint>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDateTime>
#include <QFuture>

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  include <QtWidgets>
#else
#  include <QtGui>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
#include <QtCharts/QChartGlobal>
#endif

QT_BEGIN_NAMESPACE
class QEvent;
class QObject;
class QResizeEvent;
QT_END_NAMESPACE

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
QT_CHARTS_BEGIN_NAMESPACE
class QChart;
QT_CHARTS_END_NAMESPACE
#endif

#define VOTINGDIALOG_WIDTH_RowNumber          40
#define VOTINGDIALOG_WIDTH_Title              225
#define VOTINGDIALOG_WIDTH_Expiration         175
#define VOTINGDIALOG_WIDTH_ShareType          80
#define VOTINGDIALOG_WIDTH_TotalParticipants  80
#define VOTINGDIALOG_WIDTH_TotalShares        100
#define VOTINGDIALOG_WIDTH_BestAnswer         80

class VotingItem {
public:
    unsigned int rowNumber_;
    QString title_;
    QDateTime expiration_;
    QString shareType_;
    QString question_;
    QString answers_;
    QString arrayOfAnswers_;
    unsigned int totalParticipants_;
    unsigned int totalShares_;
    QString url_;
    QString bestAnswer_;
};

// VotingTableModel
//
class VotingTableModel
    : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit VotingTableModel();
    ~VotingTableModel();

    enum ColumnIndex {
        RowNumber = 0,
        Expiration = 1,
        Title = 2,
        BestAnswer = 3,
        TotalParticipants = 4,
        TotalShares = 5,
        ShareType = 6,
    };

    enum Roles {
        RowNumberRole = Qt::UserRole,
        ExpirationRole,
        TitleRole,
        BestAnswerRole,
        TotalParticipantsRole,
        TotalSharesRole,
        ShareTypeRole,

        SortRole,
    };

    int rowCount(const QModelIndex &) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    const VotingItem *index(int row) const;
    QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &) const;
    void resetData(bool history);

private:
    QStringList columns_;
    QList<VotingItem *> data_;
};


// VotingProxyModel
//
class VotingProxyModel
    : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit VotingProxyModel(QObject *parent=0);
    void setFilterTQAU(const QString &); // filter Title

protected:
    bool filterAcceptsRow(int, const QModelIndex &) const;

private:
    QString filterTQAU_;
};

class VotingChartDialog;
class VotingVoteDialog;
class NewPollDialog;

// VotingDialog
//
class VotingDialog
    : public QWidget
{
    Q_OBJECT

public:
    explicit VotingDialog(QWidget *parent=0);

private:
    QLineEdit *filterTQAU;
    QPushButton *resetButton;
    QPushButton *histButton;
    QPushButton *newPollButton;
    QTableView *tableView_;
    VotingTableModel *tableModel_;
    VotingProxyModel *proxyModel_;
    VotingChartDialog *chartDialog_;
    VotingVoteDialog *voteDialog_;
    NewPollDialog *pollDialog_;
    QLabel *loadingIndicator;
    QFutureWatcher<void> watcher;

private:
    virtual void showEvent(QShowEvent *);
    virtual void resizeEvent(QResizeEvent *);
    void tableColResize(void);
    bool eventFilter(QObject *, QEvent *);

private slots:
    void onLoadingFinished(void);

public slots:
    void filterTQAUChanged(const QString &);
    void loadPolls(bool history);
    void resetData(void);
    void loadHistory(void);
    void showChartDialog(void);
    void showContextMenu(const QPoint &);
    void showVoteDialog(void);
    void showNewPollDialog(void);
};

// VotingChartDialog
//
class VotingChartDialog
    : public QDialog
{
    Q_OBJECT

public:
    explicit VotingChartDialog(QWidget *parent=0);
    void resetData(const VotingItem *);

private:
    QLabel *question_;
    QLabel *url_;
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    QtCharts::QChart *chart_;
#endif
    QTableWidget *answerTable_;
    QStringList answerTableHeader;
    QLabel *answer_;
};

// VotingVoteDialog
//
class VotingVoteDialog
    : public QDialog
{
    Q_OBJECT

public:
    explicit VotingVoteDialog(QWidget *parent=0);
    void resetData(const VotingItem *);

private:
    QLabel *question_;
    QLabel *url_;
    QLabel *answer_;
    QLabel *voteNote_;
    QListWidget *answerList_;
    QListWidgetItem *answerItem;
    QPushButton *voteButton;
    QString GetVoteValue(void);
    QString sVoteTitle;
    
private slots:
    void vote(void);
};

// NewPollDialog
//
class NewPollDialog
    : public QDialog
{
    Q_OBJECT

public:
    explicit NewPollDialog(QWidget *parent=0);

public slots:
    void resetData(void);

private:
    QLineEdit *title_;
    QLineEdit *days_;
    QLineEdit *question_;
    QLineEdit *url_;
    QComboBox *shareTypeBox_;
    QLabel *pollNote_;
    QListWidget *answerList_;
    QListWidgetItem *answerItem;
    QPushButton *addItemButton;
    QPushButton *removeItemButton;
    QPushButton *clearAllButton;
    QPushButton *pollButton;
    void GetPollValues(void);
    QString sPollTitle;
    QString sPollDays;
    QString sPollQuestion;
    QString sPollUrl;
    QString sPollShareType;
    QString sPollAnswers;

private slots:
    void createPoll(void);
    void editItem (QListWidgetItem *item);
    void addItem (void);
    void removeItem(void);
    void showContextMenu(const QPoint &);
};

#endif // VOTINGDIALOG_H
