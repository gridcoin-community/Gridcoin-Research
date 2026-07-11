// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include <base58.h>
#include "init.h"
#include "sync.h"
#include <key_io.h>
#include "node/ui_interface.h"
#include "server.h"
#include "client.h"
#include "protocol.h"
#include "random.h"
#include "wallet/db.h"
#include <rpc/util.h>
#include <util.h>

#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind/bind.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <algorithm>
#include <stdexcept>

#include <memory>
#include <mutex>
#include <set>
#include <vector>

using namespace std;
using namespace boost;
using namespace boost::asio;

void ThreadRPCServer2(void* parg);

static std::string strRPCUserColonPass;

// These are created by StartRPCThreads, destroyed in StopRPCThreads
static ioContext* rpc_io_service = nullptr;
static ssl::context* rpc_ssl_context = nullptr;
static boost::thread_group* rpc_worker_group = nullptr;
// Acceptors created by StartRPCThreads. Retained so (a) startup can log the
// bound endpoints and (b) StopRPCThreads can close() them before tearing down
// the io_service, which prevents the shutdown hang on an open keep-alive socket
// that authproxy.py works around on the client side.
static std::vector<boost::shared_ptr<boost::asio::ip::tcp::acceptor>> rpc_acceptors;

// Live connections currently being serviced (a worker parked in
// ServiceConnection()). Closing the acceptors stops *new* connections, but an
// already-established keep-alive client leaves its worker blocked in a
// synchronous read; StopRPCThreads() must interrupt those sockets or
// join_all() hangs forever (issue #3123). This registry is the leaf-most lock
// in the process -- it guards only the set below and never calls back into any
// subsystem while held, so it has no ordering relationship with cs_main et al.
static std::mutex g_rpc_connections_mutex;
static std::set<AcceptedConnection*> g_rpc_connections;
// Set once StopRPCThreads() has begun tearing down. A connection that is
// accepted after this point must not park in ServiceConnection(), or it would
// re-introduce the very hang we are closing this window against.
static bool g_rpc_connections_stopped = false;

//! Register a just-accepted connection so StopRPCThreads() can interrupt it.
//! Returns false if the server is already shutting down, in which case the
//! caller must not service the connection (there is no worker-drain left to
//! wake it).
static bool RegisterRPCConnection(AcceptedConnection* conn)
{
    std::lock_guard<std::mutex> lock(g_rpc_connections_mutex);
    if (g_rpc_connections_stopped) return false;
    g_rpc_connections.insert(conn);
    return true;
}

//! Remove a connection from the registry once its worker is done servicing it
//! (before the connection is closed and deleted).
static void UnregisterRPCConnection(AcceptedConnection* conn)
{
    std::lock_guard<std::mutex> lock(g_rpc_connections_mutex);
    g_rpc_connections.erase(conn);
}

const UniValue emptyobj(UniValue::VOBJ);

int GetDefaultRPCPort()
{
    return BaseParams().RPCPort();
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
    string strUserPass64 = strAuth.substr(6);
    strUserPass64 = TrimString(strUserPass64);
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
    if (64 != strHex.length())
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("%s must be of length %d (not %d)", strName, 64, strHex.length()));
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
        // Refactored rules for supporting of subcategories
        if (pcmd->category == cat_null)
            continue;

        if (strCommand.empty() && pcmd->category != category)
            continue;

        if (!strCommand.empty() && pcmd->name != strCommand)
            continue;

        // Dedupe aliases (e.g. getmininginfo -> getstakinginfo) by actor pointer:
        // both rows share the same actor and helpman, so only render once.
        if (!setDone.insert(pcmd->actor).second)
            continue;

        // Every command now has a helpman accessor (addpoll's was lifted in PR M3);
        // pull help text directly from it without invoking the command body or
        // using throw-as-control-flow.
        string strHelp = pcmd->helpman().ToString();
        if (strCommand.empty())
            if (strHelp.find('\n') != string::npos)
                strHelp = strHelp.substr(0, strHelp.find('\n'));
        strRet += strHelp + "\n";
    }
    if (strRet.empty())
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

static const RPCHelpMan help_help{
    "help",
    "List commands, or get help for a specified command or category.\n"
    "\n"
    "Categories:\n"
    "  wallet    - blockchain/wallet related commands\n"
    "  staking   - staking/cpid/beacon related commands (alias: mining)\n"
    "  developer - developer commands\n"
    "  network   - network related commands\n"
    "  voting    - voting related commands\n"
    "\n"
    "You can support the development of Gridcoin by donating GRC to the\n"
    "Gridcoin Foundation at this address: bc3NA8e8E3EoTL1qhRmeprbjWcmuoZ26A2",
    {
        {"command", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
            "The command name or category to look up. If omitted, an overview of all categories is returned."},
    },
    RPCResult{RPCResult::Type::STR, "", "The help text"},
    RPCExamples{
        HelpExampleCli("help", "") +
        HelpExampleCli("help", "getinfo") +
        HelpExampleCli("help", "wallet") +
        HelpExampleRpc("help", "\"getinfo\"")},
};
const RPCHelpMan& help_helpman() { return help_help; }

UniValue help(const UniValue& params)
{
    // Arity is pre-checked by the dispatcher via help_helpman()->IsValidNumArgs;
    // by the time we get here, params.size() is in range.
    string strCommand;

    if (params.size() > 0)
        strCommand = params[0].get_str();

    // With no command argument, return the structured help for `help` itself
    // (description + categories + arguments + examples). Without this early
    // return, an empty strCommand would fall through to the lookup loop below
    // and yield "help: unknown command:" — a regression vs. the pre-RPCHelpMan
    // behavior where `help` (no args) printed the category overview.
    if (strCommand.empty())
        return help_helpman().ToString();

    // Subcategory help area
    // Blockchain related commands
    rpccategory category;

    if (strCommand == "wallet")
        category = cat_wallet;

    else if (strCommand == "staking" || strCommand == "mining")
        category = cat_staking;

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

static const RPCHelpMan stop_help{
    "stop",
    "Stop Gridcoin server.",
    {},
    RPCResult{RPCResult::Type::STR, "", "A confirmation string."},
    RPCExamples{
        HelpExampleCli("stop", "") +
        HelpExampleRpc("stop", "")},
};
const RPCHelpMan& stop_helpman() { return stop_help; }

UniValue stop(const UniValue& params)
{
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
    { "help",                    &help,                    cat_null, &help_helpman, heritage_mixed, "4ce89ec7b45f" },

  // Wallet commands
    { "addmultisigaddress",      &addmultisigaddress,      cat_wallet, &addmultisigaddress_helpman, heritage_removed_upstream, "b0bd7231da33" },
    { "addredeemscript",         &addredeemscript,         cat_wallet, &addredeemscript_helpman, heritage_removed_upstream, "c9e4350b4a75" },
    { "backupwallet",            &backupwallet,            cat_wallet, &backupwallet_helpman, heritage_mixed, "658099c7930f" },
    { "burn",                    &burn,                    cat_wallet, &burn_helpman, heritage_pure_gridcoin, "" },
    { "checkwallet",             &checkwallet,             cat_wallet, &checkwallet_helpman, heritage_pure_gridcoin, "" },
    { "claimhtlc",              &claimhtlc,               cat_wallet, &claimhtlc_helpman, heritage_pure_gridcoin, "" },
    { "createhtlc",             &createhtlc,              cat_wallet, &createhtlc_helpman, heritage_pure_gridcoin, "" },
    { "createrawtransaction",    &createrawtransaction,    cat_wallet, &createrawtransaction_helpman, heritage_mixed, "50b21e94ef71" },
    { "consolidatemsunspent",    &consolidatemsunspent,    cat_wallet, &consolidatemsunspent_helpman, heritage_pure_gridcoin, "" },
    { "decoderawtransaction",    &decoderawtransaction,    cat_wallet, &decoderawtransaction_helpman, heritage_mixed, "ee4cdec04193" },
    { "decodescript",            &decodescript,            cat_wallet, &decodescript_helpman, heritage_mixed, "ca2d8ef9d7fb" },
    { "dumpprivkey",             &dumpprivkey,             cat_wallet, &dumpprivkey_helpman, heritage_removed_upstream, "96d3ee80fb58" },
    { "fundrawtransaction",      &fundrawtransaction,      cat_wallet, &fundrawtransaction_helpman, heritage_mixed, "5373087fac94" },
    { "dumpwallet",              &dumpwallet,              cat_wallet, &dumpwallet_helpman, heritage_removed_upstream, "eaad3c94f6a6" },
    { "encryptwallet",           &encryptwallet,           cat_wallet, &encryptwallet_helpman, heritage_mixed, "7b0696269744" },
    { "getaccount",              &getaccount,              cat_wallet, &getaccount_helpman, heritage_pure_gridcoin, "" },
    { "getaccountaddress",       &getaccountaddress,       cat_wallet, &getaccountaddress_helpman, heritage_pure_gridcoin, "" },
    { "getaddressesbyaccount",   &getaddressesbyaccount,   cat_wallet, &getaddressesbyaccount_helpman, heritage_pure_gridcoin, "" },
    { "getaddressesbylabel", &getaddressesbylabel, cat_wallet, &getaddressesbylabel_helpman, heritage_mixed, "5058a857a5e1" },
    { "getbalance",              &getbalance,              cat_wallet, &getbalance_helpman, heritage_mixed, "4bbd03d840d3" },
    { "getbalancedetail",        &getbalancedetail,        cat_wallet, &getbalancedetail_helpman, heritage_pure_gridcoin, "" },
    { "getnewaddress",           &getnewaddress,           cat_wallet, &getnewaddress_helpman, heritage_mixed, "e503f05f1459" },
    { "getnewpubkey",            &getnewpubkey,            cat_wallet, &getnewpubkey_helpman, heritage_pure_gridcoin, "" },
    { "getrawtransaction",       &getrawtransaction,       cat_wallet, &getrawtransaction_helpman, heritage_mixed, "0da7a920386c" },
    { "getrawwallettransaction", &getrawwallettransaction, cat_wallet, &getrawwallettransaction_helpman, heritage_pure_gridcoin, "" },
    { "gettxoutproof", &gettxoutproof, cat_wallet, &gettxoutproof_helpman, heritage_mixed, "0a9a9c33566c" },
    { "verifytxoutproof", &verifytxoutproof, cat_wallet, &verifytxoutproof_helpman, heritage_pure_upstream, "manual" },
    { "getreceivedbyaccount",    &getreceivedbyaccount,    cat_wallet, &getreceivedbyaccount_helpman, heritage_pure_gridcoin, "" },
    { "getreceivedbyaddress",    &getreceivedbyaddress,    cat_wallet, &getreceivedbyaddress_helpman, heritage_mixed, "abf78a6d1f07" },
    { "getreceivedbylabel",      &getreceivedbylabel,      cat_wallet, &getreceivedbylabel_helpman, heritage_mixed, "7c0f5e283380" },
    { "gettransaction",          &gettransaction,          cat_wallet, &gettransaction_helpman, heritage_mixed, "b82e0de9cd38" },
    { "abandontransaction",      &abandontransaction,      cat_wallet, &abandontransaction_helpman, heritage_pure_upstream, "8a15aece8d9f" },
    { "getunconfirmedbalance",   &getunconfirmedbalance,   cat_wallet, &getunconfirmedbalance_helpman, heritage_removed_upstream, "e169db2f48c0" },
    { "getwalletinfo",           &getwalletinfo,           cat_wallet, &getwalletinfo_helpman, heritage_mixed, "693ce305655c" },
    { "importprivkey",           &importprivkey,           cat_wallet, &importprivkey_helpman, heritage_removed_upstream, "907321bb2292" },
    { "importwallet",            &importwallet,            cat_wallet, &importwallet_helpman, heritage_removed_upstream, "eaad3c94f6a6" },
    { "inspectwalletstate",      &inspectwalletstate,      cat_wallet, &inspectwalletstate_helpman, heritage_pure_gridcoin, "" },
    { "keypoolrefill",           &keypoolrefill,           cat_wallet, &keypoolrefill_helpman, heritage_pure_upstream, "90973fb49630" },
    { "listaccounts",            &listaccounts,            cat_wallet, &listaccounts_helpman, heritage_pure_gridcoin, "" },
    { "listaddressgroupings",    &listaddressgroupings,    cat_wallet, &listaddressgroupings_helpman, heritage_mixed, "manual" },
    { "listlabels", &listlabels, cat_wallet, &listlabels_helpman, heritage_mixed, "manual" },
    { "listreceivedbyaccount",   &listreceivedbyaccount,   cat_wallet, &listreceivedbyaccount_helpman, heritage_pure_gridcoin, "" },
    { "listreceivedbyaddress",   &listreceivedbyaddress,   cat_wallet, &listreceivedbyaddress_helpman, heritage_mixed, "4b8fb0ed2cee" },
    { "listreceivedbylabel",     &listreceivedbylabel,     cat_wallet, &listreceivedbylabel_helpman, heritage_mixed, "4b8fb0ed2cee" },
    { "listsinceblock",          &listsinceblock,          cat_wallet, &listsinceblock_helpman, heritage_mixed, "4ac802bc4b7c" },
    { "liststakes",              &liststakes,              cat_wallet, &liststakes_helpman, heritage_pure_gridcoin, "" },
    { "listtransactions",        &listtransactions,        cat_wallet, &listtransactions_helpman, heritage_mixed, "60208a325cb1" },
    { "listunspent",             &listunspent,             cat_wallet, &listunspent_helpman, heritage_mixed, "55713032beff" },
    { "consolidateunspent",      &consolidateunspent,      cat_wallet, &consolidateunspent_helpman, heritage_pure_gridcoin, "" },
    { "makekeypair",             &makekeypair,             cat_wallet, &makekeypair_helpman, heritage_pure_gridcoin, "" },
    { "maintainbackups",         &maintainbackups,         cat_wallet, &maintainbackups_helpman, heritage_pure_gridcoin, "" },
    { "migratelabels", &migratelabels, cat_wallet, &migratelabels_helpman, heritage_pure_gridcoin, "" },
    { "move",                    &movecmd,                 cat_wallet, &movecmd_helpman, heritage_pure_gridcoin, "" },
    { "rainbymagnitude",         &rainbymagnitude,         cat_wallet, &rainbymagnitude_helpman, heritage_pure_gridcoin, "" },
    { "refundhtlc",             &refundhtlc,              cat_wallet, &refundhtlc_helpman, heritage_pure_gridcoin, "" },
    { "repairwallet",            &repairwallet,            cat_wallet, &repairwallet_helpman, heritage_pure_gridcoin, "" },
    { "resendtx",                &resendtx,                cat_wallet, &resendtx_helpman, heritage_removed_upstream, "e169db2f48c0" },
    { "reservebalance",          &reservebalance,          cat_wallet, &reservebalance_helpman, heritage_pure_gridcoin, "" },
    { "scanforunspent",          &scanforunspent,          cat_wallet, &scanforunspent_helpman, heritage_pure_gridcoin, "" },
    { "sendfrom",                &sendfrom,                cat_wallet, &sendfrom_helpman, heritage_pure_gridcoin, "" },
    { "sendmany",                &sendmany,                cat_wallet, &sendmany_helpman, heritage_mixed, "c48e33af5e40" },
    { "sendrawtransaction",      &sendrawtransaction,      cat_wallet, &sendrawtransaction_helpman, heritage_mixed, "52fce61c2242" },
    { "sendtoaddress",           &sendtoaddress,           cat_wallet, &sendtoaddress_helpman, heritage_mixed, "ce89abdb7a10" },
    { "setaccount",              &setaccount,              cat_wallet, &setaccount_helpman, heritage_pure_gridcoin, "" },
    { "setlabel", &setlabel, cat_wallet, &setlabel_helpman, heritage_pure_upstream, "737f242a3755" },
    { "sethdseed",               &sethdseed,               cat_wallet, &sethdseed_helpman, heritage_removed_upstream, "dab6675cb642" },
    { "settxfee",                &settxfee,                cat_wallet, &settxfee_helpman, heritage_removed_upstream, "468a6a9ff691" },
    { "stakelimit",              &stakelimit,              cat_wallet, &stakelimit_helpman, heritage_pure_gridcoin, "" },
    { "signmessage",             &signmessage,             cat_wallet, &signmessage_helpman, heritage_mixed, "6b1488c62c48" },
    { "signrawtransaction",      &signrawtransaction,      cat_wallet, &signrawtransaction_helpman, heritage_removed_upstream, "d75693d24eec" },
    { "signrawtransactionwithkey",    &signrawtransactionwithkey,    cat_wallet, &signrawtransactionwithkey_helpman, heritage_mixed, "6aca6f6eb6c9" },
    { "signrawtransactionwithwallet", &signrawtransactionwithwallet, cat_wallet, &signrawtransactionwithwallet_helpman, heritage_mixed, "1d9359847326" },
    { "upgradewallet",           &upgradewallet,           cat_wallet, &upgradewallet_helpman, heritage_removed_upstream, "2b104a7e47cc" },
    { "validateaddress",         &validateaddress,         cat_wallet, &validateaddress_helpman, heritage_mixed, "97873d91c025" },
    { "validatepubkey",          &validatepubkey,          cat_wallet, &validatepubkey_helpman, heritage_pure_gridcoin, "" },
    { "verifymessage",           &verifymessage,           cat_wallet, &verifymessage_helpman, heritage_mixed, "c9cdde03d99c" },
    { "walletlock",              &walletlock,              cat_wallet, &walletlock_helpman, heritage_pure_upstream, "e169db2f48c0" },
    { "walletpassphrase",        &walletpassphrase,        cat_wallet, &walletpassphrase_helpman, heritage_mixed, "2c3e2b290e76" },
    { "walletpassphrasechange",  &walletpassphrasechange,  cat_wallet, &walletpassphrasechange_helpman, heritage_pure_upstream, "ab947140d4c5" },
    { "walletdiagnose",          &walletdiagnose,          cat_wallet, &walletdiagnose_helpman, heritage_pure_gridcoin, "" },

  // PSGT commands
    { "analyzepsgt",             &analyzepsgt,             cat_wallet, &analyzepsgt_helpman, heritage_mixed, "96a51f778705" },
    { "createpsgt",              &createpsgt,              cat_wallet, &createpsgt_helpman, heritage_mixed, "d834d7a48b91" },
    { "decodepsgt",              &decodepsgt,              cat_wallet, &decodepsgt_helpman, heritage_mixed, "b20178d7ef4b" },
    { "combinepsgt",             &combinepsgt,             cat_wallet, &combinepsgt_helpman, heritage_mixed, "b8c2d95892e8" },
    { "finalizepsgt",            &finalizepsgt,            cat_wallet, &finalizepsgt_helpman, heritage_mixed, "bc3c28ffc9eb" },
    { "walletprocesspsgt",       &walletprocesspsgt,       cat_wallet, &walletprocesspsgt_helpman, heritage_mixed, "07d707e85e80" },
    { "utxoupdatepsgt",          &utxoupdatepsgt,          cat_wallet, &utxoupdatepsgt_helpman, heritage_mixed, "bcc738b59c8b" },
    { "converttopsgt",           &converttopsgt,           cat_wallet, &converttopsgt_helpman, heritage_mixed, "633cdf5f2833" },
    { "walletcreatefundedpsgt",  &walletcreatefundedpsgt,  cat_wallet, &walletcreatefundedpsgt_helpman, heritage_mixed, "d2f84eca950d" },

  // PSGT pool commands (#2910 Phase II)
    { "submitpsgt",              &submitpsgt,              cat_wallet, &submitpsgt_helpman, heritage_pure_gridcoin, "" },
    { "listpsgtpool",            &listpsgtpool,            cat_wallet, &listpsgtpool_helpman, heritage_pure_gridcoin, "" },
    { "signpsgtinpool",          &signpsgtinpool,          cat_wallet, &signpsgtinpool_helpman, heritage_pure_gridcoin, "" },
    { "removepsgtfrompool",      &removepsgtfrompool,      cat_wallet, &removepsgtfrompool_helpman, heritage_pure_gridcoin, "" },
    { "getpsgtpoolinfo",         &getpsgtpoolinfo,         cat_wallet, &getpsgtpoolinfo_helpman, heritage_pure_gridcoin, "" },

  // Staking commands
    { "advertisebeacon",         &advertisebeacon,         cat_staking, &advertisebeacon_helpman, heritage_pure_gridcoin, "" },
    { "advertisebeaconv3",       &advertisebeaconv3,       cat_staking, &advertisebeaconv3_helpman, heritage_pure_gridcoin, "" },
    { "beaconauth",              &beaconauth,              cat_staking, &beaconauth_helpman, heritage_pure_gridcoin, "" },
    { "beaconconvergence",       &beaconconvergence,       cat_staking, &beaconconvergence_helpman, heritage_pure_gridcoin, "" },
    { "beaconreport",            &beaconreport,            cat_staking, &beaconreport_helpman, heritage_pure_gridcoin, "" },
    { "beaconstatus",            &beaconstatus,            cat_staking, &beaconstatus_helpman, heritage_pure_gridcoin, "" },
    { "createmrcrequest",        &createmrcrequest,        cat_staking, &createmrcrequest_helpman, heritage_pure_gridcoin, "" },
    { "explainmagnitude",        &explainmagnitude,        cat_staking, &explainmagnitude_helpman, heritage_pure_gridcoin, "" },
    { "generate",                &generate,                cat_staking, &generate_helpman, heritage_pure_gridcoin, "" },
    { "generatetoaddress",       &generatetoaddress,       cat_staking, &generatetoaddress_helpman, heritage_mixed, "manual" },
    { "generatesuperblock",      &generatesuperblock,      cat_staking, &generatesuperblock_helpman, heritage_pure_gridcoin, "" },
    { "getlaststake",            &getlaststake,            cat_staking, &getlaststake_helpman, heritage_pure_gridcoin, "" },
    { "getmrcinfo",              &getmrcinfo,              cat_staking, &getmrcinfo_helpman, heritage_pure_gridcoin, "" },
    { "getstakinginfo",          &getstakinginfo,          cat_staking, &getstakinginfo_helpman, heritage_pure_gridcoin, "" },
    { "getmininginfo",           &getstakinginfo,          cat_staking, &getstakinginfo_helpman, heritage_pure_gridcoin, "" }, //alias for getstakinginfo (compatibility)
    { "lifetime",                &lifetime,                cat_staking, &lifetime_helpman, heritage_pure_gridcoin, "" },
    { "magnitude",               &magnitude,               cat_staking, &magnitude_helpman, heritage_pure_gridcoin, "" },
    { "pendingbeaconreport",     &pendingbeaconreport,     cat_staking, &pendingbeaconreport_helpman, heritage_pure_gridcoin, "" },
    { "resetcpids",              &resetcpids,              cat_staking, &resetcpids_helpman, heritage_pure_gridcoin, "" },
    { "revokebeacon",            &revokebeacon,            cat_staking, &revokebeacon_helpman, heritage_pure_gridcoin, "" },
    { "superblockage",           &superblockage,           cat_staking, &superblockage_helpman, heritage_pure_gridcoin, "" },
    { "superblocks",             &superblocks,             cat_staking, &superblocks_helpman, heritage_pure_gridcoin, "" },

  // Developer commands
    { "auditsnapshotaccrual",    &auditsnapshotaccrual,    cat_developer, &auditsnapshotaccrual_helpman, heritage_pure_gridcoin, "" },
    { "auditsnapshotaccruals",   &auditsnapshotaccruals,   cat_developer, &auditsnapshotaccruals_helpman, heritage_pure_gridcoin, "" },
    { "addkey",                  &addkey,                  cat_developer, &addkey_helpman, heritage_pure_gridcoin, "" },
    { "registerpool", &registerpool, cat_staking, &registerpool_helpman, heritage_pure_gridcoin, "" },
    { "withdrawpool", &withdrawpool, cat_staking, &withdrawpool_helpman, heritage_pure_gridcoin, "" },
    { "approvepool", &approvepool, cat_developer, &approvepool_helpman, heritage_pure_gridcoin, "" },
    { "authorizepool", &authorizepool, cat_developer, &authorizepool_helpman, heritage_pure_gridcoin, "" },
    { "removepool", &removepool, cat_developer, &removepool_helpman, heritage_pure_gridcoin, "" },
    { "listpools", &listpools, cat_developer, &listpools_helpman, heritage_pure_gridcoin, "" },
    { "beaconaudit",             &beaconaudit,             cat_developer, &beaconaudit_helpman, heritage_pure_gridcoin, "" },
    { "changesettings",          &changesettings,          cat_developer, &changesettings_helpman, heritage_pure_gridcoin, "" },
    { "currentcontractaverage",  &currentcontractaverage,  cat_developer, &currentcontractaverage_helpman, heritage_pure_gridcoin, "" },
    { "debug",                   &debug,                   cat_developer, &debug_helpman, heritage_pure_gridcoin, "" },
    { "dumpcontracts",           &dumpcontracts,           cat_developer, &dumpcontracts_helpman, heritage_pure_gridcoin, "" },
    { "exportstats1",            &rpc_exportstats,         cat_developer, &rpc_exportstats_helpman, heritage_pure_gridcoin, "" },
    { "getblockstats",           &rpc_getblockstats,       cat_developer, &rpc_getblockstats_helpman, heritage_pure_gridcoin, "" },
    { "getrecentblocks",         &rpc_getrecentblocks,     cat_developer, &rpc_getrecentblocks_helpman, heritage_pure_gridcoin, "" },
    { "getrawprojectstatus",     &getrawprojectstatus,     cat_developer, &getrawprojectstatus_helpman, heritage_pure_gridcoin, "" },
    { "inspectaccrualsnapshot",  &inspectaccrualsnapshot,  cat_developer, &inspectaccrualsnapshot_helpman, heritage_pure_gridcoin, "" },
    { "listalerts",              &listalerts,              cat_developer, &listalerts_helpman, heritage_removed_upstream, "1b838be933d7" },
    { "listprojects",            &listprojects,            cat_developer, &listprojects_helpman, heritage_pure_gridcoin, "" },
    { "getautogreylist",         &getautogreylist,         cat_developer, &getautogreylist_helpman, heritage_pure_gridcoin, "" },
    { "listprotocolentries",     &listprotocolentries,     cat_developer, &listprotocolentries_helpman, heritage_pure_gridcoin, "" },
    { "listresearcheraccounts",  &listresearcheraccounts,  cat_developer, &listresearcheraccounts_helpman, heritage_pure_gridcoin, "" },
    { "listscrapers",            &listscrapers,            cat_developer, &listscrapers_helpman, heritage_pure_gridcoin, "" },
    { "listsidestakes",          &listsidestakes,           cat_developer, &listsidestakes_helpman, heritage_pure_gridcoin, "" },
    { "listmandatorysidestakes", &listmandatorysidestakes, cat_developer, &listmandatorysidestakes_helpman, heritage_pure_gridcoin, "" },
    { "listsettings",            &listsettings,            cat_developer, &listsettings_helpman, heritage_pure_gridcoin, "" },
    { "logging",                 &logging,                 cat_developer, &logging_helpman, heritage_pure_upstream, "manual" },
    { "network",                 &network,                 cat_developer, &network_helpman, heritage_pure_gridcoin, "" },
    { "parseaccrualsnapshotfile",&parseaccrualsnapshotfile,cat_developer, &parseaccrualsnapshotfile_helpman, heritage_pure_gridcoin, "" },
    { "parselegacysb",           &parselegacysb,           cat_developer, &parselegacysb_helpman, heritage_pure_gridcoin, "" },
    { "projects",                &projects,                cat_developer, &projects_helpman, heritage_pure_gridcoin, "" },
    { "readdata",                &readdata,                cat_developer, &readdata_helpman, heritage_pure_gridcoin, "" },
    { "reorganize",              &rpc_reorganize,          cat_developer, &rpc_reorganize_helpman, heritage_pure_gridcoin, "" },
    { "sendalert",               &sendalert,               cat_developer, &sendalert_helpman, heritage_removed_upstream, "23feaca9b1f5" },
    { "sendalert2",              &sendalert2,              cat_developer, &sendalert2_helpman, heritage_removed_upstream, "088679c27b11" },
    { "sendblock",               &sendblock,               cat_developer, &sendblock_helpman, heritage_pure_gridcoin, "" },
    { "superblockaverage",       &superblockaverage,       cat_developer, &superblockaverage_helpman, heritage_pure_gridcoin, "" },
    { "versionreport",           &versionreport,           cat_developer, &versionreport_helpman, heritage_pure_gridcoin, "" },
    { "writedata",               &writedata,               cat_developer, &writedata_helpman, heritage_pure_gridcoin, "" },

    { "listmanifests",           &listmanifests,           cat_developer, &listmanifests_helpman, heritage_pure_gridcoin, "" },
    { "getmpart",                &getmpart,                cat_developer, &getmpart_helpman, heritage_pure_gridcoin, "" },
    { "sendscraperfilemanifest", &sendscraperfilemanifest, cat_developer, &sendscraperfilemanifest_helpman, heritage_pure_gridcoin, "" },
    { "savescraperfilemanifest", &savescraperfilemanifest, cat_developer, &savescraperfilemanifest_helpman, heritage_pure_gridcoin, "" },
    { "deletecscrapermanifest",  &deletecscrapermanifest,  cat_developer, &deletecscrapermanifest_helpman, heritage_pure_gridcoin, "" },
    { "archivelog",              &archivelog,              cat_developer, &archivelog_helpman, heritage_pure_gridcoin, "" },
    { "testnewsb",               &testnewsb,               cat_developer, &testnewsb_helpman, heritage_pure_gridcoin, "" },
    { "convergencereport",       &convergencereport,       cat_developer, &convergencereport_helpman, heritage_pure_gridcoin, "" },
    { "scraperreport",           &scraperreport,           cat_developer, &scraperreport_helpman, heritage_pure_gridcoin, "" },

  // Network commands
    { "addnode",                 &addnode,                 cat_network, &addnode_helpman, heritage_mixed, "00f3ee2c1fd4" },
    { "askforoutstandingblocks", &askforoutstandingblocks, cat_network, &askforoutstandingblocks_helpman, heritage_pure_gridcoin, "" },
    { "getblockchaininfo",       &getblockchaininfo,       cat_network, &getblockchaininfo_helpman, heritage_mixed, "1d89fd23ddb0" },
    { "getnetworkinfo",          &getnetworkinfo,          cat_network, &getnetworkinfo_helpman, heritage_mixed, "54255a461949" },
    { "clearbanned",             &clearbanned,             cat_network, &clearbanned_helpman, heritage_pure_upstream, "e169db2f48c0" },
    { "currenttime",             &currenttime,             cat_network, &currenttime_helpman, heritage_pure_gridcoin, "" },
    { "getaddednodeinfo",        &getaddednodeinfo,        cat_network, &getaddednodeinfo_helpman, heritage_mixed, "3825ee523156" },
    { "getnodeaddresses",        &getnodeaddresses,        cat_network, &getnodeaddresses_helpman, heritage_mixed, "c696e3da4da2" },
    { "getbestblockhash",        &getbestblockhash,        cat_network, &getbestblockhash_helpman, heritage_pure_upstream, "e169db2f48c0" },
    { "getblock",                &getblock,                cat_network, &getblock_helpman, heritage_mixed, "ef9d1e297dba" },
    { "getblockbynumber",        &getblockbynumber,        cat_network, &getblockbynumber_helpman, heritage_pure_gridcoin, "" },
    { "getblockbymintime",       &getblockbymintime,       cat_network, &getblockbymintime_helpman, heritage_pure_gridcoin, "" },
    { "getblocksbatch",          &getblocksbatch,          cat_network, &getblocksbatch_helpman, heritage_pure_gridcoin, "" },
    { "getblockcount",           &getblockcount,           cat_network, &getblockcount_helpman, heritage_pure_upstream, "e169db2f48c0" },
    { "setmocktime",             &setmocktime,             cat_developer, &setmocktime_helpman, heritage_mixed, "994b2dfc6e39" },
    { "getblockhash",            &getblockhash,            cat_network, &getblockhash_helpman, heritage_pure_upstream, "bd157738fbdb" },
    { "getburnreport",           &getburnreport,           cat_network, &getburnreport_helpman, heritage_pure_gridcoin, "" },
    { "getcheckpoint",           &getcheckpoint,           cat_network, &getcheckpoint_helpman, heritage_pure_gridcoin, "" },
    { "getconnectioncount",      &getconnectioncount,      cat_network, &getconnectioncount_helpman, heritage_pure_upstream, "e169db2f48c0" },
    { "getdifficulty",           &getdifficulty,           cat_network, &getdifficulty_helpman, heritage_mixed, "400047400aac" },
    { "getinfo",                 &getinfo,                 cat_network, &getinfo_helpman, heritage_removed_upstream, "1e5e3bf87281" },
    { "getnettotals",            &getnettotals,            cat_network, &getnettotals_helpman, heritage_mixed, "ca8fae8b33d9" },
    { "getpeerinfo",             &getpeerinfo,             cat_network, &getpeerinfo_helpman, heritage_mixed, "4720f2ba0e5d" },
    { "getrawmempool",           &getrawmempool,           cat_network, &getrawmempool_helpman, heritage_mixed, "7aaf954ee6c6" },
    { "getmempoolentry", &getmempoolentry, cat_network, &getmempoolentry_helpman, heritage_mixed, "a0a4faa15dda" },
    { "getmempoolinfo", &getmempoolinfo, cat_network, &getmempoolinfo_helpman, heritage_mixed, "2eb7e5d70d20" },
    { "testmempoolaccept", &testmempoolaccept, cat_network, &testmempoolaccept_helpman, heritage_mixed, "3119a888d278" },
    { "listbanned",              &listbanned,              cat_network, &listbanned_helpman, heritage_mixed, "2c86549f9bbd" },
    { "networktime",             &networktime,             cat_network, &networktime_helpman, heritage_pure_gridcoin, "" },
    { "ping",                    &ping,                    cat_network, &ping_helpman, heritage_pure_upstream, "e169db2f48c0" },
    { "setban",                  &setban,                  cat_network, &setban_helpman, heritage_mixed, "b5a439644c97" },
    { "showblock",               &showblock,               cat_network, &showblock_helpman, heritage_pure_gridcoin, "" },
    { "stop",                    &stop,                    cat_network, &stop_helpman, heritage_pure_upstream, "e169db2f48c0" },

  // Voting commands
    { "addpoll",                 &addpoll,                 cat_voting, &addpoll_helpman, heritage_pure_gridcoin, "" },
    { "getpollresults",          &getpollresults,          cat_voting, &getpollresults_helpman, heritage_pure_gridcoin, "" },
    { "getvotingclaim",          &getvotingclaim,          cat_voting, &getvotingclaim_helpman, heritage_pure_gridcoin, "" },
    { "listpolls",               &listpolls,               cat_voting, &listpolls_helpman, heritage_pure_gridcoin, "" },
    { "testpollnotification",    &testpollnotification,    cat_voting, &testpollnotification_helpman, heritage_pure_gridcoin, "" },
    { "vote",                    &vote,                    cat_voting, &vote_helpman, heritage_pure_gridcoin, "" },
    { "votebyid",                &votebyid,                cat_voting, &votebyid_helpman, heritage_pure_gridcoin, "" },
    { "votedetails",             &votedetails,             cat_voting, &votedetails_helpman, heritage_pure_gridcoin, "" },
};

static constexpr const char* DEPRECATED_RPCS[] {
        "debug",
        "getaccount",
        "getaccountaddress",
        "getaddressesbyaccount",
        "getreceivedbyaccount",
        "listaccounts",
        "listreceivedbyaccount",
        "move",
        "setaccount",
        "signrawtransaction",
        "vote",
};

CRPCTable::CRPCTable()
{
    for (const auto& cmd : vRPCCommands)
    {
        mapCommands[cmd.name] = &cmd;
    }
}

const CRPCCommand* CRPCTable::operator[](string name) const
{
    auto it = mapCommands.find(name);
    if (it == mapCommands.end()) {
        return nullptr;
    }
    return it->second;
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
     && (address.to_v6() <= boost::asio::ip::make_address_v6("::ffff:ffff")
      || address.to_v6().is_v4_mapped())
     && !(address.to_v6() == asio::ip::address_v6::loopback() || address.to_v6() == asio::ip::make_address_v6("::"))) {
        auto address6 = address.to_v6();
        auto bytes = address6.to_bytes();

        return ClientAllowed(boost::asio::ip::address_v4({bytes[12], bytes[13], bytes[14], bytes[15]}));
    }

    if (address == asio::ip::address_v4::loopback()
     || address == asio::ip::address_v6::loopback()
     || (address.is_v4()
         // Check whether IPv4 addresses match 127.0.0.0/8 (loopback subnet)
      && (address.to_v4().to_bytes()[0] == 127)))
        return true;

    const string strAddress = address.to_string();
    const vector<string>& vAllow = gArgs.GetArgs("-rpcallowip");
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
        // Register before servicing so a concurrent StopRPCThreads() can wake us
        // out of the blocking read loop. If the server is already stopping,
        // RegisterRPCConnection() returns false and we skip servicing entirely
        // rather than park with no drain left to interrupt us (issue #3123).
        else if (RegisterRPCConnection(conn))
        {
            ServiceConnection(conn);
            UnregisterRPCConnection(conn);
        }

        conn->close();
    }

    delete conn;
}

void StartRPCThreads()
{
    // Logged unconditionally: the absence of this line in a node's debug.log
    // distinguishes "RPC server never started" from "started but not serving"
    // when diagnosing functional-test RPC-startup timeouts.
    LogPrintf("StartRPCThreads: entered\n");

    strRPCUserColonPass = gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");

    if ((gArgs.GetArg("-rpcpassword", "") == "" ||
        (gArgs.GetArg("-rpcuser", "") == gArgs.GetArg("-rpcpassword", ""))))
    {
        unsigned char rand_pwd[32];
        GetRandBytes({rand_pwd, sizeof(rand_pwd)});
        string strWhatAmI = "To use gridcoind";
        if (gArgs.IsArgSet("-server"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-server\"");
        else if (gArgs.IsArgSet("-daemon"))
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
                _("Error"), CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }

    const bool fUseSSL = gArgs.GetBoolArg("-rpcssl");

    assert(rpc_io_service == nullptr);

    // Clear the shutdown latch so a same-process restart (stop then start) can
    // service connections again; StopRPCThreads() sets it true (issue #3123).
    // The set is already empty here (the prior StopRPCThreads()' join_all()
    // guarantees every registered connection was unregistered), but clear it
    // defensively so a future refactor that broke that invariant cannot leave a
    // dangling pointer to be shutdown() on the next teardown.
    {
        std::lock_guard<std::mutex> lock(g_rpc_connections_mutex);
        g_rpc_connections_stopped = false;
        g_rpc_connections.clear();
    }

    rpc_io_service = new ioContext();
    rpc_ssl_context = new ssl::context(ssl::context::sslv23);

    if (fUseSSL)
    {
        rpc_ssl_context->set_options(ssl::context::no_sslv2);

        fs::path pathCertFile(gArgs.GetArg("-rpcsslcertificatechainfile", "server.cert"));
        if (!pathCertFile.is_absolute()) pathCertFile = fs::path(GetDataDir()) / pathCertFile;
        if (fs::exists(pathCertFile)) rpc_ssl_context->use_certificate_chain_file(pathCertFile.string());
        else LogPrintf("ThreadRPCServer ERROR: missing server certificate file %s\n", pathCertFile.string());

        fs::path pathPKFile(gArgs.GetArg("-rpcsslprivatekeyfile", "server.pem"));
        if (!pathPKFile.is_absolute()) pathPKFile = fs::path(GetDataDir()) / pathPKFile;
        if (fs::exists(pathPKFile)) rpc_ssl_context->use_private_key_file(pathPKFile.string(), ssl::context::pem);
        else LogPrintf("ThreadRPCServer ERROR: missing server private key file %s\n", pathPKFile.string());

        string strCiphers = gArgs.GetArg("-rpcsslciphers", "TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH");
        SSL_CTX_set_cipher_list(rpc_ssl_context->native_handle(), strCiphers.c_str());
    }

    // Try a dual IPv6/IPv4 socket, falling back to separate IPv4 and IPv6 sockets
    const bool loopback = !gArgs.IsArgSet("-rpcallowip");
    asio::ip::address bindAddress = loopback ? asio::ip::address_v6::loopback() : asio::ip::address_v6::any();
    ip::tcp::endpoint endpoint(bindAddress, gArgs.GetArg("-rpcport", GetDefaultRPCPort()));
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
        acceptor->listen(socket_base::max_listen_connections);

        RPCListen(acceptor, *rpc_ssl_context, fUseSSL);
        rpc_acceptors.push_back(acceptor);
        LogPrintf("RPC: bound and listening on [%s]:%u\n", endpoint.address().to_string(), endpoint.port());

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
            acceptor->listen(socket_base::max_listen_connections);

            RPCListen(acceptor, *rpc_ssl_context, fUseSSL);
            rpc_acceptors.push_back(acceptor);
            LogPrintf("RPC: bound and listening on %s:%u\n", endpoint.address().to_string(), endpoint.port());

            fListening = true;
        }
    }
    catch(boost::system::system_error &e)
    {
        strerr = strprintf(_("An error occurred while setting up the RPC port %u for listening on IPv4: %s"), endpoint.port(), e.what());
    }

    if (!fListening)
    {
        // Log before the message box: ThreadSafeMessageBox has no UI in daemon
        // mode, so without this the failure reason never reaches debug.log and
        // the node exits (via StartShutdown) with no diagnosable cause.
        LogPrintf("ERROR: StartRPCThreads: RPC server failed to listen: %s\n", strerr);
        uiInterface.ThreadSafeMessageBox(strerr, _("Error"), CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }

    const int nRPCThreads = gArgs.GetArg("-rpcthreads", 4);
    rpc_worker_group = new boost::thread_group();
    for (int i = 0; i < nRPCThreads; i++)
        rpc_worker_group->create_thread(boost::bind(&ioContext::run, rpc_io_service));

    LogPrintf("RPC server started: %u acceptor(s), %d worker thread(s)\n",
              static_cast<unsigned>(rpc_acceptors.size()), nRPCThreads);
}

void StopRPCThreads()
{
    LogPrintf("Stop RPC IO service\n");
    if(!rpc_io_service)
    {
        LogPrintf("RPC IO server not started\n");
        return;
    }

    // Close the listening acceptors before stopping the io_service. This makes
    // any pending async_accept complete with operation_aborted (RPCAcceptHandler
    // then sees !is_open() and does not re-arm), so no further connection is
    // accepted. This is necessary but not sufficient to unblock join_all() --
    // connections already being serviced are handled by the interrupt loop
    // below (issue #3123).
    for (auto& acc : rpc_acceptors) {
        if (acc && acc->is_open()) {
            boost::system::error_code ec;
            acc->close(ec);
        }
    }
    rpc_acceptors.clear();

    // Closing the acceptors above only stops *new* connections. An established
    // keep-alive client leaves its worker blocked in a synchronous read inside
    // ServiceConnection(), which io_service->stop() cannot interrupt (that
    // worker is not in the io_service run loop -- it is parked in recv). Shut
    // those sockets down so the reads fail and the workers return; otherwise
    // join_all() below hangs forever (issue #3123). Setting the stopped flag
    // under the same lock closes the race with a connection accepted just now:
    // its RegisterRPCConnection() will observe the flag and decline to park.
    {
        std::lock_guard<std::mutex> lock(g_rpc_connections_mutex);
        g_rpc_connections_stopped = true;
        for (AcceptedConnection* conn : g_rpc_connections) {
            conn->interrupt();
        }
    }

    rpc_io_service->stop();
    if (rpc_worker_group != nullptr) {
        rpc_worker_group->join_all();
    }

    delete rpc_worker_group;
    rpc_worker_group = nullptr;
    delete rpc_ssl_context;
    rpc_ssl_context = nullptr;
    delete rpc_io_service;
    rpc_io_service = nullptr;
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
            if (gArgs.GetArgs("-rpcpassword").size() < 20)
                UninterruptibleSleep(std::chrono::milliseconds{250});

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

    // arity here and throw the help text on mismatch. PR M3 dropped the
    // `bool fHelp` parameter entirely, so the dispatcher is now the sole
    // arity-enforcement site for converted commands. Commands marked
    // variadic via RPCHelpMan's MarkVariadic() opt out of the pre-check
    // (their helpman declaration captures the typical shape but the body
    // accepts an open-ended count); their body retains the arity gate
    // appropriate to the actual semantics. addpoll's helpman is itself
    // marked variadic so the `addpoll <type>` 1-arg wizard form reaches
    // its body's hint logic.
    if (pcmd->helpman != nullptr) {
        const RPCHelpMan& help = pcmd->helpman();
        if (!help.IsVariadic() && !help.IsValidNumArgs(params.size())) {
            throw runtime_error(help.ToString());
        }
    }

    // Let's add an optional display if BCLog::LogFlags::RPC is set to show how long it takes
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
            result = pcmd->actor(params);
            nRPCtimetotal = GetTimeMillis() - nRPCtimebegin;
            LogPrintf("RPCTime : Command %s -> Totaltime %" PRId64 "ms", strMethod, nRPCtimetotal);
        }
        else
            result = pcmd->actor(params);

        return result;
    }
    catch (std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}


std::vector<std::string> CRPCTable::listCommands(bool include_deprecated) const
{
    std::vector<std::string> commandList;
    typedef std::map<std::string, const CRPCCommand*> commandMap;

    std::transform( mapCommands.begin(), mapCommands.end(),
                    std::back_inserter(commandList),
                    boost::bind(&commandMap::value_type::first,boost::placeholders::_1) );
    if (!include_deprecated) {
        // remove deprecated commands from autocomplete
        for(auto &command: DEPRECATED_RPCS) {
            commandList.erase(std::remove(commandList.begin(), commandList.end(), command), commandList.end());
        }
    }
    return commandList;
}

#ifdef TEST
int main(int argc, char *argv[])
{
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFile("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
#endif
    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    try
    {
        if (argc >= 2 && string(argv[1]) == "-server")
        {
            LogPrintf("server ready");
            ThreadRPCServer(nullptr);
        }
        else
        {
            return CommandLineRPC(argc, argv);
        }
    }
    catch (std::exception& e) {
        PrintException(&e, "main()");
    } catch (...) {
        PrintException(nullptr, "main()");
    }
    return 0;
}
#endif

const CRPCTable tableRPC;
