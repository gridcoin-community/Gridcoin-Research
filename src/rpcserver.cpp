// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "sync.h"
#include "ui_interface.h"
#include "base58.h"
#include "rpcserver.h"
#include "rpcclient.h"
#include "rpcprotocol.h"
#include "db.h"
#include "util.h"

#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/shared_ptr.hpp>
#include <list>

#include <memory>

using namespace std;
using namespace boost;
using namespace boost::asio;

void ThreadRPCServer2(void* parg);

static std::string strRPCUserColonPass;

// These are created by StartRPCThreads, destroyed in StopRPCThreads
static asio::io_service* rpc_io_service = NULL;
static ssl::context* rpc_ssl_context = NULL;
static boost::thread_group* rpc_worker_group = NULL;

const UniValue emptyobj(UniValue::VOBJ);

unsigned short GetDefaultRPCPort()
{
    return GetBoolArg("-testnet", false) ? 25715 : 15715;
}

void RPCTypeCheck(const UniValue& params,
                  const list<UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    for (UniValue::VType t : typesExpected)
    {
        if (params.size() <= i)
            break;

        const UniValue& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.isNull()))))
        {
            string err = strprintf("Expected type %s, got %s",
                                   uvTypeName(t), uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheckObj(const UniValue& o,
                  const map<string, UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    for (auto const& t : typesExpected)
    {
        const UniValue& v = find_value(o, t.first);
        if (!fAllowNull && v.isNull())
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first));

        if (!( v.type() == t.second || (fAllowNull && (v.isNull()))))
        {
            string err = strprintf("Expected type %s for %s, got %s",
                                   uvTypeName(t.second), t.first, uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

bool HTTPAuthorized(map<string, string>& mapHeaders)
{
    string strAuth = mapHeaders["authorization"];
    if (strAuth.substr(0,6) != "Basic ")
        return false;
    string strUserPass64 = strAuth.substr(6); boost::trim(strUserPass64);
    string strUserPass = DecodeBase64(strUserPass64);
    return TimingResistantEqual(strUserPass, strRPCUserColonPass);
}

int64_t AmountFromValue(const UniValue& value)
{
    double dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > MAX_MONEY)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64_t nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

UniValue ValueFromAmount(int64_t amount)
{
    return (double)amount / (double)COIN;
}


//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
uint256 ParseHashV(const UniValue& v, string strName)
{
    string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    uint256 result;
    result.SetHex(strHex);
    return result;
}

uint256 ParseHashO(const UniValue& o, string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}

vector<unsigned char> ParseHexV(const UniValue& v, string strName)
{
    string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}

vector<unsigned char> ParseHexO(const UniValue& o, string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}


///
/// Note: This interface may still be subject to change.
///

string CRPCTable::help(string strCommand, rpccategory category) const
{
    string strRet;
    set<rpcfn_type> setDone;
    for (map<string, const CRPCCommand*>::const_iterator mi = mapCommands.begin(); mi != mapCommands.end(); ++mi)
    {
        const CRPCCommand *pcmd = mi->second;
        string strMethod = mi->first;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != string::npos)
            continue;
        // Refactored rules for supporting of subcategories
        if (pcmd->category == cat_null)
            continue;

        if (strCommand.empty() && pcmd->category != category)
            continue;

        if (!strCommand.empty() && pcmd->name != strCommand)
            continue;

        try
        {
            UniValue params(UniValue::VARR);
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
            if (strCommand.empty())
                if (strHelp.find('\n') != string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));
            strRet += strHelp + "\n";
        }
    }
    if (strRet.empty())
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

UniValue help(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() == 0 || params.size() > 1)
        return
            "help [command/category]\n"
            "Returns help on a specific command or category you request\n"
            "\n"
            "help command --> Returns help for specified command; ex. help backupwallet\n"
            "\n"
            "Categories:\n"
            "wallet --------> Returns help for blockchain related commands\n"
            "mining --------> Returns help for neural network/cpid/beacon related commands\n"
            "developer -----> Returns help for developer commands\n"
            "network -------> Returns help for network related commands\n";

    // Allow to process through if params size is > 0
    string strCommand;

    if (params.size() > 0)
        strCommand = params[0].get_str();

    // Subcategory help area
    // Blockchain related commands
    rpccategory category;

    if (strCommand == "wallet")
        category = cat_wallet;

    else if (strCommand == "mining")
        category = cat_mining;

    else if (strCommand == "developer")
        category = cat_developer;

    else if (strCommand == "network")
        category = cat_network;

    else
        category = cat_null;

    if (category != cat_null)
        strCommand = "";

    return tableRPC.help(strCommand, category);
}

UniValue stop(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "stop\n"
            "Stop Gridcoin server.");
    // Shutdown will take long enough that the response should get back
    LogPrintf("Stopping...");
    StartShutdown();
    return "Gridcoin server stopping";
}



//
// Call Table
//
// We no longer use the unlocked feature here.
// Bitcoin has removed this option and placed the locks inside the rpc calls to reduce the scope
// Also removes the un needed locking when the end result is a rpc run time error reply over params!
// This also has improved the performance of rpc outputs.

static const CRPCCommand vRPCCommands[] =
{ //  name                      function                 safemd  category
  //  ------------------------  -----------------------  ------  -----------------
    { "list",                    &listitem,                true,   cat_null          },
    { "help",                    &help,                    true,   cat_null          },
    { "execute",                 &execute,                 true,   cat_null          },

  // Wallet commands
    { "addmultisigaddress",      &addmultisigaddress,      false,  cat_wallet        },
    { "addredeemscript",         &addredeemscript,         false,  cat_wallet        },
    { "backupprivatekeys",       &backupprivatekeys,       false,  cat_wallet        },
    { "backupwallet",            &backupwallet,            true,   cat_wallet        },
    { "burn",                    &burn,                    false,  cat_wallet        },
    { "burn2",                   &burn2,                   false,  cat_wallet        },
    { "checkwallet",             &checkwallet,             false,  cat_wallet        },
    { "createrawtransaction",    &createrawtransaction,    false,  cat_wallet        },
    { "decoderawtransaction",    &decoderawtransaction,    false,  cat_wallet        },
    { "decodescript",            &decodescript,            false,  cat_wallet        },
    { "dumpprivkey",             &dumpprivkey,             false,  cat_wallet        },
    { "dumpwallet",              &dumpwallet,              true,   cat_wallet        },
    { "encrypt",                 &encrypt,                 false,  cat_wallet        },
    { "encryptwallet",           &encryptwallet,           false,  cat_wallet        },
    { "getaccount",              &getaccount,              false,  cat_wallet        },
    { "getaccountaddress",       &getaccountaddress,       true,   cat_wallet        },
    { "getaddressesbyaccount",   &getaddressesbyaccount,   true,   cat_wallet        },
    { "getbalance",              &getbalance,              false,  cat_wallet        },
    { "getnewaddress",           &getnewaddress,           true,   cat_wallet        },
    { "getnewpubkey",            &getnewpubkey,            true,   cat_wallet        },
    { "getrawtransaction",       &getrawtransaction,       false,  cat_wallet        },
    { "getreceivedbyaccount",    &getreceivedbyaccount,    false,  cat_wallet        },
    { "getreceivedbyaddress",    &getreceivedbyaddress,    false,  cat_wallet        },
    { "gettransaction",          &gettransaction,          false,  cat_wallet        },
    { "getwalletinfo",           &getwalletinfo,           true,   cat_wallet        },
    { "importprivkey",           &importprivkey,           false,  cat_wallet        },
    { "importwallet",            &importwallet,            false,  cat_wallet        },
    { "keypoolrefill",           &keypoolrefill,           true,   cat_wallet        },
    { "listaccounts",            &listaccounts,            false,  cat_wallet        },
    { "listaddressgroupings",    &listaddressgroupings,    false,  cat_wallet        },
    { "listreceivedbyaccount",   &listreceivedbyaccount,   false,  cat_wallet        },
    { "listreceivedbyaddress",   &listreceivedbyaddress,   false,  cat_wallet        },
    { "listsinceblock",          &listsinceblock,          false,  cat_wallet        },
    { "listtransactions",        &listtransactions,        false,  cat_wallet        },
    { "listunspent",             &listunspent,             false,  cat_wallet        },
    { "makekeypair",             &makekeypair,             false,  cat_wallet        },
    { "move",                    &movecmd,                 false,  cat_wallet        },
    { "newburnaddress",          &newburnaddress,          false,  cat_wallet        },
    { "rain",                    &rain,                    false,  cat_wallet        },
    { "rainbymagnitude",         &rainbymagnitude,         false,  cat_wallet        },
    { "repairwallet",            &repairwallet,            false,  cat_wallet        },
    { "resendtx",                &resendtx,                false,  cat_wallet        },
    { "reservebalance",          &reservebalance,          false,  cat_wallet        },
    { "sendfrom",                &sendfrom,                false,  cat_wallet        },
    { "sendmany",                &sendmany,                false,  cat_wallet        },
    { "sendrawtransaction",      &sendrawtransaction,      false,  cat_wallet        },
    { "sendtoaddress",           &sendtoaddress,           false,  cat_wallet        },
    { "setaccount",              &setaccount,              true,   cat_wallet        },
    { "settxfee",                &settxfee,                false,  cat_wallet        },
    { "signmessage",             &signmessage,             false,  cat_wallet        },
    { "signrawtransaction",      &signrawtransaction,      false,  cat_wallet        },
    { "unspentreport",           &unspentreport,           false,  cat_wallet        },
    { "validateaddress",         &validateaddress,         true,   cat_wallet        },
    { "validatepubkey",          &validatepubkey,          true,   cat_wallet        },
    { "verifymessage",           &verifymessage,           false,  cat_wallet        },
    { "walletlock",              &walletlock,              true,   cat_wallet        },
    { "walletpassphrase",        &walletpassphrase,        true,   cat_wallet        },
    { "walletpassphrasechange",  &walletpassphrasechange,  false,  cat_wallet        },

  // Mining commands
    { "advertisebeacon",         &advertisebeacon,         false,  cat_mining        },
    { "beaconreport",            &beaconreport,            false,  cat_mining        },
    { "beaconstatus",            &beaconstatus,            false,  cat_mining        },
    { "cpids",                   &cpids,                   false,  cat_mining        },
    { "currentneuralhash",       &currentneuralhash,       false,  cat_mining        },
    { "currentneuralreport",     &currentneuralreport,     false,  cat_mining        },
    { "explainmagnitude",        &explainmagnitude,        false,  cat_mining        },
    { "getmininginfo",           &getmininginfo,           false,  cat_mining        },
    { "lifetime",                &lifetime,                false,  cat_mining        },
    { "magnitude",               &magnitude,               false,  cat_mining        },
    { "mymagnitude",             &mymagnitude,             false,  cat_mining        },
#ifdef WIN32
    { "myneuralhash",            &myneuralhash,            false,  cat_mining        },
    { "neuralhash",              &neuralhash,              false,  cat_mining        },
#endif
    { "neuralreport",            &neuralreport,            false,  cat_mining        },
    { "proveownership",          &proveownership,          false,  cat_mining        },
    { "resetcpids",              &resetcpids,              false,  cat_mining        },
    { "rsa",                     &rsa,                     false,  cat_mining        },
    { "rsaweight",               &rsaweight,               false,  cat_mining        },
    { "staketime",               &staketime,               false,  cat_mining        },
    { "superblockage",           &superblockage,           false,  cat_mining        },
    { "superblocks",             &superblocks,             false,  cat_mining        },
    { "syncdpor2",               &syncdpor2,               false,  cat_mining        },
    { "upgradedbeaconreport",    &upgradedbeaconreport,    false,  cat_mining        },
    { "validcpids",              &validcpids,              false,  cat_mining        },

  // Developer commands
    { "addkey",                  &addkey,                  false,  cat_developer     },
#ifdef WIN32
    { "currentcontractaverage",  &currentcontractaverage,  false,  cat_developer     },
#endif
    { "debug",                   &debug,                   true,   cat_developer     },
    { "debug10",                 &debug10,                 true,   cat_developer     },
    { "debug2",                  &debug2,                  true,   cat_developer     },
    { "debug3",                  &debug3,                  true,   cat_developer     },
    { "debug4",                  &debug4,                  true,   cat_developer     },
    { "debugnet",                &debugnet,                true,   cat_developer     },
    { "dportally",               &dportally,               false,  cat_developer     },
    { "exportstats1",            &rpc_exportstats,         false,  cat_developer     },
    { "forcequorom",             &forcequorom,             false,  cat_developer     },
    { "gatherneuralhashes",      &gatherneuralhashes,      false,  cat_developer     },
    { "genboinckey",             &genboinckey,             false,  cat_developer     },
    { "getblockstats",           &rpc_getblockstats,       false,  cat_developer     },
    { "getlistof",               &getlistof,               false,  cat_developer     },
    { "getnextproject",          &getnextproject,          false,  cat_developer     },
    { "getrecentblocks",         &rpc_getrecentblocks,     false,  cat_developer     },
    { "getsupervotes",           &rpc_getsupervotes,       false,  cat_developer     },
    { "listdata",                &listdata,                false,  cat_developer     },
    { "memorizekeys",            &memorizekeys,            false,  cat_developer     },
    { "network",                 &network,                 false,  cat_developer     },
    { "neuralrequest",           &neuralrequest,           false,  cat_developer     },
    { "projects",                &projects,                false,  cat_developer     },
    { "readconfig",              &readconfig,              false,  cat_developer     },
    { "readdata",                &readdata,                false,  cat_developer     },
    { "refhash",                 &refhash,                 false,  cat_developer     },
    { "reorganize",              &rpc_reorganize,          false,  cat_developer     },
    { "sendalert",               &sendalert,               false,  cat_developer     },
    { "sendalert2",              &sendalert2,              false,  cat_developer     },
    { "sendblock",               &sendblock,               false,  cat_developer     },
    { "sendrawcontract",         &sendrawcontract,         false,  cat_developer     },
    { "superblockaverage",       &superblockaverage,       false,  cat_developer     },
    { "tally",                   &tally,                   false,  cat_developer     },
    { "tallyneural",             &tallyneural,             false,  cat_developer     },
#ifdef WIN32
    { "testnewcontract",         &testnewcontract,         false,  cat_developer     },
#endif
    { "updatequoromdata",        &updatequoromdata,        false,  cat_developer     },
    { "versionreport",           &versionreport,           false,  cat_developer     },
    { "writedata",               &writedata,               false,  cat_developer     },

  // Network commands
    { "addnode",                 &addnode,                 false,  cat_network       },
    { "addpoll",                 &addpoll,                 false,  cat_network       },
    { "askforoutstandingblocks", &askforoutstandingblocks, false,  cat_network       },
    { "getblockchaininfo",       &getblockchaininfo,       true,   cat_network       },
    { "getnetworkinfo",          &getnetworkinfo,          true,   cat_network       },
    { "currenttime",             &currenttime,             false,  cat_network       },
    { "decryptphrase",           &decryptphrase,           false,  cat_network       },
    { "encryptphrase",           &encryptphrase,           false,  cat_network       },
    { "getaddednodeinfo",        &getaddednodeinfo,        true,   cat_network       },
    { "getbestblockhash",        &getbestblockhash,        true,   cat_network       },
    { "getblock",                &getblock,                true,   cat_network       },
    { "getblockbynumber",        &getblockbynumber,        true,   cat_network       },
    { "getblockcount",           &getblockcount,           true,   cat_network       },
    { "getblockhash",            &getblockhash,            true,   cat_network       },
    { "getcheckpoint",           &getcheckpoint,           true,   cat_network       },
    { "getconnectioncount",      &getconnectioncount,      true,   cat_network       },
    { "getdifficulty",           &getdifficulty,           true,   cat_network       },
    { "getinfo",                 &getinfo,                 true,   cat_network       },
    { "getnettotals",            &getnettotals,            true,   cat_network       },
    { "getpeerinfo",             &getpeerinfo,             true,   cat_network       },
    { "getrawmempool",           &getrawmempool,           true,   cat_network       },
    { "listallpolls",            &listallpolls,            true,   cat_network       },
    { "listallpolldetails",      &listallpolldetails,      true,   cat_network       },
    { "listpolldetails",         &listpolldetails,         true,   cat_network       },
    { "listpollresults",         &listpollresults,         true,   cat_network       },
    { "listpolls",               &listpolls,               true,   cat_network       },
    { "memorypool",              &memorypool,              true,   cat_network       },
    { "networktime",             &networktime,             true,   cat_network       },
    { "ping",                    &ping,                    true,   cat_network       },
    { "showblock",               &showblock,               true,   cat_network       },
    { "stop",                    &stop,                    true,   cat_network       },
    { "vote",                    &vote,                    false,  cat_network       },
    { "votedetails",             &votedetails,             true,   cat_network       },
};

CRPCTable::CRPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++)
    {
        const CRPCCommand *pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](string name) const
{
    map<string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return NULL;
    return (*it).second;
}

void ErrorReply(std::ostream& stream, const UniValue& objError, const UniValue& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();
    if (code == RPC_INVALID_REQUEST) nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND) nStatus = HTTP_NOT_FOUND;
    string strReply = JSONRPCReply(NullUniValue, objError, id);
    stream << HTTPReply(nStatus, strReply, false) << std::flush;
}

bool ClientAllowed(const boost::asio::ip::address& address)
{
    // Make sure that IPv4-compatible and IPv4-mapped IPv6 addresses are treated as IPv4 addresses
    if (address.is_v6()
     && (address.to_v6().is_v4_compatible()
      || address.to_v6().is_v4_mapped()))
        return ClientAllowed(address.to_v6().to_v4());

    if (address == asio::ip::address_v4::loopback()
     || address == asio::ip::address_v6::loopback()
     || (address.is_v4()
         // Check whether IPv4 addresses match 127.0.0.0/8 (loopback subnet)
      && (address.to_v4().to_ulong() & 0xff000000) == 0x7f000000))
        return true;

    const string strAddress = address.to_string();
    const vector<string>& vAllow = mapMultiArgs["-rpcallowip"];
    for (auto const& strAllow : vAllow)
        if (WildcardMatch(strAddress, strAllow))
            return true;
    return false;
}

void ServiceConnection(AcceptedConnection *conn);

// Forward declaration required for RPCListen
template <typename Protocol>
static void RPCAcceptHandler(boost::shared_ptr< basic_socket_acceptor<Protocol> > acceptor,
                             ssl::context& context,
                             bool fUseSSL,
                             AcceptedConnection* conn,
                             const boost::system::error_code& error);

/**
 * Sets up I/O resources to accept and handle a new connection.
 */
template <typename Protocol>
static void RPCListen(boost::shared_ptr< basic_socket_acceptor<Protocol> > acceptor,
                      ssl::context& context,
                      const bool fUseSSL)
{
    // Accept connection
    AcceptedConnectionImpl<Protocol>* conn = new AcceptedConnectionImpl<Protocol>(acceptor->get_io_service(), context, fUseSSL);

    acceptor->async_accept(
                conn->sslStream.lowest_layer(),
                conn->peer,
                boost::bind(&RPCAcceptHandler<Protocol>,
                            acceptor,
                            boost::ref(context),
                            fUseSSL,
                            conn,
                            boost::asio::placeholders::error));
}

/**
 * Accept and handle incoming connection.
 */
template <typename Protocol>
static void RPCAcceptHandler(boost::shared_ptr< basic_socket_acceptor<Protocol> > acceptor,
                             ssl::context& context,
                             const bool fUseSSL,
                             AcceptedConnection* conn,
                             const boost::system::error_code& error)
{
    // Immediately start accepting new connections, except when we're cancelled or our socket is closed.
    if (error != asio::error::operation_aborted && acceptor->is_open())
        RPCListen(acceptor, context, fUseSSL);

    // TODO : Actually handle errors
    if (!error)
    {
        // Restrict callers by IP.  It is important to
        // do this before starting client thread, to filter out
        // certain DoS and misbehaving clients.
        AcceptedConnectionImpl<ip::tcp>* tcp_conn = dynamic_cast< AcceptedConnectionImpl<ip::tcp>* >(conn);
        if (tcp_conn && !ClientAllowed(tcp_conn->peer.address()))
        {
            // Only send a 403 if we're not using SSL to prevent a DoS during the SSL handshake.
            if (!fUseSSL)
                conn->stream() << HTTPReply(HTTP_FORBIDDEN, "", false) << std::flush;
        }
        else
            ServiceConnection(conn);

        conn->close();
    }

    delete conn;
}

void StartRPCThreads()
{
    strRPCUserColonPass = mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"];
    if ((mapArgs["-rpcpassword"] == "") ||
        (mapArgs["-rpcuser"] == mapArgs["-rpcpassword"]))
    {
        unsigned char rand_pwd[32];
        RAND_bytes(rand_pwd, 32);
        string strWhatAmI = "To use gridcoind";
        if (mapArgs.count("-server"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-server\"");
        else if (mapArgs.count("-daemon"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-daemon\"");
        uiInterface.ThreadSafeMessageBox(strprintf(
                                             _("%s, you must set a rpcpassword in the configuration file:\n %s\n"
                                               "It is recommended you use the following random password:\n"
                                               "rpcuser=gridcoinrpc\n"
                                               "rpcpassword=%s\n"
                                               "(you do not need to remember this password)\n"
                                               "The username and password MUST NOT be the same.\n"
                                               "If the file does not exist, create it with owner-readable-only file permissions.\n"
                                               "It is also recommended to set alertnotify so you are notified of problems;\n"
                                               "for example: alertnotify=echo %%s | mail -s \"Gridcoin Alert\" admin@foo.com\n"),
                                             strWhatAmI,
                                             GetConfigFile().string(),
                                             EncodeBase58(&rand_pwd[0],&rand_pwd[0]+32)),
                _("Error"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }

    const bool fUseSSL = GetBoolArg("-rpcssl");

    assert(rpc_io_service == NULL);
    rpc_io_service = new asio::io_service();
    rpc_ssl_context = new ssl::context(ssl::context::sslv23);

    if (fUseSSL)
    {
        rpc_ssl_context->set_options(ssl::context::no_sslv2);

        filesystem::path pathCertFile(GetArg("-rpcsslcertificatechainfile", "server.cert"));
        if (!pathCertFile.is_complete()) pathCertFile = filesystem::path(GetDataDir()) / pathCertFile;
        if (filesystem::exists(pathCertFile)) rpc_ssl_context->use_certificate_chain_file(pathCertFile.string());
        else LogPrintf("ThreadRPCServer ERROR: missing server certificate file %s\n", pathCertFile.string());

        filesystem::path pathPKFile(GetArg("-rpcsslprivatekeyfile", "server.pem"));
        if (!pathPKFile.is_complete()) pathPKFile = filesystem::path(GetDataDir()) / pathPKFile;
        if (filesystem::exists(pathPKFile)) rpc_ssl_context->use_private_key_file(pathPKFile.string(), ssl::context::pem);
        else LogPrintf("ThreadRPCServer ERROR: missing server private key file %s\n", pathPKFile.string());

        string strCiphers = GetArg("-rpcsslciphers", "TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH");
        SSL_CTX_set_cipher_list(rpc_ssl_context->native_handle(), strCiphers.c_str());
    }

    // Try a dual IPv6/IPv4 socket, falling back to separate IPv4 and IPv6 sockets
    const bool loopback = !mapArgs.count("-rpcallowip");
    asio::ip::address bindAddress = loopback ? asio::ip::address_v6::loopback() : asio::ip::address_v6::any();
    ip::tcp::endpoint endpoint(bindAddress, GetArg("-rpcport", GetDefaultRPCPort()));
    boost::system::error_code v6_only_error;
    boost::shared_ptr<ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(*rpc_io_service));

    bool fListening = false;
    std::string strerr;
    try
    {
        acceptor->open(endpoint.protocol());
        acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

        // Try making the socket dual IPv6/IPv4 (if listening on the "any" address)
        acceptor->set_option(boost::asio::ip::v6_only(loopback), v6_only_error);

        acceptor->bind(endpoint);
        acceptor->listen(socket_base::max_connections);

        RPCListen(acceptor, *rpc_ssl_context, fUseSSL);

        fListening = true;
    }
    catch(boost::system::system_error &e)
    {
        strerr = strprintf(_("An error occurred while setting up the RPC port %u for listening on IPv6, falling back to IPv4: %s"), endpoint.port(), e.what());
    }

    try
    {
        // If dual IPv6/IPv4 failed (or we're opening loopback interfaces only), open IPv4 separately
        if (!fListening || loopback || v6_only_error)
        {
            bindAddress = loopback ? asio::ip::address_v4::loopback() : asio::ip::address_v4::any();
            endpoint.address(bindAddress);

            acceptor.reset(new ip::tcp::acceptor(*rpc_io_service));
            acceptor->open(endpoint.protocol());
            acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor->bind(endpoint);
            acceptor->listen(socket_base::max_connections);

            RPCListen(acceptor, *rpc_ssl_context, fUseSSL);

            fListening = true;
        }
    }
    catch(boost::system::system_error &e)
    {
        strerr = strprintf(_("An error occurred while setting up the RPC port %u for listening on IPv4: %s"), endpoint.port(), e.what());
    }

    if (!fListening)
    {
        uiInterface.ThreadSafeMessageBox(strerr, _("Error"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }
    
    rpc_worker_group = new boost::thread_group();
    for (int i = 0; i < GetArg("-rpcthreads", 4); i++)
        rpc_worker_group->create_thread(boost::bind(&asio::io_service::run, rpc_io_service));
}

void StopRPCThreads()
{
    LogPrintf("Stop RPC IO service\n");
    if(!rpc_io_service)
    {
        LogPrintf("RPC IO server not started\n");
        return;
    }

    rpc_io_service->stop();
    if (rpc_worker_group != NULL)
        rpc_worker_group->join_all();
    
    delete rpc_worker_group;
    rpc_worker_group = NULL;
    delete rpc_ssl_context;
    rpc_ssl_context = NULL;
    delete rpc_io_service;
    rpc_io_service = NULL;
}

class JSONRequest
{
public:
    UniValue id;
    string strMethod;
    UniValue params;

    JSONRequest() { id = NullUniValue; }
    void parse(const UniValue& valRequest);
};

void JSONRequest::parse(const UniValue& valRequest)
{
    // Parse request
    if (!valRequest.isObject())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const UniValue& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    UniValue valMethod = find_value(request, "method");
    if (valMethod.isNull())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (!valMethod.isStr())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    if (strMethod != "getwork" && strMethod != "getblocktemplate")
        if (fDebug10) LogPrintf("ThreadRPCServer method=%s", strMethod);

    // Parse params
    UniValue valParams = find_value(request, "params");
    if (valParams.isArray())
        params = valParams.get_array();
    else if (valParams.isNull())
        params = UniValue(UniValue::VARR);
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}

static UniValue JSONRPCExecOne(const UniValue& req)
{
    UniValue rpc_result(UniValue::VOBJ);

    JSONRequest jreq;
    try
    {
        jreq.parse(req);

        UniValue result = tableRPC.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, NullUniValue, jreq.id);
    }
    catch (UniValue& objError)
    {
        rpc_result = JSONRPCReplyObj(NullUniValue, objError, jreq.id);
    }
    catch (std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(NullUniValue,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

static string JSONRPCExecBatch(const UniValue& vReq)
{
    UniValue ret(UniValue::VARR);
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return UniValue(ret).write() + "\n";
}

void ServiceConnection(AcceptedConnection *conn)
{
    bool fRun = true;
    while (fRun && !fShutdown)
    {
        int nProto = 0;
        map<string, string> mapHeaders;
        string strRequest, strMethod, strURI;

        // Read HTTP request line
        if (!ReadHTTPRequestLine(conn->stream(), nProto, strMethod, strURI))
            break;

        // Read HTTP message headers and body
        ReadHTTPMessage(conn->stream(), mapHeaders, strRequest, nProto);

        if (strURI != "/") {
            conn->stream() << HTTPReply(HTTP_NOT_FOUND, "", false) << std::flush;
            break;
        }

        // Check authorization
        if (mapHeaders.count("authorization") == 0)
        {
            conn->stream() << HTTPReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }
        if (!HTTPAuthorized(mapHeaders))
        {
            LogPrintf("ThreadRPCServer incorrect password attempt from %s\n", conn->peer_address_to_string());
            /* Deter brute-forcing short passwords.
               If this results in a DOS the user really
               shouldn't have their RPC port exposed.*/
            if (mapArgs["-rpcpassword"].size() < 20)
                MilliSleep(250);

            conn->stream() << HTTPReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }
        if (mapHeaders["connection"] == "close")
            fRun = false;

        JSONRequest jreq;
        try
        {
            // Parse request
            UniValue valRequest(UniValue::VSTR);
            if (!valRequest.read(strRequest))
                throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

            string strReply;

            // singleton request
            if (valRequest.isObject()) {
                jreq.parse(valRequest);

                UniValue result = tableRPC.execute(jreq.strMethod, jreq.params);

                // Send reply
                strReply = JSONRPCReply(result, NullUniValue, jreq.id);

            // array of requests
            } else if (valRequest.isArray())
                strReply = JSONRPCExecBatch(valRequest.get_array());
            else
                throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

            conn->stream() << HTTPReply(HTTP_OK, strReply, fRun) << std::flush;
        }
        catch (UniValue& objError)
        {
            ErrorReply(conn->stream(), objError, jreq.id);
            break;
        }
        catch (std::exception& e)
        {
            ErrorReply(conn->stream(), JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
            break;
        }
    }
}

UniValue CRPCTable::execute(const std::string& strMethod, const UniValue& params) const
{
    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode") &&
        !pcmd->okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);

    // Lets add a optional debug4 to display how long it takes the rpc commands to be performed in ms
    // We will do this only on successful calls not exceptions
    try
    {
        UniValue result(UniValue::VSTR);

        if (fDebug4)
        {
            int64_t nRPCtimebegin;
            int64_t nRPCtimetotal;
            nRPCtimebegin = GetTimeMillis();
            result = pcmd->actor(params, false);
            nRPCtimetotal = GetTimeMillis() - nRPCtimebegin;
            LogPrintf("RPCTime : Command %s -> Totaltime %" PRId64 "ms", strMethod, nRPCtimetotal);
        }
        else
            result = pcmd->actor(params, false);

        return result;
    }
    catch (std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

#ifdef TEST
int main(int argc, char *argv[])
{
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFile("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    try
    {
        if (argc >= 2 && string(argv[1]) == "-server")
        {
            LogPrintf("server ready");
            ThreadRPCServer(NULL);
        }
        else
        {
            return CommandLineRPC(argc, argv);
        }
    }
    catch (std::exception& e) {
        PrintException(&e, "main()");
    } catch (...) {
        PrintException(NULL, "main()");
    }
    return 0;
}
#endif

const CRPCTable tableRPC;
