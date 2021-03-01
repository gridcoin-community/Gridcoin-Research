// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <list>
#include <map>

class CBlockIndex;
class uint256;

#include <univalue.h>

void StartRPCThreads();
void StopRPCThreads();
int CommandLineRPC(int argc, char *argv[]);

/*
  Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
  the right number of arguments are passed, just that any passed are the correct type.
  Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
*/
void RPCTypeCheck(const UniValue& params,
                  const std::list<UniValue::VType>& typesExpected, bool fAllowNull=false);
/*
  Check for expected keys/value types in an Object.
  Use like: RPCTypeCheckObj(object, boost::assign::map_list_of("name", str_type)("value", int_type));
*/
void RPCTypeCheckObj(const UniValue& o,
                  const std::map<std::string, UniValue::VType>& typesExpected, bool fAllowNull=false);

typedef UniValue(*rpcfn_type)(const UniValue& params, bool fHelp);

enum rpccategory
{
    cat_null,
    cat_wallet,
    cat_mining,
    cat_developer,
    cat_network,
    cat_voting,
};

class CRPCCommand
{
public:
    std::string name;
    rpcfn_type actor;
    rpccategory category;
};

/**
 * Bitcoin RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;
public:
    CRPCTable();
    const CRPCCommand* operator[](std::string name) const;
    std::string help(std::string name, rpccategory category) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception when an error happens.
     */
    UniValue execute(const std::string &method, const UniValue& params) const;

    /**
    * Returns a list of registered commands
    * @returns List of registered commands.
    */
    std::vector<std::string> listCommands() const;

};
extern const CRPCTable tableRPC;

extern int64_t nWalletUnlockTime;
extern int64_t AmountFromValue(const UniValue& value);
extern UniValue ValueFromAmount(int64_t amount);

extern std::string HelpRequiringPassphrase();
extern void EnsureWalletIsUnlocked();

//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
extern uint256 ParseHashV(const UniValue& v, std::string strName);
extern uint256 ParseHashO(const UniValue& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName);

// Rpc reordered by category

// Deprecated
extern UniValue listitem(const UniValue& params, bool fHelp);
extern UniValue execute(const UniValue& params, bool fHelp);

// Wallet
extern UniValue addmultisigaddress(const UniValue& params, bool fHelp);
extern UniValue addredeemscript(const UniValue& params, bool fHelp);
extern UniValue backupprivatekeys(const UniValue& params, bool fHelp);
extern UniValue backupwallet(const UniValue& params, bool fHelp);
extern UniValue burn(const UniValue& params, bool fHelp);
extern UniValue checkwallet(const UniValue& params, bool fHelp);
extern UniValue createrawtransaction(const UniValue& params, bool fHelp);
extern UniValue consolidatemsunspent(const UniValue& params, bool fHelp);
extern UniValue decoderawtransaction(const UniValue& params, bool fHelp);
extern UniValue decodescript(const UniValue& params, bool fHelp);
extern UniValue dumpprivkey(const UniValue& params, bool fHelp);
extern UniValue dumpwallet(const UniValue& params, bool fHelp);
extern UniValue encryptwallet(const UniValue& params, bool fHelp);
extern UniValue getaccount(const UniValue& params, bool fHelp);
extern UniValue getaccountaddress(const UniValue& params, bool fHelp);
extern UniValue getaddressesbyaccount(const UniValue& params, bool fHelp);
extern UniValue getbalance(const UniValue& params, bool fHelp);
extern UniValue getbalancedetail(const UniValue& params, bool fHelp);
extern UniValue getnewaddress(const UniValue& params, bool fHelp);
extern UniValue getnewpubkey(const UniValue& params, bool fHelp);
extern UniValue getrawtransaction(const UniValue& params, bool fHelp);
extern UniValue getrawwallettransaction(const UniValue& params, bool fHelp);
extern UniValue getreceivedbyaccount(const UniValue& params, bool fHelp);
extern UniValue getreceivedbyaddress(const UniValue& params, bool fHelp);
extern UniValue gettransaction(const UniValue& params, bool fHelp);
extern UniValue getunconfirmedbalance(const UniValue& params, bool fHelp);
extern UniValue getwalletinfo(const UniValue& params, bool fHelp);
extern UniValue importprivkey(const UniValue& params, bool fHelp);
extern UniValue importwallet(const UniValue& params, bool fHelp);
extern UniValue keypoolrefill(const UniValue& params, bool fHelp);
extern UniValue listaccounts(const UniValue& params, bool fHelp);
extern UniValue listaddressgroupings(const UniValue& params, bool fHelp);
extern UniValue listreceivedbyaccount(const UniValue& params, bool fHelp);
extern UniValue listreceivedbyaddress(const UniValue& params, bool fHelp);
extern UniValue listsinceblock(const UniValue& params, bool fHelp);
extern UniValue liststakes(const UniValue& params, bool fHelp);
extern UniValue listtransactions(const UniValue& params, bool fHelp);
extern UniValue listunspent(const UniValue& params, bool fHelp);
extern UniValue consolidateunspent(const UniValue& params, bool fHelp);
extern UniValue makekeypair(const UniValue& params, bool fHelp);
extern UniValue maintainbackups(const UniValue& params, bool fHelp);
extern UniValue movecmd(const UniValue& params, bool fHelp);
extern UniValue rainbymagnitude(const UniValue& params, bool fHelp);
extern UniValue repairwallet(const UniValue& params, bool fHelp);
extern UniValue resendtx(const UniValue& params, bool fHelp);
extern UniValue reservebalance(const UniValue& params, bool fHelp);
extern UniValue scanforunspent(const UniValue& params, bool fHelp);
extern UniValue sendfrom(const UniValue& params, bool fHelp);
extern UniValue sendmany(const UniValue& params, bool fHelp);
extern UniValue sendrawtransaction(const UniValue& params, bool fHelp);
extern UniValue sendtoaddress(const UniValue& params, bool fHelp);
extern UniValue setaccount(const UniValue& params, bool fHelp);
extern UniValue settxfee(const UniValue& params, bool fHelp);
extern UniValue signmessage(const UniValue& params, bool fHelp);
extern UniValue signrawtransaction(const UniValue& params, bool fHelp);
extern UniValue validateaddress(const UniValue& params, bool fHelp);
extern UniValue validatepubkey(const UniValue& params, bool fHelp);
extern UniValue verifymessage(const UniValue& params, bool fHelp);
extern UniValue walletlock(const UniValue& params, bool fHelp);
extern UniValue walletpassphrase(const UniValue& params, bool fHelp);
extern UniValue walletpassphrasechange(const UniValue& params, bool fHelp);

//Mining
extern UniValue advertisebeacon(const UniValue& params, bool fHelp);
extern UniValue beaconreport(const UniValue& params, bool fHelp);
extern UniValue beaconconvergence(const UniValue& params, bool fHelp);
extern UniValue beaconstatus(const UniValue& params, bool fHelp);
extern UniValue explainmagnitude(const UniValue& params, bool fHelp);
extern UniValue getlaststake(const UniValue& params, bool fHelp);
extern UniValue getmininginfo(const UniValue& params, bool fHelp);
extern UniValue lifetime(const UniValue& params, bool fHelp);
extern UniValue magnitude(const UniValue& params, bool fHelp);
extern UniValue pendingbeaconreport(const UniValue& params, bool fHelp);
extern UniValue resetcpids(const UniValue& params, bool fHelp);
extern UniValue revokebeacon(const UniValue& params, bool fHelp);
extern UniValue superblockage(const UniValue& params, bool fHelp);
extern UniValue superblocks(const UniValue& params, bool fHelp);

// Developers
extern UniValue auditsnapshotaccrual(const UniValue& params, bool fHelp);
extern UniValue auditsnapshotaccruals(const UniValue& params, bool fHelp);
extern UniValue addkey(const UniValue& params, bool fHelp);
extern UniValue comparesnapshotaccrual(const UniValue& params, bool fHelp);
extern UniValue currentcontractaverage(const UniValue& params, bool fHelp);
extern UniValue debug(const UniValue& params, bool fHelp);
extern UniValue debug10(const UniValue& params, bool fHelp);
extern UniValue debug2(const UniValue& params, bool fHelp);
extern UniValue dumpcontracts(const UniValue& params, bool fHelp);
extern UniValue rpc_getblockstats(const UniValue& params, bool fHelp);
extern UniValue getlistof(const UniValue& params, bool fHelp);
extern UniValue inspectaccrualsnapshot(const UniValue& params, bool fHelp);
extern UniValue listdata(const UniValue& params, bool fHelp);
extern UniValue listprojects(const UniValue& params, bool fHelp);
extern UniValue listresearcheraccounts(const UniValue& params, bool fHelp);
extern UniValue logging(const UniValue& params, bool fHelp);
extern UniValue network(const UniValue& params, bool fHelp);
extern UniValue parseaccrualsnapshotfile(const UniValue& params, bool fHelp);
extern UniValue parselegacysb(const UniValue& params, bool fHelp);
extern UniValue projects(const UniValue& params, bool fHelp);
extern UniValue readconfig(const UniValue& params, bool fHelp);
extern UniValue readdata(const UniValue& params, bool fHelp);
extern UniValue rpc_reorganize(const UniValue& params, bool fHelp);
extern UniValue sendalert(const UniValue& params, bool fHelp);
extern UniValue sendalert2(const UniValue& params, bool fHelp);
extern UniValue sendblock(const UniValue& params, bool fHelp);
extern UniValue superblockaverage(const UniValue& params, bool fHelp);
extern UniValue versionreport(const UniValue& params, bool fhelp);
extern UniValue writedata(const UniValue& params, bool fHelp);

extern UniValue listmanifests(const UniValue& params, bool fHelp);
extern UniValue getmanifest(const UniValue& params, bool fHelp);
extern UniValue getmpart(const UniValue& params, bool fHelp);
extern UniValue sendscraperfilemanifest(const UniValue& params, bool fHelp);
extern UniValue savescraperfilemanifest(const UniValue& params, bool fHelp);
extern UniValue deletecscrapermanifest(const UniValue& params, bool fHelp);
extern UniValue archivelog(const UniValue& params, bool fHelp);
extern UniValue testnewsb(const UniValue& params, bool fHelp);
extern UniValue convergencereport(const UniValue& params, bool fHelp);
extern UniValue scraperreport(const UniValue& params, bool fHelp);

// Network
extern UniValue addnode(const UniValue& params, bool fHelp);
extern UniValue askforoutstandingblocks(const UniValue& params, bool fHelp);
extern UniValue clearbanned(const UniValue& params, bool fHelp);
extern UniValue currenttime(const UniValue& params, bool fHelp);
extern UniValue getaddednodeinfo(const UniValue& params, bool fHelp);
extern UniValue getbestblockhash(const UniValue& params, bool fHelp);
extern UniValue getblock(const UniValue& params, bool fHelp);
extern UniValue getblockbynumber(const UniValue& params, bool fHelp);
extern UniValue getblockchaininfo(const UniValue& params, bool fHelp);
extern UniValue getblockcount(const UniValue& params, bool fHelp);
extern UniValue getblockhash(const UniValue& params, bool fHelp);
extern UniValue getcheckpoint(const UniValue& params, bool fHelp);
extern UniValue getconnectioncount(const UniValue& params, bool fHelp);
extern UniValue getdifficulty(const UniValue& params, bool fHelp);
extern UniValue getinfo(const UniValue& params, bool fHelp); // To Be Deprecated --> getblockchaininfo getnetworkinfo getwalletinfo
extern UniValue getnettotals(const UniValue& params, bool fHelp);
extern UniValue getnetworkinfo(const UniValue& params, bool fHelp);
extern UniValue getpeerinfo(const UniValue& params, bool fHelp);
extern UniValue getrawmempool(const UniValue& params, bool fHelp);
extern UniValue listbanned(const UniValue& params, bool fHelp);
extern UniValue memorypool(const UniValue& params, bool fHelp);
extern UniValue networktime(const UniValue& params, bool fHelp);
extern UniValue ping(const UniValue& params, bool fHelp);
extern UniValue rpc_getsupervotes(const UniValue& params, bool fHelp);
extern UniValue rpc_exportstats(const UniValue& params, bool fHelp);
extern UniValue rpc_getrecentblocks(const UniValue& params, bool fHelp);
extern UniValue setban(const UniValue& params, bool fHelp);
extern UniValue showblock(const UniValue& params, bool fHelp);

// Voting
extern UniValue addpoll(const UniValue& params, bool fHelp);
extern UniValue getpollresults(const UniValue& params, bool fHelp);
extern UniValue getvotingclaim(const UniValue& params, bool fHelp);
extern UniValue listpolls(const UniValue& params, bool fHelp);
extern UniValue vote(const UniValue& params, bool fHelp);
extern UniValue votebyid(const UniValue& params, bool fHelp);
extern UniValue votedetails(const UniValue& params, bool fHelp);

unsigned short GetDefaultRPCPort();
