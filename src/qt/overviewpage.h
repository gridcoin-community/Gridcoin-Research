#ifndef BITCOIN_QT_OVERVIEWPAGE_H
#define BITCOIN_QT_OVERVIEWPAGE_H

#include "uint256.h"

#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace Ui {
    class OverviewPage;
}
class ResearcherModel;
class MRCModel;
class WalletModel;
class TxViewDelegate;
class OverviewTxModel;

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget* parent = nullptr);
    ~OverviewPage();

    void setResearcherModel(ResearcherModel *model);
    void setMRCModel(MRCModel *model);
    void setWalletModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setHeight(int height, int height_of_peers, bool in_sync);
    void setDifficulty(double difficulty, double net_weight);
    void setCoinWeight(double coin_weight);
    void setCurrentPollTitle(const QString& title);
    void setPrivacy(bool privacy);
    void showHideMRCToolButton();

signals:
    //! Carries the clicked recent-transaction's id (not a model index): the
    //! detailed view re-resolves it to a row through its own windowed cursor
    //! (TransactionView::focusTransaction -> DetailedTxModel::indexForTxid), so
    //! no full-replica TransactionTableModel index is needed.
    void transactionClicked(const uint256& hash);
    void pollLabelClicked();

protected:
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);

private:
    int getNumTransactionsForView();

    Ui::OverviewPage *ui;
    ResearcherModel *researcherModel;
    MRCModel *m_mrc_model;
    WalletModel *walletModel;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;
    int scaledDecorationSize;
    bool m_privacy = false;

    TxViewDelegate *txdelegate;
    //! Windowed-model recent-transactions list (PR3): a VIEW_OVERVIEW cursor in
    //! the producer store backs this instead of a client-side QSortFilterProxy.
    std::unique_ptr<OverviewTxModel> m_overviewTxModel;

private slots:
    void updateDisplayUnit();
    void updateTransactions();
    void updateResearcherStatus();
    void updateMagnitude();
    void updatePendingAccrual();
    void updateResearcherAlert();
    void onBeaconButtonClicked();
    void onMRCRequestClicked();
    void handleTransactionClicked(const QModelIndex &index);
    void handlePollLabelClicked();
};

#endif // BITCOIN_QT_OVERVIEWPAGE_H
