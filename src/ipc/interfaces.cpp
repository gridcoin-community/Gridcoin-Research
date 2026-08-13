// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "ipc/capnp/protocol.h"
#include "ipc/peercred.h"
#include "ipc/process.h"
#include "ipc/protocol.h"
#include "util.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <typeindex>
#ifndef WIN32
#include <unistd.h>
#else
#include <winsock2.h> // closesocket
#endif
#include <utility>

namespace ipc {
namespace {

//! Closes a socket fd on scope exit unless release()d. Protects the window
//! between acquiring the fd (Process::connect/bind) and the protocol layer taking
//! ownership of it -- if the protocol call throws, the fd is not leaked.
struct FdGuard
{
    int fd;
    ~FdGuard()
    {
        if (fd == -1) return;
#ifdef WIN32
        // compat.h closesocket() -> myclosesocket() -> the winsock closesocket();
        // it takes a SOCKET& (zeroes the handle), so pass an lvalue.
        SOCKET s = static_cast<SOCKET>(fd);
        closesocket(s);
#else
        ::close(fd);
#endif
    }
    void release() { fd = -1; }
};

class IpcImpl : public interfaces::Ipc
{
public:
    IpcImpl(const char* exe_name, interfaces::MakeServeInitFn make_init)
        : m_exe_name(exe_name), m_make_init(std::move(make_init)),
          m_protocol(ipc::capnp::MakeCapnpProtocol()), m_process(ipc::MakeProcess())
    {
    }

    std::unique_ptr<interfaces::Init> connectAddress(std::string& address,
                                                     std::function<void()> on_disconnect) override
    {
        if (address.empty() || address == "0") return nullptr;
        int fd;
        if (address == "auto") {
            // "auto": connect if a node is listening, otherwise return null so
            // the caller can report "no daemon" cleanly (Gridcoin does not spawn).
            try {
                fd = m_process->connect(GetDataDir(), address);
            } catch (const std::system_error& e) {
                if (e.code() == std::errc::connection_refused ||
                    e.code() == std::errc::no_such_file_or_directory ||
                    e.code() == std::errc::not_a_directory) {
                    return nullptr;
                }
                throw;
            } catch (const std::invalid_argument&) {
                return nullptr;
            }
        } else {
            fd = m_process->connect(GetDataDir(), address);
        }
        FdGuard guard{fd};
        // Authenticate the NODE to US before we speak. The cookie is a bearer
        // token: ClientHandshake's very first act is to hand it to whatever
        // answered on node.sock. A same-uid process that won the race to bind the
        // socket would otherwise receive it, and on a first connect the GUI would
        // silently bind to the impostor's identity token. Checking the peer's uid
        // here costs one getsockopt and refuses a foreign-uid listener outright.
        // (Same-uid impostors are not excluded by this -- nothing at the OS level
        // can distinguish them -- but a failed listenAddress() is now fatal in the
        // daemon, so the window where node.sock is unowned has been closed too.)
        //
        // ENFORCING. An earlier revision used Advisory here and justified it as
        // defense-in-depth "backstopped by the cookie". That reasoning is circular:
        // the cookie cannot backstop this check, because the cookie is the very
        // thing the next call discloses. ClientHandshake hands it to whatever
        // answered on node.sock, so "we could not determine the peer's uid" has to
        // abort BEFORE the disclosure, not be logged and stepped over. An
        // unverifiable listener is exactly the case where sending a bearer token is
        // least defensible.
        //
        // The cost of Enforcing is bounded: CheckPeerCredentials returns true on
        // WIN32 (no uid to compare), and Darwin was measured to answer on the
        // connecting side, so this refuses only where the platform can answer and
        // the answer is wrong or unobtainable.
        if (!ipc::CheckPeerCredentials(fd, ipc::PeerCredPolicy::Enforcing)) {
            throw std::runtime_error("The process listening on the multiprocess socket belongs to "
                                     "a different OS user, or its owner could not be determined; "
                                     "refusing to send the authentication cookie to it.");
        }
        auto init = m_protocol->connect(fd, m_exe_name, std::move(on_disconnect));
        guard.release(); // the protocol/stream now owns the fd
        return init;
    }

    void listenAddress(std::string& address) override
    {
        // A connect-only Ipc is built with an empty serve-init factory (the GUI).
        // Fail fast here rather than let the empty std::function throw
        // std::bad_function_call deep in the async accept path on first connect.
        if (!m_make_init) {
            throw std::logic_error("Ipc::listenAddress() called without a serve-init factory "
                                   "(this process is connect-only)");
        }
        int fd = m_process->bind(GetDataDir(), address);
        FdGuard guard{fd};
        m_protocol->listen(fd, m_exe_name, m_make_init);
        guard.release(); // the protocol/listener now owns the fd
    }

    void disconnectIncoming() override { m_protocol->disconnectIncoming(); }

    void addCleanup(std::type_index type, void* iface, std::function<void()> cleanup) override
    {
        m_protocol->addCleanup(type, iface, std::move(cleanup));
    }

    Context& context() override { return m_protocol->context(); }

    const char* m_exe_name;
    interfaces::MakeServeInitFn m_make_init;
    std::unique_ptr<Protocol> m_protocol;
    std::unique_ptr<Process> m_process;
};
} // namespace
} // namespace ipc

namespace interfaces {
std::unique_ptr<Ipc> MakeIpc(const char* exe_name, MakeServeInitFn make_serve_init)
{
    return std::make_unique<ipc::IpcImpl>(exe_name, std::move(make_serve_init));
}
} // namespace interfaces
