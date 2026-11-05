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
#include "util/syserror.h"

#include <cerrno>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <typeindex>
#ifndef WIN32
#include <fcntl.h> // F_DUPFD_CLOEXEC
#include <unistd.h>
#else
#include <winsock2.h> // closesocket, WSADuplicateSocketW, WSASocketW
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

//! A second handle on the connected socket \p fd, for ConnectInfo::shutdown_fd,
//! that a child process does not inherit. Returns -1, and logs why, when none can
//! be made.
int DuplicateSocket(int fd)
{
#ifdef WIN32
    WSAPROTOCOL_INFOW protocol_info;
    if (::WSADuplicateSocketW(static_cast<SOCKET>(fd), ::GetCurrentProcessId(), &protocol_info) != 0) {
        LogPrintf("WARN: IPC: could not duplicate the connection's socket (WSA error %d)", ::WSAGetLastError());
        return -1;
    }
    // The flags of every socket this layer makes (MakeCloexecStreamSocket in
    // process.cpp): overlapped, and not inherited by a child process.
    const SOCKET duplicate = ::WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &protocol_info, 0,
                                          WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (duplicate == INVALID_SOCKET) {
        LogPrintf("WARN: IPC: could not duplicate the connection's socket (WSA error %d)", ::WSAGetLastError());
        return -1;
    }
    return static_cast<int>(duplicate);
#else
    const int duplicate = ::fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if (duplicate == -1) {
        LogPrintf("WARN: IPC: could not duplicate the connection's socket: %s", SysErrorString(errno));
    }
    return duplicate;
#endif
}

class IpcImpl : public interfaces::Ipc
{
public:
    IpcImpl(const char* exe_name, interfaces::MakeServeInitFn make_init)
        : m_exe_name(exe_name), m_make_init(std::move(make_init)),
          m_protocol(ipc::capnp::MakeCapnpProtocol()), m_process(ipc::MakeProcess())
    {
    }

    std::unique_ptr<interfaces::Init> connectAddress(std::string& address,
                                                     std::function<void()> on_disconnect,
                                                     interfaces::ConnectInfo* info) override
    {
        if (address.empty() || address == "0") return nullptr;
        int fd;
        if (address == "auto") {
            // "auto": connect if a node is listening, otherwise return null so
            // the caller can report "no daemon" cleanly (Gridcoin does not spawn).
            try {
                fd = m_process->connect(ipc::GetIpcDir(), address);
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
            fd = m_process->connect(ipc::GetIpcDir(), address);
        }
        FdGuard guard{fd};
        // Check the NODE before we speak. The cookie is a bearer token:
        // ClientHandshake's very first act is to hand it to whatever answered on
        // node.sock, so the peer check has to refuse before that, and a peer
        // whose identity cannot be read is refused too. ClientPeerPolicy admits
        // a core running as this account, or the configured -ipcserviceuser
        // account when the socket's directory belongs to that account or root
        // and is not writable by group or others. Two processes of the same
        // account are not told apart by this; the daemon refusing to start when
        // it cannot bind keeps the socket from being left unowned. On Windows,
        // where the peer's identity cannot be read, Process::connect has already
        // checked the socket's owner before connecting, and its directories when
        // another account can write the socket; in the same-account mode a socket
        // swapped in between is for the attestation step to find.
        //
        // Process::connect rewrote address to its canonical "unix:<path>" form,
        // so the socket's directory is the parent of that path.
        const fs::path socket_dir = ipc::SocketDirOf(address);
        if (socket_dir.empty()) {
            throw std::logic_error("Process::connect did not return a unix: address");
        }
        const std::optional<ipc::PeerIdentity> server = ipc::ReadPeerIdentity(fd);
        if (!server || ipc::ClientPeerPolicy(*server, socket_dir) == ipc::PeerVerdict::Refuse) {
            throw interfaces::PeerRefusedError("The process listening on the multiprocess socket belongs to "
                                               "a different OS user, or its owner could not be determined, "
                                               "or it is not the configured service account; refusing to "
                                               "send the authentication cookie to it.");
        }
        if (info) {
#ifndef WIN32
            info->server_other_account = server->uid != ::geteuid();
#else
            info->server_other_account = false;
#endif
        }
        // A second handle on the socket lets the caller shut the connection down
        // from another thread (ConnectToNode's handshake watchdog). shutdown()
        // acts on the socket, not on one handle, and the caller closes this one
        // whatever the protocol does with its own. It is handed over before the
        // protocol's bootstrap round trip below, which a peer that does not
        // answer never completes.
        if (info) {
            info->shutdown_fd = DuplicateSocket(fd);
            if (info->shutdown_fd != -1 && info->on_shutdown_fd) info->on_shutdown_fd(info->shutdown_fd);
        }
        // Protocol::connect owns fd from here, also when it throws.
        guard.release();
        return m_protocol->connect(fd, m_exe_name, std::move(on_disconnect));
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
        int fd = m_process->bind(ipc::GetIpcDir(), address);
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
