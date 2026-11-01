// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying file
// COPYING or https://opensource.org/licenses/mit-license.php

#include "balancecopyfilter.h"
#include "bitcoinunits.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>

BalanceCopyFilter::BalanceCopyFilter(QLabel* label,
                                     std::function<QString()> full_balance,
                                     QString copy_action_text,
                                     std::function<bool()> menu_enabled)
    : QObject(label),
      m_label(label),
      m_full_balance(std::move(full_balance)),
      m_copy_action_text(std::move(copy_action_text)),
      m_menu_enabled(std::move(menu_enabled))
{
    m_label->installEventFilter(this);
}

bool BalanceCopyFilter::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != m_label) {
        return QObject::eventFilter(obj, event);
    }

    if (event->type() == QEvent::ContextMenu) {
        if (!m_menu_enabled()) {
            return true; // consume; do not show menu (e.g. privacy mode)
        }
        auto* ce = static_cast<QContextMenuEvent*>(event);
        QMenu menu;
        menu.addAction(m_copy_action_text, this, [this]() {
            QString text = m_full_balance();
            text.remove(BitcoinUnits::THIN_SPACE);
            QApplication::clipboard()->setText(text);
        });
        menu.exec(m_label->mapToGlobal(ce->pos()));
        return true;
    }

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->matches(QKeySequence::Copy) && m_label->hasSelectedText()) {
            QString text = m_label->selectedText();
            text.remove(BitcoinUnits::THIN_SPACE);
            QApplication::clipboard()->setText(text);
            return true; // consume; we wrote the stripped selection ourselves
        }
    }

    return QObject::eventFilter(obj, event);
}
