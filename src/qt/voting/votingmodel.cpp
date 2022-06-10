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
#include "qt/clientmodel.h"
#include "qt/voting/votingmodel.h"
#include "qt/walletmodel.h"
#include "sync.h"
#include "node/ui_interface.h"

#include <boost/signals2/signal.hpp>


using namespace GRC;
using LogFlags = BCLog::LogFlags;

extern CCriticalSection cs_main;

namespace {
//!
//! \brief Model callback bound to the \c NewPollReceived core signal.
//!
void NewPollReceived(VotingModel* model, int64_t poll_time)
{
    LogPrint(LogFlags::QT, "GUI: received NewPollReceived() core signal");

    QMetaObject::invokeMethod(model, "handleNewPoll", Qt::QueuedConnection,
        Q_ARG(int64_t, poll_time));
}

std::optional<PollItem> BuildPollItem(const PollRegistry::Sequence::Iterator& iter)
{
    const PollReference& ref = iter->Ref();
    const PollResultOption result = PollResult::BuildFor(ref);

    if (!result) {
        return std::nullopt;
    }

    const Poll& poll = result->m_poll;

    PollItem item;
    item.m_id = QString::fromStdString(iter->Ref().Txid().ToString());
    item.m_title = QString::fromStdString(poll.m_title).replace("_", " ");
    item.m_question = QString::fromStdString(poll.m_question).replace("_", " ");
    item.m_url = QString::fromStdString(poll.m_url).trimmed();
    item.m_start_time = QDateTime::fromMSecsSinceEpoch(poll.m_timestamp * 1000);
    item.m_expiration = QDateTime::fromMSecsSinceEpoch(poll.Expiration() * 1000);
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

    for (size_t i = 0; i < result->m_responses.size(); ++i) {
        item.m_choices.emplace_back(
            QString::fromStdString(poll.Choices().At(i)->m_label),
            result->m_responses[i].m_votes,
            result->m_responses[i].m_weight / COIN);
    }

    if (!result->m_votes.empty()) {
        item.m_top_answer = QString::fromStdString(result->WinnerLabel()).replace("_", " ");
    }

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

std::vector<PollItem> VotingModel::buildPollTable(const PollFilterFlag flags) const
{
    std::vector<PollItem> items;

    LOCK(cs_main);

    for (const auto& iter : m_registry.Polls().Where(flags)) {
        if (std::optional<PollItem> item = BuildPollItem(iter)) {
            items.push_back(std::move(*item));
        }
    }

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
    const QStringList& choices) const
{
    // The poll types must be constrained based on the poll payload version, since < v3 only the SURVEY type is
    // actually used, regardless of what is selected in the GUI. In v3+, all of the types are valid. This code
    // can be removed at the next mandatory after Kermit's Mom, when PollV3Height is passed.
    PollType type_by_poll_payload_version;

    {
        LOCK(cs_main);

        type_by_poll_payload_version = IsPollV3Enabled(nBestHeight) ? type : PollType::SURVEY;
    }

    PollBuilder builder = PollBuilder();

    try {
        builder = builder
            .SetType(type_by_poll_payload_version)
            .SetTitle(title.toStdString())
            .SetDuration(duration_days)
            .SetQuestion(question.toStdString())
            .SetWeightType(weight_type)
            .SetResponseType(response_type)
            .SetUrl(url.toStdString());

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
