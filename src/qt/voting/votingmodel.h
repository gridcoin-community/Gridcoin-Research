// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_VOTINGMODEL_H
#define GRIDCOIN_QT_VOTING_VOTINGMODEL_H

#include "amount.h"
#include "gridcoin/voting/filter.h"
#include "qt/voting/poll_types.h"
#include "gridcoin/voting/poll.h"

#include <QDateTime>
#include <QObject>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <QVariant>
#include <QStringList>

namespace interfaces {
class Handler;
class ResearcherContext;
class VotingManager;
} // namespace interfaces

class ClientModel;
class OptionsModel;
class uint256;
class WalletModel;

//!
//! \brief This is the UI equivalent of the core Poll::AdditonalField class
//!
class AdditionalFieldEntry
{
public:
    QString m_name;
    QString m_value;
    bool m_required;

    explicit AdditionalFieldEntry(QString name, QString value, bool required);
};

//!
//! \brief An aggregate result for one choice of a poll.
//!
class VoteResultItem
{
public:
    QString m_label;
    double m_votes;
    uint64_t m_weight;

    explicit VoteResultItem(QString label, double votes, uint64_t weight);
    bool operator<(const VoteResultItem& other) const;
};

//!
//! \brief One selected choice of the wallet holder's own vote: the offset into
//! the poll's choices and the voting weight resolved for it.
//!
//! This is the GUI value type that replaces the former embedded core
//! GRC::PollResult::VoteDetail — the node side (interfaces::PollTableItem)
//! precomputes it, so no core type crosses into the GUI.
//!
class PollSelfVoteResponse
{
public:
    uint8_t m_choice_offset = 0;
    uint64_t m_weight = 0;
};

//!
//! \brief Represents a poll contract and associated responses.
//!
class PollItem
{
public:
    QString m_id;
    uint32_t m_version;
    QString m_type_str;
    QString m_title;
    QString m_question;
    QString m_url;
    QDateTime m_start_time;
    QDateTime m_expiration;
    uint32_t m_duration;
    int m_weight_type;
    QString m_weight_type_str;
    QString m_response_type;
    QString m_top_answer;
    uint32_t m_total_votes;
    uint64_t m_total_weight;
    uint64_t m_active_weight;
    double m_vote_percent_AVW;
    QVariant m_validated;
    bool m_finished;
    bool m_multiple_choice;
    std::vector<AdditionalFieldEntry> m_additional_field_entries;
    std::vector<VoteResultItem> m_choices;
    bool m_self_voted;
    std::vector<PollSelfVoteResponse> m_self_vote_responses;

    //! Display flag for the "Stale results" column / pollcard indicator: set when
    //! a new vote for this poll arrives (handlePollStaleFlag) after the row was
    //! built, and cleared when the table is next rebuilt. This is purely a GUI
    //! freshness hint — the authoritative tally invalidation lives in the core
    //! PollResultCache — so a freshly mapped row starts not stale.
    bool m_stale = false;

    bool m_expire_notified = false;
};

//!
//! \brief A variant-like object that stores the result of an attempt to create
//! a poll or vote contract transaction.
//!
class VotingResult
{
public:
    explicit VotingResult(const uint256& txid);
    explicit VotingResult(const QString& error);

    bool ok() const;
    QString error() const;
    QString txid() const;

private:
    QString m_value;
    bool m_ok;
};

//!
//! \brief Presents voting information for UI components.
//!
class VotingModel : public QObject
{
    Q_OBJECT

public:
    VotingModel(
        interfaces::VotingManager& voting_manager,
        interfaces::ResearcherContext& researcher_context,
        ClientModel& client_model,
        OptionsModel& options_model,
        WalletModel& wallet_model);
    ~VotingModel();

    static int minPollDurationDays();
    static int maxPollDurationDays();
    static int maxPollTitleLength();
    static int maxPollUrlLength();
    static int maxPollQuestionLength();
    static int maxPollChoiceLabelLength();
    static int maxPollAdditionalFieldNameLength();
    static int maxPollAdditionalFieldValueLength();
    // Non-static (unlike the other max* limits): these read GRC::Project field
    // sizes through the researcher interface rather than gridcoin/project.h.
    int maxPollProjectNameLength() const;
    int maxPollProjectUrlLength() const;

    OptionsModel& getOptionsModel();
    QString getCurrentPollTitle() const;
    QStringList getActiveProjectNames() const;
    QStringList getActiveProjectUrls() const;

    //!
    //! \brief getExpiringPollsNotNotified. This method populates a QStringList with
    //! the polls in the pollitems cache that are within the m_poll_expire_warning window
    //! and which have not previously been notified to the user. Since this method is
    //! to be used to have the GUI immediately provide notification to the user, it also
    //! marks each of the polls in the QStringList m_expire_notified = true so that they
    //! will not appear again on this list (unless the wallet is restarted). This accomplishes
    //! a single shot notification for each poll that is about to expire.
    //!
    //! \return QStringList of polls that are about to expire (within m_poll_expire_warning of
    //! expiration), and which have not previously been included on the list (i.e. notified).
    //!
    QStringList getExpiringPollsNotNotified();
    std::vector<PollItem> buildPollTable(const GRC::PollFilterFlag flags);

    CAmount estimatePollFee() const;

    VotingResult sendPoll(
            const GRC::PollType& type,
            const QString& title,
            const int duration_days,
            const QString& question,
            const QString& url,
            const int weight_type,
            const int response_type,
            const QStringList& choices,
            const std::vector<AdditionalFieldEntry>& additional_field_entries = {}) const;

    VotingResult sendVote(
            const QString& poll_id,
            const std::vector<uint8_t>& choice_offsets) const;

signals:
    void newPollReceived();
    void newVoteReceived(QString poll_txid_string);

private:
    //! The node-side voting boundary (Phase 1d-iii): poll table over the core
    //! result cache, poll/vote submission, and the new-poll / new-vote
    //! notifications. Owned by the process and outlives this model.
    interfaces::VotingManager& m_voting;

    //! The researcher/beacon boundary (Phase 1d-iv). Used here only for the poll
    //! wizard's active-project pickers (getActiveProjectNames/getActiveProjectUrls),
    //! which read the whitelist through it. Owned by the process; outlives this model.
    interfaces::ResearcherContext& m_researcher_context;

    ClientModel& m_client_model;
    OptionsModel& m_options_model;
    WalletModel& m_wallet_model;
    int64_t m_last_poll_time;

    //!
    //! \brief m_pollitems. A GUI-side store of the poll items last returned by
    //! buildPollTable, keyed by poll txid. It is NOT the tally cache — the core
    //! PollResultCache memoizes tallies now — but it retains the per-poll
    //! m_expire_notified flag across rebuilds so getExpiringPollsNotNotified can
    //! give a single-shot expiry notification per poll.
    //!
    std::map<uint256, PollItem> m_pollitems;

    //! Guards m_pollitems: buildPollTable updates it on the PollTableModel worker
    //! thread while getExpiringPollsNotNotified reads/mutates it on the GUI thread,
    //! so both must hold this mutex.
    std::mutex m_pollitems_mutex;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    //! Retained interface subscriptions, released on teardown so a notification
    //! that fires after this model is destroyed cannot invoke a callback bound to
    //! freed memory. The Handlers disconnect on destruction (issue #3129).
    std::vector<std::unique_ptr<interfaces::Handler>> m_handlers;

private slots:
    void handleNewPoll(int64_t poll_time);
    void handleNewVote(QString poll_txid_string);
}; // VotingModel

#endif // GRIDCOIN_QT_VOTING_VOTINGMODEL_H
