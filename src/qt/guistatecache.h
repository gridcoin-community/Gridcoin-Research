// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_GUISTATECACHE_H
#define GRIDCOIN_QT_GUISTATECACHE_H

#include "interfaces/gui_state.h"
#include "qt/notificationlifetime.h"

#include <QObject>

#include <memory>

namespace interfaces {
class Handler;
} // namespace interfaces

//!
//! \brief The GUI's copy of the state the core computes and pushes
//! (interfaces::GuiState). GUI thread only.
//!
//! The core calls the subscription fn on one of its own threads, so the fn only
//! copies the snapshot and queues it to the GUI thread. The stored snapshot is
//! replaced only by one with a greater sequence, since delivered sequences never
//! decrease but may repeat (and a queued older one may land after a newer).
//! Nothing here takes a core lock.
//!
class GuiStateCache : public QObject
{
    Q_OBJECT
public:
    //! Reads the core's current snapshot once, then subscribes to the pushes.
    explicit GuiStateCache(interfaces::GuiState& gui_state, QObject* parent = nullptr);
    ~GuiStateCache() override;

    const interfaces::GuiStateSnapshot& snapshot() const { return m_snapshot; }

    //! False until the core has pushed a first computed snapshot.
    bool computed() const { return m_snapshot.sequence != 0; }

Q_SIGNALS:
    //! Emitted after the stored snapshot is replaced.
    void changed();

private:
    void accept(const interfaces::GuiStateSnapshot& incoming);

    interfaces::GuiStateSnapshot m_snapshot;

    //! Guards the subscription fn against running while this is destroyed.
    NotificationLifetime m_notify_lifetime;
    std::unique_ptr<interfaces::Handler> m_handler;
};

#endif // GRIDCOIN_QT_GUISTATECACHE_H
