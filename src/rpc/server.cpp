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

using namespace std;
using namespace boost;
using namespace boost::asio;

void ThreadRPCServer2(void* parg);

static std::string strRPCUserColonPass;

// These are created by StartRPCThreads, destroyed in StopRPCThreads
static ioContext* rpc_io_service = nullptr;
static ssl::context* rpc_ssl_context = nullptr;
static boost::thread_group* rpc_worker_group = nullptr;

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

        string strHelp;
        if (pcmd->helpman != nullptr) {
            // Modern path: pull help text directly from the RPCHelpMan
            // accessor without invoking the command body or using
            // throw-as-control-flow.
            strHelp = pcmd->helpman().ToString();
        } else {
            // Legacy fallback: call the actor with fHelp=true and capture the
            // help text from the thrown runtime_error. Retained for commands
            // that have not yet been converted to the RPCHelpMan pattern
            // (PSGT and a handful of stragglers).
            try {
                UniValue params(UniValue::VARR);
                (*pcmd->actor)(params, true);
            } catch (std::exception& e) {
                strHelp = string(e.what());
            }
        }
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

UniValue help(const UniValue& params, bool fHelp)
{
    const RPCHelpMan& help_rpc = help_helpman();
    if (fHelp || !help_rpc.IsValidNumArgs(params.size()))
        throw runtime_error(help_rpc.ToString());

    string strCommand;

    if (params.size() > 0)
        strCommand = params[0].get_str();

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

UniValue stop(const UniValue& params, bool fHelp)
{
    const RPCHelpMan& help = stop_helpman();
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

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
    { "help",                    &help,                    cat_null, &help_helpman          },

  // Wallet commands
    { "addmultisigaddress",      &addmultisigaddress,      cat_wallet, &addmultisigaddress_helpman        },
    { "addredeemscript",         &addredeemscript,         cat_wallet, &addredeemscript_helpman        },
    { "backupwallet",            &backupwallet,            cat_wallet, &backupwallet_helpman        },
    { "burn",                    &burn,                    cat_wallet, &burn_helpman        },
    { "checkwallet",             &checkwallet,             cat_wallet, &checkwallet_helpman        },
    { "claimhtlc",              &claimhtlc,               cat_wallet, &claimhtlc_helpman        },
    { "createhtlc",             &createhtlc,              cat_wallet, &createhtlc_helpman        },
    { "createrawtransaction",    &createrawtransaction,    cat_wallet, &createrawtransaction_helpman        },
    { "consolidatemsunspent",    &consolidatemsunspent,    cat_wallet, &consolidatemsunspent_helpman        },
    { "decoderawtransaction",    &decoderawtransaction,    cat_wallet, &decoderawtransaction_helpman        },
    { "decodescript",            &decodescript,            cat_wallet, &decodescript_helpman        },
    { "dumpprivkey",             &dumpprivkey,             cat_wallet, &dumpprivkey_helpman        },
    { "fundrawtransaction",      &fundrawtransaction,      cat_wallet, &fundrawtransaction_helpman        },
    { "dumpwallet",              &dumpwallet,              cat_wallet, &dumpwallet_helpman        },
    { "encryptwallet",           &encryptwallet,           cat_wallet, &encryptwallet_helpman        },
    { "getaccount",              &getaccount,              cat_wallet, &getaccount_helpman        },
    { "getaccountaddress",       &getaccountaddress,       cat_wallet, &getaccountaddress_helpman        },
    { "getaddressesbyaccount",   &getaddressesbyaccount,   cat_wallet, &getaddressesbyaccount_helpman        },
    { "getbalance",              &getbalance,              cat_wallet, &getbalance_helpman        },
    { "getbalancedetail",        &getbalancedetail,        cat_wallet, &getbalancedetail_helpman        },
    { "getnewaddress",           &getnewaddress,           cat_wallet, &getnewaddress_helpman        },
    { "getnewpubkey",            &getnewpubkey,            cat_wallet, &getnewpubkey_helpman        },
    { "getrawtransaction",       &getrawtransaction,       cat_wallet, &getrawtransaction_helpman        },
    { "getrawwallettransaction", &getrawwallettransaction, cat_wallet, &getrawwallettransaction_helpman        },
    { "getreceivedbyaccount",    &getreceivedbyaccount,    cat_wallet, &getreceivedbyaccount_helpman        },
    { "getreceivedbyaddress",    &getreceivedbyaddress,    cat_wallet, &getreceivedbyaddress_helpman        },
    { "gettransaction",          &gettransaction,          cat_wallet, &gettransaction_helpman        },
    { "abandontransaction",      &abandontransaction,      cat_wallet, &abandontransaction_helpman        },
    { "getunconfirmedbalance",   &getunconfirmedbalance,   cat_wallet, &getunconfirmedbalance_helpman        },
    { "getwalletinfo",           &getwalletinfo,           cat_wallet, &getwalletinfo_helpman        },
    { "importprivkey",           &importprivkey,           cat_wallet, &importprivkey_helpman        },
    { "importwallet",            &importwallet,            cat_wallet, &importwallet_helpman        },
    { "inspectwalletstate",      &inspectwalletstate,      cat_wallet, &inspectwalletstate_helpman        },
    { "keypoolrefill",           &keypoolrefill,           cat_wallet, &keypoolrefill_helpman        },
    { "listaccounts",            &listaccounts,            cat_wallet, &listaccounts_helpman        },
    { "listaddressgroupings",    &listaddressgroupings,    cat_wallet, &listaddressgroupings_helpman        },
    { "listreceivedbyaccount",   &listreceivedbyaccount,   cat_wallet, &listreceivedbyaccount_helpman        },
    { "listreceivedbyaddress",   &listreceivedbyaddress,   cat_wallet, &listreceivedbyaddress_helpman        },
    { "listsinceblock",          &listsinceblock,          cat_wallet, &listsinceblock_helpman        },
    { "liststakes",              &liststakes,              cat_wallet, &liststakes_helpman        },
    { "listtransactions",        &listtransactions,        cat_wallet, &listtransactions_helpman        },
    { "listunspent",             &listunspent,             cat_wallet, &listunspent_helpman        },
    { "consolidateunspent",      &consolidateunspent,      cat_wallet, &consolidateunspent_helpman        },
    { "makekeypair",             &makekeypair,             cat_wallet, &makekeypair_helpman        },
    { "maintainbackups",         &maintainbackups,         cat_wallet, &maintainbackups_helpman        },
    { "move",                    &movecmd,                 cat_wallet, &movecmd_helpman        },
    { "rainbymagnitude",         &rainbymagnitude,         cat_wallet, &rainbymagnitude_helpman        },
    { "refundhtlc",             &refundhtlc,              cat_wallet, &refundhtlc_helpman        },
    { "repairwallet",            &repairwallet,            cat_wallet, &repairwallet_helpman        },
    { "resendtx",                &resendtx,                cat_wallet, &resendtx_helpman        },
    { "reservebalance",          &reservebalance,          cat_wallet, &reservebalance_helpman        },
    { "scanforunspent",          &scanforunspent,          cat_wallet, &scanforunspent_helpman        },
    { "sendfrom",                &sendfrom,                cat_wallet, &sendfrom_helpman        },
    { "sendmany",                &sendmany,                cat_wallet, &sendmany_helpman        },
    { "sendrawtransaction",      &sendrawtransaction,      cat_wallet, &sendrawtransaction_helpman        },
    { "sendtoaddress",           &sendtoaddress,           cat_wallet, &sendtoaddress_helpman        },
    { "setaccount",              &setaccount,              cat_wallet, &setaccount_helpman        },
    { "sethdseed",               &sethdseed,               cat_wallet, &sethdseed_helpman        },
    { "settxfee",                &settxfee,                cat_wallet, &settxfee_helpman        },
    { "signmessage",             &signmessage,             cat_wallet, &signmessage_helpman        },
    { "signrawtransaction",      &signrawtransaction,      cat_wallet, &signrawtransaction_helpman        },
    { "signrawtransactionwithkey",    &signrawtransactionwithkey,    cat_wallet, &signrawtransactionwithkey_helpman },
    { "signrawtransactionwithwallet", &signrawtransactionwithwallet, cat_wallet, &signrawtransactionwithwallet_helpman },
    { "upgradewallet",           &upgradewallet,           cat_wallet, &upgradewallet_helpman        },
    { "validateaddress",         &validateaddress,         cat_wallet, &validateaddress_helpman        },
    { "validatepubkey",          &validatepubkey,          cat_wallet, &validatepubkey_helpman        },
    { "verifymessage",           &verifymessage,           cat_wallet, &verifymessage_helpman        },
    { "walletlock",              &walletlock,              cat_wallet, &walletlock_helpman        },
    { "walletpassphrase",        &walletpassphrase,        cat_wallet, &walletpassphrase_helpman        },
    { "walletpassphrasechange",  &walletpassphrasechange,  cat_wallet, &walletpassphrasechange_helpman        },
    { "walletdiagnose",          &walletdiagnose,          cat_wallet, &walletdiagnose_helpman        },

  // PSGT commands
    { "createpsgt",              &createpsgt,              cat_wallet, &createpsgt_helpman        },
    { "decodepsgt",              &decodepsgt,              cat_wallet, &decodepsgt_helpman        },
    { "combinepsgt",             &combinepsgt,             cat_wallet, &combinepsgt_helpman        },
    { "finalizepsgt",            &finalizepsgt,            cat_wallet, &finalizepsgt_helpman        },
    { "walletprocesspsgt",       &walletprocesspsgt,       cat_wallet, &walletprocesspsgt_helpman        },
    { "utxoupdatepsgt",          &utxoupdatepsgt,          cat_wallet, &utxoupdatepsgt_helpman        },
    { "converttopsgt",           &converttopsgt,           cat_wallet, &converttopsgt_helpman        },
    { "walletcreatefundedpsgt",  &walletcreatefundedpsgt,  cat_wallet, &walletcreatefundedpsgt_helpman        },

  // Staking commands
    { "advertisebeacon",         &advertisebeacon,         cat_staking, &advertisebeacon_helpman        },
    { "advertisebeaconv3",       &advertisebeaconv3,       cat_staking, &advertisebeaconv3_helpman        },
    { "beaconauth",              &beaconauth,              cat_staking, &beaconauth_helpman        },
    { "beaconconvergence",       &beaconconvergence,       cat_staking, &beaconconvergence_helpman        },
    { "beaconreport",            &beaconreport,            cat_staking, &beaconreport_helpman        },
    { "beaconstatus",            &beaconstatus,            cat_staking, &beaconstatus_helpman        },
    { "createmrcrequest",        &createmrcrequest,        cat_staking, &createmrcrequest_helpman        },
    { "explainmagnitude",        &explainmagnitude,        cat_staking, &explainmagnitude_helpman        },
    { "getlaststake",            &getlaststake,            cat_staking, &getlaststake_helpman        },
    { "getmrcinfo",              &getmrcinfo,              cat_staking, &getmrcinfo_helpman        },
    { "getstakinginfo",          &getstakinginfo,          cat_staking, &getstakinginfo_helpman        },
    { "getmininginfo",           &getstakinginfo,          cat_staking, &getstakinginfo_helpman        }, //alias for getstakinginfo (compatibility)
    { "lifetime",                &lifetime,                cat_staking, &lifetime_helpman        },
    { "magnitude",               &magnitude,               cat_staking, &magnitude_helpman        },
    { "pendingbeaconreport",     &pendingbeaconreport,     cat_staking, &pendingbeaconreport_helpman        },
    { "resetcpids",              &resetcpids,              cat_staking, &resetcpids_helpman        },
    { "revokebeacon",            &revokebeacon,            cat_staking, &revokebeacon_helpman        },
    { "superblockage",           &superblockage,           cat_staking, &superblockage_helpman        },
    { "superblocks",             &superblocks,             cat_staking, &superblocks_helpman        },

  // Developer commands
    { "auditsnapshotaccrual",    &auditsnapshotaccrual,    cat_developer, &auditsnapshotaccrual_helpman     },
    { "auditsnapshotaccruals",   &auditsnapshotaccruals,   cat_developer, &auditsnapshotaccruals_helpman     },
    { "addkey",                  &addkey,                  cat_developer, &addkey_helpman     },
    { "beaconaudit",             &beaconaudit,             cat_developer, &beaconaudit_helpman     },
    { "changesettings",          &changesettings,          cat_developer, &changesettings_helpman     },
    { "currentcontractaverage",  &currentcontractaverage,  cat_developer, &currentcontractaverage_helpman     },
    { "debug",                   &debug,                   cat_developer, &debug_helpman     },
    { "dumpcontracts",           &dumpcontracts,           cat_developer, &dumpcontracts_helpman     },
    { "exportstats1",            &rpc_exportstats,         cat_developer, &rpc_exportstats_helpman     },
    { "getblockstats",           &rpc_getblockstats,       cat_developer, &rpc_getblockstats_helpman     },
    { "getrecentblocks",         &rpc_getrecentblocks,     cat_developer, &rpc_getrecentblocks_helpman     },
    { "inspectaccrualsnapshot",  &inspectaccrualsnapshot,  cat_developer, &inspectaccrualsnapshot_helpman     },
    { "listalerts",              &listalerts,              cat_developer, &listalerts_helpman     },
    { "listprojects",            &listprojects,            cat_developer, &listprojects_helpman     },
    { "getautogreylist",         &getautogreylist,         cat_developer, &getautogreylist_helpman     },
    { "listprotocolentries",     &listprotocolentries,     cat_developer, &listprotocolentries_helpman     },
    { "listresearcheraccounts",  &listresearcheraccounts,  cat_developer, &listresearcheraccounts_helpman     },
    { "listscrapers",            &listscrapers,            cat_developer, &listscrapers_helpman     },
    { "listsidestakes",          &listsidestakes,           cat_developer, &listsidestakes_helpman     },
    { "listmandatorysidestakes", &listmandatorysidestakes, cat_developer, &listmandatorysidestakes_helpman     },
    { "listsettings",            &listsettings,            cat_developer, &listsettings_helpman     },
    { "logging",                 &logging,                 cat_developer, &logging_helpman     },
    { "network",                 &network,                 cat_developer, &network_helpman     },
    { "parseaccrualsnapshotfile",&parseaccrualsnapshotfile,cat_developer, &parseaccrualsnapshotfile_helpman     },
    { "parselegacysb",           &parselegacysb,           cat_developer, &parselegacysb_helpman     },
    { "projects",                &projects,                cat_developer, &projects_helpman     },
    { "readdata",                &readdata,                cat_developer, &readdata_helpman     },
    { "reorganize",              &rpc_reorganize,          cat_developer, &rpc_reorganize_helpman     },
    { "sendalert",               &sendalert,               cat_developer, &sendalert_helpman     },
    { "sendalert2",              &sendalert2,              cat_developer, &sendalert2_helpman     },
    { "sendblock",               &sendblock,               cat_developer, &sendblock_helpman     },
    { "superblockaverage",       &superblockaverage,       cat_developer, &superblockaverage_helpman     },
    { "versionreport",           &versionreport,           cat_developer, &versionreport_helpman     },
    { "writedata",               &writedata,               cat_developer, &writedata_helpman     },

    { "listmanifests",           &listmanifests,           cat_developer, &listmanifests_helpman     },
    { "getmpart",                &getmpart,                cat_developer, &getmpart_helpman     },
    { "sendscraperfilemanifest", &sendscraperfilemanifest, cat_developer, &sendscraperfilemanifest_helpman     },
    { "savescraperfilemanifest", &savescraperfilemanifest, cat_developer, &savescraperfilemanifest_helpman     },
    { "deletecscrapermanifest",  &deletecscrapermanifest,  cat_developer, &deletecscrapermanifest_helpman     },
    { "archivelog",              &archivelog,              cat_developer, &archivelog_helpman     },
    { "testnewsb",               &testnewsb,               cat_developer, &testnewsb_helpman     },
    { "convergencereport",       &convergencereport,       cat_developer, &convergencereport_helpman     },
    { "scraperreport",           &scraperreport,           cat_developer, &scraperreport_helpman     },

  // Network commands
    { "addnode",                 &addnode,                 cat_network, &addnode_helpman       },
    { "askforoutstandingblocks", &askforoutstandingblocks, cat_network, &askforoutstandingblocks_helpman       },
    { "getblockchaininfo",       &getblockchaininfo,       cat_network, &getblockchaininfo_helpman       },
    { "getnetworkinfo",          &getnetworkinfo,          cat_network, &getnetworkinfo_helpman       },
    { "clearbanned",             &clearbanned,             cat_network, &clearbanned_helpman       },
    { "currenttime",             &currenttime,             cat_network, &currenttime_helpman       },
    { "getaddednodeinfo",        &getaddednodeinfo,        cat_network, &getaddednodeinfo_helpman       },
    { "getnodeaddresses",        &getnodeaddresses,        cat_network, &getnodeaddresses_helpman       },
    { "getbestblockhash",        &getbestblockhash,        cat_network, &getbestblockhash_helpman       },
    { "getblock",                &getblock,                cat_network, &getblock_helpman       },
    { "getblockbynumber",        &getblockbynumber,        cat_network, &getblockbynumber_helpman       },
    { "getblockbymintime",       &getblockbymintime,       cat_network, &getblockbymintime_helpman       },
    { "getblocksbatch",          &getblocksbatch,          cat_network, &getblocksbatch_helpman       },
    { "getblockcount",           &getblockcount,           cat_network, &getblockcount_helpman       },
    { "getblockhash",            &getblockhash,            cat_network, &getblockhash_helpman       },
    { "getburnreport",           &getburnreport,           cat_network, &getburnreport_helpman       },
    { "getcheckpoint",           &getcheckpoint,           cat_network, &getcheckpoint_helpman       },
    { "getconnectioncount",      &getconnectioncount,      cat_network, &getconnectioncount_helpman       },
    { "getdifficulty",           &getdifficulty,           cat_network, &getdifficulty_helpman       },
    { "getinfo",                 &getinfo,                 cat_network, &getinfo_helpman       },
    { "getnettotals",            &getnettotals,            cat_network, &getnettotals_helpman       },
    { "getpeerinfo",             &getpeerinfo,             cat_network, &getpeerinfo_helpman       },
    { "getrawmempool",           &getrawmempool,           cat_network, &getrawmempool_helpman       },
    { "listbanned",              &listbanned,              cat_network, &listbanned_helpman       },
    { "networktime",             &networktime,             cat_network, &networktime_helpman       },
    { "ping",                    &ping,                    cat_network, &ping_helpman       },
    { "setban",                  &setban,                  cat_network, &setban_helpman       },
    { "showblock",               &showblock,               cat_network, &showblock_helpman       },
    { "stop",                    &stop,                    cat_network, &stop_helpman       },

  // Voting commands
    { "addpoll",                 &addpoll,                 cat_voting, nullptr        },
    { "getpollresults",          &getpollresults,          cat_voting, &getpollresults_helpman        },
    { "getvotingclaim",          &getvotingclaim,          cat_voting, &getvotingclaim_helpman        },
    { "listpolls",               &listpolls,               cat_voting, &listpolls_helpman        },
    { "testpollnotification",    &testpollnotification,    cat_voting, &testpollnotification_helpman        },
    { "vote",                    &vote,                    cat_voting, &vote_helpman        },
    { "votebyid",                &votebyid,                cat_voting, &votebyid_helpman        },
    { "votedetails",             &votedetails,             cat_voting, &votedetails_helpman        },
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
        else
            ServiceConnection(conn);

        conn->close();
    }

    delete conn;
}

void StartRPCThreads()
{
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

            fListening = true;
        }
    }
    catch(boost::system::system_error &e)
    {
        strerr = strprintf(_("An error occurred while setting up the RPC port %u for listening on IPv4: %s"), endpoint.port(), e.what());
    }

    if (!fListening)
    {
        uiInterface.ThreadSafeMessageBox(strerr, _("Error"), CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }

    rpc_worker_group = new boost::thread_group();
    for (int i = 0; i < gArgs.GetArg("-rpcthreads", 4); i++)
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
                    boost::bind(&commandMap::value_type::first,boost::placeholders::_1) );
    // remove deprecated commands from autocomplete
    for(auto &command: DEPRECATED_RPCS) {
        commandList.erase(std::remove(commandList.begin(), commandList.end(), command), commandList.end());
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
