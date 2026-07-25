// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "ipc/capnp/protocol.h"
#include "ipc/process.h"
#include "ipc/protocol.h"
#include "util.h"

#include <functional>
#include <memory>
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
        // compat.h closesocket(): close() on POSIX, closesocket() on Windows.
        // Takes a SOCKET& (it zeroes the handle), so pass an lvalue.
        SOCKET s = static_cast<SOCKET>(fd);
        closesocket(s);
    }
    void release() { fd = -1; }
};

class IpcImpl : public interfaces::Ipc
{
public:
    IpcImpl(const char* exe_name, interfaces::Init& init)
        : m_exe_name(exe_name), m_init(init),
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
        auto init = m_protocol->connect(fd, m_exe_name, std::move(on_disconnect));
        guard.release(); // the protocol/stream now owns the fd
        return init;
    }

    void listenAddress(std::string& address) override
    {
        int fd = m_process->bind(GetDataDir(), address);
        FdGuard guard{fd};
        m_protocol->listen(fd, m_exe_name, m_init);
        guard.release(); // the protocol/listener now owns the fd
    }

    void disconnectIncoming() override { m_protocol->disconnectIncoming(); }

    void addCleanup(std::type_index type, void* iface, std::function<void()> cleanup) override
    {
        m_protocol->addCleanup(type, iface, std::move(cleanup));
    }

    Context& context() override { return m_protocol->context(); }

    const char* m_exe_name;
    interfaces::Init& m_init;
    std::unique_ptr<Protocol> m_protocol;
    std::unique_ptr<Process> m_process;
};
} // namespace
} // namespace ipc

namespace interfaces {
std::unique_ptr<Ipc> MakeIpc(const char* exe_name, Init& init)
{
    return std::make_unique<ipc::IpcImpl>(exe_name, init);
}
} // namespace interfaces
