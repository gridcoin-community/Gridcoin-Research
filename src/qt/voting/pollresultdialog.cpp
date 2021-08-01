// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_pollresultdialog.h"
#include "qt/voting/pollresultchoiceitem.h"
#include "qt/voting/pollresultdialog.h"
#include "qt/voting/votingmodel.h"

#include <QPushButton>

namespace {
std::vector<const VoteResultItem*>
SortResultsByWeight(const std::vector<VoteResultItem>& choices)
{
    std::vector<const VoteResultItem*> sorted;

    for (const auto& choice_result : choices) {
        sorted.emplace_back(&choice_result);
    }

    constexpr auto descending = [](const VoteResultItem* a, const VoteResultItem* b) {
        return *b < *a;
    };

    std::stable_sort(sorted.begin(), sorted.end(), descending);

    return sorted;
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: PollResultChoiceItem
// -----------------------------------------------------------------------------

PollResultDialog::PollResultDialog(const PollItem& poll_item, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::PollResultDialog)
{
    ui->setupUi(this);

    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    resize(GRC::ScaleSize(this, width(), height()));

    GRC::ScaleFontPointSize(ui->idLabel, 8);

    ui->details->setItem(poll_item);
    ui->idLabel->setText(poll_item.m_id);

    const auto sorted = SortResultsByWeight(poll_item.m_choices);

    ui->choicesLayout->addWidget(new PollResultChoiceItem(
        *sorted.front(),
        poll_item.m_total_weight,
        sorted.front()->m_weight));

    for (size_t i = 1; i < poll_item.m_choices.size(); ++i) {
        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        ui->choicesLayout->addWidget(line);

        ui->choicesLayout->addWidget(new PollResultChoiceItem(
            *sorted[i],
            poll_item.m_total_weight,
            sorted.front()->m_weight));
    }

    ui->buttonBox->button(QDialogButtonBox::Close)->setIcon(QIcon());
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

PollResultDialog::~PollResultDialog()
{
    delete ui;
}
