// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/guistatecache.h"

#include "interfaces/handler.h"

#include <QMetaObject>
#include <Qt>

GuiStateCache::GuiStateCache(interfaces::GuiState& gui_state, QObject* parent)
    : QObject(parent)
{
    // Subscribe first, then read the current snapshot. Only the first
    // subscriber is forced an update, so a snapshot published between a read
    // and the subscription would otherwise never be delivered here. One
    // delivered meanwhile, or the same one again, is dropped by accept() as
    // not newer.
    //
    // Runs on a core thread: copy the snapshot and queue it, touch nothing else.
    m_handler = gui_state.handleChanged(m_notify_lifetime.guard(
        [this](const interfaces::GuiStateSnapshot& incoming) {
            QMetaObject::invokeMethod(this, [this, copy = incoming]() { accept(copy); },
                                      Qt::QueuedConnection);
        }));

    m_snapshot = gui_state.current();
}

GuiStateCache::~GuiStateCache()
{
    // Refuse further deliveries and wait out one already running, before any
    // member is destroyed; clearing the handler then disconnects it.
    m_notify_lifetime.retire();
    m_handler.reset();
}

void GuiStateCache::accept(const interfaces::GuiStateSnapshot& incoming)
{
    if (incoming.sequence <= m_snapshot.sequence) return;

    m_snapshot = incoming;

    emit changed();
}
