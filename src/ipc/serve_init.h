// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_SERVE_INIT_H
#define GRIDCOIN_IPC_SERVE_INIT_H

#include "interfaces/init.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ipc {

//! How long an accepted connection may go without authenticating and holding
//! the attach slot before the node drops it (doc/multiprocess_design.md section
//! 4.3).
//!
//! Why this exists: the listener accepts at most two simultaneous connections
//! (the attached GUI, and one more that is told another GUI is attached), and the
//! accept is not re-armed while both are occupied. Without a deadline, a local
//! process that connects and then says nothing -- or presents a wrong cookie and
//! sits there, or authenticates while another GUI holds the slot -- occupies a
//! connection for as long as it lives, and two of them deny the real GUI service,
//! with nothing in the transport ever reclaiming the slots. It also bounds the
//! window in which an unauthenticated peer can reach the pre-auth surface of the
//! capnp interface (Init.construct -> ThreadMap; see
//! MAX_SERVER_THREADS_PER_CONNECTION).
//!
//! The value is generous on purpose. A real GUI authenticates in the same
//! millisecond it connects (ClientHandshake's first act is authenticate()), so
//! anything above a second or two is already unreachable for legitimate traffic;
//! 30s only avoids punishing a heavily loaded or debugger-stopped client.
inline constexpr std::chrono::seconds IPC_AUTH_DEADLINE{30};

//! The core's one attach slot: the core serves one GUI at a time. A connection
//! claims it as it authenticates, and holds it until its teardown gives it back;
//! another connection that authenticates meanwhile is told another GUI is
//! attached and gets no capabilities. One slot is shared by every connection of
//! a listener. Connections are named by an id that is never reused, so a stale
//! Release cannot free a later holder's slot. The mutex inside is a leaf.
class AttachSlot
{
public:
    virtual ~AttachSlot();

    //! True if the slot was free (and is now held by \p conn_id) or is already
    //! held by \p conn_id.
    virtual bool TryClaim(uint64_t conn_id) = 0;

    //! Free the slot. Does nothing unless \p conn_id holds it.
    virtual void Release(uint64_t conn_id) = 0;

    //! The connection holding the slot, or nullopt when it is free.
    virtual std::optional<uint64_t> Holder() const = 0;
};

//! Return a new, free slot.
std::shared_ptr<AttachSlot> MakeAttachSlot();

//! Wrap the node's real (in-process) Init so it can be served over IPC: it adds
//! the connect handshake (authenticate / getBuildInfo / getIdentity) and gates
//! the delegated methods so nothing is served before authenticate() succeeds
//! (doc/multiprocess_design.md section 4.3). Only the IPC-exposed Init methods
//! are wrapped; this must grow as init.capnp exposes more factories.
//!
//! Each served connection gets its own wrapper, and every wrapper of a listener
//! shares that listener's \p slot (which must not be null). The connection that
//! claims the slot at authentication gets the makeX factories; any other is
//! refused them. Each wrapper owns its connection's interfaces::ConnectionScope,
//! which the wallet it hands out is bound to. \p on_slot_closed, when set, runs
//! when the connection that held the slot is torn down, first among that
//! connection's teardown hooks and before the slot is given back; it runs on the
//! IPC event-loop thread, so it must not block on cs_main or cs_wallet or make an
//! IPC call. The daemon's ends the wallet's IPC capability.
std::unique_ptr<interfaces::Init> MakeServeInit(std::unique_ptr<interfaces::Init> inner,
                                                std::string cookie,
                                                interfaces::NodeIdentity identity,
                                                std::shared_ptr<AttachSlot> slot,
                                                std::function<void()> on_slot_closed);

} // namespace ipc

#endif // GRIDCOIN_IPC_SERVE_INIT_H
