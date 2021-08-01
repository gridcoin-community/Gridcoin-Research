// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTING_POLLRESULTCHOICEITEM_H
#define VOTING_POLLRESULTCHOICEITEM_H

#include <QFrame>

namespace Ui {
class PollResultChoiceItem;
}

class VoteResultItem;

class PollResultChoiceItem : public QFrame
{
    Q_OBJECT

public:
    explicit PollResultChoiceItem(
        const VoteResultItem& choice,
        const double total_poll_weight,
        const double top_choice_weight,
        QWidget* parent = nullptr);
    ~PollResultChoiceItem();

private:
    Ui::PollResultChoiceItem* ui;
};

#endif // VOTING_POLLRESULTCHOICEITEM_H
