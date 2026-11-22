// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/gui_state.h"
#include "interfaces/handler.h"
#include "qt/guilog.h"
#include "interfaces/mrc.h"
#include "mrcmodel.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "guistatecache.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "researcher/researchermodel.h"
#include "qt/mrcrequestpage.h"

#include <QMetaObject>
#include <QtConcurrentRun>

#include <exception>
#include <limits>

namespace {
//! The wait after the last fee boost edit before the core is asked for a quote.
constexpr int QUOTE_DEBOUNCE_MS = 250;
} // anonymous namespace

MRCModel::MRCModel(interfaces::MRC& mrc, interfaces::GuiState& gui_state, WalletModel* wallet_model,
                   ClientModel *client_model, ResearcherModel *researcher_model, QObject *parent)
    : QObject(parent)
    , m_mrc(mrc)
    , m_gui_state(gui_state)
    , m_cache(client_model ? client_model->guiState() : nullptr)
    , m_wallet_model(wallet_model)
    , m_client_model(client_model)
    , m_researcher_model(researcher_model)
    , m_mrc_request(nullptr)
    , m_snap_present(false)
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
    , m_quote_id(0)
    , m_quote_updating(false)
    , m_submitting(false)
{
    subscribeToQuotes();

    // The quote is requested a moment after the last fee boost edit, so typing
    // an amount does not ask the core for a quote per keystroke.
    m_quote_timer.setSingleShot(true);
    m_quote_timer.setInterval(QUOTE_DEBOUNCE_MS);
    connect(&m_quote_timer, &QTimer::timeout, this, &MRCModel::issueQuote);

    connect(&m_submit_watcher, &QFutureWatcher<interfaces::MRCSubmitResult>::finished, this, &MRCModel::finishSubmit);

    // The pushed state drives the model: the cache emits when the core pushes a
    // new one. One already computed when the cache was built emits nothing, so
    // it is read once here.
    if (m_cache) {
        connect(m_cache, &GuiStateCache::changed, this, &MRCModel::applyPushedState);
    }

    if (m_wallet_model) {
        // Detects whether the wallet has been locked or unlocked.
        connect(m_wallet_model, &WalletModel::encryptionStatusChanged, this, &MRCModel::walletStatusChanged);

        m_wallet_locked = (m_wallet_model->getEncryptionStatus() == WalletModel::EncryptionStatus::Locked);
    }

    applyPushedState();
}

MRCModel::~MRCModel()
{
    if (m_mrc_request) {
        m_mrc_request->done(QDialog::Accepted);
    }

    unsubscribeFromQuotes();
    waitForSubmit();

    // The submit's finished signal may never be delivered now, so release the
    // unlock here, or a wallet unlocked or elevated for it stays so (the node
    // keeps running under -multiprocess). The wallet model outlives this one:
    // bitcoin.cpp constructs it first.
    m_submit_unlock.reset();
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
    if (fee_boost == m_mrc_fee_boost) return;

    m_mrc_fee_boost = fee_boost;
    m_submit_notice.clear();
    m_fresh_terms.reset();
    m_quote_failure.reset();

    // Whatever quote is out or waiting was for another boost.
    ++m_quote_id;

    if (m_mrc_fee_boost == 0) {
        // The pushed figures are those of no boost.
        m_quote_timer.stop();
        m_quote_updating = false;
        takePushedState(/*force=*/true);
        return;
    }

    // Keep the figures on screen, marked as updating, until the quote lands.
    m_quote_updating = true;
    m_quote_timer.start();

    refresh();
    emit mrcChanged();
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
    return m_fresh_terms ? m_fresh_terms->second : m_reward;
}

CAmount MRCModel::getMRCTotalFee() const
{
    if (m_fresh_terms) return m_fresh_terms->first;

    return m_mrc_fee_boost != 0 ? m_mrc_fee : m_mrc_min_fee;
}

CAmount MRCModel::getMRCNetPayout() const
{
    return (m_fresh_terms ? m_fresh_terms->second : m_reward) - getMRCTotalFee();
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
    } else if (!m_snap_present) {
        // The core pushes MRC figures only while it is in sync, and pushes a
        // new state the moment that changes.
        return MRCModel::ModelStatus::OUT_OF_SYNC;
    } else if (!m_block_version_valid) {
        return MRCModel::ModelStatus::INVALID_BLOCK_VERSION;
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

bool MRCModel::submitMRC(MRCRequestStatus& s, QString& e, CAmount confirmed_fee, CAmount confirmed_net,
                         std::unique_ptr<WalletModel::UnlockContext> unlock)
{
    // Every refusal below releases the unlock at once.
    if (m_submitting) {
        e = tr("A submit is already in progress.");
        s = m_mrc_status;
        unlock.reset();
        return false;
    }

    if (m_mrc_status != MRCRequestStatus::ELIGIBLE || m_quote_updating) {
        s = m_mrc_status;
        e = tr("The MRC request cannot be submitted now.");
        unlock.reset();

        return GUIError("%s: submitMRC called while m_mrc_status, %i, is not ELIGIBLE or the figures are updating.",
                        __func__,
                        static_cast<int>(m_mrc_status));
    }

    m_submit_notice.clear();

    // The terms are those the caller took when the user confirmed, not the
    // current figures: a push can land between the click and here (while the
    // unlock prompt is open). The core sends the request only if it would pay
    // no more, and the user receive no less, than these.
    const CAmount fee_boost = m_mrc_fee_boost;
    const bool wallet_locked = m_wallet_locked;

    m_submitting = true;
    m_submit_unlock = std::move(unlock);
    s = m_mrc_status;

    // submitConfirmed takes cs_main in the core, so it runs off the GUI thread.
    // An exception (a lost connection to a separate wallet process) must not
    // escape into the future, where the join would rethrow it.
    m_submit_watcher.setFuture(QtConcurrent::run([this, fee_boost, wallet_locked, confirmed_fee, confirmed_net]() {
        try {
            return m_mrc.submitConfirmed(fee_boost, wallet_locked, confirmed_fee, confirmed_net);
        } catch (const std::exception& ex) {
            interfaces::MRCSubmitResult failed;
            failed.error = std::string("The MRC request could not be submitted: ") + ex.what();
            return failed;
        } catch (...) {
            interfaces::MRCSubmitResult failed;
            failed.error = "The MRC request could not be submitted: unknown exception";
            return failed;
        }
    }));

    emit mrcChanged();

    return true;
}

void MRCModel::finishSubmit()
{
    const interfaces::MRCSubmitResult result = m_submit_watcher.result();

    m_submitting = false;

    // Hand the unlock back before anything reports the end of the submit.
    m_submit_unlock.reset();

    if (result.success) {
        m_submitted_research_subsidy = result.research_subsidy;
        m_submitted_height = result.submitted_height;
        m_mrc_error = false;
        m_mrc_status = MRCRequestStatus::PENDING;
        m_mrc_error_desc = QString{};
        m_fresh_terms.reset();

        // The core pushes the queue state that follows from the new request.
        m_gui_state.requestUpdate();
    } else if (result.terms_changed) {
        // Nothing was sent. Show the figures the core would now send, ask for a
        // fresh quote to bring the queue figures along, and leave the decision
        // to the user: this never resubmits.
        int display_unit = BitcoinUnits::BTC;
        if (m_wallet_model && m_wallet_model->getOptionsModel()) {
            display_unit = m_wallet_model->getOptionsModel()->getDisplayUnit();
        }

        m_fresh_terms = std::make_pair(result.fresh_fee, result.fresh_reward);
        m_submit_notice = tr("The fee or the reward changed, so the MRC request was not submitted. The total fee is now "
                             "%1 and the net payout %2. Submit again to accept these.")
                .arg(BitcoinUnits::formatWithUnit(display_unit, result.fresh_fee),
                     BitcoinUnits::formatWithUnit(display_unit, result.fresh_reward - result.fresh_fee));

        // issueQuote() marks the figures as updating, which would disable
        // Submit. Cleared again on purpose: the user may accept the fresh terms
        // at once, and Submit always sends exactly the terms shown at the click
        // (the core refuses if they moved again), so a quote landing later only
        // replaces the figures with newer ones.
        ++m_quote_id;
        issueQuote();
        m_quote_updating = false;
    } else {
        // Refused for another reason, for instance the boost now exceeds the
        // reward. The figures shown are stale, so quote again and let the user
        // confirm what comes back.
        m_mrc_error = true;
        m_mrc_status = MRCRequestStatus::SUBMIT_ERROR;
        m_mrc_error_desc = QString::fromStdString(result.error);
        m_submit_notice = m_mrc_error_desc;

        ++m_quote_id;
        issueQuote();
    }

    emit mrcChanged();
    emit submitFinished(result.success);
}

void MRCModel::waitForSubmit()
{
    // Join the worker without propagating anything it left behind: this runs
    // from the destructor. The worker body contains its own exceptions.
    try {
        m_submit_watcher.waitForFinished();
    } catch (...) {
    }
}

void MRCModel::requestUpdate()
{
    m_gui_state.requestUpdate();
}

bool MRCModel::isWalletLocked()
{
    return m_wallet_locked;
}

void MRCModel::subscribeToQuotes()
{
    // Retain the handler so it is severed in unsubscribeFromQuotes() (from
    // ~MRCModel); the callback captures `this` and a signal firing after this
    // object is gone would otherwise invoke a slot on freed memory (issue
    // #3129). It runs on a core thread: copy the quote and queue it to the GUI
    // thread.
    m_quote_handler = m_gui_state.handleMRCQuote(m_notify_lifetime.guard(
        [this](uint64_t request_id, const interfaces::MRCSnapshot& quote) {
            QMetaObject::invokeMethod(this, [this, request_id, copy = quote]() { applyQuote(request_id, copy); },
                                      Qt::QueuedConnection);
        }));
}

void MRCModel::unsubscribeFromQuotes()
{
    // Retire before disconnecting: severing a Handler does not wait for a
    // callback already running on a core thread (qt/notificationlifetime.h).
    m_notify_lifetime.retire();

    m_quote_handler.reset();
}

namespace {
//! Whether two MRC parts carry the same figures: the chain state, the trial claim
//! and the queue.
bool SameMRCPart(const interfaces::MRCSnapshot& a, const interfaces::MRCSnapshot& b)
{
    return a.out_of_sync == b.out_of_sync
        && a.block_height == b.block_height
        && a.block_version_valid == b.block_version_valid
        && a.output_limit == b.output_limit
        && a.create_error == b.create_error
        && a.create_error_msg == b.create_error_msg
        && a.boosted_fee_error == b.boosted_fee_error
        && a.boosted_fee_error_msg == b.boosted_fee_error_msg
        && a.reward == b.reward
        && a.min_fee == b.min_fee
        && a.fee == b.fee
        && a.queue_length == b.queue_length
        && a.pos == b.pos
        && a.found_in_queue == b.found_in_queue
        && a.queue_head_fee == b.queue_head_fee
        && a.queue_tail_fee == b.queue_tail_fee
        && a.queue_pay_limit_fee == b.queue_pay_limit_fee
        && a.mature_balance == b.mature_balance;
}
} // anonymous namespace

void MRCModel::applyPushedState()
{
    takePushedState(/*force=*/false);
}

void MRCModel::takePushedState(bool force)
{
    if (!m_cache) {
        refresh();
        emit mrcChanged();
        return;
    }

    const interfaces::GuiStateSnapshot& pushed = m_cache->snapshot();

    // The node pushes about every eight seconds whether or not the MRC part
    // changed. When it did not, the figures on the page, a quote in flight and
    // the fresh terms of a refused submit all stand: only the rest of the model
    // (the researcher, the wallet) is read again below.
    const bool unchanged = !force && pushed.has_mrc == m_last_pushed_has_mrc
        && (!pushed.has_mrc || SameMRCPart(pushed.mrc, m_last_pushed_mrc));

    if (!unchanged) {
        m_last_pushed_has_mrc = pushed.has_mrc;
        m_last_pushed_mrc = pushed.mrc;

        m_snap_present = pushed.has_mrc;
        m_fresh_terms.reset();
        m_quote_failure.reset();

        if (!pushed.has_mrc) {
            // The core is out of sync: a quote still on its way is for a state that
            // is gone, and must not bring the figures back.
            ++m_quote_id;
            m_quote_updating = false;
            m_quote_timer.stop();
        }

        if (pushed.has_mrc) {
            m_snap = pushed.mrc;

            if (m_mrc_fee_boost != 0) {
                // The pushed figures are for no boost: they stand in, marked as
                // updating, until the quote against the new state lands.
                ++m_quote_id;
                issueQuote();
            }
        }
    }

    refresh();
    emit mrcChanged();
}

void MRCModel::issueQuote()
{
    m_quote_updating = true;
    m_gui_state.requestMRCQuote(m_mrc_fee_boost, m_quote_id);
}

void MRCModel::applyQuote(uint64_t request_id, const interfaces::MRCSnapshot& quote)
{
    if (request_id != m_quote_id) return;

    m_quote_updating = false;
    m_fresh_terms.reset();

    if (quote.create_error && !quote.block_version_valid && !quote.out_of_sync) {
        // A quote that failed to compute is answered with the error alone.
        // Keep the last good figures and chain state (block height, version
        // gate) and show the error over them.
        m_quote_failure = QString::fromStdString(quote.create_error_msg);
    } else {
        m_quote_failure.reset();
        m_snap = quote;
        m_snap_present = !quote.out_of_sync;
    }

    refresh();
    emit mrcChanged();
}

void MRCModel::refresh()
{
    m_mrc_error = false;
    m_mrc_status = MRCRequestStatus::NONE;
    m_mrc_error_desc = QString{};

    // Researcher eligibility is owned GUI-side (ResearcherModel; migrated in
    // 1d-iv).
    const bool researcher_eligible = m_researcher_model
            && m_researcher_model->hasActiveBeacon()
            && !m_researcher_model->configuredForNoncruncherMode()
            && !m_researcher_model->detectedPoolMode();

    // The figures the core pushed or quoted. Absent while the core is out of
    // sync (it pushes MRC figures only while in sync): the page shows the
    // out-of-sync state and the rest is ignored.
    if (!m_snap_present) {
        return;
    }

    const interfaces::MRCSnapshot& snap = m_snap;

    // Cache the version gate for getMRCModelStatus() (block-driven, so it cannot
    // go stale without a refresh trigger).
    m_block_version_valid = snap.block_version_valid;

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

    m_block_height = snap.block_height;

    // A quote that failed to compute: the error wins over the figures kept.
    if (m_quote_failure) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::CREATE_ERROR;
        m_mrc_error_desc = *m_quote_failure;
        return;
    }

    // Do a balance check before anything else. The mature balance is the
    // core's, pushed with the figures.
    if (snap.mature_balance < COIN) {
        m_mrc_error |= true;
        m_mrc_status = MRCRequestStatus::INSUFFICIENT_MATURE_FUNDS;
        m_mrc_error_desc = tr("You must have a mature balance of at least 1 GRC to submit an MRC.");
        return;
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
