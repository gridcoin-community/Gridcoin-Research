// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTING_POLLDETAILS_H
#define VOTING_POLLDETAILS_H

#include <QWidget>

namespace Ui {
class PollDetails;
}

class PollItem;

class PollDetails : public QWidget
{
    Q_OBJECT

public:
    explicit PollDetails(QWidget* parent = nullptr);
    ~PollDetails();

    void setItem(const PollItem& poll_item);

private:
    Ui::PollDetails* ui;
};

#endif // VOTING_POLLDETAILS_H
