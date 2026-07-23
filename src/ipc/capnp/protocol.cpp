// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/capnp/protocol.h"

#include "interfaces/init.h"
#include "ipc/capnp/init.capnp.h"
#include "ipc/capnp/init.capnp.proxy.h"
#include "ipc/context.h"
#include "ipc/protocol.h"
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
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <typeindex>

namespace ipc {
namespace capnp {
namespace {

//! Route libmultiprocess log messages into the Gridcoin log. Raise-level
//! messages are turned into exceptions (mp's convention for fatal protocol
//! errors), matching Bitcoin Core's IpcLogFn.
void IpcLogFn(mp::LogMessage message)
{
    if (message.level == mp::Log::Raise) {
        LogPrintf("ipc: %s\n", message.message);
        throw std::runtime_error(message.message);
    }
    LogPrintf("ipc: %s\n", message.message);
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

    std::unique_ptr<interfaces::Init> connect(int fd, const char* exe_name) override
    {
        startLoop(exe_name);
        return mp::ConnectStream<messages::Init>(*m_loop, mp::MakeStream(*m_loop, fd));
    }

    void listen(int listen_fd, const char* exe_name, interfaces::Init& init) override
    {
        startLoop(exe_name);
        if (::listen(listen_fd, /*backlog=*/5) != 0) {
            throw std::system_error(errno, std::system_category());
        }
        mp::ListenConnections<messages::Init>(*m_loop, listen_fd, init);
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
            RenameThread("capnp-loop");
            m_loop.emplace(exe_name, mp::LogFn{IpcLogFn}, &m_context);
            m_loop_ref.emplace(*m_loop);
            promise.set_value();
            m_loop->loop();
            m_loop.reset();
        });
        promise.get_future().wait();
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
