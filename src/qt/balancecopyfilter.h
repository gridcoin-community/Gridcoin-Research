// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying file
// COPYING or https://opensource.org/licenses/mit-license.php

#ifndef BITCOIN_QT_BALANCECOPYFILTER_H
#define BITCOIN_QT_BALANCECOPYFILTER_H

#include <QObject>
#include <QString>
#include <functional>

class QEvent;
class QLabel;

/**
 * Event filter that provides correct copy behavior for currency-formatted
 * balance labels:
 *
 *  - Right-click (or Shift+F10) -> "Copy amount" menu -> writes the full
 *    formatted balance to the clipboard, with U+2009 (THIN SPACE) thousands
 *    separators stripped so the value pastes cleanly into consumers that
 *    don't normalize whitespace.
 *  - QKeySequence::Copy (Ctrl-C / Ctrl-Ins / Shift-Ins) with an active text
 *    selection on the label -> writes the selected substring to the clipboard,
 *    also with thin spaces stripped.
 *
 * Background (issue #2994): the earlier per-label
 * setContextMenuPolicy(Qt::CustomContextMenu) wiring (PR #2849) suppressed
 * Qt's standard Ctrl-C copy path on a QLabel with Qt::TextSelectableByMouse.
 * The clipboard retained the last full-balance value written by the right-
 * click "Copy amount" action regardless of what the user had selected, so
 * Ctrl-C with a partial selection appeared to copy the entire balance.
 *
 * The event-filter approach intercepts only QEvent::ContextMenu and key
 * presses that match QKeySequence::Copy; mouse selection, focus, and the
 * rest of the QLabel / QWidgetTextControl path stay at Qt defaults. The
 * filter parents itself to the label, so its lifetime is tied to the label.
 */
class BalanceCopyFilter : public QObject
{
    Q_OBJECT

public:
    /**
     * @param label             Balance label to install on. For the Ctrl-C
     *                          path to be reachable the label's
     *                          textInteractionFlags should include
     *                          Qt::TextSelectableByMouse.
     * @param full_balance      Returns the full formatted balance string for
     *                          the right-click "Copy amount" action. Thin
     *                          spaces are stripped before clipboard write.
     * @param copy_action_text  Pre-translated label for the right-click menu
     *                          action. The translation context belongs to
     *                          the caller so existing translated strings
     *                          stay matched.
     * @param menu_enabled      Returns true if the right-click menu should
     *                          be shown. Use to suppress the menu in
     *                          privacy mode.
     */
    BalanceCopyFilter(QLabel* label,
                      std::function<QString()> full_balance,
                      QString copy_action_text,
                      std::function<bool()> menu_enabled);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QLabel* m_label;
    std::function<QString()> m_full_balance;
    QString m_copy_action_text;
    std::function<bool()> m_menu_enabled;
};

#endif // BITCOIN_QT_BALANCECOPYFILTER_H
