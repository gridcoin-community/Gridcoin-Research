// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_CLIENT_H
#define BITCOIN_RPC_CLIENT_H

#include <univalue.h>

#include <string>
#include <utility>
#include <vector>

int CommandLineRPC(int argc, char *argv[]);

UniValue RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams);

//! The CLI client's conversion table as (method, 0-based param index) pairs;
//! exposed for the rpchelpman_tests cross-check of the table against each
//! command's declared RPCHelpMan arguments.
std::vector<std::pair<std::string, int>> ListConvertedParams();

#endif // BITCOIN_RPC_CLIENT_H
