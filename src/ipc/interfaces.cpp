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
#include <utility>

namespace ipc {
namespace {

class IpcImpl : public interfaces::Ipc
{
public:
    IpcImpl(const char* exe_name, interfaces::Init& init)
        : m_exe_name(exe_name), m_init(init),
          m_protocol(ipc::capnp::MakeCapnpProtocol()), m_process(ipc::MakeProcess())
    {
    }

    std::unique_ptr<interfaces::Init> connectAddress(std::string& address) override
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
        return m_protocol->connect(fd, m_exe_name);
    }

    void listenAddress(std::string& address) override
    {
        int fd = m_process->bind(GetDataDir(), address);
        m_protocol->listen(fd, m_exe_name, m_init);
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
