// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/capnp/protocol.h"

#include "interfaces/init.h"
#include "ipc/capnp/init.capnp.h"
#include "ipc/capnp/init.capnp.proxy.h"
#include "ipc/context.h"
#include "ipc/protocol.h"
#include "ipc/serve_init.h" // IPC_AUTH_DEADLINE
#include "util.h"

#include <mp/proxy-io.h>
#include <mp/proxy-types.h>

#include <cassert>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#ifndef WIN32
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif
#include <system_error>
#include <thread>
#include <typeindex>

namespace ipc {
namespace capnp {
namespace {

//! Route libmultiprocess log messages into the Gridcoin log, split by mp's own
//! severity rather than lumped together.
//!
//! Trace/Debug/Info are high-volume bookkeeping (per-request send/recv, proxy
//! create/destroy), so they go to the dedicated IPC category -- silent by
//! default, surfaced with -debug=ipc. Warning and Error are real signal, and
//! burying them in a debug category is how you lose the one message that
//! mattered in a flood of bookkeeping, so they take the uncategorised WARN /
//! ERROR forms and are always logged. Raise stays on the quiet category and is
//! converted to an exception below -- see the case for why it must not be
//! grouped with Error.
//!
//! The switch has no default so -Wswitch flags an mp::Log level that gained no
//! mapping; the post-switch assert is the runtime backstop, mirroring
//! ToCoreFlag() in qt/guilog.cpp. The backstop is doing the real work here --
//! this target is not currently built with -Wall/-Wextra, so the compile-time
//! half is aspirational. The fallback logs before it asserts, so the unhandled
//! level is recorded whether or not asserts are live: dropping an mp message
//! outright would be worse than misclassifying one.
void LogIpcMessage(const mp::LogMessage& message)
{
    switch (message.level) {
    case mp::Log::Trace:
    case mp::Log::Debug:
    case mp::Log::Info:
        LogPrint(BCLog::LogFlags::IPC, "ipc: %s", message.message);
        return;
    case mp::Log::Warning:
        LogPrintf("WARN: ipc: %s", message.message);
        return;
    case mp::Log::Error:
        error("ipc: %s", message.message);
        return;
    case mp::Log::Raise:
        // Deliberately quiet, and NOT grouped with Error. Raise is mp's "turn
        // this into an exception" level -- IpcLogFn does exactly that below, so
        // the throw is the signal and this line is only belt-and-braces. It is
        // not reserved for fatal faults: the dominant Raise in practice is
        // "IPC client method called after disconnect." (proxy-types.h:721/772),
        // which fires whenever a one-way notification races a GUI close.
        // interfaces::GuardNotify swallows that by design and logs its own gated
        // line, so promoting Raise to error() would stamp an ERROR into the
        // daemon log for every ordinary GUI shutdown, across every notification
        // forwarder -- the exact noise this category exists to remove.
        LogPrint(BCLog::LogFlags::IPC, "ipc: %s", message.message);
        return;
    }

    // Log BEFORE asserting. This target compiles with -DNDEBUG followed by
    // -UNDEBUG, so asserts are live and would abort the process before the
    // diagnostic was ever written -- leaving no record of the level that went
    // unhandled, which is the opposite of the intent. In this order the message
    // survives in both build configurations.
    LogPrintf("WARN: ipc: (unhandled mp log level %d) %s",
              static_cast<int>(message.level), message.message);
    assert(false && "unhandled mp::Log level");
}

void IpcLogFn(mp::LogMessage message)
{
    LogIpcMessage(message);

    if (message.level == mp::Log::Raise) {
        throw std::runtime_error(message.message);
    }
}

//! Invoke a caller-supplied disconnect callback with a guard. It runs on the
//! event-loop thread, so a throw would unwind through libmultiprocess's task set
//! and terminate the process; contain and log it instead.
void InvokeDisconnectCb(const std::function<void()>& cb)
{
    if (!cb) return;
    try {
        cb();
    } catch (const std::exception& e) {
        error("ipc: disconnect callback threw: %s", e.what());
    } catch (...) {
        error("ipc: disconnect callback threw a non-standard exception");
    }
}

class CapnpProtocol : public Protocol
{
public:
    ~CapnpProtocol() noexcept override
    {
        m_loop_ref.reset();
        if (m_loop_thread.joinable()) m_loop_thread.join();
        assert(!m_loop);
    }

    std::unique_ptr<interfaces::Init> connect(int fd, const char* exe_name,
                                              std::function<void()> on_disconnect) override
    {
        startLoop(exe_name);
        auto client = mp::ConnectStream<messages::Init>(*m_loop, mp::MakeStream(*m_loop, fd));
        if (on_disconnect && client) {
            // Register a caller-facing disconnect handler ALONGSIDE ConnectStream's
            // own (which logs and frees the Connection). Connection::onDisconnect
            // accumulates handlers and defers each to the EventLoop task set, so
            // both run as independent tasks; ours only forwards a notification and
            // never touches the Connection, so it stays safe even after the
            // internal handler frees it. Registered under m_loop->sync() because
            // Connection state is owned by the event-loop thread.
            m_loop->sync([&] {
                if (client->m_context.connection) {
                    client->m_context.connection->onDisconnect(
                        [on_disconnect = std::move(on_disconnect)]() mutable { InvokeDisconnectCb(on_disconnect); });
                } else {
                    // Early-disconnect race: the peer dropped between ConnectStream()
                    // and here, so libmultiprocess cleanup already nulled the
                    // connection and the onDisconnect above would never register.
                    // Notify now (on the event-loop thread, same as the handler would
                    // run) so the caller still learns the connection is gone.
                    InvokeDisconnectCb(on_disconnect);
                }
            });
        }
        return client;
    }

    void listen(int listen_fd, const char* exe_name, interfaces::MakeServeInitFn make_init,
                std::chrono::seconds auth_deadline) override
    {
        startLoop(exe_name);
        if (::listen(listen_fd, /*backlog=*/5) != 0) {
#ifdef WIN32
            throw std::system_error(::WSAGetLastError(), std::system_category());
#else
            throw std::system_error(errno, std::system_category());
#endif
        }
        // Per-connection authentication: build a FRESH Init for each accepted
        // connection via make_init (which also runs the peer-credential check and
        // returns null to reject a connection before serving). Each connection
        // therefore authenticates on its own -- a new peer cannot ride an earlier
        // peer's authenticated session. The single simultaneous-connection cap
        // remains the v1 "one GUI" model; it is no longer load-bearing for auth.
        // InitImpl (interfaces::Init) is specified explicitly: it cannot be deduced
        // from the lambda, which converts to the std::function<...> parameter only
        // once the parameter type is known.
        // The single slot is why the deadline matters: accept() is not re-armed
        // while it is occupied, so an unauthenticated squatter is a denial of
        // service against the real GUI until something reclaims the slot.
        mp::ListenConnectionsFactory<messages::Init, interfaces::Init>(
            *m_loop, listen_fd,
            [make_init = std::move(make_init)](int peer_fd) -> std::shared_ptr<interfaces::Init> {
                return make_init(peer_fd); // unique_ptr -> shared_ptr; null => reject
            },
            /*max_connections=*/1,
            [](interfaces::Init& init) { return init.isAuthenticated(); },
            auth_deadline);
    }

    void disconnectIncoming() override
    {
        if (!m_loop) return;
        m_loop->sync([&] {
            m_loop->m_incoming_connections.remove_if(
                [this](mp::Connection& c) { return &c != m_parent_connection; });
        });
    }

    void addCleanup(std::type_index type, void* iface, std::function<void()> cleanup) override
    {
        mp::ProxyTypeRegister::types().at(type)(iface).cleanup_fns.emplace_back(std::move(cleanup));
    }

    Context& context() override { return m_context; }

    //! Start the background event-loop thread (idempotent). The loop is
    //! constructed on that thread; a promise makes startLoop() block until it
    //! exists, and an EventLoopRef keeps it alive until the last reference drops.
    void startLoop(const char* exe_name)
    {
        if (m_loop) return;
        std::promise<void> promise;
        m_loop_thread = std::thread([&] {
            try {
                RenameThread("capnp-loop");
                m_loop.emplace(exe_name, mp::LogFn{IpcLogFn}, &m_context);
                m_loop_ref.emplace(*m_loop);
            } catch (...) {
                // Surface a construction failure to the waiting thread instead of
                // escaping std::thread (which would call std::terminate).
                promise.set_exception(std::current_exception());
                return;
            }
            promise.set_value();
            m_loop->loop();
            m_loop.reset();
        });
        // Rethrows here if the loop thread failed to construct the EventLoop, so
        // the caller (IpcImpl / AppInit) can log and continue instead of crashing.
        promise.get_future().get();
    }

    Context m_context;
    std::thread m_loop_thread;
    std::optional<mp::EventLoop> m_loop;
    std::optional<mp::EventLoopRef> m_loop_ref;
    //! Connection to a controlling parent process, if any. Gridcoin never spawns,
    //! so this stays null; kept so disconnectIncoming() reads cleanly.
    mp::Connection* m_parent_connection{nullptr};
};
} // namespace

std::unique_ptr<Protocol> MakeCapnpProtocol() { return std::make_unique<CapnpProtocol>(); }
} // namespace capnp
} // namespace ipc
