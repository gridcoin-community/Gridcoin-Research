// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <optional>

#include "hash.h"
#include "chainparams.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "gridcoin/voting/builders.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "gridcoin/voting/payloads.h"
#include "logging.h"
#include "main.h"
#include "optionsmodel.h"
#include "qt/clientmodel.h"
#include "qt/voting/votingmodel.h"
#include "qt/walletmodel.h"
#include "sync.h"
#include "node/ui_interface.h"

#include <boost/signals2/signal.hpp>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

extern CCriticalSection cs_main;
extern int nBestHeight;

namespace {
//!
//! \brief Model callback bound to the \c NewPollReceived core signal.
//!
void NewPollReceived(VotingModel* model, int64_t poll_time)
{
    LogPrint(LogFlags::QT, "INFO: %s: received NewPollReceived() core signal", __func__);

    QMetaObject::invokeMethod(model, "handleNewPoll", Qt::QueuedConnection,
                              Q_ARG(int64_t, poll_time));
}

void NewVoteReceived(VotingModel* model, uint256 poll_txid)
{
    LogPrint(LogFlags::QT, "INFO: %s: received NewVoteReceived() core signal", __func__);

    // Ugly but uint256 is not registered as a Metatype.
    QMetaObject::invokeMethod(model, "handleNewVote", Qt::QueuedConnection,
                              Q_ARG(QString, QString().fromStdString(poll_txid.ToString())));
}

std::optional<PollItem> BuildPollItem(const PollRegistry::Sequence::Iterator& iter)
{
    g_timer.GetTimes(std::string{"Begin "} + std::string{__func__}, "buildPollTable");

    const PollReference& ref = iter->Ref();
    const PollResultOption result = PollResult::BuildFor(ref);

    if (!result) {
        return std::nullopt;
    }

    const Poll& poll = result->m_poll;

    PollItem item;
    item.m_id = QString::fromStdString(iter->Ref().Txid().ToString());
    item.m_version = ref.GetPollPayloadVersion();
    item.m_title = QString::fromStdString(poll.m_title).replace("_", " ");
    item.m_type_str = QString::fromStdString(poll.PollTypeToString());
    item.m_question = QString::fromStdString(poll.m_question).replace("_", " ");
    item.m_url = QString::fromStdString(poll.m_url).trimmed();
    item.m_start_time = QDateTime::fromMSecsSinceEpoch(poll.m_timestamp * 1000);
    item.m_expiration = QDateTime::fromMSecsSinceEpoch(poll.Expiration() * 1000);
    item.m_duration = poll.m_duration_days;
    item.m_weight_type = poll.m_weight_type.Raw();
    item.m_weight_type_str = QString::fromStdString(poll.WeightTypeToString());
    item.m_response_type = QString::fromStdString(poll.ResponseTypeToString());
    item.m_total_votes = result->m_votes.size();
    item.m_total_weight = result->m_total_weight / COIN;

    item.m_active_weight = 0;
    if (result->m_active_vote_weight) {
        item.m_active_weight = *result->m_active_vote_weight / COIN;
    }

    item.m_vote_percent_AVW = 0;
    if (result->m_vote_percent_avw) {
        item.m_vote_percent_AVW = *result->m_vote_percent_avw;
    }

    item.m_validated = QString{};
    if (result->m_poll_results_validated) {
        item.m_validated = *result->m_poll_results_validated;
    }

    item.m_finished = result->m_finished;
    item.m_multiple_choice = poll.AllowsMultipleChoices();

    if (!item.m_url.startsWith("http://") && !item.m_url.startsWith("https://")) {
        item.m_url.prepend("http://");
    }

    for (size_t i = 0; i < poll.m_additional_fields.size(); ++i) {
        item.m_additional_field_entries.emplace_back(
                    QString::fromStdString(poll.AdditionalFields().At(i)->m_name),
                    QString::fromStdString(poll.AdditionalFields().At(i)->m_value),
                    poll.AdditionalFields().At(i)->m_required);
    }

    for (size_t i = 0; i < result->m_responses.size(); ++i) {
        item.m_choices.emplace_back(
            QString::fromStdString(poll.Choices().At(i)->m_label),
            result->m_responses[i].m_votes,
            result->m_responses[i].m_weight / COIN);
    }

    item.m_self_voted = result->m_self_voted;
    if (result->m_self_voted) {
        item.m_self_vote_detail = result->m_self_vote_detail;
    }

    if (!result->m_votes.empty()) {
        item.m_top_answer = QString::fromStdString(result->WinnerLabel()).replace("_", " ");
    }

    // Mark stale flag false since we just rebuilt the item.
    item.m_stale = false;

    g_timer.GetTimes(std::string{"End "} + std::string{__func__}, "buildPollTable");
    return item;
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: VotingModel
// -----------------------------------------------------------------------------

VotingModel::VotingModel(
    ClientModel& client_model,
    OptionsModel& options_model,
    WalletModel& wallet_model)
    : m_registry(GetPollRegistry())
    , m_client_model(client_model)
    , m_options_model(options_model)
    , m_wallet_model(wallet_model)
    , m_last_poll_time(0)
    , m_pollitems()
{
    subscribeToCoreSignals();

    // The voting model is constructed after core init finishes. Remember the
    // time of the most recent active poll found on start-up to avoid showing
    // notifications for these if the node reorganizes the chain:
    {
        LOCK(cs_main);

        for (const auto& iter : m_registry.Polls().OnlyActive()) {
            m_last_poll_time = std::max(m_last_poll_time, iter->Ref().Time());
        }
    }
}

VotingModel::~VotingModel()
{
    unsubscribeFromCoreSignals();
}

int VotingModel::minPollDurationDays()
{
    return Poll::MIN_DURATION_DAYS;
}

int VotingModel::maxPollDurationDays()
{
    // The protocol allows poll durations up to 180 days. To limit unhelpful
    // or unintentional poll durations, user-facing pieces discourage a poll
    // longer than:
    //
    return 90; // days
}

int VotingModel::maxPollTitleLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Poll::MAX_TITLE_SIZE;
}

int VotingModel::maxPollUrlLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Poll::MAX_URL_SIZE;
}

int VotingModel::maxPollQuestionLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Poll::MAX_QUESTION_SIZE;
}

int VotingModel::maxPollChoiceLabelLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Poll::Choice::MAX_LABEL_SIZE;
}

int VotingModel::maxPollAdditionalFieldNameLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Poll::AdditionalField::MAX_NAME_SIZE;
}

int VotingModel::maxPollAdditionalFieldValueLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Poll::AdditionalField::MAX_VALUE_SIZE;
}

int VotingModel::maxPollProjectNameLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Project::MAX_NAME_SIZE;
}

int VotingModel::maxPollProjectUrlLength()
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return Project::MAX_URL_SIZE;
}

OptionsModel& VotingModel::getOptionsModel()
{
    return m_options_model;
}

QString VotingModel::getCurrentPollTitle() const
{
    return QString::fromStdString(GRC::GetCurrentPollTitle())
        .left(80)
        .replace(QChar('_'), QChar(' '), Qt::CaseSensitive);
}

QStringList VotingModel::getActiveProjectNames() const
{
    QStringList names;

    for (const auto& project : GetWhitelist().Snapshot().Sorted()) {
        names << QString::fromStdString(project.m_name);
    }

    return names;
}

QStringList VotingModel::getActiveProjectUrls() const
{
    QStringList Urls;

    for (const auto& project : GetWhitelist().Snapshot().Sorted()) {
        Urls << QString::fromStdString(project.m_url);
    }

    return Urls;
}

QStringList VotingModel::getExpiringPollsNotNotified()
{
    QStringList expiring_polls;

    QDateTime now = QDateTime::fromMSecsSinceEpoch(GetAdjustedTime() * 1000);

    qint64 poll_expire_warning = static_cast<qint64>(m_options_model.getPollExpireNotification() * 3600.0 * 1000.0);

    // Populate the list and mark the poll items included in the list m_expire_notified true.
    for (auto& poll : m_pollitems) {
        if (!poll.second.m_finished
            && now.msecsTo(poll.second.m_expiration) <= poll_expire_warning
            && !poll.second.m_expire_notified
            && !poll.second.m_self_voted) {
            expiring_polls << poll.second.m_title;
            poll.second.m_expire_notified = true;
        }
    }

    return expiring_polls;
}

std::vector<PollItem> VotingModel::buildPollTable(const PollFilterFlag flags)
{
    g_timer.InitTimer(__func__, LogInstance().WillLogCategory(BCLog::LogFlags::VOTE));
    g_timer.GetTimes(std::string{"Begin "} + std::string{__func__}, __func__);

    std::vector<PollItem> items;

    m_registry.registry_traversal_in_progress = true;

    bool fork_reorg_during_run = false;

    // We do up to three tries if there was a reorg/fork during the middle of the run. This is more than enough.
    for (unsigned int i = 0; i < 3; ++i)
    {
        for (const auto& iter : WITH_LOCK(m_registry.cs_poll_registry, return m_registry.Polls().Where(flags))) {
            // First check to see if the poll item already exists, and if so is it stale (i.e. a new vote has
            // been received for that poll). If it is stale, it will need rebuilding. If not, we insert the cached
            // poll item into the results and move on.

            bool pollitem_needs_rebuild = true;
            bool pollitem_expire_notified = false;
            auto pollitems_iter = m_pollitems.find(iter->Ref().Txid());

            // Note that the NewVoteReceived core signal will also be fired during reorgs where votes are reverted,
            // i.e. unreceived. This will cause the stale flag to be set on polls during reorg where votes have been
            // removed during reorg, which is what is desired.
            if (pollitems_iter != m_pollitems.end()) {
                if (!pollitems_iter->second.m_stale) {
                    // Not stale... the cache entry is good. Insert into items to return and go to the next one.
                    items.push_back(pollitems_iter->second);
                    pollitem_needs_rebuild = false;
                } else {
                    // Retain state for expire notification in the case of a stale poll item that needs to be
                    // refreshed.
                    pollitem_expire_notified = pollitems_iter->second.m_expire_notified;
                }
            }

            // Note that we are implementing a coarse-grained fork/rollback detector here.
            // We do this because we have eliminated the cs_main lock to free up the GUI.
            // Instead we have reversed the locking scheme and have the contract actions (add/delete)
            // place a lock on cs_poll_registry when they make an update. This preserves the integrity
            // of the registry itself. It also will preserve the integrity of transaction access in leveldb,
            // PROVIDED that a rollback/reorg has not happened during this run. The main state
            // will not change during the individual BuildPollItem calls, because BuildPollItem places
            // a lock on cs_poll_registry for its duration. This means a contract change from the
            // contract interface handler will block on it, given that it also puts a lock on
            // cs_poll_registry. I am a little worried about holding up main for this, but that was happening
            // on a recursive lock on cs_main before for the ENTIRE run, and this is certainly better.
            // Transactions that have not been rolled back by a reorg can be safely accessed for reading
            // by another thread as we are doing here.

            if (pollitem_needs_rebuild) {
                try {
                    if (std::optional<PollItem> item = BuildPollItem(iter)) {
                        // This will replace any stale existing entry in the cache with the freshly built item.
                        // It will also correctly add a new entry for a new item. The state of the pending expiry
                        // notification is retained from the stale entry to the refreshed one.
                        item->m_expire_notified = pollitem_expire_notified;
                        m_pollitems[iter->Ref().Txid()] = *item;
                        items.push_back(std::move(*item));
                    }
                } catch (InvalidDuetoReorgFork& e) {
                    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Invalidated due to reorg/fork. Starting over.",
                             __func__);
                }
            }

            // This must be AFTER BuildPollItem. If a reorg occurred during reg traversal that could invalidate
            // a Ref pointed to by the sequence, the increment of the iterator to the next position must be
            // prevented to avoid a segfault. A new Sequence must be formed and the loop started over.

            if (m_registry.reorg_occurred_during_reg_traversal) {
                items.clear();
                fork_reorg_during_run = true;

                g_timer.GetTimes(std::string{"Restart due to reorg "} + std::string{__func__}, __func__);

                // Break from the poll registry traversal loop
                break;
            }
        }

        // exit retry loop if no fork/reorg during run
        if (!fork_reorg_during_run) break;

        // Periodically recheck until reorg/fork has cleared. The reorg_occurred_during_reg_traversal will
        // be cleared by the DetectReorg function as soon as the g_reorg_in_progress is cleared by the caller of
        // ReorganizeChain. If ReorganizeChain returns a fatal error, but does not directly end program execution,
        // then g_reorg_in_progress may remain set. The call chain will eventually end program execution in that case,
        // in which this thread will be interrupted on the MilliSleep call. We do not want to attempt to resume
        // the tally if a reorganize is unsuccessful.
        while (m_registry.reorg_occurred_during_reg_traversal) {
            // Return here is for thread interrupt during shutdown.
            if (!MilliSleep(1000)) return items;

            m_registry.PollRegistry::DetectReorg();
        }

        // If the fork_reorg_during_run was set (true), then this run through the loop is invalid due to a
        // fork/reorg. Now that reorg_occurred_during_reg_traversal has cleared, reset to false for another try.
        fork_reorg_during_run = false;
    }

    m_registry.registry_traversal_in_progress = false;

    g_timer.GetTimes(std::string{"End "} + std::string{__func__}, __func__);
    return items;
}

CAmount VotingModel::estimatePollFee() const
{
    // TODO: add core API for more precise fee estimation.
    return 50 * COIN;
}

VotingResult VotingModel::sendPoll(
        const PollType& type,
        const QString& title,
        const int duration_days,
        const QString& question,
        const QString& url,
        const int weight_type,
        const int response_type,
        const QStringList& choices,
        const std::vector<AdditionalFieldEntry>& additional_field_entries) const
{
    // The poll types must be constrained based on the poll payload version, since < v3 only the SURVEY type is
    // actually used, regardless of what is selected in the GUI. In v3+, all of the types are valid. This code
    // can be removed at the next mandatory after Kermit's Mom, when PollV3Height is passed.
    uint32_t payload_version = 0;
    PollType type_by_poll_payload_version;

    {
        LOCK(cs_main);

        bool v3_enabled = IsPollV3Enabled(nBestHeight);

        payload_version = v3_enabled ? 3 : 2;

        // This is slightly different than what is in the rpc addpoll, because the types have already been constrained
        // by the GUI code.
        type_by_poll_payload_version = v3_enabled ? type : PollType::SURVEY;
    }

    std::vector<Poll::AdditionalField> additional_fields;

    for (const auto& field : additional_field_entries) {
        additional_fields.push_back(Poll::AdditionalField(field.m_name.toStdString(),
                                                          field.m_value.toStdString(),
                                                          field.m_required));
    }

    PollBuilder builder = PollBuilder();

    try {
        builder = builder
            .SetPayloadVersion(payload_version)
            .SetType(type_by_poll_payload_version)
            .SetTitle(title.toStdString())
            .SetDuration(duration_days)
            .SetQuestion(question.toStdString())
            .SetWeightType(weight_type)
            .SetResponseType(response_type)
            .SetUrl(url.toStdString())
            .SetAdditionalFields(additional_fields);

        for (const auto& choice : choices) {
            builder = builder.AddChoice(choice.toStdString());
        }
    } catch (const VotingError& e) {
        return VotingResult(QString::fromStdString(e.what()));
    }

    const WalletModel::UnlockContext unlock_context(m_wallet_model.requestUnlock());

    if (!unlock_context.isValid()) {
        return VotingResult(tr("Please unlock the wallet."));
    }

    uint256 txid;

    try {
        txid = SendPollContract(std::move(builder));
    } catch (const VotingError& e) {
        return VotingResult(QString::fromStdString(e.what()));
    }

    return VotingResult(txid);
}

VotingResult VotingModel::sendVote(
    const QString& poll_id,
    const std::vector<uint8_t>& choice_offsets) const
{
    LOCK(cs_main);

    const uint256 poll_txid = uint256S(poll_id.toStdString());
    const PollReference* ref = m_registry.TryByTxid(poll_txid);

    if (!ref) {
        return VotingResult(tr("Poll not found."));
    }

    const PollOption poll = ref->TryReadFromDisk();

    if (!poll) {
        return VotingResult(tr("Failed to load poll from disk"));
    }

    try {
        VoteBuilder builder = VoteBuilder::ForPoll(*poll, ref->Txid());
        builder = builder.AddResponses(choice_offsets);

        const WalletModel::UnlockContext unlock_context(m_wallet_model.requestUnlock());

        if (!unlock_context.isValid()) {
            return VotingResult(tr("Please unlock the wallet."));
        }

        const uint256 txid = SendVoteContract(std::move(builder));

        return VotingResult(txid);
    } catch (const VotingError& e){
        return VotingResult(e.what());
    }
}

void VotingModel::subscribeToCoreSignals()
{
    uiInterface.NewPollReceived_connect(std::bind(NewPollReceived, this, std::placeholders::_1));
    uiInterface.NewVoteReceived_connect(std::bind(NewVoteReceived, this, std::placeholders::_1));
}

void VotingModel::unsubscribeFromCoreSignals()
{
}

void VotingModel::handleNewPoll(int64_t poll_time)
{
    if (poll_time <= m_last_poll_time || m_client_model.inInitialBlockDownload()) {
        return;
    }

    m_last_poll_time = poll_time;

    emit newPollReceived();
}

void VotingModel::handleNewVote(QString poll_txid_string)
{
    uint256 poll_txid;

    poll_txid.SetHex(poll_txid_string.toStdString());

    auto pollitems_iter = m_pollitems.find(poll_txid);

    if (pollitems_iter != m_pollitems.end()) {
        // Set stale flag on poll item associated with vote.
        pollitems_iter->second.m_stale = true;
    }

    emit newVoteReceived(poll_txid_string);
}

// -----------------------------------------------------------------------------
// Class: AdditionalFieldEntry
// -----------------------------------------------------------------------------
AdditionalFieldEntry::AdditionalFieldEntry(QString name, QString value, bool required)
    : m_name(name)
    , m_value(value)
    , m_required(required)
{
}

// -----------------------------------------------------------------------------
// Class: VoteResultItem
// -----------------------------------------------------------------------------

VoteResultItem::VoteResultItem(QString label, double votes, uint64_t weight)
    : m_label(label)
    , m_votes(votes)
    , m_weight(weight)
{
}

bool VoteResultItem::operator<(const VoteResultItem& other) const
{
    return m_weight < other.m_weight;
}

// -----------------------------------------------------------------------------
// Class: VotingResult
// -----------------------------------------------------------------------------

VotingResult::VotingResult(const uint256& txid)
    : m_value(QString::fromStdString(txid.ToString()))
    , m_ok(true)
{
}

VotingResult::VotingResult(const QString& error)
    : m_value(error)
    , m_ok(false)
{
}

bool VotingResult::ok() const
{
    return m_ok;
}

QString VotingResult::txid() const
{
    if (!m_ok) {
        return QString::fromStdString(uint256().ToString());
    }

    return m_value;
}

QString VotingResult::error() const
{
    if (m_ok) {
        return QString();
    }

    return m_value;
}
