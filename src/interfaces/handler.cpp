// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/handler.h"

#include "logging.h"

#include <boost/signals2/connection.hpp>

#include <string>
#include <utility>

namespace interfaces {

void LogDroppedNotification(const char* what)
{
    const std::string msg(what != nullptr ? what : "");

    // The disconnect case is what this guard exists for: a notification to a client
    // that is gone is a no-op by definition, and on a clean shutdown many drop at
    // once, so it is expected rather than an error. Keep those to the IPC category
    // (surfaces with -debug=ipc). The two texts below are what libmultiprocess
    // raises through Gridcoin's IpcLogFn on a dead connection.
    if (msg.find("called after disconnect") != std::string::npos
        || msg.find("interrupted by disconnect") != std::string::npos) {
        LogPrint(BCLog::LogFlags::IPC, "IPC: dropped a notification to a disconnected client: %s", msg);
        return;
    }

    // Anything else is a genuine defect -- a logic error, a bad_alloc, a throwing
    // callback body -- that this guard would otherwise hide. It must NOT be silent:
    // log at the default level so a real bug cannot vanish in production merely
    // because it happened to escape through a notification.
    LogPrintf("WARN: %s: swallowed a non-disconnect exception from a notification callback: %s\n",
              __func__, msg);
}

namespace {

class SignalHandler : public Handler
{
public:
    explicit SignalHandler(boost::signals2::connection connection)
        : m_connection(std::move(connection))
    {
    }

    void disconnect() override { m_connection.disconnect(); }

    boost::signals2::scoped_connection m_connection;
};

class CleanupHandler : public Handler
{
public:
    explicit CleanupHandler(std::function<void()> cleanup)
        : m_cleanup(std::move(cleanup))
    {
    }

    ~CleanupHandler() override
    {
        if (m_cleanup) {
            m_cleanup();
        }
    }

    void disconnect() override
    {
        if (m_cleanup) {
            m_cleanup();
            m_cleanup = nullptr;
        }
    }

    std::function<void()> m_cleanup;
};

} // namespace

std::unique_ptr<Handler> MakeSignalHandler(boost::signals2::connection connection)
{
    return std::make_unique<SignalHandler>(std::move(connection));
}

std::unique_ptr<Handler> MakeCleanupHandler(std::function<void()> cleanup)
{
    return std::make_unique<CleanupHandler>(std::move(cleanup));
}

} // namespace interfaces
