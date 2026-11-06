// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_CLIENT_H
#define BITCOIN_RPC_CLIENT_H

#include <rpc/listener_check.h>
#include <support/allocators/secure.h>
#include <univalue.h>

#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

int CommandLineRPC(int argc, char *argv[]);

struct RpcCallOptions {
    //! Runs after the connection is made and before anything is written to it. Refuse closes the
    //! connection and CallRPC throws with the reason. Skipped under -rpcskiplistenercheck.
    ListenerCheckFn listener_check{CheckRpcListener};
    //! When set, the request carries this wallet passphrase in the X-Gridcoin-Elevate header
    //! (base64), so that this one call may use the wallet's keys while the wallet is unlocked for
    //! staking only. Not owned; CallRPC cleanses its own copies of the encoded value.
    const SecureString* elevate_passphrase{nullptr};
};

//! One line into `out`, without its "\n" and one trailing "\r"; no other trimming; NUL bytes kept.
//! false on end of input before any byte. With drop_leading_bom the first three bytes of the line are
//! dropped when they are a UTF-8 byte-order mark (EF BB BF); only the start of the line is looked at.
bool ReadSecureLine(std::istream& in, SecureString& out, bool drop_leading_bom = false);

//! Params for `method` with the passphrases at their indexes and visible_args filling the other
//! positions in order, converted by RPCConvertValues at their final indexes. Throws
//! std::runtime_error if the method has no passphrase parameters or secrets.size() differs from
//! their number.
UniValue BuildParamsWithSecrets(const std::string& method, const std::vector<std::string>& visible_args,
                                const std::vector<SecureString>& secrets);

//! The longest X-Gridcoin-Elevate value the server's header line bound leaves room for.
static constexpr size_t MAX_ELEVATION_HEADER_VALUE_BYTES{8000};

//! The header value for X-Gridcoin-Elevate: the passphrase in base64 (standard alphabet, padded).
//! nullopt when the passphrase is empty or the encoded value is longer than
//! MAX_ELEVATION_HEADER_VALUE_BYTES.
std::optional<std::string> EncodeElevationHeaderValue(const SecureString& passphrase);

//! The prompt at `index` of a method's prompt list; a generic prompt when the list is shorter, so
//! that a passphrase without a row of its own is still asked for and nothing is read out of range.
std::string PassphrasePromptAt(const std::vector<std::string>& prompts, size_t index);

//! Passphrase positions confirmed by a second prompt when stdin is a terminal:
//! encryptwallet {0}, walletpassphrasechange {1}; empty otherwise, as for walletpassphrase,
//! setautounlock and clearautounlock, whose passphrase the wallet checks.
std::vector<int> NewPassphraseIndexes(const std::string& method);

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
