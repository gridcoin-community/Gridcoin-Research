// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_GUISTATECACHE_H
#define GRIDCOIN_QT_GUISTATECACHE_H

#include "interfaces/gui_state.h"
#include "qt/notificationlifetime.h"

#include <QObject>
#include <QString>
#include <QStringList>

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

    //! Ask the core for the "add existing" picker's list
    //! (interfaces::GuiState::requestUnbookedReceiveAddresses). Returns at
    //! once; the answer arrives as unbookedReceiveAddressesAnswered() with
    //! \p request_id. A request across IPC can throw once the core has gone.
    void requestUnbookedReceiveAddresses(quint64 request_id);

Q_SIGNALS:
    //! Emitted after the stored snapshot is replaced.
    void changed();

    //! The core's answer to requestUnbookedReceiveAddresses() \p request_id:
    //! the list, or an empty list and \p error. Emitted on the GUI thread.
    void unbookedReceiveAddressesAnswered(quint64 request_id, const QStringList& addresses, const QString& error);

private:
    void accept(const interfaces::GuiStateSnapshot& incoming);

    //! The core's GUI state the requests go to. It must outlive this, as the
    //! subscriptions' handlers already require.
    interfaces::GuiState& m_gui_state;

    interfaces::GuiStateSnapshot m_snapshot;

    //! Guards the subscription fns against running while this is destroyed.
    NotificationLifetime m_notify_lifetime;
    std::unique_ptr<interfaces::Handler> m_handler;
    //! The subscription to the picker's answers.
    std::unique_ptr<interfaces::Handler> m_unbooked_handler;
};

#endif // GRIDCOIN_QT_GUISTATECACHE_H
