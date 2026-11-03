// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_RPC_BIND_POLICY_H
#define GRIDCOIN_RPC_BIND_POLICY_H

#include <boost/system/error_code.hpp>

#include <string>
#include <vector>

enum class RpcBindError {
    None,
    Conflict,
    Skip,
    Other,
};

//! Classify a failed RPC listener bind. Conflict is EADDRINUSE (all platforms) and WSAEACCES (Windows).
//! Skip is EADDRNOTAVAIL and EAFNOSUPPORT and their WSA forms. Other is everything else, POSIX EACCES
//! included. The value is compared when the category is the system category. A conflict is only ever
//! reported for an address the host has: binding an address it lacks gives EADDRNOTAVAIL.
RpcBindError ClassifyRpcBindError(const boost::system::error_code& ec);

struct RpcBindAttempt {
    std::string address;                //!< "127.0.0.1:15715", "[::1]:15715"
    bool conflict_disables_rpc{false};  //!< a loopback -rpcbind entry, or any implicit-path bind
    RpcBindError result{RpcBindError::None};
};

enum class RpcListenOutcome {
    Serve,
    DisabledByConflict,
    NothingBound,
};

//! Any conflict_disables_rpc attempt with Conflict gives DisabledByConflict; else any None gives Serve;
//! else any Conflict gives DisabledByConflict; else NothingBound.
RpcListenOutcome DecideRpcListenOutcome(const std::vector<RpcBindAttempt>& attempts);

//! Status-bar text for RPC bind conflicts; empty when none. GetWarnings("statusbar") appends it.
std::string GetRPCBindWarning();

//! Replace the warning text. Guarded by the leaf mutex g_rpc_bind_warning_mutex.
void SetRPCBindWarning(const std::string& text);

#endif // GRIDCOIN_RPC_BIND_POLICY_H
