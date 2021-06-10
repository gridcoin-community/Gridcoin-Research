// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/voting/ui_pollresultchoiceitem.h"
#include "qt/voting/pollresultchoiceitem.h"
#include "qt/voting/votingmodel.h"

namespace {
constexpr char CHUNK_STYLE_TEMPLATE[] =
    "QProgressBar::chunk {"
    "border-radius: 0.25em;"
    "background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 %1, stop: 1 %2);"
    "}";

const QColor BAR_START_COLOR(0, 219, 222);
const QColor BAR_END_COLOR(252, 0, 255);

QString CalculateBarStyle(const double ratio)
{
    const QColor end_color(
        (1 - ratio) * BAR_START_COLOR.red() + ratio * BAR_END_COLOR.red(),
        (1 - ratio) * BAR_START_COLOR.green() + ratio * BAR_END_COLOR.green(),
        (1 - ratio) * BAR_START_COLOR.blue() + ratio * BAR_END_COLOR.blue());

    return QString(CHUNK_STYLE_TEMPLATE)
        .arg(BAR_START_COLOR.name())
        .arg(end_color.name());
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: PollResultChoiceItem
// -----------------------------------------------------------------------------

PollResultChoiceItem::PollResultChoiceItem(
    const VoteResultItem& choice,
    const double total_poll_weight,
    const double top_choice_weight,
    QWidget* parent)
    : QFrame(parent)
    , ui(new Ui::PollResultChoiceItem)
{
    ui->setupUi(this);

    ui->choiceLabel->setText(choice.m_label);
    ui->weightLabel->setText(QString::number(choice.m_weight));

    // If the poll has no responses yet, skip the rest of the ratio-dependent
    // calculations and formatting:
    //
    if (total_poll_weight == 0) {
        ui->percentageLabel->hide();
        return;
    }

    const double relative_ratio = choice.m_weight / top_choice_weight;
    const double percentage = 100.0 * choice.m_weight / total_poll_weight;

    ui->percentageLabel->setText(QStringLiteral("%1%").arg(percentage, 0, 'f', 2));

    // Limit the lower bound to the smallest degree that Qt will draw the
    // progress bar's rounded border at for the default dialog size:
    //
    if (percentage < 0.5) {
        ui->weightBar->setValue(0);
    } else {
        ui->weightBar->setValue(std::max(12.0, 1000.0 * relative_ratio));
    }

    ui->weightBar->setStyleSheet(CalculateBarStyle(relative_ratio));
}

PollResultChoiceItem::~PollResultChoiceItem()
{
    delete ui;
}
