#ifndef BITCOIN_QT_OVERVIEWPAGE_H
#define BITCOIN_QT_OVERVIEWPAGE_H

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
    explicit OverviewPage(QWidget* parent = nullptr);
    ~OverviewPage();

    void setResearcherModel(ResearcherModel *model);
    void setWalletModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setHeight(int height);
    void setDifficulty(double difficulty, double net_weight);
    void setCoinWeight(double coin_weight);
    void setCurrentPollTitle(const QString& title);

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

#endif // BITCOIN_QT_OVERVIEWPAGE_H
