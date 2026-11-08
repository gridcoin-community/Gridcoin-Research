// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_MRCMODEL_H
#define GRIDCOIN_QT_MRCMODEL_H

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include "amount.h"
#include "interfaces/mrc.h"
#include "qt/notificationlifetime.h"

class WalletModel;
class ClientModel;
class ResearcherModel;
class MRCRequestPage;
class GuiStateCache;

namespace interfaces {
class GuiState;
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
//!
//! The model takes no core lock and makes no core read on the GUI thread. It
//! classifies from the MRC part of the state the core pushes (GuiStateCache),
//! asks the core for a quote when the fee boost is not zero, and submits through
//! interfaces::MRC::submitConfirmed on a worker thread.
class MRCModel : public QObject
{
    Q_OBJECT

public:
    explicit MRCModel(interfaces::MRC& mrc,
                      interfaces::GuiState& gui_state,
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
    MRCRequestStatus getMRCRequestStatus() const { return m_mrc_status; }

    //! The total fee the request pays: the boosted fee when a boost is set, else
    //! the minimum fee. Shows the fresh figure after a submit refused on changed
    //! terms.
    CAmount getMRCTotalFee() const;
    //! The reward less the total fee.
    CAmount getMRCNetPayout() const;

    //! A quote for the current fee boost is outstanding; the figures shown are
    //! not yet those of the boost.
    bool isQuoteUpdating() const { return m_quote_updating; }
    //! A submit is running on its worker thread.
    bool isSubmitting() const { return m_submitting; }
    //! The message of the last refused or failed submit (empty if none), kept
    //! until the next submit or fee boost edit.
    QString getSubmitNotice() const { return m_submit_notice; }

    //! Starts the submit of the figures shown (their fee and net payout are the
    //! terms the core holds the send to). Returns false, with \p e set, if the
    //! request cannot be submitted now; true once the submit runs. The outcome
    //! arrives as submitFinished().
    bool submitMRC(MRCRequestStatus& s, QString& e);
    bool isWalletLocked();

    //! Asks the core to push its state now (the page's update button).
    void requestUpdate();

private:
    void subscribeToQuotes();
    void unsubscribeFromQuotes();

    //! Takes the pushed state into the figures, and quotes again if a fee boost
    //! is set. Called when the cache changes and once at attach.
    void applyPushedState();
    //! applyPushedState's body. \p force takes the pushed figures although the
    //! MRC part is as the last push's, as when a fee boost is cleared.
    void takePushedState(bool force);
    //! Applies an answered quote when it is the latest request's.
    void applyQuote(uint64_t request_id, const interfaces::MRCSnapshot& quote);
    void issueQuote();
    void finishSubmit();
    void waitForSubmit();

    //! The node-side MRC boundary, used only for submitConfirmed on a worker
    //! thread. Owned by bitcoin.cpp and outlives this model.
    interfaces::MRC& m_mrc;
    interfaces::GuiState& m_gui_state;

    //! The pushed state (ClientModel::guiState()). Null only without a client
    //! model.
    GuiStateCache* m_cache;

    //! Retained quote subscription, severed on teardown (issue #3129).
    std::unique_ptr<interfaces::Handler> m_quote_handler;

    //! Closes the destruction race that disconnecting alone leaves open.
    //! See qt/notificationlifetime.h.
    NotificationLifetime m_notify_lifetime;

    WalletModel* m_wallet_model;
    ClientModel* m_client_model;
    ResearcherModel* m_researcher_model;
    MRCRequestPage* m_mrc_request;

    //! The figures being classified: the pushed ones, or the latest quote's.
    interfaces::MRCSnapshot m_snap;
    //! False while the core has pushed no MRC state (out of sync, or not yet).
    bool m_snap_present;

    //! Research subsidy of the last submitted MRC (empty if none pending) --
    //! retained to detect a stale/unbound MRC without a further core read.
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

    //! The version gate from the figures (block-driven).
    bool m_block_version_valid;

    int m_init_block_height;
    int m_block_height;
    int m_submitted_height;

    //! The id of the latest quote request; an answer for any other is ignored.
    //! Every fee boost edit takes a new one, so an answer for an earlier boost
    //! can never be applied, even before the debounced request is sent.
    uint64_t m_quote_id;
    bool m_quote_updating;
    QTimer m_quote_timer;

    bool m_submitting;
    QFutureWatcher<interfaces::MRCSubmitResult> m_submit_watcher;
    //! Fee and reward the core returned for a submit refused on changed terms.
    std::optional<std::pair<CAmount, CAmount>> m_fresh_terms;
    QString m_submit_notice;

    //! The MRC part of the last push that changed it, to tell an unchanged one.
    bool m_last_pushed_has_mrc = false;
    interfaces::MRCSnapshot m_last_pushed_mrc;

    //! The message of the latest quote, if it failed to compute.
    std::optional<QString> m_quote_failure;

signals:
    void mrcChanged();
    void walletStatusChangedSignal();
    //! A submit ended; \p success is true if the request was sent.
    void submitFinished(bool success);

public slots:
    //!
    //! \brief refresh does the brunt of the work in the MRCModel.
    //!
    //! It classifies the MRC figures the core pushed (or quoted) and provides the necessary information for the
    //! MRCRequestPage to control the MRC submission by the user. It calls no core method.
    //!
    //! It provides structured status and error states of the MRC request(s).
    //!
    void refresh();
    void walletStatusChanged(int encryption_status);
};

#endif // GRIDCOIN_QT_MRCMODEL_H
