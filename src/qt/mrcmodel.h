// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_MRCMODEL_H
#define GRIDCOIN_QT_MRCMODEL_H

#include <QObject>
#include "amount.h"
#include "gridcoin/mrc.h"

class WalletModel;
class ClientModel;

enum class MRCRequestStatus
{
    NONE,
    CREATE_ERROR,
    ELIGIBLE,
    PENDING,
    QUEUE_FULL,
    ZERO_PAYOUT,
    EXCESSIVE_FEE,
    WALLET_LOCKED,
    SUBMIT_ERROR
};

class MRCModel : public QObject
{
    Q_OBJECT

public:
    explicit MRCModel(WalletModel* wallet_model, ClientModel* client_model, QObject* parent = nullptr);
    ~MRCModel();

    enum ModelStatus {
        VALID,
        INVALID_BLOCK_VERSION,
        OUT_OF_SYNC,
        NO_BLOCK_UPDATE_FROM_INIT
    };

    WalletModel* getWalletModel();

    void showMRCDialog();
    void setMRCFeeBoost(CAmount& fee);
    int getMRCFeeBoost();
    int getMRCQueueLength();
    int getMRCPos();
    CAmount getMRCQueueTailFee();
    CAmount getMRCQueuePayLimitFee();
    CAmount getMRCQueueHeadFee();
    CAmount getMRCMinimumSubmitFee();
    int getMRCOutputLimit();
    ModelStatus getMRCModelStatus();
    bool isMRCError(MRCRequestStatus& s, std::string& e);
    bool submitMRC(MRCRequestStatus& s, std::string& e);
    bool isWalletLocked();

private:
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    WalletModel* m_wallet_model;
    ClientModel* m_client_model;

    GRC::MRC m_mrc;
    MRCRequestStatus m_mrc_status;
    CAmount m_reward;
    CAmount m_mrc_min_fee;
    CAmount m_mrc_fee;
    CAmount m_mrc_fee_boost;
    int m_mrc_queue_length;
    int m_mrc_pos;
    CAmount m_mrc_queue_tail_fee;
    CAmount m_mrc_queue_pay_limit_fee;
    CAmount m_mrc_queue_head_fee;
    int m_mrc_output_limit;

    bool m_mrc_error;
    std::string m_mrc_error_desc;
    bool m_wallet_locked;

    int m_init_block_height;
    int m_block_height;

signals:
    void mrcChanged();
    void walletStatusChangedSignal();

public slots:
    void refresh();
    void walletStatusChanged(int encryption_status);
};

#endif // GRIDCOIN_QT_MRCMODEL_H
