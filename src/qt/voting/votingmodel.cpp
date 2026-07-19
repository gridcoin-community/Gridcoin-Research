// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "qt/guilog.h"
#include "gridcoin/voting/poll.h"
#include "interfaces/handler.h"
#include "interfaces/researcher.h"
#include "interfaces/voting.h"
#include "logging.h"
#include "optionsmodel.h"
#include "qt/clientmodel.h"
#include "qt/voting/votingmodel.h"
#include "qt/walletmodel.h"
#include "uint256.h"
#include "util/time.h"

#include <optional>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
//!
//! \brief Map a node-side poll row (interfaces::PollTableItem — all value data,
//! tally already run and canonical strings already resolved by the core) to the
//! Qt PollItem the views render. Only cosmetic transforms remain here: '_' -> ' '
//! in free text, trimming and http:// prefixing the URL, and Unix-seconds ->
//! QDateTime (milliseconds). Weights that display in whole GRC were already
//! divided by COIN on the node side.
//!
PollItem MapToPollItem(const interfaces::PollTableItem& src)
{
    PollItem item;
    item.m_id = QString::fromStdString(src.txid);
    item.m_version = src.payload_version;
    item.m_title = QString::fromStdString(src.title).replace("_", " ");
    item.m_type_str = QString::fromStdString(src.type_str);
    item.m_question = QString::fromStdString(src.question).replace("_", " ");
    item.m_url = QString::fromStdString(src.url).trimmed();
    item.m_start_time = QDateTime::fromMSecsSinceEpoch(src.start_time * 1000);
    item.m_expiration = QDateTime::fromMSecsSinceEpoch(src.expiration * 1000);
    item.m_duration = src.duration_days;
    item.m_weight_type = src.weight_type;
    item.m_weight_type_str = QString::fromStdString(src.weight_type_str);
    item.m_response_type = QString::fromStdString(src.response_type);
    item.m_total_votes = src.total_votes;
    item.m_total_weight = src.total_weight;
    item.m_active_weight = src.active_weight;
    item.m_vote_percent_AVW = src.vote_percent_avw;

    item.m_validated = QString{};
    if (src.validated) {
        item.m_validated = *src.validated;
    }

    item.m_finished = src.finished;
    item.m_multiple_choice = src.multiple_choice;

    if (!item.m_url.startsWith("http://") && !item.m_url.startsWith("https://")) {
        item.m_url.prepend("http://");
    }

    for (const auto& field : src.additional_fields) {
        item.m_additional_field_entries.emplace_back(
            QString::fromStdString(field.name),
            QString::fromStdString(field.value),
            field.required);
    }

    for (const auto& choice : src.choices) {
        item.m_choices.emplace_back(
            QString::fromStdString(choice.label),
            choice.votes,
            choice.weight);
    }

    item.m_self_voted = src.self_voted;
    for (const auto& response : src.self_vote_responses) {
        item.m_self_vote_responses.push_back(
            PollSelfVoteResponse{response.choice_offset, response.weight});
    }

    if (!src.top_answer.empty()) {
        item.m_top_answer = QString::fromStdString(src.top_answer).replace("_", " ");
    }

    return item;
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Class: VotingModel
// -----------------------------------------------------------------------------

VotingModel::VotingModel(
    interfaces::VotingManager& voting_manager,
    interfaces::ResearcherContext& researcher_context,
    ClientModel& client_model,
    OptionsModel& options_model,
    WalletModel& wallet_model)
    : m_voting(voting_manager)
    , m_researcher_context(researcher_context)
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
    m_last_poll_time = m_voting.latestActivePollTime();
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

int VotingModel::maxPollProjectNameLength() const
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return m_researcher_context.maxProjectNameLength();
}

int VotingModel::maxPollProjectUrlLength() const
{
    // Not strictly accurate: the protocol limits the max length in bytes, but
    // Qt limits field lengths in UTF-8 characters which may be represented by
    // more than one byte.
    //
    return m_researcher_context.maxProjectUrlLength();
}

OptionsModel& VotingModel::getOptionsModel()
{
    return m_options_model;
}

QString VotingModel::getCurrentPollTitle() const
{
    return QString::fromStdString(m_voting.currentPollTitle())
        .left(80)
        .replace(QChar('_'), QChar(' '), Qt::CaseSensitive);
}

QStringList VotingModel::getActiveProjectNames() const
{
    QStringList names;

    // The whitelist read moved node-side (Phase 1d-iv); whitelistProjects()
    // preserves the former ACTIVE-filter, Sorted(), raw-m_name semantics.
    for (const auto& project : m_researcher_context.whitelistProjects()) {
        names << QString::fromStdString(project.name);
    }

    return names;
}

QStringList VotingModel::getActiveProjectUrls() const
{
    QStringList Urls;

    for (const auto& project : m_researcher_context.whitelistProjects()) {
        Urls << QString::fromStdString(project.url);
    }

    return Urls;
}

QStringList VotingModel::getExpiringPollsNotNotified()
{
    QStringList expiring_polls;

    QDateTime now = QDateTime::fromMSecsSinceEpoch(GetAdjustedTime() * 1000);

    qint64 poll_expire_warning = static_cast<qint64>(m_options_model.getPollExpireNotification() * 3600.0 * 1000.0);

    // Runs on the GUI thread while buildPollTable may be updating m_pollitems on
    // the PollTableModel worker thread, so guard the whole read/mutate.
    std::lock_guard<std::mutex> lock(m_pollitems_mutex);

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
    // The tally, its cache, the reorg-retry and the registry walk all live in the
    // core now (interfaces::VotingManager over PollResultCache). This maps the
    // value snapshot the node returns into Qt PollItems and refreshes the GUI-side
    // store, carrying each poll's m_expire_notified across rebuilds so the expiry
    // notifier stays single-shot.
    std::vector<PollItem> items;

    for (const interfaces::PollTableItem& src : m_voting.buildPollTable(static_cast<int>(flags))) {
        items.push_back(MapToPollItem(src));
    }

    // Refresh the GUI-side store under its mutex (getExpiringPollsNotNotified reads
    // it on the GUI thread), carrying each poll's m_expire_notified across rebuilds.
    // The expensive interface tally above ran outside the lock.
    {
        std::lock_guard<std::mutex> lock(m_pollitems_mutex);

        for (PollItem& item : items) {
            const uint256 txid = uint256S(item.m_id.toStdString());
            if (auto existing = m_pollitems.find(txid); existing != m_pollitems.end()) {
                item.m_expire_notified = existing->second.m_expire_notified;
            }

            m_pollitems[txid] = item;
        }
    }

    return items;
}

CAmount VotingModel::estimatePollFee() const
{
    return m_voting.estimatePollFee();
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
    // The payload version (and, pre-v3, the forced SURVEY type) are resolved by
    // the node from the chain height, so the GUI just forwards the entered
    // fields as a value submission.
    interfaces::PollSubmission submission;
    submission.type = static_cast<int>(type);
    submission.title = title.toStdString();
    submission.duration_days = duration_days;
    submission.question = question.toStdString();
    submission.url = url.toStdString();
    submission.weight_type = weight_type;
    submission.response_type = response_type;

    for (const auto& choice : choices) {
        submission.choices.push_back(choice.toStdString());
    }

    for (const auto& field : additional_field_entries) {
        submission.additional_fields.push_back(
            {field.m_name.toStdString(), field.m_value.toStdString(), field.m_required});
    }

    // Unlock the wallet before handing the submission to the node. The modal
    // stays in the GUI and is raised OUTSIDE any core lock.
    const WalletModel::UnlockContext unlock_context(m_wallet_model.requestUnlock());

    if (!unlock_context.isValid()) {
        return VotingResult(tr("Please unlock the wallet."));
    }

    const interfaces::VotingSubmitResult result = m_voting.submitPoll(submission);

    // submitPoll yields only OK or FAILED (with a dynamic message).
    if (result.status != interfaces::VotingSubmitStatus::OK) {
        return VotingResult(QString::fromStdString(result.error));
    }

    return VotingResult(uint256S(result.txid));
}

VotingResult VotingModel::sendVote(
    const QString& poll_id,
    const std::vector<uint8_t>& choice_offsets) const
{
    // Unlock the wallet before the node builds and broadcasts the vote. The modal
    // is raised here in the GUI, no longer while holding cs_main (the node takes
    // cs_main internally for the registry lookup and broadcast).
    const WalletModel::UnlockContext unlock_context(m_wallet_model.requestUnlock());

    if (!unlock_context.isValid()) {
        return VotingResult(tr("Please unlock the wallet."));
    }

    const interfaces::VotingSubmitResult result =
        m_voting.submitVote(poll_id.toStdString(), choice_offsets);

    // Map the node's categorized status to translated GUI text; FAILED carries a
    // dynamic message (e.g. a VotingError) shown as-is.
    switch (result.status) {
    case interfaces::VotingSubmitStatus::OK:
        return VotingResult(uint256S(result.txid));
    case interfaces::VotingSubmitStatus::POLL_NOT_FOUND:
        return VotingResult(tr("Poll not found."));
    case interfaces::VotingSubmitStatus::POLL_LOAD_FAILED:
        return VotingResult(tr("Failed to load poll from disk"));
    case interfaces::VotingSubmitStatus::FAILED:
        break;
    }

    return VotingResult(QString::fromStdString(result.error));
}

void VotingModel::subscribeToCoreSignals()
{
    // Retain each subscription so it is released in unsubscribeFromCoreSignals()
    // (from ~VotingModel). The callbacks fire on a core thread and capture `this`,
    // so they marshal to the GUI thread via a queued slot invocation; releasing
    // the Handler on teardown severs the callback before `this` is destroyed
    // (issue #3129).
    m_handlers.emplace_back(m_voting.handleNewPollReceived([this](int64_t poll_time) {
        GUILogPrint(GUILogCategory::QT, "INFO: VotingModel: received NewPollReceived() notification");
        QMetaObject::invokeMethod(this, "handleNewPoll", Qt::QueuedConnection, Q_ARG(int64_t, poll_time));
    }));

    m_handlers.emplace_back(m_voting.handleNewVoteReceived([this](std::string poll_txid) {
        GUILogPrint(GUILogCategory::QT, "INFO: VotingModel: received NewVoteReceived() notification");
        // uint256 is not a registered Qt metatype, so marshal the hex string.
        QMetaObject::invokeMethod(this, "handleNewVote", Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromStdString(poll_txid)));
    }));
}

void VotingModel::unsubscribeFromCoreSignals()
{
    // Clearing the retained subscriptions runs each Handler's destructor, which
    // disconnects it (issue #3129).
    m_handlers.clear();
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
    // Tally invalidation for the affected poll now lives in the core
    // PollResultCache (a new vote arrives in a block, which moves the tip and
    // invalidates the active poll's cached tally). The GUI just forwards the
    // notification so the views refresh.
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
