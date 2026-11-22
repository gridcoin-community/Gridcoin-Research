// Copyright (c) 2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/syncoverlay.h"
#include "qt/decoration.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

SyncOverlay::SyncOverlay(QWidget* parent)
    : QWidget(parent)
    , m_dismissed(false)
    , m_out_of_sync(true)
{
    // Fill the entire parent widget area and sit on top of everything.
    setAttribute(Qt::WA_TranslucentBackground, false);

    if (parent) {
        parent->installEventFilter(this);
        resize(parent->size());
    }

    // --- Central panel ---

    // Each theme's stylesheet styles #syncOverlayPanel.
    QWidget* panel = new QWidget(this);
    panel->setObjectName("syncOverlayPanel");
    panel->setFixedWidth(480);

    // Icon
    m_icon_label = new QLabel(panel);
    m_icon_label->setPixmap(GRC::ScaleIcon(this, ":/icons/no_result", 48));
    m_icon_label->setAlignment(Qt::AlignCenter);

    // Title
    m_title_label = new QLabel(panel);
    m_title_label->setText(tr("Wallet is syncing"));
    m_title_label->setAlignment(Qt::AlignCenter);
    QFont title_font = m_title_label->font();
    title_font.setPointSize(title_font.pointSize() + 4);
    title_font.setBold(true);
    m_title_label->setFont(title_font);

    // Detail
    m_detail_label = new QLabel(panel);
    m_detail_label->setText(
        tr("The displayed information may be out of date. Your wallet "
           "automatically synchronizes with the Gridcoin network after a "
           "connection is established, but this process has not completed yet."
           "\n\n"
           "Operations such as beacon management, voting, and sending "
           "transactions should not be performed until synchronization "
           "is complete.")
    );
    m_detail_label->setAlignment(Qt::AlignCenter);
    m_detail_label->setWordWrap(true);

    // Hide button
    m_hide_button = new QPushButton(tr("Hide"), panel);
    m_hide_button->setFixedWidth(120);
    connect(m_hide_button, &QPushButton::clicked, this, &SyncOverlay::dismiss);

    // Panel layout
    QVBoxLayout* panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(32, 24, 32, 24);
    panel_layout->setSpacing(12);
    panel_layout->addWidget(m_icon_label);
    panel_layout->addWidget(m_title_label);
    panel_layout->addWidget(m_detail_label);

    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    button_layout->addWidget(m_hide_button);
    button_layout->addStretch();
    panel_layout->addSpacing(8);
    panel_layout->addLayout(button_layout);

    // Overlay layout — center the panel
    QVBoxLayout* outer_v = new QVBoxLayout(this);
    outer_v->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* outer_h = new QHBoxLayout();
    outer_h->addStretch();
    outer_h->addWidget(panel);
    outer_h->addStretch();

    outer_v->addStretch();
    outer_v->addLayout(outer_h);
    outer_v->addStretch();
}

bool SyncOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parent() && event->type() == QEvent::Resize) {
        resize(parentWidget()->size());
        raise();
    }

    return QWidget::eventFilter(watched, event);
}

void SyncOverlay::setSyncState(bool out_of_sync, int blocks, int total_blocks)
{
    m_out_of_sync = out_of_sync;

    if (!out_of_sync) {
        // In sync — hide unconditionally and reset dismissed state so the
        // overlay will reappear if the wallet falls out of sync again later.
        m_dismissed = false;
        QWidget::hide();
        return;
    }

    if (m_dismissed) {
        return;
    }

    // Update progress detail. Only show block count and percentage when the
    // peer median is meaningful (blocks < total_blocks). Otherwise fall back
    // to the generic message — showing "100%" while out of sync would mislead.
    if (total_blocks > 0 && blocks < total_blocks) {
        int pct = static_cast<int>(100.0 * blocks / total_blocks);
        m_detail_label->setText(
            tr("The wallet is currently synchronizing with the Gridcoin "
               "network. Progress: %1 of %2 blocks (%3%)."
               "\n\n"
               "Operations such as beacon management, voting, and sending "
               "transactions should not be performed until synchronization "
               "is complete.")
            .arg(blocks)
            .arg(total_blocks)
            .arg(pct)
        );
    } else {
        m_detail_label->setText(
            tr("The displayed information may be out of date. Your wallet "
               "automatically synchronizes with the Gridcoin network after a "
               "connection is established, but this process has not completed yet."
               "\n\n"
               "Operations such as beacon management, voting, and sending "
               "transactions should not be performed until synchronization "
               "is complete.")
        );
    }

    raise();
    show();
}

bool SyncOverlay::isActive() const
{
    return m_out_of_sync && !m_dismissed;
}

void SyncOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 160));
}

void SyncOverlay::dismiss()
{
    m_dismissed = true;
    QWidget::hide();
}
