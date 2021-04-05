#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace Ui {
    class OverviewPage;
}
class ResearcherModel;
class WalletModel;
class TxViewDelegate;
class TransactionFilterProxy;

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setResearcherModel(ResearcherModel *model);
    void setWalletModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);
	void updateGlobalStatus();
	void UpdateBoincUtilization();

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);

signals:
    void transactionClicked(const QModelIndex &index);
    void pollLabelClicked();

protected:
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);

private:
    int getNumTransactionsForView();

    Ui::OverviewPage *ui;
    ResearcherModel *researcherModel;
    WalletModel *walletModel;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;
    int scaledDecorationSize;

    TxViewDelegate *txdelegate;
    std::unique_ptr<TransactionFilterProxy> filter;

private slots:
    void updateDisplayUnit();
    void updateTransactions();
    void updateResearcherStatus();
    void updateMagnitude();
    void updatePendingAccrual();
    void updateResearcherAlert();
    void onBeaconButtonClicked();
    void handleTransactionClicked(const QModelIndex &index);
    void handlePollLabelClicked();
};

#endif // OVERVIEWPAGE_H
