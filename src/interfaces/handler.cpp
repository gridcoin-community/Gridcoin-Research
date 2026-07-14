// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/handler.h"

#include <boost/signals2/connection.hpp>

#include <utility>

namespace interfaces {
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
