// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "init.h"
#include "sync.h"
#include "ui_interface.h"
#include "base58.h"
#include "server.h"
#include "client.h"
#include "protocol.h"
#include "wallet/db.h"
#include "util.h"

#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <algorithm>

#include <memory>

using namespace std;
using namespace boost;
using namespace boost::asio;

void ThreadRPCServer2(void* parg);

static std::string strRPCUserColonPass;

// These are created by StartRPCThreads, destroyed in StopRPCThreads
static ioContext* rpc_io_service = NULL;
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
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return UniValue(UniValue::VNUM, strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder));
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
            "mining --------> Returns help for staking/cpid/beacon related commands\n"
            "developer -----> Returns help for developer commands\n"
            "network -------> Returns help for network related commands\n"
            "voting --------> Returns help for voting related commands\n"
	    "\n"
	    "You can support the development of Gridcoin by donating GRC to the\n"
	    "Gridcoin Foundation at this address: bc3NA8e8E3EoTL1qhRmeprbjWcmuoZ26A2\n";

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

    else if (strCommand == "voting")
        category = cat_voting;

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
{ //  name                      function                 category
  //  ------------------------  -----------------------  -----------------
    { "list",                    &listitem,                cat_null          },
    { "help",                    &help,                    cat_null          },
    { "execute",                 &execute,                 cat_null          },

  // Wallet commands
    { "addmultisigaddress",      &addmultisigaddress,      cat_wallet        },
    { "addredeemscript",         &addredeemscript,         cat_wallet        },
    { "backupprivatekeys",       &backupprivatekeys,       cat_wallet        },
    { "backupwallet",            &backupwallet,            cat_wallet        },
    { "burn",                    &burn,                    cat_wallet        },
    { "checkwallet",             &checkwallet,             cat_wallet        },
    { "createrawtransaction",    &createrawtransaction,    cat_wallet        },
    { "consolidatemsunspent",    &consolidatemsunspent,    cat_wallet        },
    { "decoderawtransaction",    &decoderawtransaction,    cat_wallet        },
    { "decodescript",            &decodescript,            cat_wallet        },
    { "dumpprivkey",             &dumpprivkey,             cat_wallet        },
    { "dumpwallet",              &dumpwallet,              cat_wallet        },
    { "encryptwallet",           &encryptwallet,           cat_wallet        },
    { "getaccount",              &getaccount,              cat_wallet        },
    { "getaccountaddress",       &getaccountaddress,       cat_wallet        },
    { "getaddressesbyaccount",   &getaddressesbyaccount,   cat_wallet        },
    { "getbalance",              &getbalance,              cat_wallet        },
    { "getbalancedetail",        &getbalancedetail,        cat_wallet        },
    { "getnewaddress",           &getnewaddress,           cat_wallet        },
    { "getnewpubkey",            &getnewpubkey,            cat_wallet        },
    { "getrawtransaction",       &getrawtransaction,       cat_wallet        },
    { "getrawwallettransaction", &getrawwallettransaction, cat_wallet        },
    { "getreceivedbyaccount",    &getreceivedbyaccount,    cat_wallet        },
    { "getreceivedbyaddress",    &getreceivedbyaddress,    cat_wallet        },
    { "gettransaction",          &gettransaction,          cat_wallet        },
    { "getunconfirmedbalance",   &getunconfirmedbalance,   cat_wallet        },
    { "getwalletinfo",           &getwalletinfo,           cat_wallet        },
    { "importprivkey",           &importprivkey,           cat_wallet        },
    { "importwallet",            &importwallet,            cat_wallet        },
    { "keypoolrefill",           &keypoolrefill,           cat_wallet        },
    { "listaccounts",            &listaccounts,            cat_wallet        },
    { "listaddressgroupings",    &listaddressgroupings,    cat_wallet        },
    { "listreceivedbyaccount",   &listreceivedbyaccount,   cat_wallet        },
    { "listreceivedbyaddress",   &listreceivedbyaddress,   cat_wallet        },
    { "listsinceblock",          &listsinceblock,          cat_wallet        },
    { "liststakes",              &liststakes,              cat_wallet        },
    { "listtransactions",        &listtransactions,        cat_wallet        },
    { "listunspent",             &listunspent,             cat_wallet        },
    { "consolidateunspent",      &consolidateunspent,      cat_wallet        },
    { "makekeypair",             &makekeypair,             cat_wallet        },
    { "maintainbackups",         &maintainbackups,         cat_wallet        },
    { "move",                    &movecmd,                 cat_wallet        },
    { "rainbymagnitude",         &rainbymagnitude,         cat_wallet        },
    { "repairwallet",            &repairwallet,            cat_wallet        },
    { "resendtx",                &resendtx,                cat_wallet        },
    { "reservebalance",          &reservebalance,          cat_wallet        },
    { "scanforunspent",          &scanforunspent,          cat_wallet        },
    { "sendfrom",                &sendfrom,                cat_wallet        },
    { "sendmany",                &sendmany,                cat_wallet        },
    { "sendrawtransaction",      &sendrawtransaction,      cat_wallet        },
    { "sendtoaddress",           &sendtoaddress,           cat_wallet        },
    { "setaccount",              &setaccount,              cat_wallet        },
    { "settxfee",                &settxfee,                cat_wallet        },
    { "signmessage",             &signmessage,             cat_wallet        },
    { "signrawtransaction",      &signrawtransaction,      cat_wallet        },
    { "validateaddress",         &validateaddress,         cat_wallet        },
    { "validatepubkey",          &validatepubkey,          cat_wallet        },
    { "verifymessage",           &verifymessage,           cat_wallet        },
    { "walletlock",              &walletlock,              cat_wallet        },
    { "walletpassphrase",        &walletpassphrase,        cat_wallet        },
    { "walletpassphrasechange",  &walletpassphrasechange,  cat_wallet        },

  // Mining commands
    { "advertisebeacon",         &advertisebeacon,         cat_mining        },
    { "beaconconvergence",       &beaconconvergence,       cat_mining        },
    { "beaconreport",            &beaconreport,            cat_mining        },
    { "beaconstatus",            &beaconstatus,            cat_mining        },
    { "explainmagnitude",        &explainmagnitude,        cat_mining        },
    { "getlaststake",            &getlaststake,            cat_mining        },
    { "getmininginfo",           &getmininginfo,           cat_mining        },
    { "lifetime",                &lifetime,                cat_mining        },
    { "magnitude",               &magnitude,               cat_mining        },
    { "pendingbeaconreport",     &pendingbeaconreport,     cat_mining        },
    { "resetcpids",              &resetcpids,              cat_mining        },
    { "revokebeacon",            &revokebeacon,            cat_mining        },
    { "superblockage",           &superblockage,           cat_mining        },
    { "superblocks",             &superblocks,             cat_mining        },

  // Developer commands
    { "auditsnapshotaccrual",    &auditsnapshotaccrual,    cat_developer     },
    { "auditsnapshotaccruals",   &auditsnapshotaccruals,   cat_developer     },
    { "addkey",                  &addkey,                  cat_developer     },
    { "comparesnapshotaccrual",  &comparesnapshotaccrual,  cat_developer     },
    { "currentcontractaverage",  &currentcontractaverage,  cat_developer     },
    { "debug",                   &debug,                   cat_developer     },
    { "debug10",                 &debug10,                 cat_developer     },
    { "dumpcontracts",           &dumpcontracts,           cat_developer     },
    { "exportstats1",            &rpc_exportstats,         cat_developer     },
    { "getblockstats",           &rpc_getblockstats,       cat_developer     },
    { "getlistof",               &getlistof,               cat_developer     },
    { "getrecentblocks",         &rpc_getrecentblocks,     cat_developer     },
    { "getsupervotes",           &rpc_getsupervotes,       cat_developer     },
    { "inspectaccrualsnapshot",  &inspectaccrualsnapshot,  cat_developer     },
    { "listdata",                &listdata,                cat_developer     },
    { "listprojects",            &listprojects,            cat_developer     },
    { "listresearcheraccounts",  &listresearcheraccounts,  cat_developer     },
    { "logging",                 &logging,                 cat_developer     },
    { "network",                 &network,                 cat_developer     },
    { "parseaccrualsnapshotfile",&parseaccrualsnapshotfile,cat_developer     },
    { "parselegacysb",           &parselegacysb,           cat_developer     },
    { "projects",                &projects,                cat_developer     },
    { "readconfig",              &readconfig,              cat_developer     },
    { "readdata",                &readdata,                cat_developer     },
    { "reorganize",              &rpc_reorganize,          cat_developer     },
    { "sendalert",               &sendalert,               cat_developer     },
    { "sendalert2",              &sendalert2,              cat_developer     },
    { "sendblock",               &sendblock,               cat_developer     },
    { "superblockaverage",       &superblockaverage,       cat_developer     },
    { "versionreport",           &versionreport,           cat_developer     },
    { "writedata",               &writedata,               cat_developer     },

    { "listmanifests",           &listmanifests,           cat_developer     },
    { "getmpart",                &getmpart,                cat_developer     },
    { "sendscraperfilemanifest", &sendscraperfilemanifest, cat_developer     },
    { "savescraperfilemanifest", &savescraperfilemanifest, cat_developer     },
    { "deletecscrapermanifest",  &deletecscrapermanifest,  cat_developer     },
    { "archivelog",              &archivelog,              cat_developer     },
    { "testnewsb",               &testnewsb,               cat_developer     },
    { "convergencereport",       &convergencereport,       cat_developer     },
    { "scraperreport",           &scraperreport,           cat_developer     },

  // Network commands
    { "addnode",                 &addnode,                 cat_network       },
    { "askforoutstandingblocks", &askforoutstandingblocks, cat_network       },
    { "getblockchaininfo",       &getblockchaininfo,       cat_network       },
    { "getnetworkinfo",          &getnetworkinfo,          cat_network       },
    { "clearbanned",             &clearbanned,             cat_network       },
    { "currenttime",             &currenttime,             cat_network       },
    { "getaddednodeinfo",        &getaddednodeinfo,        cat_network       },
    { "getbestblockhash",        &getbestblockhash,        cat_network       },
    { "getblock",                &getblock,                cat_network       },
    { "getblockbynumber",        &getblockbynumber,        cat_network       },
    { "getblockcount",           &getblockcount,           cat_network       },
    { "getblockhash",            &getblockhash,            cat_network       },
    { "getcheckpoint",           &getcheckpoint,           cat_network       },
    { "getconnectioncount",      &getconnectioncount,      cat_network       },
    { "getdifficulty",           &getdifficulty,           cat_network       },
    { "getinfo",                 &getinfo,                 cat_network       },
    { "getnettotals",            &getnettotals,            cat_network       },
    { "getpeerinfo",             &getpeerinfo,             cat_network       },
    { "getrawmempool",           &getrawmempool,           cat_network       },
    { "listbanned",              &listbanned,              cat_network       },
    { "memorypool",              &memorypool,              cat_network       },
    { "networktime",             &networktime,             cat_network       },
    { "ping",                    &ping,                    cat_network       },
    { "setban",                  &setban,                  cat_network       },
    { "showblock",               &showblock,               cat_network       },
    { "stop",                    &stop,                    cat_network       },

  // Voting commands
    { "addpoll",                 &addpoll,                 cat_voting        },
    { "getpollresults",          &getpollresults,          cat_voting        },
    { "getvotingclaim",          &getvotingclaim,          cat_voting        },
    { "listpolls",               &listpolls,               cat_voting        },
    { "vote",                    &vote,                    cat_voting        },
    { "votebyid",                &votebyid,                cat_voting        },
    { "votedetails",             &votedetails,             cat_voting        },
};

static constexpr const char* DEPRECATED_RPCS[] {
        "debug",
        "debug10",
        "execute" ,
        "getaccount",
        "getaccountaddress",
        "getaddressesbyaccount",
        "getreceivedbyaccount",
        "listaccounts",
        "listreceivedbyaccount",
        "list",
        "move",
        "setaccount",
        "vote",
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
    AcceptedConnectionImpl<Protocol>* conn = new AcceptedConnectionImpl<Protocol>(GetIOServiceFromPtr(acceptor), context, fUseSSL);

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
    rpc_io_service = new ioContext();
    rpc_ssl_context = new ssl::context(ssl::context::sslv23);

    if (fUseSSL)
    {
        rpc_ssl_context->set_options(ssl::context::no_sslv2);

        fs::path pathCertFile(GetArg("-rpcsslcertificatechainfile", "server.cert"));
        if (!pathCertFile.is_absolute()) pathCertFile = fs::path(GetDataDir()) / pathCertFile;
        if (fs::exists(pathCertFile)) rpc_ssl_context->use_certificate_chain_file(pathCertFile.string());
        else LogPrintf("ThreadRPCServer ERROR: missing server certificate file %s\n", pathCertFile.string());

        fs::path pathPKFile(GetArg("-rpcsslprivatekeyfile", "server.pem"));
        if (!pathPKFile.is_absolute()) pathPKFile = fs::path(GetDataDir()) / pathPKFile;
        if (fs::exists(pathPKFile)) rpc_ssl_context->use_private_key_file(pathPKFile.string(), ssl::context::pem);
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
        rpc_worker_group->create_thread(boost::bind(&ioContext::run, rpc_io_service));
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
        LogPrint(BCLog::LogFlags::NOISY, "ThreadRPCServer method=%s", strMethod);

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

    // Lets add a optional display if BCLog::LogFlags::RPC is set to show how long it takes
    // the rpc commands to be performed in milliseconds. We will do this only on successful
    // calls not exceptions.
    try
    {
        UniValue result(UniValue::VSTR);

        if (LogInstance().WillLogCategory(BCLog::LogFlags::RPC))
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


std::vector<std::string> CRPCTable::listCommands() const
{
    std::vector<std::string> commandList;
    typedef std::map<std::string, const CRPCCommand*> commandMap;

    std::transform( mapCommands.begin(), mapCommands.end(),
                    std::back_inserter(commandList),
                    boost::bind(&commandMap::value_type::first,_1) );
    // remove deprecated commands from autocomplete
    for(auto &command: DEPRECATED_RPCS) {
        std::remove(commandList.begin(), commandList.end(), command);
    }
    return commandList;
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
