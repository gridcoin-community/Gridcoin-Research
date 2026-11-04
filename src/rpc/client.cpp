// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <compat/stdin.h>
#include <rpc/passphrase_params.h>
#include <support/cleanse.h>
#include "init.h"
#include "sync.h"
#include "node/ui_interface.h"
#include <key_io.h>
#include "protocol.h"
#include "client.h"
#include "server.h"
#include "wallet/db.h"
#include <util.h>

// #undef printf
#include <set>
#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind/bind.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <stdexcept>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <optional>
#include <iostream>
#include <memory>

using namespace std;
using namespace boost;
using namespace boost::asio;

/** The default timeout for an RPC client to wait for the RPC server to start in seconds */
static constexpr int DEFAULT_WAIT_CLIENT_TIMEOUT = 0;

//! The listener check on a connected socket. An endpoint that cannot be read refuses, as does a
//! missing check.
static ListenerCheckResult CheckConnectedListener(const asio::ip::tcp::socket& socket, const ListenerCheckFn& check,
                                                  const std::string& target)
{
    boost::system::error_code local_error;
    boost::system::error_code remote_error;
    const asio::ip::tcp::endpoint local_endpoint = socket.local_endpoint(local_error);
    const asio::ip::tcp::endpoint remote_endpoint = socket.remote_endpoint(remote_error);
    CService local;
    CService remote;
    if (!check || local_error || remote_error || !local.SetSockAddr(local_endpoint.data()) ||
        !remote.SetSockAddr(remote_endpoint.data())) {
        return {ListenerVerdict::Refuse, UnidentifiedListenerMessage(target)};
    }
    return check(local, remote);
}

//! Overwrites the bytes of a string in place; its size is kept.
static void CleanseString(std::string& text)
{
    if (!text.empty()) memory_cleanse(&text[0], text.size());
}

UniValue CallRPC(const string& strMethod, const UniValue& params, const RpcCallOptions& options)
{
    if (!gArgs.IsArgSet("-rpcuser") || !gArgs.IsArgSet("-rpcpassword"))
        throw runtime_error(strprintf(
                                _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
                                  "If the file does not exist, create it with owner-readable-only file permissions."),
                                GetConfigFile().string()));

    // -rpcssl is gone, and this path never reaches StartRPCThreads(), so the
    // warning the daemon logs for it is never seen by someone running the CLI.
    // Say it here too: an operator who believes this connection is wrapped in
    // TLS is about to send Basic-auth credentials in clear text.
    //
    // Loopback only warns. Crossing a network refuses outright -- that is where
    // the credentials are actually exposed, and continuing silently would be a
    // downgrade the operator did not ask for and cannot see.
    // Connect to localhost
    ioContext io_context;
    asio::ip::tcp::socket socket(io_context);
    SocketIOStreamDevice<asio::ip::tcp> d(socket);
    iostreams::stream< SocketIOStreamDevice<asio::ip::tcp> > stream(d);

    bool fWait = gArgs.GetBoolArg("-rpcwait", false); // -rpcwait means try until server has started or timeout is reached
    int timeout = gArgs.GetArg("-rpcwaittimeout", DEFAULT_WAIT_CLIENT_TIMEOUT); // The max time to wait

    std::chrono::seconds deadline = GetTime<std::chrono::seconds>() + std::chrono::seconds{timeout};

    const std::string rpc_host = gArgs.GetArg("-rpcconnect", "127.0.0.1");
    const std::string rpc_port = gArgs.GetArg("-rpcport", ToString(GetDefaultRPCPort()));

    // GetBoolArg, not IsArgSet: IsArgSet is true for "rpcssl=0" and for
    // "-norpcssl", configurations that explicitly turned TLS OFF, and treating
    // those as a request for encryption would refuse remote calls from
    // operators who never wanted it. Presence is not intent.
    const bool ssl_requested = gArgs.GetBoolArg("-rpcssl", false);

    do {
        // Resolve ONCE per attempt, and connect to exactly what was resolved.
        //
        // -rpcssl is gone, and this path never reaches StartRPCThreads(), so the
        // warning the daemon logs for it is never seen here. Someone running the
        // CLI with it set believes this connection is encrypted while Basic-auth
        // credentials go out in clear text. On loopback that warns; to anything
        // else it refuses, since that is where the credentials cross a network.
        //
        // The endpoints are handed to connect() rather than re-resolved inside
        // it. Checking one resolution and connecting on another leaves a window
        // where rotation between the two lookups passes the check with loopback
        // and then connects somewhere else.
        boost::asio::ip::tcp::resolver::results_type endpoints;
        try {
            asio::ip::tcp::resolver res(io_context);
            endpoints = res.resolve(rpc_host, rpc_port);
        } catch (const std::exception&) {
            endpoints = {};
        }

        if (ssl_requested) {
            bool loopback = endpoints.begin() != endpoints.end();
            for (const auto& ep : endpoints) {
                if (!ep.endpoint().address().is_loopback()) { loopback = false; break; }
            }

            if (loopback) {
                LogPrintf("WARNING: -rpcssl is no longer supported and is being ignored. This "
                          "connection to %s is NOT encrypted.\n", rpc_host);
            } else {
                throw std::runtime_error(strprintf(
                    _("-rpcssl is no longer supported, and -rpcconnect=%s does not resolve to a "
                      "loopback address. Sending RPC credentials to it would transmit them "
                      "unencrypted. Remove -rpcssl to proceed deliberately, and tunnel the "
                      "connection (for example over SSH) if it crosses an untrusted network."),
                    rpc_host));
            }
        }

        // If connection succeeds, immediately break. No need to wait.
        if (d.connect(endpoints)) {
            break;
        }

        std::chrono::seconds now = GetTime<std::chrono::seconds>();

        // Note that timeout <= 0 means wait until connected with one second between connection attempts.
        if (fWait && (timeout <= 0 || now < deadline)) {
            UninterruptibleSleep(std::chrono::seconds{1});
        } else {
            throw runtime_error("couldn't connect to server");
        }
    } while (true);

    // The request below carries the RPC password, so identify who holds the listener before
    // anything is written to the connection. A refusal is final: CommandLineRPC does not call
    // again under -rpcwait.
    if (gArgs.GetBoolArg("-rpcskiplistenercheck", false)) {
        static std::atomic<bool> warned{false};
        if (!warned.exchange(true)) {
            tfm::format(std::cerr, "warning: -rpcskiplistenercheck is set; the RPC listener's owner is not checked\n");
        }
    } else {
        const ListenerCheckResult check = CheckConnectedListener(socket, options.listener_check, rpc_host + ":" + rpc_port);
        if (check.verdict != ListenerVerdict::Accept) {
            boost::system::error_code ignored;
            socket.close(ignored);
            throw std::runtime_error(check.reason);
        }
    }

    // HTTP basic authentication
    string strUserPass64 = EncodeBase64(gArgs.GetArg("-rpcuser", "dummy") + ":" + gArgs.GetArg("-rpcpassword", "dummy"));
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

    // The elevation header carries the passphrase, and the request body the passphrase
    // parameters, so every buffer that holds either is overwritten once the request is sent,
    // however this function leaves.
    std::string strRequest;
    std::string strPost;

    struct RequestCleanser {
        std::string& request;
        std::string& post;
        std::map<std::string, std::string>& headers;

        ~RequestCleanser()
        {
            CleanseString(request);
            CleanseString(post);
            for (auto& header : headers) CleanseString(header.second);
        }
    } request_cleanser{strRequest, strPost, mapRequestHeaders};

    if (options.elevate_passphrase != nullptr) {
        std::optional<std::string> elevation = EncodeElevationHeaderValue(*options.elevate_passphrase);
        if (!elevation) {
            throw std::runtime_error("the passphrase cannot be sent in a request header");
        }
        mapRequestHeaders["X-Gridcoin-Elevate"] = *elevation;
        CleanseString(*elevation);
    }

    // Send request
    strRequest = JSONRPCRequest(strMethod, params, 1);
    strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    CleanseString(strRequest);
    CleanseString(strPost);
    for (auto& header : mapRequestHeaders) CleanseString(header.second);

    // Receive HTTP reply status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Receive HTTP reply message headers and body
    map<string, string> mapHeaders;
    string strReply;
    ReadHTTPMessage(stream, mapHeaders, strReply, nProto, MAX_RPC_RESPONSE_SIZE);

    if (nStatus == HTTP_UNAUTHORIZED)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");

    // Parse reply
    UniValue valReply(UniValue::VSTR);
    if (!valReply.read(strReply))
        throw runtime_error("couldn't parse reply from server");
    const UniValue& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    return reply;
}

class CRPCConvertParam
{
public:
    std::string methodName;            // method whose params want conversion
    int paramIdx;                      // 0-based idx of param to convert
};

static const CRPCConvertParam vRPCConvertParams[] =
{
    // Special case non-string parameter types
    //
    // Wallet
    { "addmultisigaddress"     , 0 },
    { "addmultisigaddress"     , 1 },
    { "burn"                   , 0 },
    { "claimhtlc"              , 1 },
    { "createhtlc"             , 3 },
    { "createhtlc"             , 4 },
    { "generatesuperblock"     , 0 },
    { "generatesuperblock"     , 1 },
    { "generatesuperblock"     , 2 },
    { "createrawtransaction"   , 0 },
    { "createrawtransaction"   , 1 },
    { "consolidatemsunspent"   , 1 },
    { "consolidatemsunspent"   , 2 },
    { "consolidatemsunspent"   , 3 },
    { "consolidatemsunspent"   , 4 },
    { "consolidateunspent"     , 3 },
    { "consolidateunspent"     , 4 },
    { "dumpprivkey"            , 1 },
    { "fundrawtransaction"     , 1 },
    { "getbalance"             , 1 },
    { "getbalance"             , 2 },
    { "getbalancedetail"       , 0 },
    { "getbalancedetail"       , 1 },
    { "getrawtransaction"      , 1 },
    { "gettxoutproof"          , 0 },
    { "getreceivedbyaccount"   , 1 },
    { "getreceivedbyaddress"   , 1 },
    { "getreceivedbylabel"     , 1 },
    { "gettransaction"         , 1 },
    { "importprivkey"          , 2 },
    { "inspectwalletstate"     , 0 },
    { "keypoolrefill"          , 0 },
    { "listaccounts"           , 0 },
    { "listaccounts"           , 1 },
    { "listreceivedbyaccount"  , 0 },
    { "listreceivedbyaccount"  , 1 },
    { "listreceivedbyaccount"  , 2 },
    { "listreceivedbyaddress"  , 0 },
    { "listreceivedbyaddress"  , 1 },
    { "listreceivedbyaddress"  , 2 },
    { "listreceivedbylabel"    , 0 },
    { "listreceivedbylabel"    , 1 },
    { "listreceivedbylabel"    , 2 },
    { "listsinceblock"         , 1 },
    { "listsinceblock"         , 2 },
    { "liststakes"             , 0 },
    { "listtransactions"       , 1 },
    { "listtransactions"       , 2 },
    { "listtransactions"       , 3 },
    { "listunspent"            , 0 },
    { "listunspent"            , 1 },
    { "listunspent"            , 2 },
    { "consolidateunspent"     , 1 },
    { "consolidateunspent"     , 2 },
    { "maintainbackups"        , 0 },
    { "maintainbackups"        , 1 },
    { "move"                   , 2 },
    { "move"                   , 3 },
    { "rainbymagnitude"        , 1 },
    { "rainbymagnitude"        , 2 },
    { "rainbymagnitude"        , 3 },
    { "refundhtlc"             , 1 },
    { "reservebalance"         , 0 },
    { "reservebalance"         , 1 },
    { "restoreseedphrase"      , 2 },
    { "scanforunspent"         , 1 },
    { "scanforunspent"         , 2 },
    { "scanforunspent"         , 3 },
    { "sendfrom"               , 2 },
    { "sendfrom"               , 3 },
    { "sendmany"               , 1 },
    { "sendmany"               , 2 },
    { "sendtoaddress"          , 1 },
    { "settxfee"               , 0 },
    { "sweepuncoveredcoins"    , 1 },
    { "sweepuncoveredcoins"    , 2 },
    { "sethdseed"              , 0 },
    { "splitunspent"           , 1 },
    { "splitunspent"           , 2 },
    { "stakelimit"             , 0 },
    { "signrawtransaction"              , 1 },
    { "signrawtransaction"              , 2 },
    { "signrawtransactionwithkey"       , 1 },
    { "signrawtransactionwithkey"       , 2 },
    { "signrawtransactionwithwallet"    , 1 },
    { "createpsgt"                      , 0 },
    { "createpsgt"                      , 1 },
    { "createpsgt"                      , 2 },
    { "combinepsgt"                     , 0 },
    { "walletcreatefundedpsgt"          , 0 },
    { "walletcreatefundedpsgt"          , 1 },
    { "walletcreatefundedpsgt"          , 2 },
    { "walletcreatefundedpsgt"          , 3 },
    { "listpsgtpool"                    , 0 },
    { "upgradewallet"          , 0 },
    { "walletpassphrase"       , 1 },
    { "walletpassphrase"       , 2 },

    // Staking
    { "advertisebeacon"        , 0 },
    { "advertisebeaconv3"      , 1 },
    { "beaconreport"           , 0 },
    { "createmrcrequest"       , 0 },
    { "createmrcrequest"       , 1 },
    { "createmrcrequest"       , 2 },
    { "generate"               , 0 },
    { "generatetoaddress"      , 0 },
    { "getmrcinfo"             , 0 },
    { "getmrcinfo"             , 2 },
    { "getmrcinfo"             , 3 },
    { "superblocks"            , 0 },
    { "superblocks"            , 1 },

    // Developer
    { "auditsnapshotaccrual"   , 1 },
    { "auditsnapshotaccruals"  , 0 },
    { "beaconaudit"            , 0 },
    { "convergencereport"      , 0 },
    { "debug"                  , 0 },
    { "dumpcontracts"          , 2 },
    { "dumpcontracts"          , 3 },
    { "dumpcontracts"          , 4 },
    { "getblockstats"          , 0 },
    { "getblockstats"          , 1 },
    { "getblockstats"          , 2 },
    { "inspectaccrualsnapshot" , 0 },
    { "listmanifests"          , 0 },
    { "listpools"              , 0 },
    { "listprojects"           , 0 },
    { "getautogreylist"        , 0 },
    { "getautogreylist"        , 1 },
    { "sendalert"              , 2 },
    { "sendalert"              , 3 },
    { "sendalert"              , 4 },
    { "sendalert"              , 5 },
    { "sendalert"              , 6 },
    { "sendalert2"             , 1 },
    { "sendalert2"             , 4 },
    { "sendalert2"             , 5 },
    { "testnewsb"              , 0 },
    { "versionreport"          , 0 },
    { "versionreport"          , 1 },

    // Network
    { "getrawmempool"          , 0 },
    { "testmempoolaccept"      , 0 },
    { "getaddednodeinfo"       , 0 },
    { "getnodeaddresses"       , 0 },
    { "getblock"               , 1 },
    { "getblockbynumber"       , 0 },
    { "getblockbynumber"       , 1 },
    { "getblockbymintime"      , 0 },
    { "getblockbymintime"      , 1 },
    { "getblocksbatch"         , 1 },
    { "getblocksbatch"         , 2 },
    { "getblockhash"           , 0 },
    { "setmocktime"            , 0 },
    { "disconnectnode"         , 1 },
    { "setban"                 , 2 },
    { "setban"                 , 3 },
    { "showblock"              , 0 },

    // Voting
    { "addpoll"                , 2 },
    { "addpoll"                , 5 },
    { "addpoll"                , 6 },
    { "listpolls"              , 0 },
    { "votebyid"               , 1 },
    { "votebyid"               , 2 },
    { "votebyid"               , 3 },
    { "votebyid"               , 4 },
    { "votebyid"               , 5 },
    { "votebyid"               , 6 },
    { "votebyid"               , 7 },
    { "votebyid"               , 8 },
    { "votebyid"               , 9 },
    { "votebyid"               , 10 },
    { "votebyid"               , 11 },
    { "votebyid"               , 12 },
    { "votebyid"               , 13 },
    { "votebyid"               , 14 },
    { "votebyid"               , 15 },
    { "votebyid"               , 16 },
    { "votebyid"               , 17 },
    { "votebyid"               , 18 },
    { "votebyid"               , 19 },
    { "votebyid"               , 20 },
    { "votebyid"               , 21 },
};

std::vector<std::pair<std::string, int>> ListConvertedParams()
{
    std::vector<std::pair<std::string, int>> entries;
    for (const auto& elem : vRPCConvertParams) {
        entries.emplace_back(elem.methodName, elem.paramIdx);
    }
    return entries;
}

class CRPCConvertTable
{
private:
    std::set<std::pair<std::string, int> > members;

public:
    CRPCConvertTable();

    bool convert(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
};

CRPCConvertTable::CRPCConvertTable()
{
    for (const auto& elem : vRPCConvertParams) {
        members.insert(std::make_pair(elem.methodName,
                                      elem.paramIdx));
    }
}

static CRPCConvertTable rpcCvtTable;

/** Non-RFC4627 JSON parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
UniValue ParseNonRFCJSONValue(const std::string& strVal)
{
    UniValue jVal;
    if (!jVal.read(std::string("[")+strVal+std::string("]")) ||
        !jVal.isArray() || jVal.size()!=1)
        throw runtime_error(string("Error parsing JSON:")+strVal);
    return jVal[0];
}

bool ShouldRetryWarmupReply(bool rpcwait, const UniValue& reply)
{
    if (!rpcwait || !reply.isObject()) return false;
    // Held by value, since GCC's -Wdangling-reference cannot tell that find_value returns a
    // reference into its object and not into the temporary key string.
    const UniValue error = find_value(reply, "error");
    if (!error.isObject()) return false;
    const UniValue code = find_value(error, "code");
    return code.isNum() && code.get_int() == RPC_IN_WARMUP;
}

// Convert strings to command-specific RPC representation
UniValue RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VARR);

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string& strVal = strParams[idx];

        if (!rpcCvtTable.convert(strMethod, idx)) {
            // insert string value directly
            params.push_back(strVal);
        }

        // parse string as JSON, insert bool/number/object/etc. value
        else {
            params.push_back(ParseNonRFCJSONValue(strVal));
        }

    }
    return params;
}

bool ReadSecureLine(std::istream& in, SecureString& out)
{
    out.clear();

    // Reserved past the short-string capacity first, so that even a short passphrase is held in
    // the secure allocator's memory rather than inside the string object, where it would never
    // be cleansed.
    if (out.capacity() < 100) out.reserve(100);

    bool any_byte{false};

    for (;;) {
        const std::istream::int_type c = in.get();

        if (c == std::istream::traits_type::eof()) return any_byte;

        any_byte = true;

        if (c == '\n') {
            if (!out.empty() && out.back() == '\r') out.pop_back();

            return true;
        }

        out.push_back(std::istream::traits_type::to_char_type(c));
    }
}

UniValue BuildParamsWithSecrets(const std::string& method, const std::vector<std::string>& visible_args,
                                const std::vector<SecureString>& secrets)
{
    const std::vector<int>* indexes = PassphraseParamIndexes(method);

    if (indexes == nullptr) {
        throw std::runtime_error(strprintf("%s has no passphrase parameters", method));
    }

    if (secrets.size() != indexes->size()) {
        throw std::runtime_error(strprintf("%s takes %u passphrase(s), got %u", method, indexes->size(), secrets.size()));
    }

    // The positions of the passphrases hold placeholders while the other arguments are converted
    // at their final indexes, then the passphrases take the placeholders' places.
    const size_t total = visible_args.size() + secrets.size();
    std::vector<std::string> slots;
    slots.reserve(total);

    size_t next_visible{0};

    for (size_t position = 0; position < total; ++position) {
        const bool is_secret = std::find(indexes->begin(), indexes->end(), static_cast<int>(position)) != indexes->end();

        slots.push_back(is_secret ? std::string{} : visible_args[next_visible++]);
    }

    const UniValue converted = RPCConvertValues(method, slots);

    UniValue params(UniValue::VARR);
    size_t next_secret{0};

    for (size_t position = 0; position < total; ++position) {
        const bool is_secret = std::find(indexes->begin(), indexes->end(), static_cast<int>(position)) != indexes->end();

        if (!is_secret) {
            params.push_back(converted[position]);
            continue;
        }

        const SecureString& secret = secrets[next_secret++];
        // The passphrase is copied into a named string and a named value, both overwritten once
        // pushed. The univalue constructor and push_back copy it again into temporaries that this
        // code cannot reach; the copy kept in params is overwritten later by
        // PassphraseParamsCleanser.
        std::string copy(secret.begin(), secret.end());
        UniValue value(UniValue::VSTR, copy);
        params.push_back(value);
        CleanseString(copy);
        const std::string& text = value.get_str();
        if (!text.empty()) memory_cleanse(const_cast<char*>(text.data()), text.size());
    }

    return params;
}

std::optional<std::string> EncodeElevationHeaderValue(const SecureString& passphrase)
{
    if (passphrase.empty()) return std::nullopt;

    // Checked before encoding, so that an oversized passphrase is not copied at all.
    if ((passphrase.size() + 2) / 3 * 4 > MAX_ELEVATION_HEADER_VALUE_BYTES) return std::nullopt;

    return EncodeBase64(reinterpret_cast<const unsigned char*>(passphrase.data()), passphrase.size());
}

std::vector<int> NewPassphraseIndexes(const std::string& method)
{
    if (method == "encryptwallet") return {0};
    if (method == "walletpassphrasechange") return {1};

    return {};
}

std::string PassphrasePromptAt(const std::vector<std::string>& prompts, size_t index)
{
    return index < prompts.size() ? prompts[index] : std::string{"Wallet passphrase> "};
}

//! The prompts of a method's passphrases, in the order of PassphraseParamIndexes.
static std::vector<std::string> PassphrasePrompts(const std::string& method)
{
    if (method == "walletpassphrasechange") return {"Old wallet passphrase> ", "New wallet passphrase> "};
    if (method == "encryptwallet") return {"New wallet passphrase> "};

    return {"Wallet passphrase> "};
}

//! One line from standard input into `out` with echo off, prompting on stderr when a person is
//! at the terminal. false at end of input.
static bool PromptSecretLine(const std::string& prompt, SecureString& out)
{
    // The C stream under std::cin is made unbuffered before the first passphrase is read, so
    // that its buffer keeps no copy of the line. Changing the buffering is allowed only before
    // the first read.
    static bool unbuffered{false};
    if (!unbuffered) {
        std::setvbuf(stdin, nullptr, _IONBF, 0);
        unbuffered = true;
    }

    const bool prompting = !StdinReady();

    if (prompting) {
        tfm::format(std::cerr, "%s", prompt);
        std::cerr.flush();
    }

    bool got;
    {
        NO_STDIN_ECHO();
        got = ReadSecureLine(std::cin, out);
    }

    // Echo was off, so the person's Enter did not move the cursor.
    if (prompting) tfm::format(std::cerr, "\n");

    return got;
}

//! Reads the passphrases of `method` from standard input, one line each. When standard input is
//! a terminal the new passphrases are asked for twice. Throws the pinned error text on a short
//! input or a mismatch.
static std::vector<SecureString> ReadMethodPassphrases(const std::string& method)
{
    const std::vector<int>& indexes = *PassphraseParamIndexes(method);
    const std::vector<std::string> prompts = PassphrasePrompts(method);
    const std::vector<int> confirmed = StdinTerminal() ? NewPassphraseIndexes(method) : std::vector<int>{};

    std::vector<SecureString> secrets;
    secrets.reserve(indexes.size());

    for (size_t i = 0; i < indexes.size(); ++i) {
        SecureString line;

        if (!PromptSecretLine(PassphrasePromptAt(prompts, i), line)) {
            throw std::runtime_error(strprintf("expected %u passphrase line(s) on standard input, got %u",
                                               indexes.size(), secrets.size()));
        }

        if (std::find(confirmed.begin(), confirmed.end(), indexes[i]) != confirmed.end()) {
            SecureString again;

            if (!PromptSecretLine("Repeat new wallet passphrase> ", again) || again != line) {
                throw std::runtime_error("the two entries of the new passphrase do not match; nothing was sent");
            }
        }

        secrets.push_back(std::move(line));
    }

    return secrets;
}

int CommandLineRPC(int argc, char *argv[])
{
    string strPrint;
    int nRet = 0;
    try
    {
        // Skip switches
        while (argc > 1 && IsSwitchChar(argv[1][0]))
        {
            argc--;
            argv++;
        }

        // Method
        if (argc < 2)
            throw runtime_error("too few parameters");
        string strMethod = argv[1];

        // Parameters default to strings
        std::vector<std::string> strParams(&argv[2], &argv[argc]);

        const bool stdin_passphrase = gArgs.GetBoolArg("-stdinwalletpassphrase", false);
        const bool stdin_elevate = gArgs.GetBoolArg("-stdinelevate", false);
        const bool passphrase_method = PassphraseParamIndexes(strMethod) != nullptr;

        if (stdin_passphrase && !passphrase_method) {
            throw runtime_error("-stdinwalletpassphrase applies only to walletpassphrase, walletpassphrasechange and encryptwallet");
        }

        if (stdin_elevate && passphrase_method) {
            throw runtime_error(strprintf("-stdinelevate is not used with %s; give its passphrases with -stdinwalletpassphrase",
                                          strMethod));
        }

        // Everything is read from standard input before the connection is made.
        SecureString elevate_passphrase;
        RpcCallOptions call_options;

        UniValue params;

        if (stdin_passphrase) {
            std::vector<SecureString> secrets = ReadMethodPassphrases(strMethod);
            params = BuildParamsWithSecrets(strMethod, strParams, secrets);
        } else {
            if (passphrase_method && !strParams.empty()) {
                tfm::format(std::cerr, "warning: the passphrase was given on the command line, where other users and "
                                       "the shell history can see it; use -stdinwalletpassphrase\n");
            }

            params = RPCConvertValues(strMethod, strParams);
        }

        // Cleanses the passphrases in the params however this scope is left.
        const PassphraseParamsCleanser params_cleanser(strMethod, params);

        if (stdin_elevate) {
            if (!PromptSecretLine("Wallet passphrase for this request> ", elevate_passphrase)) {
                throw runtime_error("expected 1 passphrase line(s) on standard input, got 0");
            }

            std::optional<std::string> encoded = EncodeElevationHeaderValue(elevate_passphrase);

            if (!encoded) {
                throw runtime_error("this passphrase cannot be sent in a request header (it is empty or longer than "
                                    "what a header line holds); unlock with walletpassphrase instead");
            }

            CleanseString(*encoded);
            call_options.elevate_passphrase = &elevate_passphrase;
        }

        // Execute
        // Under -rpcwait a daemon that is still initialising answers RPC_IN_WARMUP; call again
        // once a second, within the same -rpcwaittimeout that bounds the wait for the connection
        // (0 or less waits without limit).
        const bool rpcwait = gArgs.GetBoolArg("-rpcwait", false);
        const int wait_timeout = gArgs.GetArg("-rpcwaittimeout", DEFAULT_WAIT_CLIENT_TIMEOUT);
        const std::chrono::seconds warmup_deadline = GetTime<std::chrono::seconds>() + std::chrono::seconds{wait_timeout};
        UniValue reply = CallRPC(strMethod, params, call_options);
        while (ShouldRetryWarmupReply(rpcwait, reply) &&
               (wait_timeout <= 0 || GetTime<std::chrono::seconds>() < warmup_deadline)) {
            UninterruptibleSleep(std::chrono::seconds{1});
            reply = CallRPC(strMethod, params, call_options);
        }

        // Parse reply
        UniValue result = find_value(reply, "result");
        UniValue error  = find_value(reply, "error");

        if (!error.isNull())
        {
            // Error
            strPrint = "error: " + error.write();
            int code = find_value(error.get_obj(), "code").get_int();
            nRet = abs(code);
        }
        else
        {
            // Result
            if (result.isNull())
                strPrint = "";
            else if (result.isStr())
                strPrint = result.get_str();
            else
                strPrint = result.write(2);
        }
    }
    catch (std::exception& e)
    {
        strPrint = string("error: ") + e.what();
        nRet = 87;
    }
    catch (...)
    {
        PrintException(nullptr, "CommandLineRPC()");
    }

    if (strPrint != "")
    {
        tfm::format(nRet == 0 ? std::cout : std::cerr, "%s\n", strPrint.c_str());
    }
    return nRet;
}


