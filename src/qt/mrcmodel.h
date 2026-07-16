// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_MRCMODEL_H
#define GRIDCOIN_QT_MRCMODEL_H

#include <QObject>
#include <memory>
#include <optional>
#include "amount.h"

class WalletModel;
class ClientModel;
class ResearcherModel;
class MRCRequestPage;

namespace interfaces {
class Handler;
class MRC;
} // namespace interfaces

//!
//! \brief The MRCRequestStatus enum describes the status of the MRC request
//!
enum class MRCRequestStatus
{
    NONE,
    ELIGIBLE,
    NOT_VALID_RESEARCHER,
    CREATE_ERROR,
    PENDING,
    QUEUE_FULL,
    PENDING_CANCEL,
    STALE_CANCEL,
    ZERO_PAYOUT,
    EXCESSIVE_FEE,
    WALLET_LOCKED,
    SUBMIT_ERROR,
    INSUFFICIENT_MATURE_FUNDS
};

//!
//! \brief The MRCModel class provides a GUI model of the state of the MRC queue and the researcher's MRC status
//!
//! Note that the MRC queue is "synthetic". It is filtered from the transactions in the memory pool and consists
//! of the transactions that have a valid MRC contract.
class MRCModel : public QObject
{
    Q_OBJECT

public:
    explicit MRCModel(interfaces::MRC& mrc,
                      WalletModel* wallet_model,
                      ClientModel* client_model,
                      ResearcherModel* researcher_model,
                      QObject* parent = nullptr);
    ~MRCModel();

    //!
    //! \brief The ModelStatus enum describes the overall status of the MRC model.
    //!
    //! The MRC request widgets are only shown in the MRCRequestPage if the model is VALID.
    //!
    enum ModelStatus {
        VALID,
        NOT_VALID_RESEARCHER,
        INVALID_BLOCK_VERSION,
        OUT_OF_SYNC,
        NO_BLOCK_UPDATE_FROM_INIT,
        INSUFFICIENT_MATURE_FUNDS
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
    CAmount getMRCReward();
    int getMRCOutputLimit();
    ModelStatus getMRCModelStatus();
    bool isMRCError(MRCRequestStatus& s, QString& e);
    bool submitMRC(MRCRequestStatus& s, QString& e);
    bool isWalletLocked();

private:
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    //! The node-side MRC boundary: one atomic snapshot() per refresh and a
    //! submit() command, both taken under core locks on the node side. Owned by
    //! bitcoin.cpp and outlives this model (Phase 1d-i).
    interfaces::MRC& m_mrc;

    //! Retained MRCChanged subscription, released on teardown so a signal that
    //! fires after this model is destroyed cannot invoke a slot bound to freed
    //! memory (issue #3129). The Handler disconnects on destruction.
    std::unique_ptr<interfaces::Handler> m_mrc_handler;

    WalletModel* m_wallet_model;
    ClientModel* m_client_model;
    ResearcherModel* m_researcher_model;
    MRCRequestPage* m_mrc_request;

    //! Research subsidy of the last submitted MRC (empty if none pending) —
    //! retained to detect a stale/unbound MRC without a further core read; the
    //! full contract lives node-side (Phase 1d-i).
    std::optional<CAmount> m_submitted_research_subsidy;
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
    QString m_mrc_error_desc;
    bool m_wallet_locked;

    //! Snapshot-cached version gate (from interfaces::MRCSnapshot), so
    //! getMRCModelStatus() classifies the block version without a core read. It
    //! is block-driven, so it cannot go stale without a refresh; out-of-sync is
    //! instead queried live (m_mrc.isOutOfSync()) because it is time-based.
    bool m_block_version_valid;

    int m_init_block_height;
    int m_block_height;
    int m_submitted_height;

signals:
    void mrcChanged();
    void walletStatusChangedSignal();

public slots:
    //!
    //! \brief refresh does the brunt of the work in the MRCModel.
    //!
    //! It runs trial CreateMRCs and loops through the memory pool to construct the synthetic MRC queue and
    //! provides the necessary information for the MRCRequestPage to control the MRC submission by the user.
    //!
    //! It provides structured status and error states of the MRC request(s).
    //!
    void refresh();
    void walletStatusChanged(int encryption_status);
};

#endif // GRIDCOIN_QT_MRCMODEL_H
