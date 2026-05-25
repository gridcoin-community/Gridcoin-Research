// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_SERVER_H
#define BITCOIN_RPC_SERVER_H

#include <string>
#include <list>
#include <map>

class CBlockIndex;
class uint256;
// Forward-declare RPCHelpMan rather than #include <rpc/util.h>: the util
// header transitively defines `enum class Optional { NO, ... }`, and on
// macOS `NO` is a Foundation/Objective-C macro that clobbers the enumerator
// when the header reaches any TU that also pulls in <Foundation/Foundation.h>
// (e.g. via Qt's macOS shims). Forward-declaring lets us name `RPCHelpMan`
// in the `rpchelpman_fn` typedef below without forcing util.h on every
// includer of server.h. The full type is needed only inside server.cpp,
// where util.h is already included directly.
class RPCHelpMan;

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

// Accessor pointer for an RPCHelpMan tied to a registered command. Used by the
// dispatcher to render help text and validate arity directly, without invoking
// the command body via the legacy throw-on-fHelp convention. Every registered
// command must provide a non-null accessor (the last nullptr — addpoll — was
// lifted to a dynamic helpman in PR M3).
typedef const RPCHelpMan& (*rpchelpman_fn)();

enum rpccategory
{
    cat_null,
    cat_wallet,
    cat_staking,
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
    rpchelpman_fn helpman; // required for all commands; see typedef above.
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

// Wallet
extern UniValue addmultisigaddress(const UniValue& params, bool fHelp);
extern UniValue addredeemscript(const UniValue& params, bool fHelp);
extern UniValue backupwallet(const UniValue& params, bool fHelp);
extern UniValue burn(const UniValue& params, bool fHelp);
extern UniValue checkwallet(const UniValue& params, bool fHelp);
extern UniValue claimhtlc(const UniValue& params, bool fHelp);
extern UniValue createhtlc(const UniValue& params, bool fHelp);
extern UniValue createrawtransaction(const UniValue& params, bool fHelp);
extern UniValue consolidatemsunspent(const UniValue& params, bool fHelp);
extern UniValue decoderawtransaction(const UniValue& params, bool fHelp);
extern UniValue decodescript(const UniValue& params, bool fHelp);
extern UniValue dumpprivkey(const UniValue& params, bool fHelp);
extern UniValue fundrawtransaction(const UniValue& params, bool fHelp);
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
extern UniValue abandontransaction(const UniValue& params, bool fHelp);
extern UniValue getunconfirmedbalance(const UniValue& params, bool fHelp);
extern UniValue getwalletinfo(const UniValue& params, bool fHelp);
extern UniValue importprivkey(const UniValue& params, bool fHelp);
extern UniValue inspectwalletstate(const UniValue& params, bool fHelp);
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
extern UniValue refundhtlc(const UniValue& params, bool fHelp);
extern UniValue repairwallet(const UniValue& params, bool fHelp);
extern UniValue resendtx(const UniValue& params, bool fHelp);
extern UniValue reservebalance(const UniValue& params, bool fHelp);
extern UniValue scanforunspent(const UniValue& params, bool fHelp);
extern UniValue sendfrom(const UniValue& params, bool fHelp);
extern UniValue sendmany(const UniValue& params, bool fHelp);
extern UniValue sendrawtransaction(const UniValue& params, bool fHelp);
extern UniValue sendtoaddress(const UniValue& params, bool fHelp);
extern UniValue setaccount(const UniValue& params, bool fHelp);
extern UniValue sethdseed(const UniValue& params, bool fHelp);
extern UniValue settxfee(const UniValue& params, bool fHelp);
extern UniValue signmessage(const UniValue& params, bool fHelp);
extern UniValue signrawtransaction(const UniValue& params, bool fHelp);
extern UniValue signrawtransactionwithkey(const UniValue& params, bool fHelp);
extern UniValue signrawtransactionwithwallet(const UniValue& params, bool fHelp);
extern UniValue upgradewallet(const UniValue& params, bool fHelp);
extern UniValue validateaddress(const UniValue& params, bool fHelp);
extern UniValue validatepubkey(const UniValue& params, bool fHelp);
extern UniValue verifymessage(const UniValue& params, bool fHelp);
extern UniValue walletlock(const UniValue& params, bool fHelp);
extern UniValue walletpassphrase(const UniValue& params, bool fHelp);
extern UniValue walletpassphrasechange(const UniValue& params, bool fHelp);
extern UniValue walletdiagnose(const UniValue& params, bool fHelp);

// PSGT (Partially Signed Gridcoin Transactions)
extern UniValue createpsgt(const UniValue& params, bool fHelp);
extern UniValue decodepsgt(const UniValue& params, bool fHelp);
extern UniValue combinepsgt(const UniValue& params, bool fHelp);
extern UniValue finalizepsgt(const UniValue& params, bool fHelp);
extern UniValue walletprocesspsgt(const UniValue& params, bool fHelp);
extern UniValue utxoupdatepsgt(const UniValue& params, bool fHelp);
extern UniValue converttopsgt(const UniValue& params, bool fHelp);
extern UniValue walletcreatefundedpsgt(const UniValue& params, bool fHelp);

// Staking
extern UniValue advertisebeacon(const UniValue& params, bool fHelp);
extern UniValue advertisebeaconv3(const UniValue& params, bool fHelp);
extern UniValue beaconauth(const UniValue& params, bool fHelp);
extern UniValue beaconreport(const UniValue& params, bool fHelp);
extern UniValue beaconconvergence(const UniValue& params, bool fHelp);
extern UniValue beaconstatus(const UniValue& params, bool fHelp);
extern UniValue createmrcrequest(const UniValue& params, const bool fHelp);
extern UniValue explainmagnitude(const UniValue& params, bool fHelp);
extern UniValue getlaststake(const UniValue& params, bool fHelp);
extern UniValue getmrcinfo(const UniValue& params, bool fHelp);
extern UniValue getstakinginfo(const UniValue& params, bool fHelp);
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
extern UniValue beaconaudit(const UniValue& params, bool fHelp);
extern UniValue currentcontractaverage(const UniValue& params, bool fHelp);
extern UniValue debug(const UniValue& params, bool fHelp);
extern UniValue dumpcontracts(const UniValue& params, bool fHelp);
extern UniValue rpc_getblockstats(const UniValue& params, bool fHelp);
extern UniValue inspectaccrualsnapshot(const UniValue& params, bool fHelp);
extern UniValue listalerts(const UniValue& params, bool fHelp);
extern UniValue listprojects(const UniValue& params, bool fHelp);
extern UniValue getrawprojectstatus(const UniValue& params, bool fHelp);
extern UniValue getautogreylist(const UniValue& params, bool fHelp);
extern UniValue listprotocolentries(const UniValue& params, bool fHelp);
extern UniValue listresearcheraccounts(const UniValue& params, bool fHelp);
extern UniValue listscrapers(const UniValue& params, bool fHelp);
extern UniValue listsidestakes(const UniValue& params, bool fHelp);
extern UniValue listmandatorysidestakes(const UniValue& params, bool fHelp);
extern UniValue listsettings(const UniValue& params, bool fHelp);
extern UniValue logging(const UniValue& params, bool fHelp);
extern UniValue network(const UniValue& params, bool fHelp);
extern UniValue parseaccrualsnapshotfile(const UniValue& params, bool fHelp);
extern UniValue parselegacysb(const UniValue& params, bool fHelp);
extern UniValue projects(const UniValue& params, bool fHelp);
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
extern UniValue changesettings(const UniValue& params, bool fHelp);
extern UniValue clearbanned(const UniValue& params, bool fHelp);
extern UniValue currenttime(const UniValue& params, bool fHelp);
extern UniValue getaddednodeinfo(const UniValue& params, bool fHelp);
extern UniValue getnodeaddresses(const UniValue& params, bool fHelp);
extern UniValue getbestblockhash(const UniValue& params, bool fHelp);
extern UniValue getblock(const UniValue& params, bool fHelp);
extern UniValue getblockbynumber(const UniValue& params, bool fHelp);
extern UniValue getblockbymintime(const UniValue& params, bool fHelp);
extern UniValue getblocksbatch(const UniValue& params, bool fHelp);
extern UniValue getblockchaininfo(const UniValue& params, bool fHelp);
extern UniValue getblockcount(const UniValue& params, bool fHelp);
extern UniValue getblockhash(const UniValue& params, bool fHelp);
extern UniValue getburnreport(const UniValue& params, bool fHelp);
extern UniValue getcheckpoint(const UniValue& params, bool fHelp);
extern UniValue getconnectioncount(const UniValue& params, bool fHelp);
extern UniValue getdifficulty(const UniValue& params, bool fHelp);
extern UniValue getinfo(const UniValue& params, bool fHelp); // To Be Deprecated --> getblockchaininfo getnetworkinfo getwalletinfo
extern UniValue getnettotals(const UniValue& params, bool fHelp);
extern UniValue getnetworkinfo(const UniValue& params, bool fHelp);
extern UniValue getpeerinfo(const UniValue& params, bool fHelp);
extern UniValue getrawmempool(const UniValue& params, bool fHelp);
extern UniValue listbanned(const UniValue& params, bool fHelp);
extern UniValue networktime(const UniValue& params, bool fHelp);
extern UniValue ping(const UniValue& params, bool fHelp);
extern UniValue rpc_exportstats(const UniValue& params, bool fHelp);
extern UniValue rpc_getrecentblocks(const UniValue& params, bool fHelp);
extern UniValue setban(const UniValue& params, bool fHelp);
extern UniValue showblock(const UniValue& params, bool fHelp);

// Voting
extern UniValue addpoll(const UniValue& params, bool fHelp);
extern UniValue getpollresults(const UniValue& params, bool fHelp);
extern UniValue getvotingclaim(const UniValue& params, bool fHelp);
extern UniValue listpolls(const UniValue& params, bool fHelp);
extern UniValue testpollnotification(const UniValue& params, bool fHelp);
extern UniValue vote(const UniValue& params, bool fHelp);
extern UniValue votebyid(const UniValue& params, bool fHelp);
extern UniValue votedetails(const UniValue& params, bool fHelp);

int GetDefaultRPCPort();

// RPCHelpMan accessors — one per converted command. Each returns the
// command's file-scope `static const RPCHelpMan` instance. Wired into
// `CRPCCommand::helpman` so the dispatcher can render help text and validate
// arity without invoking the command body. Generated by the lift-to-accessor
// refactor in PR M (see plan in tingly-hugging-book.md).
extern const RPCHelpMan& abandontransaction_helpman();
extern const RPCHelpMan& addkey_helpman();
extern const RPCHelpMan& addmultisigaddress_helpman();
extern const RPCHelpMan& addnode_helpman();
extern const RPCHelpMan& addpoll_helpman();
extern const RPCHelpMan& addredeemscript_helpman();
extern const RPCHelpMan& advertisebeacon_helpman();
extern const RPCHelpMan& advertisebeaconv3_helpman();
extern const RPCHelpMan& archivelog_helpman();
extern const RPCHelpMan& askforoutstandingblocks_helpman();
extern const RPCHelpMan& auditsnapshotaccrual_helpman();
extern const RPCHelpMan& auditsnapshotaccruals_helpman();
extern const RPCHelpMan& backupwallet_helpman();
extern const RPCHelpMan& beaconaudit_helpman();
extern const RPCHelpMan& beaconauth_helpman();
extern const RPCHelpMan& beaconconvergence_helpman();
extern const RPCHelpMan& beaconreport_helpman();
extern const RPCHelpMan& beaconstatus_helpman();
extern const RPCHelpMan& burn_helpman();
extern const RPCHelpMan& changesettings_helpman();
extern const RPCHelpMan& checkwallet_helpman();
extern const RPCHelpMan& claimhtlc_helpman();
extern const RPCHelpMan& clearbanned_helpman();
extern const RPCHelpMan& consolidatemsunspent_helpman();
extern const RPCHelpMan& consolidateunspent_helpman();
extern const RPCHelpMan& convergencereport_helpman();
extern const RPCHelpMan& createhtlc_helpman();
extern const RPCHelpMan& createmrcrequest_helpman();
extern const RPCHelpMan& createrawtransaction_helpman();
extern const RPCHelpMan& currentcontractaverage_helpman();
extern const RPCHelpMan& combinepsgt_helpman();
extern const RPCHelpMan& converttopsgt_helpman();
extern const RPCHelpMan& createpsgt_helpman();
extern const RPCHelpMan& decodepsgt_helpman();
extern const RPCHelpMan& finalizepsgt_helpman();
extern const RPCHelpMan& inspectwalletstate_helpman();
extern const RPCHelpMan& utxoupdatepsgt_helpman();
extern const RPCHelpMan& walletcreatefundedpsgt_helpman();
extern const RPCHelpMan& walletprocesspsgt_helpman();
extern const RPCHelpMan& currenttime_helpman();
extern const RPCHelpMan& debug_helpman();
extern const RPCHelpMan& decoderawtransaction_helpman();
extern const RPCHelpMan& decodescript_helpman();
extern const RPCHelpMan& deletecscrapermanifest_helpman();
extern const RPCHelpMan& dumpcontracts_helpman();
extern const RPCHelpMan& dumpprivkey_helpman();
extern const RPCHelpMan& dumpwallet_helpman();
extern const RPCHelpMan& encryptwallet_helpman();
extern const RPCHelpMan& explainmagnitude_helpman();
extern const RPCHelpMan& fundrawtransaction_helpman();
extern const RPCHelpMan& getaccount_helpman();
extern const RPCHelpMan& getaccountaddress_helpman();
extern const RPCHelpMan& getaddednodeinfo_helpman();
extern const RPCHelpMan& getaddressesbyaccount_helpman();
extern const RPCHelpMan& getautogreylist_helpman();
extern const RPCHelpMan& getbalance_helpman();
extern const RPCHelpMan& getbalancedetail_helpman();
extern const RPCHelpMan& getbestblockhash_helpman();
extern const RPCHelpMan& getblock_helpman();
extern const RPCHelpMan& getblockbymintime_helpman();
extern const RPCHelpMan& getblockbynumber_helpman();
extern const RPCHelpMan& getblockchaininfo_helpman();
extern const RPCHelpMan& getblockcount_helpman();
extern const RPCHelpMan& getblockhash_helpman();
extern const RPCHelpMan& getblocksbatch_helpman();
extern const RPCHelpMan& getburnreport_helpman();
extern const RPCHelpMan& getcheckpoint_helpman();
extern const RPCHelpMan& getconnectioncount_helpman();
extern const RPCHelpMan& getdifficulty_helpman();
extern const RPCHelpMan& getinfo_helpman();
extern const RPCHelpMan& getlaststake_helpman();
extern const RPCHelpMan& getmpart_helpman();
extern const RPCHelpMan& getmrcinfo_helpman();
extern const RPCHelpMan& getnettotals_helpman();
extern const RPCHelpMan& getnetworkinfo_helpman();
extern const RPCHelpMan& getnewaddress_helpman();
extern const RPCHelpMan& getnewpubkey_helpman();
extern const RPCHelpMan& getnodeaddresses_helpman();
extern const RPCHelpMan& getpeerinfo_helpman();
extern const RPCHelpMan& getpollresults_helpman();
extern const RPCHelpMan& getrawmempool_helpman();
extern const RPCHelpMan& getrawprojectstatus_helpman();
extern const RPCHelpMan& getrawtransaction_helpman();
extern const RPCHelpMan& getrawwallettransaction_helpman();
extern const RPCHelpMan& getreceivedbyaccount_helpman();
extern const RPCHelpMan& getreceivedbyaddress_helpman();
extern const RPCHelpMan& getstakinginfo_helpman();
extern const RPCHelpMan& gettransaction_helpman();
extern const RPCHelpMan& getunconfirmedbalance_helpman();
extern const RPCHelpMan& getvotingclaim_helpman();
extern const RPCHelpMan& getwalletinfo_helpman();
extern const RPCHelpMan& help_helpman();
extern const RPCHelpMan& importprivkey_helpman();
extern const RPCHelpMan& importwallet_helpman();
extern const RPCHelpMan& inspectaccrualsnapshot_helpman();
extern const RPCHelpMan& keypoolrefill_helpman();
extern const RPCHelpMan& lifetime_helpman();
extern const RPCHelpMan& listaccounts_helpman();
extern const RPCHelpMan& listaddressgroupings_helpman();
extern const RPCHelpMan& listalerts_helpman();
extern const RPCHelpMan& listbanned_helpman();
extern const RPCHelpMan& listmandatorysidestakes_helpman();
extern const RPCHelpMan& listmanifests_helpman();
extern const RPCHelpMan& listpolls_helpman();
extern const RPCHelpMan& listprojects_helpman();
extern const RPCHelpMan& listprotocolentries_helpman();
extern const RPCHelpMan& listreceivedbyaccount_helpman();
extern const RPCHelpMan& listreceivedbyaddress_helpman();
extern const RPCHelpMan& listresearcheraccounts_helpman();
extern const RPCHelpMan& listscrapers_helpman();
extern const RPCHelpMan& listsettings_helpman();
extern const RPCHelpMan& listsidestakes_helpman();
extern const RPCHelpMan& listsinceblock_helpman();
extern const RPCHelpMan& liststakes_helpman();
extern const RPCHelpMan& listtransactions_helpman();
extern const RPCHelpMan& listunspent_helpman();
extern const RPCHelpMan& logging_helpman();
extern const RPCHelpMan& magnitude_helpman();
extern const RPCHelpMan& maintainbackups_helpman();
extern const RPCHelpMan& makekeypair_helpman();
extern const RPCHelpMan& movecmd_helpman();
extern const RPCHelpMan& network_helpman();
extern const RPCHelpMan& networktime_helpman();
extern const RPCHelpMan& parseaccrualsnapshotfile_helpman();
extern const RPCHelpMan& parselegacysb_helpman();
extern const RPCHelpMan& pendingbeaconreport_helpman();
extern const RPCHelpMan& ping_helpman();
extern const RPCHelpMan& projects_helpman();
extern const RPCHelpMan& rainbymagnitude_helpman();
extern const RPCHelpMan& readdata_helpman();
extern const RPCHelpMan& refundhtlc_helpman();
extern const RPCHelpMan& repairwallet_helpman();
extern const RPCHelpMan& resendtx_helpman();
extern const RPCHelpMan& reservebalance_helpman();
extern const RPCHelpMan& resetcpids_helpman();
extern const RPCHelpMan& revokebeacon_helpman();
extern const RPCHelpMan& rpc_exportstats_helpman();
extern const RPCHelpMan& rpc_getblockstats_helpman();
extern const RPCHelpMan& rpc_getrecentblocks_helpman();
extern const RPCHelpMan& rpc_reorganize_helpman();
extern const RPCHelpMan& savescraperfilemanifest_helpman();
extern const RPCHelpMan& scanforunspent_helpman();
extern const RPCHelpMan& scraperreport_helpman();
extern const RPCHelpMan& sendalert_helpman();
extern const RPCHelpMan& sendalert2_helpman();
extern const RPCHelpMan& sendblock_helpman();
extern const RPCHelpMan& sendfrom_helpman();
extern const RPCHelpMan& sendmany_helpman();
extern const RPCHelpMan& sendrawtransaction_helpman();
extern const RPCHelpMan& sendscraperfilemanifest_helpman();
extern const RPCHelpMan& sendtoaddress_helpman();
extern const RPCHelpMan& setaccount_helpman();
extern const RPCHelpMan& setban_helpman();
extern const RPCHelpMan& sethdseed_helpman();
extern const RPCHelpMan& settxfee_helpman();
extern const RPCHelpMan& showblock_helpman();
extern const RPCHelpMan& signmessage_helpman();
extern const RPCHelpMan& signrawtransaction_helpman();
extern const RPCHelpMan& signrawtransactionwithkey_helpman();
extern const RPCHelpMan& signrawtransactionwithwallet_helpman();
extern const RPCHelpMan& stop_helpman();
extern const RPCHelpMan& superblockage_helpman();
extern const RPCHelpMan& superblockaverage_helpman();
extern const RPCHelpMan& superblocks_helpman();
extern const RPCHelpMan& testnewsb_helpman();
extern const RPCHelpMan& testpollnotification_helpman();
extern const RPCHelpMan& upgradewallet_helpman();
extern const RPCHelpMan& validateaddress_helpman();
extern const RPCHelpMan& validatepubkey_helpman();
extern const RPCHelpMan& verifymessage_helpman();
extern const RPCHelpMan& versionreport_helpman();
extern const RPCHelpMan& vote_helpman();
extern const RPCHelpMan& votebyid_helpman();
extern const RPCHelpMan& votedetails_helpman();
extern const RPCHelpMan& walletdiagnose_helpman();
extern const RPCHelpMan& walletlock_helpman();
extern const RPCHelpMan& walletpassphrase_helpman();
extern const RPCHelpMan& walletpassphrasechange_helpman();
extern const RPCHelpMan& writedata_helpman();

#endif // BITCOIN_RPC_SERVER_H
