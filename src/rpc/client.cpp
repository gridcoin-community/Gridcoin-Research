// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "sync.h"
#include "ui_interface.h"
#include "base58.h"
#include "protocol.h"
#include "client.h"
#include "server.h"
#include "wallet/db.h"
#include "util.h"

// #undef printf
#include <set>
#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/shared_ptr.hpp>
#include <list>

#include <memory>

#define printf OutputDebugStringF

using namespace std;
using namespace boost;
using namespace boost::asio;

UniValue CallRPC(const string& strMethod, const UniValue& params)
{
    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
        throw runtime_error(strprintf(
                                _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
                                  "If the file does not exist, create it with owner-readable-only file permissions."),
                                GetConfigFile().string()));

    // Connect to localhost
    bool fUseSSL = GetBoolArg("-rpcssl");
    ioContext io_context;
    ssl::context context(ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_context, context);
    SSLIOStreamDevice<asio::ip::tcp> d(sslStream, fUseSSL);
    iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);
    if (!d.connect(GetArg("-rpcconnect", "127.0.0.1"), GetArg("-rpcport", ToString(GetDefaultRPCPort()))))
        throw runtime_error("couldn't connect to server");

    // HTTP basic authentication
    string strUserPass64 = EncodeBase64(mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"]);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

    // Send request
    std::string strRequest = JSONRPCRequest(strMethod, params, 1);
    std::string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive HTTP reply status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Receive HTTP reply message headers and body
    map<string, string> mapHeaders;
    string strReply;
    ReadHTTPMessage(stream, mapHeaders, strReply, nProto);

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
    { "createrawtransaction"   , 0 },
    { "createrawtransaction"   , 1 },
    { "consolidatemsunspent"   , 1 },
    { "consolidatemsunspent"   , 2 },
    { "consolidatemsunspent"   , 3 },
    { "consolidatemsunspent"   , 4 },
    { "consolidatemsunspent"   , 5 },
    { "consolidatemsunspent"   , 6 },
    { "consolidateunspent"     , 3 },
    { "consolidateunspent"     , 4 },
    { "getbalance"             , 1 },
    { "getbalance"             , 2 },
    { "getbalancedetail"       , 0 },
    { "getbalancedetail"       , 1 },
    { "getrawtransaction"      , 1 },
    { "getreceivedbyaccount"   , 1 },
    { "getreceivedbyaddress"   , 1 },
    { "gettransaction"         , 1 },
    { "importprivkey"          , 2 },
    { "keypoolrefill"          , 0 },
    { "listaccounts"           , 0 },
    { "listaccounts"           , 1 },
    { "listreceivedbyaccount"  , 0 },
    { "listreceivedbyaccount"  , 1 },
    { "listreceivedbyaccount"  , 2 },
    { "listreceivedbyaddress"  , 0 },
    { "listreceivedbyaddress"  , 1 },
    { "listreceivedbyaddress"  , 2 },
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
    { "reservebalance"         , 0 },
    { "reservebalance"         , 1 },
    { "scanforunspent"         , 1 },
    { "scanforunspent"         , 2 },
    { "scanforunspent"         , 3 },
    { "sendfrom"               , 2 },
    { "sendfrom"               , 3 },
    { "sendmany"               , 1 },
    { "sendmany"               , 2 },
    { "sendtoaddress"          , 1 },
    { "settxfee"               , 0 },
    { "signrawtransaction"     , 1 },
    { "signrawtransaction"     , 2 },
    { "walletpassphrase"       , 1 },
    { "walletpassphrase"       , 2 },

    // Mining
    { "advertisebeacon"        , 0 },
    { "beaconreport"           , 0 },
    { "superblocks"            , 0 },
    { "superblocks"            , 1 },

    // Developer
    { "auditsnapshotaccrual"   , 1 },
    { "auditsnapshotaccruals"  , 0 },
    { "convergencereport"      , 0 },
    { "debug"                  , 0 },
    { "debug10"                , 0 },
    { "debug2"                 , 0 },
    { "dumpcontracts"          , 2 },
    { "dumpcontracts"          , 3 },
    { "getblockstats"          , 0 },
    { "getblockstats"          , 1 },
    { "getblockstats"          , 2 },
    { "inspectaccrualsnapshot" , 0 },
    { "listmanifests"          , 0 },
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
    { "getaddednodeinfo"       , 0 },
    { "getblock"               , 1 },
    { "getblockbynumber"       , 0 },
    { "getblockbynumber"       , 1 },
    { "getblockhash"           , 0 },
    { "setban"                 , 2 },
    { "setban"                 , 3 },
    { "showblock"              , 0 },

    // Voting
    { "addpoll"                , 1 },
    { "addpoll"                , 4 },
    { "addpoll"                , 5 },
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
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
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
        UniValue params = RPCConvertValues(strMethod, strParams);

        // Execute
        const UniValue reply = CallRPC(strMethod, params);

        // Parse reply
        const UniValue& result = find_value(reply, "result");
        const UniValue& error  = find_value(reply, "error");

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
        PrintException(NULL, "CommandLineRPC()");
    }

    if (strPrint != "")
    {
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
    }
    return nRet;
}


