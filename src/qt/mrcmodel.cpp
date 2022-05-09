// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "wallet/wallet.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "mrcmodel.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "qt/mrcrequestpage.h"
#include "node/ui_interface.h"

extern CWallet* pwalletMain;

namespace {
//!
//! \brief Model callback bound to the \c MRCChanged core signal.
//!
void MRCChanged(MRCModel* model)
{
    LogPrint(BCLog::LogFlags::QT, "GUI: received MRCChanged() core signal");

    QMetaObject::invokeMethod(model, "mrcChanged", Qt::QueuedConnection);

}
} // anonymous namespace

MRCModel::MRCModel(WalletModel* wallet_model, ClientModel *client_model, QObject *parent)
    : QObject(parent)
    , m_wallet_model(wallet_model)
    , m_client_model(client_model)
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
    , m_mrc_error_desc(std::string{})
    , m_wallet_locked(false)
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
    unsubscribeFromCoreSignals();
}

WalletModel* MRCModel::getWalletModel()
{
    return m_wallet_model;
}

void MRCModel::showMRCDialog()
{
    MRCRequestPage *mrc_request = new MRCRequestPage(nullptr, this);

    mrc_request->show();
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

int MRCModel::getMRCOutputLimit()
{
    return m_mrc_output_limit;
}

bool MRCModel::isMRCError(MRCRequestStatus &s, std::string& e)
{
    if (m_mrc_error) {
        e = m_mrc_error_desc;
        s = m_mrc_status;
        return true;
    }

    return false;
}

bool MRCModel::submitMRC(MRCRequestStatus& s, std::string& e) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (m_mrc_status != MRCRequestStatus::ELIGIBLE) {
        return error("%s: submitMRC called while m_mrc_status, %i, is not ELIGIBLE.",
                     __func__,
                     static_cast<int>(m_mrc_status));
    }

    LOCK(pwalletMain->cs_wallet);

    CWalletTx wtx;

    std::tie(wtx, e) = GRC::SendContract(GRC::MakeContract<GRC::MRC>(GRC::ContractAction::ADD, m_mrc));
    if (!e.empty()) {
        m_mrc_error = true;
        m_mrc_status = MRCRequestStatus::SUBMIT_ERROR;
        m_mrc_error_desc = e;
        s = m_mrc_status;
        return false;
    } else {
        m_mrc_error = false;
        m_mrc_status = MRCRequestStatus::PENDING;
        m_mrc_error_desc = std::string{};
        s = m_mrc_status;
    }

    return true;
}

bool MRCModel::isWalletLocked()
{
    return m_wallet_locked;
}

void MRCModel::subscribeToCoreSignals()
{
    uiInterface.MRCChanged_connect(std::bind(MRCChanged, this));
}

void MRCModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client (no-op currently)
}

void MRCModel::refresh() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // This is similar to createmrcrequest

    AssertLockHeld(cs_main);

    CBlockIndex* pindex = mapBlockIndex[hashBestChain];

    if (!IsV12Enabled(pindex->nHeight)) {
        return;
    }

    m_mrc_error = false;
    m_mrc_error_desc = std::string{};
    m_mrc_min_fee = 0;
    m_mrc_fee = 0;

    m_mrc_output_limit = static_cast<int>(GetMRCOutputLimit(pindex->nVersion, false));

    // Do a first run with m_mrc_min_fee = 0 to compute the mrc min fee required.
    try {
        GRC::CreateMRC(pindex, m_mrc, m_reward, m_mrc_min_fee, pwalletMain, m_wallet_locked);
    } catch (GRC::MRC_error& e) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::CREATE_ERROR;
        m_mrc_error_desc = e.what();
    }

    // If the (mininum) fee comes back equal to the reward we are in the zero payout interval (i.e. too soon).
    if (m_mrc_min_fee == m_reward) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::ZERO_PAYOUT;
        m_mrc_error_desc = "Too soon since your last research rewards payment.";
    }

    // If there is a fee boost, add the boost to the fee from the initial run above.
    if (m_mrc_fee_boost != 0) {
        m_mrc_fee = m_mrc_min_fee + m_mrc_fee_boost;
    }

    // If the total mrc free which is the min fee + boost is greater than the reward, then the fee is excessive.
    if (m_mrc_fee > m_reward) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::EXCESSIVE_FEE;
        m_mrc_error_desc = "The total fee (the minimum fee + fee boost) is greater than the rewards due.";
    }

    // Rerun CreateMRC with that new total fee
    if (m_mrc_fee_boost != 0) {
        try {
            GRC::CreateMRC(pindex, m_mrc, m_reward, m_mrc_fee, pwalletMain, m_wallet_locked);
        } catch (GRC::MRC_error& e) {
            m_mrc_error |= true;
            m_mrc_status = MRCRequestStatus::CREATE_ERROR;
            m_mrc_error_desc = e.what();
        }
    }

    LogPrintf("INFO: %s: After stage 1: m_mrc_error = %u, m_mrc_status = %i, m_mrc_error_desc = %s",
              __func__,
              m_mrc_error,
              static_cast<int>(m_mrc_status),
              m_mrc_error_desc);

    // We do the mempool loop here regardless of whether there is an error condition or not.
    m_mrc_queue_length = 0;
    m_mrc_pos = 0;
    m_mrc_queue_tail_fee = std::numeric_limits<CAmount>::max();
    m_mrc_queue_head_fee = 0;

    bool found{false};

    // This will allow sorting the fees in descending order to help determine the payout limit fee.
    std::vector<CAmount> mrc_fee_vector;

    for (const auto& [_, tx] : mempool.mapTx) {
        if (!tx.GetContracts().empty()) {
            // By protocol the MRC contract MUST be the only one in the transaction.
            const GRC::Contract& contract = tx.GetContracts()[0];

            if (contract.m_type == GRC::ContractType::MRC) {
                GRC::MRC mempool_mrc = contract.CopyPayloadAs<GRC::MRC>();

                found |= m_mrc.m_mining_id == mempool_mrc.m_mining_id;

                if (!found && mempool_mrc.m_fee >= m_mrc.m_fee) ++m_mrc_pos;
                m_mrc_queue_head_fee = std::max(m_mrc_queue_head_fee, mempool_mrc.m_fee);
                m_mrc_queue_tail_fee = std::min(m_mrc_queue_tail_fee, mempool_mrc.m_fee);

                mrc_fee_vector.push_back(mempool_mrc.m_fee);

                ++m_mrc_queue_length;
            } // match to mrc contract type
        } // contract present in transaction?
    } // mempool transaction iterator

    // The tail fee converges from the max numeric limit of CAmount; however, when the above loop is done
    // it cannot end up with a number higher than the head fee. This can happen if there are no MRC transactions
    // in the loop.
    m_mrc_queue_tail_fee = std::min(m_mrc_queue_head_fee, m_mrc_queue_tail_fee);

    // Sort the fees in descending order for the pay limit calculation.
    std::sort(mrc_fee_vector.begin(), mrc_fee_vector.end(), std::greater<CAmount>());

    // Here we select the minimum of the mrc_fee_vector.size() - 1 in the case where the sorted vector does not reach the
    // m_mrc_output_limit - 1, or the m_mrc_output_limit - 1 if the sorted vector indicates the queue is (over)full,
    // i.e. the number of MRC's in the queue exceeds the m_mrc_output_limit for paying in a block.
    int pay_limit_fee_pos = std::min<int>(mrc_fee_vector.size(), m_mrc_output_limit) - 1;

    if (pay_limit_fee_pos >= 0) {
        m_mrc_queue_pay_limit_fee = mrc_fee_vector[pay_limit_fee_pos];
    }

    m_mrc_queue_pay_limit_fee = std::min(m_mrc_queue_head_fee, m_mrc_queue_pay_limit_fee);

    LogPrintf("INFO: %s: Post mempool loop: m_mrc_pos = %i, m_mrc_output_limit - 1 = %i",
              __func__,
              m_mrc_pos,
              m_mrc_output_limit - 1);

    if (found) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::PENDING;
        m_mrc_error_desc = "You have a pending MRC request.";
    } else if (m_mrc_pos > m_mrc_output_limit - 1) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::QUEUE_FULL;
        m_mrc_error_desc = "The MRC queue is full. You can try inputting a provided fee high enough to put your MRC "
                           "request in the queue and displace another MRC request.";
    } else if (m_wallet_locked) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::WALLET_LOCKED;
        m_mrc_error_desc = "The wallet is locked.";
    } else if (!m_mrc_error) {
        m_mrc_status = MRCRequestStatus::ELIGIBLE;
        m_mrc_error_desc = std::string{};
    }

    LogPrintf("INFO: %s: After stage 2: m_mrc_error = %u, m_mrc_status = %i, m_mrc_error_desc = %s",
              __func__,
              m_mrc_error,
              static_cast<int>(m_mrc_status),
              m_mrc_error_desc);
}

void MRCModel::walletStatusChanged(int encryption_status) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    m_wallet_locked = (encryption_status == static_cast<int>(WalletModel::EncryptionStatus::Locked));

    refresh();

    emit walletStatusChangedSignal();
}
