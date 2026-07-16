// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/handler.h"
#include "interfaces/mrc.h"
#include "logging.h"
#include "mrcmodel.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "researcher/researchermodel.h"
#include "qt/mrcrequestpage.h"

#include <limits>

MRCModel::MRCModel(interfaces::MRC& mrc, WalletModel* wallet_model, ClientModel *client_model,
                   ResearcherModel *researcher_model, QObject *parent)
    : QObject(parent)
    , m_mrc(mrc)
    , m_wallet_model(wallet_model)
    , m_client_model(client_model)
    , m_researcher_model(researcher_model)
    , m_mrc_request(nullptr)
    , m_submitted_research_subsidy({})
    , m_mrc_status(MRCRequestStatus::NONE)
    , m_reward(0)
    , m_mrc_min_fee(0)
    , m_mrc_fee(0)
    , m_mrc_fee_boost(0)
    , m_mrc_queue_length(0)
    , m_mrc_pos(0)
    , m_mrc_queue_tail_fee(std::numeric_limits<CAmount>::max())
    , m_mrc_queue_pay_limit_fee(std::numeric_limits<CAmount>::max())
    , m_mrc_queue_head_fee(0)
    , m_mrc_output_limit(0)
    , m_mrc_error(false)
    , m_mrc_error_desc(QString{})
    , m_wallet_locked(false)
    , m_block_version_valid(false)
    , m_init_block_height(0)
    , m_block_height(0)
    , m_submitted_height(0)
{
    subscribeToCoreSignals();

    // Here instead of the core signal for number of blocks changed, we take it from the client model, because
    // there is a rate limiter there if the wallet is not in sync. This is necessary, because a new block can be
    // received with no MRC contract, and therefore an m_mrc already generated here will then be stale. So
    // to force a generation of an updated m_mrc against the new block, we need to connect the mrcChanged slot
    // to this as well as the core MRCChanged signal.
    connect(m_client_model, &ClientModel::numBlocksChanged, this, &MRCModel::mrcChanged);

    // Detects whether the wallet has been locked or unlocked.
    connect(m_wallet_model, &WalletModel::encryptionStatusChanged, this, &MRCModel::walletStatusChanged);

    WalletModel::EncryptionStatus encryption_status = m_wallet_model->getEncryptionStatus();

    walletStatusChanged(encryption_status);

    refresh();
}

MRCModel::~MRCModel()
{
    if (m_mrc_request) {
        m_mrc_request->done(QDialog::Accepted);
    }

    unsubscribeFromCoreSignals();
}

WalletModel* MRCModel::getWalletModel()
{
    return m_wallet_model;
}

void MRCModel::showMRCDialog()
{
    if (!m_mrc_request) {
        m_mrc_request = new MRCRequestPage(nullptr, this);
    }

    m_mrc_request->show();
}

void MRCModel::setMRCFeeBoost(CAmount& fee_boost)
{
    m_mrc_fee_boost = fee_boost;

    refresh();
}

int MRCModel::getMRCFeeBoost()
{
    return m_mrc_fee_boost;
}

int MRCModel::getMRCQueueLength()
{
    return m_mrc_queue_length;
}

int MRCModel::getMRCPos()
{
    return m_mrc_pos;
}

CAmount MRCModel::getMRCQueueTailFee()
{
    return m_mrc_queue_tail_fee;
}

CAmount MRCModel::getMRCQueuePayLimitFee()
{
    return m_mrc_queue_pay_limit_fee;
}

CAmount MRCModel::getMRCQueueHeadFee()
{
    return m_mrc_queue_head_fee;
}

CAmount MRCModel::getMRCMinimumSubmitFee()
{
    return m_mrc_min_fee;
}

CAmount MRCModel::getMRCReward()
{
    return m_reward;
}

int MRCModel::getMRCOutputLimit()
{
    return m_mrc_output_limit;
}

MRCModel::ModelStatus MRCModel::getMRCModelStatus()
{
    if (m_mrc_status == MRCRequestStatus::NOT_VALID_RESEARCHER) {
        return MRCModel::ModelStatus::NOT_VALID_RESEARCHER;
    } else if (m_mrc_status == MRCRequestStatus::INSUFFICIENT_MATURE_FUNDS) {
        return MRCModel::ModelStatus::INSUFFICIENT_MATURE_FUNDS;
    } else if (!m_block_version_valid) {
        return MRCModel::ModelStatus::INVALID_BLOCK_VERSION;
    } else if (m_mrc.isOutOfSync()) {
        // Live check (not the cached snapshot value): OutOfSyncByAge() is
        // time-based and can flip true with no new block/MRCChanged refresh.
        // Note that m_mrc_status == MRCRequestStatus::NONE if out of sync.
        return MRCModel::ModelStatus::OUT_OF_SYNC;
    } else if (m_block_height <= m_init_block_height) {
        return MRCModel::ModelStatus::NO_BLOCK_UPDATE_FROM_INIT;
    }

    return MRCModel::ModelStatus::VALID;
}

bool MRCModel::isMRCError(MRCRequestStatus &s, QString& e)
{
    if (m_mrc_error) {
        e = m_mrc_error_desc;
        s = m_mrc_status;
        return true;
    }

    return false;
}

bool MRCModel::submitMRC(MRCRequestStatus& s, QString& e)
{
    if (m_mrc_status != MRCRequestStatus::ELIGIBLE) {
        return error("%s: submitMRC called while m_mrc_status, %i, is not ELIGIBLE.",
                     __func__,
                     static_cast<int>(m_mrc_status));
    }

    // The node recreates the claim fresh at the current tip and broadcasts it
    // under cs_main + cs_wallet, so the submission is atomic. The same fee boost
    // the user acted on is applied there.
    const interfaces::MRCSubmitResult result = m_mrc.submit(m_mrc_fee_boost, m_wallet_locked);

    if (!result.success) {
        m_mrc_error = true;
        s = m_mrc_status = MRCRequestStatus::SUBMIT_ERROR;
        e = m_mrc_error_desc = QString::fromStdString(result.error);
        return false;
    } else {
        m_submitted_research_subsidy = result.research_subsidy;
        m_submitted_height = result.submitted_height;
        m_mrc_error = false;
        m_mrc_status = MRCRequestStatus::PENDING;
        m_mrc_error_desc = QString{};
        s = m_mrc_status;
    }

    return true;

    // note this will call refresh() indirectly through the signalling from the core.
}

bool MRCModel::isWalletLocked()
{
    return m_wallet_locked;
}

void MRCModel::subscribeToCoreSignals()
{
    // Retain the handler so it is severed in unsubscribeFromCoreSignals() (from
    // ~MRCModel); the callback captures `this` and a signal firing after this
    // object is gone would otherwise invoke a slot on freed memory. The
    // notification is marshaled to the GUI thread, so the lambda touches no core
    // state (issue #3129).
    m_mrc_handler = m_mrc.handleMRCChanged([this]() {
        LogPrint(BCLog::LogFlags::QT, "GUI: received MRCChanged() core signal");

        QMetaObject::invokeMethod(this, "mrcChanged", Qt::QueuedConnection);
    });
}

void MRCModel::unsubscribeFromCoreSignals()
{
    // Disconnect from the node: resetting the handler runs its destructor, which
    // disconnects the subscription (issue #3129).
    m_mrc_handler.reset();
}

void MRCModel::refresh()
{
    m_mrc_error = false;
    m_mrc_status = MRCRequestStatus::NONE;
    m_mrc_error_desc = QString{};

    // Researcher eligibility is owned GUI-side (ResearcherModel; migrated in
    // 1d-iv). The node-side snapshot skips its trial CreateMRC when this is
    // false, so a non-researcher does not raise MRC_error on every block.
    const bool researcher_eligible = m_researcher_model
            && m_researcher_model->hasActiveBeacon()
            && !m_researcher_model->configuredForNoncruncherMode()
            && !m_researcher_model->detectedPoolMode();

    // One atomic node-side read of the chain tip, version gate, trial
    // CreateMRC, and mempool MRC queue under a single cs_main hold. This
    // replaces the former GUI-thread core reads that were annotated
    // EXCLUSIVE_LOCKS_REQUIRED(cs_main) but ran holding nothing.
    const interfaces::MRCSnapshot snap = m_mrc.snapshot(m_mrc_fee_boost, m_wallet_locked, researcher_eligible);

    // Cache the version gate for getMRCModelStatus() (block-driven, so it cannot
    // go stale without a refresh trigger). block_version_valid reflects the
    // current tip even while out of sync, matching the former
    // IsV12Enabled(m_block_height) ordering. Out-of-sync is queried live there
    // (m_mrc.isOutOfSync()), since it is time-based.
    m_block_version_valid = snap.block_version_valid;

    // Stop here if out of sync.
    if (snap.out_of_sync) {
        return;
    }

    if (!m_researcher_model) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::NOT_VALID_RESEARCHER;
        return;
    }

    if (!researcher_eligible) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::NOT_VALID_RESEARCHER;
        return;
    }

    // This is similar to createmrcrequest in many ways, but the state tracking is more complicated.

    // Record initial block height during init run.
    if (!m_init_block_height) {
        m_init_block_height = snap.block_height;
    }

    // Store this locally so we don't have to get this from the client model, which takes another lock on cs_main.
    m_block_height = snap.block_height;

    // Do a balance check before anything else.
    if (m_wallet_model) {
        CAmount mature_balance = m_wallet_model->getBalance();

        if (mature_balance < COIN) {
            m_mrc_error |= true;
            m_mrc_status = MRCRequestStatus::INSUFFICIENT_MATURE_FUNDS;
            m_mrc_error_desc = tr("You must have a mature balance of at least 1 GRC to submit an MRC.");
            return;
        }
    }

    if (!snap.block_version_valid) {
        return;
    }

    // Clear the submitted mrc once the block advances again after the stake (which is one more than the submission block).
    if (m_submitted_research_subsidy && m_block_height >= m_submitted_height + 2) {
        m_submitted_research_subsidy = {};
        m_submitted_height = 0;
    }

    // Trial-claim results from the snapshot. m_mrc_fee mirrors the former model
    // member: min_fee + boost when a boost is set, 0 otherwise.
    m_mrc_min_fee = snap.min_fee;
    m_mrc_fee = snap.fee;
    m_reward = snap.reward;
    m_mrc_output_limit = snap.output_limit;

    // The four checks below mirror MRCModel::refresh()'s former sequence exactly:
    // the minimum-fee CreateMRC error, zero-payout, excessive-fee, then the
    // boosted-fee CreateMRC error. Each overrides the previous status, so the
    // order is load-bearing (e.g. an out-of-range boost sets EXCESSIVE_FEE, then
    // the node's boosted-run error overrides it with CREATE_ERROR).
    if (snap.create_error) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::CREATE_ERROR;
        m_mrc_error_desc = QString::fromStdString(snap.create_error_msg);
    }

    // If the (minimum) fee comes back equal to the reward we are in the zero payout interval (i.e. too soon).
    if (m_mrc_min_fee == m_reward) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::ZERO_PAYOUT;
        m_mrc_error_desc = tr("Too soon to submit an MRC request. At least 14 days must elapse from your original beacon "
                              "advertisement or last research reward payment, whether by stake or MRC, whichever is later.");
    }

    // If the total mrc fee which is the min fee + boost is greater than the reward, then the fee is excessive.
    if (m_mrc_fee > m_reward) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::EXCESSIVE_FEE;
        m_mrc_error_desc = tr("The total fee (the minimum fee + fee boost) is greater than the rewards due.");
    }

    // The boosted rerun's CreateMRC error (out-of-range boost), applied last as
    // in the original so the node's fee-validity verdict wins over EXCESSIVE_FEE.
    if (snap.boosted_fee_error) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::CREATE_ERROR;
        m_mrc_error_desc = QString::fromStdString(snap.boosted_fee_error_msg);
    }

    // Synthetic MRC queue state, evaluated node-side against this request's fee.
    m_mrc_queue_length = snap.queue_length;
    m_mrc_pos = snap.pos;
    m_mrc_queue_tail_fee = snap.queue_tail_fee;
    m_mrc_queue_head_fee = snap.queue_head_fee;
    m_mrc_queue_pay_limit_fee = snap.queue_pay_limit_fee;

    const bool found = snap.found_in_queue;

    // The first if statement is rather complex, but it looks for the situation where... 1. an mrc has been submitted,
    // 2. The block height has advanced from the original submission height, and 3. The accrual is equal or higher than
    // the research subsidy in the submitted MRC, which means the MRC has not been paid by the staker. This method avoids
    // more expensive lookups against the block/transactions.
    if (m_submitted_research_subsidy
            && m_block_height > m_submitted_height
            && *m_submitted_research_subsidy <= m_researcher_model->getAccrual()) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::STALE_CANCEL;
        m_mrc_error_desc = tr("Your MRC was successfully submitted earlier but has now become stale without being bound "
                              "to the just received block by a staker. This may be because your MRC was submitted just "
                              "before the block was staked and the MRC didn't make it to the staker in time, or your MRC "
                              "was pushed down in the queue past the pay limit. Please wait for the next block to clear "
                              "the queue and try again.");
    } else if (found && m_mrc_pos <= m_mrc_output_limit - 1) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::PENDING;
        m_mrc_error_desc = tr("You have a pending MRC request.");
    } else if (found && m_mrc_pos > m_mrc_output_limit - 1) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::PENDING_CANCEL;
        m_mrc_error_desc = tr("Your MRC was successfully submitted, but other MRCs with higher fees have pushed your MRC "
                              "down in the queue past the pay limit, and your MRC will be canceled. Wait until the next "
                              "block is received and the queue clears and try again. Your fee for the canceled MRC will "
                              "be refunded.");
    } else if (m_mrc_pos > m_mrc_output_limit - 1) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::QUEUE_FULL;
        m_mrc_error_desc = tr("The MRC queue is full. You can try boosting your fee to put your MRC request in the queue "
                              "and displace another MRC request.");
    } else if (m_wallet_locked && !found) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::WALLET_LOCKED;
        m_mrc_error_desc = tr("The wallet is locked.");
    } else if (!m_mrc_error) {
        m_mrc_status = MRCRequestStatus::ELIGIBLE;
        m_mrc_error_desc = QString{};
    }
}

void MRCModel::walletStatusChanged(int encryption_status)
{
    m_wallet_locked = (encryption_status == static_cast<int>(WalletModel::EncryptionStatus::Locked));

    refresh();

    emit walletStatusChangedSignal();
}
