// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/forms/voting/ui_pollcard.h"
#include "qt/guiutil.h"
#include "qt/voting/pollcard.h"
#include "qt/voting/votingmodel.h"

// -----------------------------------------------------------------------------
// Class: PollCard
// -----------------------------------------------------------------------------

PollCard::PollCard(const PollItem& poll_item, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::PollCard)
    , m_expiration(poll_item.m_expiration)
{
    ui->setupUi(this);

    ui->titleLabel->setText(poll_item.m_title);

    ui->typeLabel->setText(poll_item.m_type_str);
    if (poll_item.m_version >= 3) {
        ui->typeLabel->show();
    } else {
        ui->typeLabel->hide();
    }

    ui->expirationLabel->setText(GUIUtil::dateTimeStr(poll_item.m_expiration));
    ui->voteCountLabel->setText(QString::number(poll_item.m_total_votes));
    ui->totalWeightLabel->setText(QString::number(poll_item.m_total_weight));
    ui->activeVoteWeightLabel->setText(QString::number(poll_item.m_active_weight));
    ui->votePercentAVWLabel->setText(QString::number(poll_item.m_vote_percent_AVW, 'f', 4) + '\%');

    if (!poll_item.m_self_voted) {
        ui->myLastVoteAnswerLabel->setText("No Vote");
        ui->myVoteWeightLabel->setText("N/A");
        ui->myPercentAVWLabel->setText("N/A");
    } else {
        QString choices_str;

        int64_t my_total_weight = 0;

        for (const auto& choice : poll_item.m_self_vote_detail.m_responses) {
            if (!choices_str.isEmpty()) {
                choices_str += ", " + QString(poll_item.m_choices[choice.first].m_label);
            } else {
                choices_str = QString(poll_item.m_choices[choice.first].m_label);
            }

            my_total_weight += choice.second / COIN;
        }

        ui->myLastVoteAnswerLabel->setText(choices_str);
        ui->myVoteWeightLabel->setText(QString::number(my_total_weight));
        if (poll_item.m_active_weight) ui->myPercentAVWLabel->setText(QString::number((double) my_total_weight
                                                                                      / (double) poll_item.m_active_weight
                                                                                      * (double) 100.0, 'f', 4) + '\%');
    }

    if (!(poll_item.m_weight_type == (int)GRC::PollWeightType::BALANCE ||
          poll_item.m_weight_type == (int)GRC::PollWeightType::BALANCE_AND_MAGNITUDE)) {
        ui->balanceLabel->hide();
    }

    if (!(poll_item.m_weight_type == (int)GRC::PollWeightType::MAGNITUDE ||
          poll_item.m_weight_type == (int)GRC::PollWeightType::BALANCE_AND_MAGNITUDE)) {
        ui->magnitudeLabel->hide();
    }

    if (poll_item.m_validated.toString() == QString{} || (!poll_item.m_validated.toBool() && !poll_item.m_finished)) {
        // Hide both validated and invalid tags if less than v3 poll, or, not valid and not finished
        ui->validatedLabel->hide();
        ui->invalidLabel->hide();
    } else if (poll_item.m_validated.toBool()) {
        // Show validated if v3 poll and valid by vote weight % of AVW, even if not finished
        ui->validatedLabel->show();
        ui->invalidLabel->hide();
    } else if (!poll_item.m_validated.toBool() && poll_item.m_finished) {
        // Show invalid if v3 poll and invalid by vote weight % of AVW and finished
        ui->validatedLabel->hide();
        ui->invalidLabel->show();
    }

    ui->topAnswerLabel->setText(poll_item.m_top_answer);

    if (!poll_item.m_finished) {
        connect(ui->voteButton, &QPushButton::clicked, this, &PollCard::voteRequested);
    } else {
        delete ui->voteButton;
        ui->voteButton = nullptr;
    }

    connect(ui->detailsButton, &QPushButton::clicked, this, &PollCard::detailsRequested);
}

PollCard::~PollCard()
{
    delete ui;
}

void PollCard::updateRemainingTime(const QDateTime& now)
{
    if (ui->voteButton == nullptr) {
        return;
    }

    if (m_expiration < now) {
        delete ui->voteButton;
        ui->voteButton = nullptr;
        ui->remainingLabel->setText(tr("Voting finished."));

        return;
    }

    constexpr int64_t three_days = 60 * 60 * 24 * 3;
    const int64_t remaining_secs = now.secsTo(m_expiration);

    ui->remainingLabel->setText(QObject::tr("%1 remaining.")
        .arg(GUIUtil::formatNiceTimeOffset(remaining_secs)));

    if (remaining_secs < three_days) {
        ui->remainingLabel->setStyleSheet(QStringLiteral("color: red"));
    }
}

void PollCard::updateIcons(const QString& theme)
{
    if (ui->voteButton != nullptr) {
        ui->voteButton->setIcon(QIcon(":/icons/" + theme + "_vote"));
    }

    ui->detailsButton->setIcon(QIcon(":/icons/" + theme + "_hamburger"));
}
