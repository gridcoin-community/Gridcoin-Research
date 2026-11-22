// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/bind_policy.h>

#include <algorithm>
#include <mutex>

#ifdef WIN32
#include <winsock2.h>
#else
#include <cerrno>
#endif

// Leaf mutex. Held only to copy or replace the text, never across a call into another subsystem.
static std::mutex g_rpc_bind_warning_mutex;
static std::string g_rpc_bind_warning;

RpcBindError ClassifyRpcBindError(const boost::system::error_code& ec)
{
    if (!ec) return RpcBindError::None;
    if (ec.category() != boost::system::system_category()) return RpcBindError::Other;

    const int value = ec.value();
#ifdef WIN32
    // WSAEACCES is how Windows reports an address held with SO_EXCLUSIVEADDRUSE.
    if (value == WSAEADDRINUSE || value == WSAEACCES) return RpcBindError::Conflict;
    if (value == WSAEADDRNOTAVAIL || value == WSAEAFNOSUPPORT) return RpcBindError::Skip;
#else
    // POSIX EACCES is a privileged port or a policy denial, not another listener.
    if (value == EADDRINUSE) return RpcBindError::Conflict;
    if (value == EADDRNOTAVAIL || value == EAFNOSUPPORT) return RpcBindError::Skip;
#endif
    return RpcBindError::Other;
}

RpcListenOutcome DecideRpcListenOutcome(const std::vector<RpcBindAttempt>& attempts)
{
    const auto has = [&attempts](auto pred) { return std::any_of(attempts.begin(), attempts.end(), pred); };

    if (has([](const RpcBindAttempt& a) { return a.conflict_disables_rpc && a.result == RpcBindError::Conflict; })) {
        return RpcListenOutcome::DisabledByConflict;
    }
    if (has([](const RpcBindAttempt& a) { return a.result == RpcBindError::None; })) {
        return RpcListenOutcome::Serve;
    }
    if (has([](const RpcBindAttempt& a) { return a.result == RpcBindError::Conflict; })) {
        return RpcListenOutcome::DisabledByConflict;
    }
    return RpcListenOutcome::NothingBound;
}

std::string GetRPCBindWarning()
{
    std::lock_guard<std::mutex> lock(g_rpc_bind_warning_mutex);
    return g_rpc_bind_warning;
}

void SetRPCBindWarning(const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_rpc_bind_warning_mutex);
    g_rpc_bind_warning = text;
}
