// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/guistatecache.h"

#include "interfaces/handler.h"

#include <QMetaObject>
#include <Qt>

#include <string>
#include <vector>

GuiStateCache::GuiStateCache(interfaces::GuiState& gui_state, QObject* parent)
    : QObject(parent)
    , m_gui_state(gui_state)
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

    // The picker's answers, on a core thread (NoGuiState's on the caller's, inside
    // the request): convert the list and queue the emit, so none is delivered inside the request.
    m_unbooked_handler = gui_state.handleUnbookedReceiveAddresses(m_notify_lifetime.guard(
        [this](uint64_t request_id, const std::vector<std::string>& addresses, const std::string& error) {
            QStringList list;
            for (const std::string& address : addresses) list << QString::fromStdString(address);
            QMetaObject::invokeMethod(this, [this, request_id, list, message = QString::fromStdString(error)]() {
                emit unbookedReceiveAddressesAnswered(request_id, list, message);
            }, Qt::QueuedConnection);
        }));

    m_snapshot = gui_state.current();
}

GuiStateCache::~GuiStateCache()
{
    // Refuse further deliveries and wait out one already running, before any
    // member is destroyed; clearing the handlers then disconnects them.
    m_notify_lifetime.retire();
    m_unbooked_handler.reset();
    m_handler.reset();
}

void GuiStateCache::accept(const interfaces::GuiStateSnapshot& incoming)
{
    if (incoming.sequence <= m_snapshot.sequence) return;

    m_snapshot = incoming;

    emit changed();
}

void GuiStateCache::requestUnbookedReceiveAddresses(quint64 request_id)
{
    m_gui_state.requestUnbookedReceiveAddresses(request_id);
}
