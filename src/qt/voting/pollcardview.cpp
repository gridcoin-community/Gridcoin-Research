// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/forms/voting/ui_pollcardview.h"
#include "qt/voting/pollcard.h"
#include "qt/voting/pollcardview.h"
#include "qt/voting/polltablemodel.h"
#include "qt/voting/votingmodel.h"

#include <QScrollBar>
#include <QTimer>

namespace {
constexpr int REFRESH_TIMER_INTERVAL_MSECS = 60 * 1000;
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: PollCardView
// -----------------------------------------------------------------------------

PollCardView::PollCardView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::PollCardView)
{
    ui->setupUi(this);
}

PollCardView::~PollCardView()
{
    delete ui;
}

void PollCardView::setModel(PollTableModel* model)
{
    m_polltable_model = model;

    if (!model) {
        return;
    }

    connect(model, &PollTableModel::layoutChanged, this, &PollCardView::redraw);

    if (!m_refresh_timer && m_polltable_model->includesActivePolls()) {
        m_refresh_timer.reset(new QTimer(this));
        m_refresh_timer->setTimerType(Qt::VeryCoarseTimer);

        connect(
            m_refresh_timer.get(), &QTimer::timeout,
            this, &PollCardView::updateRemainingTime);
    }
}

void PollCardView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (m_refresh_timer) {
        updateRemainingTime();
        m_refresh_timer->start(REFRESH_TIMER_INTERVAL_MSECS);
    }
}

void PollCardView::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);

    if (m_refresh_timer) {
        m_refresh_timer->stop();
    }
}

void PollCardView::redraw()
{
    // TODO: destroying and re-creating the widgets is not very efficient for
    // sorting and filtering. Hook up model events for these operations.
    clear();

    if (!m_polltable_model) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QModelIndex dummy_parent;

    for (int i = 0; i < m_polltable_model->rowCount(dummy_parent); ++i) {
        if (const PollItem* poll_item = m_polltable_model->rowItem(i)) {
            PollCard* card = new PollCard(*poll_item, this);
            card->updateRemainingTime(now);
            card->updateIcons(m_theme);

            ui->cardsLayout->addWidget(card);

            if (!poll_item->m_finished) {
                connect(card, &PollCard::voteRequested, [this, i]() {
                    emit voteRequested(i);
                });
            }

            connect(card, &PollCard::detailsRequested, [this, i]() {
                emit detailsRequested(i);
            });
        }
    }
}

void PollCardView::clear()
{
    while (ui->cardsLayout->count() > 0) {
        delete ui->cardsLayout->takeAt(0)->widget();
    }

    ui->scrollArea->verticalScrollBar()->setValue(0);
}

void PollCardView::updateRemainingTime()
{
    if (ui->cardsLayout->count() == 0) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (int i = 0; i < ui->cardsLayout->count(); ++i) {
        QLayoutItem* item = ui->cardsLayout->itemAt(i);

        if (auto* card = qobject_cast<PollCard*>(item->widget())) {
            card->updateRemainingTime(now);
        }
    }
}

void PollCardView::updateIcons(const QString& theme)
{
    m_theme = theme;

    for (int i = 0; i < ui->cardsLayout->count(); ++i) {
        QLayoutItem* item = ui->cardsLayout->itemAt(i);

        if (auto* card = qobject_cast<PollCard*>(item->widget())) {
            card->updateIcons(theme);
        }
    }
}
