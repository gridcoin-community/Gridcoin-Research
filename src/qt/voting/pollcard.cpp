// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
    ui->expirationLabel->setText(GUIUtil::dateTimeStr(poll_item.m_expiration));
    ui->voteCountLabel->setText(QString::number(poll_item.m_total_votes));
    ui->totalWeightLabel->setText(QString::number(poll_item.m_total_weight));
    ui->activeVoteWeightLabel->setText(QString::number(poll_item.m_active_weight));
    ui->votePercentAVWLabel->setText(QString::number(poll_item.m_vote_percent_AVW, 'f', 4) + '\%');
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
