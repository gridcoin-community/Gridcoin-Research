// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTING_POLLRESULTDIALOG_H
#define VOTING_POLLRESULTDIALOG_H

#include <QDialog>

namespace Ui {
class PollResultDialog;
}

class PollItem;

class PollResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PollResultDialog(const PollItem& poll_item, QWidget* parent = nullptr);
    ~PollResultDialog();

private:
    Ui::PollResultDialog* ui;
};

#endif // VOTING_POLLRESULTDIALOG_H
