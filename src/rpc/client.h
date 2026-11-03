// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_CLIENT_H
#define BITCOIN_RPC_CLIENT_H

#include <rpc/listener_check.h>
#include <univalue.h>

#include <string>
#include <utility>
#include <vector>

int CommandLineRPC(int argc, char *argv[]);

struct RpcCallOptions {
    //! Runs after the connection is made and before anything is written to it. Refuse closes the
    //! connection and CallRPC throws with the reason. Skipped under -rpcskiplistenercheck.
    ListenerCheckFn listener_check{CheckRpcListener};
};

//! One JSON-RPC call to the server named by -rpcconnect and -rpcport, with the credentials of
//! -rpcuser and -rpcpassword. Returns the reply object (result, error, id).
UniValue CallRPC(const std::string& method, const UniValue& params, const RpcCallOptions& options = {});

//! The CLI should call again: -rpcwait is set and the reply's error code is RPC_IN_WARMUP.
bool ShouldRetryWarmupReply(bool rpcwait, const UniValue& reply);

UniValue RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams);

//! The CLI client's conversion table as (method, 0-based param index) pairs;
//! exposed for the rpchelpman_tests cross-check of the table against each
//! command's declared RPCHelpMan arguments.
std::vector<std::pair<std::string, int>> ListConvertedParams();

#endif // BITCOIN_RPC_CLIENT_H
