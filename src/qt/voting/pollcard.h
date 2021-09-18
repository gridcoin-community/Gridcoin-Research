// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLCARD_H
#define GRIDCOIN_QT_VOTING_POLLCARD_H

#include <QDateTime>
#include <QWidget>

namespace Ui {
class PollCard;
}

class PollItem;

class PollCard : public QWidget
{
    Q_OBJECT

public:
    explicit PollCard(const PollItem& poll_item, QWidget* parent = nullptr);
    ~PollCard();

signals:
    void voteRequested();
    void detailsRequested();

public slots:
    void updateRemainingTime(const QDateTime& now);
    void updateIcons(const QString& theme);

private:
    Ui::PollCard* ui;
    QDateTime m_expiration;
};

#endif // GRIDCOIN_QT_VOTING_POLLCARD_H
