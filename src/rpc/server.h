// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_SERVER_H
#define BITCOIN_RPC_SERVER_H

#include <support/allocators/secure.h>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <optional>

class CBlockIndex;
class CWallet;
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

#include <chrono>
#include <univalue.h>

//! Idle deadline for a serviced RPC connection, in seconds. One connection pins
//! one worker thread, so this is what bounds how long a silent client can hold
//! one of only -rpcthreads of them.
static constexpr int DEFAULT_RPC_SERVER_TIMEOUT = 30;

//! Longest a closing RPC connection waits for the client to close its side before it is reset.
static constexpr std::chrono::milliseconds RPC_GRACEFUL_CLOSE_TIMEOUT{2000};

//! Parse -rpcallowip into the RPC allow list. Called by StartRPCThreads().
//! The overload taking entries does the parsing and is what tests drive; the
//! no-argument form supplies them from gArgs.
void InitRPCAllowList(const std::vector<std::string>& entries);
void InitRPCAllowList();

//! True if the address may open a JSON-RPC connection. Loopback is always
//! allowed; everything else must match the -rpcallowip list.
bool ClientAllowed(const boost::asio::ip::address& address);

void StartRPCThreads();

//! StartRPCThreads() binds the listener early, while initialisation is still running, so that
//! the port is held from the datadir lock onward. Until this is called the server answers
//! RPC_IN_WARMUP to every method except "stop".
void SetRPCWarmupFinished();
bool RPCIsInWarmup();
//! Test hook; tests restore false.
void SetRPCWarmupForTest(bool in_warmup);
//! True when `method` must be answered RPC_IN_WARMUP now: in warmup, every method except "stop".
bool RPCRefusedInWarmup(const std::string& method);
//! Throws the RPC_IN_WARMUP error when RPCRefusedInWarmup(method). Every path that dispatches into
//! tableRPC outside the socket server (the GUI console) applies it too.
void ThrowIfRefusedInWarmup(const std::string& method);

//! Test hook for the file-static acceptor setup, so a test binds exactly as the RPC server does.
void OpenRpcAcceptorForTest(boost::asio::ip::tcp::acceptor& acceptor, const boost::asio::ip::tcp& protocol);
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

typedef UniValue(*rpcfn_type)(const UniValue& params);

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

// RPC heritage bucket: how this command relates to upstream Bitcoin, for backport
// hygiene (issue #3069). Enforced by test/lint/lint-rpc-heritage.py, which reads these
// values straight off vRPCCommands[] and re-checks the surface fingerprint.
//   pure_upstream    -- behaviorally faithful to current upstream; backport-safe.
//   mixed            -- shares upstream's name/surface but diverges; port carefully.
//   removed_upstream -- upstream-derived but deleted from current upstream (frozen fork);
//                       the backport target is the last valid pre-removal version.
//   pure_gridcoin    -- no upstream analogue; not fingerprinted.
enum rpcheritage
{
    heritage_pure_upstream,
    heritage_mixed,
    heritage_removed_upstream,
    heritage_pure_gridcoin,
};

// RPC access class, enforced for access_elevate by CRPCTable::execute.
//   read_only      -- changes nothing.
//   free           -- changes state that neither moves nor redirects value, widens
//                     exposure, nor handles keys.
//   elevate        -- signs or reveals keys, writes key material, moves or redirects
//                     value, widens exposure, or exports or deletes wallet files; needs
//                     a full unlock or the passphrase sent with the request.
//   elevate_inband -- the method verifies a passphrase given as its own argument.
//   settings       -- changesettings; each setting applies its own class.
enum rpcaccess
{
    access_read_only,
    access_free,
    access_elevate,
    access_elevate_inband,
    access_settings,
};

//! "read_only", "free", "elevate", "elevate_inband", "settings"
const char* RpcAccessName(rpcaccess access);

class CRPCCommand
{
public:
    // All fields are required at construction, so a command cannot be added to
    // vRPCCommands[] without a heritage classification or an access class (classify-at-birth).
    CRPCCommand(std::string _name, rpcfn_type _actor, rpccategory _category,
                rpchelpman_fn _helpman, rpcheritage _heritage, std::string _heritage_fp, rpcaccess _access)
        : name(std::move(_name)), actor(_actor), category(_category), helpman(_helpman),
          heritage(_heritage), heritage_fp(std::move(_heritage_fp)), access(_access) {}

    std::string name;
    rpcfn_type actor;
    rpccategory category;
    rpchelpman_fn helpman; // required for all commands; see typedef above.
    rpcheritage heritage;  // backport-heritage bucket; see rpcheritage above.
    // Surface-fingerprint baseline (args + result-keys), 12 hex chars; "manual" when the
    // output isn't literal-key-trackable; empty for pure_gridcoin (not fingerprinted).
    std::string heritage_fp;
    rpcaccess access;
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
    * @param include_deprecated   Also list commands in DEPRECATED_RPCS
    *                             (hidden from autocomplete but still callable).
    * @returns List of registered commands.
    */
    std::vector<std::string> listCommands(bool include_deprecated = false) const;

};
extern const CRPCTable tableRPC;

extern int64_t AmountFromValue(const UniValue& value);
extern UniValue ValueFromAmount(int64_t amount);

extern std::string HelpRequiringPassphrase();
extern void EnsureWalletIsUnlocked();

//! \brief Everything dumpwallet does after its unlock check: write every key
//! \p wallet holds to \p filename, a bare name resolving to the data directory.
//!
//! A key the wallet holds that cannot be read on this thread -- withheld while
//! the wallet is unlocked for staking only, or one that does not decrypt --
//! fails the dump rather than being left out of it: it throws
//! RPC_WALLET_UNLOCK_NEEDED, or RPC_WALLET_ERROR when the wallet may use its
//! keys. Metadata for a key the wallet does not hold is no key, and is left
//! out. A dump that cannot be written throws RPC_WALLET_ERROR. On any failure
//! part way through, a regular file at the path is removed before the error
//! leaves; a symbolic link or a special file there is left as it is.
void WriteWalletDump(CWallet& wallet, const std::string& filename);

//! \brief Take a request's elevation header out of its headers.
//!
//! The X-Gridcoin-Elevate header (stored as "x-gridcoin-elevate", the parser
//! lower-cases names) carries the wallet passphrase for one request,
//! base64-encoded in the standard alphabet with padding, so that the parser's
//! trimming of whitespace cannot change it. Returns the value, still encoded,
//! in secure memory, or nullopt when the request carries none. The header is
//! erased from \p headers, and the memory that held its value is overwritten
//! first, so nothing that reads the headers afterwards, a request handler or a
//! log, can see it.
std::optional<SecureString> TakeElevationHeader(std::map<std::string, std::string>& headers);

//! \brief Run one request, or one batch, under the capability its elevation
//! header proves.
//!
//! With no header, or with no wallet to authorise anything, \p execute runs as
//! it would without one. Otherwise the header is decoded and the wallet checks
//! the passphrase (CWallet::BeginElevation(), for CapabilityKind::RpcRequest).
//! \p execute then runs on the calling thread under a CapabilityScope for that
//! capability, and the capability ends when \p execute returns or throws. A
//! value that is not base64, or a passphrase the wallet does not accept,
//! throws RPC_WALLET_PASSPHRASE_INCORRECT without running \p execute.
//!
//! An unencrypted wallet issues a capability for any passphrase, so a request
//! with a well-formed header runs there as it would without one.
//!
//! The passphrase check derives a key and may wait out the failed-passphrase
//! delay, so the caller must hold no lock.
UniValue ExecuteWithElevation(const std::optional<SecureString>& header, const std::function<UniValue()>& execute);

//! \brief Execute a JSON-RPC batch under the capability its elevation header
//! proves.
//!
//! The header is checked once for the whole batch, as ExecuteWithElevation()
//! checks it, and every call in \p requests then runs in order under that one
//! capability. Each call is answered by its own entry in the returned array:
//! a call that fails gets an error entry, as without a header, and the calls
//! after it still run under the capability. A header that is refused throws
//! before any call runs.
UniValue ExecuteBatchWithElevation(const UniValue& requests, const std::optional<SecureString>& header);

//! The passphrase parameters of `method` (walletpassphrase, walletpassphrasechange or encryptwallet)
//! moved out of `params` into SecureStrings, in parameter order, each in a secure buffer of its own
//! (capacity at least 100), whatever its length. Throws what get_str() throws for a non-string.
std::vector<SecureString> ReadPassphraseParams(const std::string& method, const UniValue& params);

//! walletpassphrase, walletpassphrasechange or encryptwallet with the passphrases already in
//! SecureStrings (parameter order). `params` holds the other parameters at their real indexes; the
//! passphrase positions are ignored. Throws exactly what the RPCs throw; a wrong passphrase count
//! throws JSONRPCError(RPC_INVALID_PARAMETER, "<method> takes <n> passphrase(s)").
UniValue ExecutePassphraseCommand(const std::string& method, const std::vector<SecureString>& passphrases, const UniValue& params);

//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
extern uint256 ParseHashV(const UniValue& v, std::string strName);
extern uint256 ParseHashO(const UniValue& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName);

// Rpc reordered by category

// Wallet
extern UniValue addmultisigaddress(const UniValue& params);
extern UniValue addredeemscript(const UniValue& params);
extern UniValue backupwallet(const UniValue& params);
extern UniValue burn(const UniValue& params);
extern UniValue checkwallet(const UniValue& params);
extern UniValue claimhtlc(const UniValue& params);
extern UniValue createhtlc(const UniValue& params);
extern UniValue createrawtransaction(const UniValue& params);
extern UniValue consolidatemsunspent(const UniValue& params);
extern UniValue decoderawtransaction(const UniValue& params);
extern UniValue decodescript(const UniValue& params);
extern UniValue dumpprivkey(const UniValue& params);
extern UniValue dumpseedphrase(const UniValue& params);
extern UniValue fundrawtransaction(const UniValue& params);
extern UniValue dumpwallet(const UniValue& params);
extern UniValue encryptwallet(const UniValue& params);
extern UniValue getaccount(const UniValue& params);
extern UniValue getaccountaddress(const UniValue& params);
extern UniValue getaddressesbyaccount(const UniValue& params);
extern UniValue getaddressesbylabel(const UniValue& params);
extern UniValue getbalance(const UniValue& params);
extern UniValue getbalancedetail(const UniValue& params);
extern UniValue getnewaddress(const UniValue& params);
extern UniValue getnewpubkey(const UniValue& params);
extern UniValue getseedphraseinfo(const UniValue& params);
extern UniValue getrawtransaction(const UniValue& params);
extern UniValue getrawwallettransaction(const UniValue& params);
extern UniValue gettxoutproof(const UniValue& params);
extern UniValue verifytxoutproof(const UniValue& params);
extern UniValue getreceivedbyaccount(const UniValue& params);
extern UniValue getreceivedbyaddress(const UniValue& params);
extern UniValue getreceivedbylabel(const UniValue& params);
extern UniValue gettransaction(const UniValue& params);
extern UniValue abandontransaction(const UniValue& params);
extern UniValue cancelunbroadcasttransaction(const UniValue& params);
extern UniValue getunconfirmedbalance(const UniValue& params);
extern UniValue getwalletinfo(const UniValue& params);
extern UniValue importprivkey(const UniValue& params);
extern UniValue inspectwalletstate(const UniValue& params);
extern UniValue importwallet(const UniValue& params);
extern UniValue keypoolrefill(const UniValue& params);
extern UniValue listaccounts(const UniValue& params);
extern UniValue listaddressgroupings(const UniValue& params);
extern UniValue listlabels(const UniValue& params);
extern UniValue listreceivedbyaccount(const UniValue& params);
extern UniValue listreceivedbyaddress(const UniValue& params);
extern UniValue listreceivedbylabel(const UniValue& params);
extern UniValue listsinceblock(const UniValue& params);
extern UniValue liststakes(const UniValue& params);
extern UniValue listtransactions(const UniValue& params);
extern UniValue listunspent(const UniValue& params);
extern UniValue consolidateunspent(const UniValue& params);
extern UniValue splitunspent(const UniValue& params);
extern UniValue makekeypair(const UniValue& params);
extern UniValue makeseedphrase(const UniValue& params);
extern UniValue maintainbackups(const UniValue& params);
extern UniValue migratelabels(const UniValue& params);
extern UniValue movecmd(const UniValue& params);
extern UniValue rainbymagnitude(const UniValue& params);
extern UniValue refundhtlc(const UniValue& params);
extern UniValue repairwallet(const UniValue& params);
extern UniValue resendtx(const UniValue& params);
extern UniValue reservebalance(const UniValue& params);
extern UniValue restoreseedphrase(const UniValue& params);
extern UniValue scanforunspent(const UniValue& params);
extern UniValue sendfrom(const UniValue& params);
extern UniValue sendmany(const UniValue& params);
extern UniValue sendrawtransaction(const UniValue& params);
extern UniValue sendtoaddress(const UniValue& params);
extern UniValue setaccount(const UniValue& params);
extern UniValue sethdseed(const UniValue& params);
extern UniValue setlabel(const UniValue& params);
extern UniValue settxfee(const UniValue& params);
extern UniValue stakelimit(const UniValue& params);
extern UniValue signmessage(const UniValue& params);
extern UniValue signrawtransaction(const UniValue& params);
extern UniValue signrawtransactionwithkey(const UniValue& params);
extern UniValue signrawtransactionwithwallet(const UniValue& params);
extern UniValue sweepuncoveredcoins(const UniValue& params);
extern UniValue upgradewallet(const UniValue& params);
extern UniValue validateaddress(const UniValue& params);
extern UniValue validatepubkey(const UniValue& params);
extern UniValue verifymessage(const UniValue& params);
extern UniValue walletlock(const UniValue& params);
extern UniValue walletpassphrase(const UniValue& params);
extern UniValue walletpassphrasechange(const UniValue& params);
extern UniValue walletdiagnose(const UniValue& params);

// PSGT (Partially Signed Gridcoin Transactions)
extern UniValue analyzepsgt(const UniValue& params);
extern UniValue createpsgt(const UniValue& params);
extern UniValue decodepsgt(const UniValue& params);
extern UniValue combinepsgt(const UniValue& params);
extern UniValue finalizepsgt(const UniValue& params);
extern UniValue walletprocesspsgt(const UniValue& params);
extern UniValue utxoupdatepsgt(const UniValue& params);
extern UniValue converttopsgt(const UniValue& params);
extern UniValue walletcreatefundedpsgt(const UniValue& params);

// PSGT pool (#2910 Phase II)
extern UniValue submitpsgt(const UniValue& params);
extern UniValue listpsgtpool(const UniValue& params);
extern UniValue signpsgtinpool(const UniValue& params);
extern UniValue removepsgtfrompool(const UniValue& params);
extern UniValue getpsgtpoolinfo(const UniValue& params);

// Staking
extern UniValue advertisebeacon(const UniValue& params);
extern UniValue advertisebeaconv3(const UniValue& params);
extern UniValue beaconauth(const UniValue& params);
extern UniValue beaconreport(const UniValue& params);
extern UniValue beaconconvergence(const UniValue& params);
extern UniValue beaconstatus(const UniValue& params);
extern UniValue createmrcrequest(const UniValue& params);
extern UniValue explainmagnitude(const UniValue& params);
extern UniValue generate(const UniValue& params);
extern UniValue generatetoaddress(const UniValue& params);
extern UniValue generatesuperblock(const UniValue& params);
extern UniValue getlaststake(const UniValue& params);
extern UniValue getmrcinfo(const UniValue& params);
extern UniValue getstakinginfo(const UniValue& params);
extern UniValue lifetime(const UniValue& params);
extern UniValue magnitude(const UniValue& params);
extern UniValue pendingbeaconreport(const UniValue& params);
extern UniValue resetcpids(const UniValue& params);
extern UniValue revokebeacon(const UniValue& params);
extern UniValue superblockage(const UniValue& params);
extern UniValue superblocks(const UniValue& params);

// Developers
extern UniValue auditsnapshotaccrual(const UniValue& params);
extern UniValue auditsnapshotaccruals(const UniValue& params);
extern UniValue addkey(const UniValue& params);
extern UniValue registerpool(const UniValue& params);
extern UniValue withdrawpool(const UniValue& params);
extern UniValue approvepool(const UniValue& params);
extern UniValue authorizepool(const UniValue& params);
extern UniValue removepool(const UniValue& params);
extern UniValue listpools(const UniValue& params);
extern UniValue beaconaudit(const UniValue& params);
extern UniValue currentcontractaverage(const UniValue& params);
extern UniValue debug(const UniValue& params);
extern UniValue dumpcontracts(const UniValue& params);
extern UniValue rpc_getblockstats(const UniValue& params);
extern UniValue inspectaccrualsnapshot(const UniValue& params);
extern UniValue listalerts(const UniValue& params);
extern UniValue listprojects(const UniValue& params);
extern UniValue getrawprojectstatus(const UniValue& params);
extern UniValue getautogreylist(const UniValue& params);
extern UniValue listprotocolentries(const UniValue& params);
extern UniValue listresearcheraccounts(const UniValue& params);
extern UniValue listscrapers(const UniValue& params);
extern UniValue listsidestakes(const UniValue& params);
extern UniValue listmandatorysidestakes(const UniValue& params);
extern UniValue listsettings(const UniValue& params);
extern UniValue logging(const UniValue& params);
extern UniValue network(const UniValue& params);
extern UniValue parseaccrualsnapshotfile(const UniValue& params);
extern UniValue parselegacysb(const UniValue& params);
extern UniValue projects(const UniValue& params);
extern UniValue readdata(const UniValue& params);
extern UniValue rpc_reorganize(const UniValue& params);
extern UniValue sendalert(const UniValue& params);
extern UniValue sendalert2(const UniValue& params);
extern UniValue sendblock(const UniValue& params);
extern UniValue superblockaverage(const UniValue& params);
extern UniValue versionreport(const UniValue& params);
extern UniValue writedata(const UniValue& params);

extern UniValue listmanifests(const UniValue& params);
extern UniValue getmanifest(const UniValue& params);
extern UniValue getmpart(const UniValue& params);
extern UniValue sendscraperfilemanifest(const UniValue& params);
extern UniValue savescraperfilemanifest(const UniValue& params);
extern UniValue deletecscrapermanifest(const UniValue& params);
extern UniValue archivelog(const UniValue& params);
extern UniValue testnewsb(const UniValue& params);
extern UniValue convergencereport(const UniValue& params);
extern UniValue scraperreport(const UniValue& params);

// Network
extern UniValue addnode(const UniValue& params);
extern UniValue askforoutstandingblocks(const UniValue& params);
extern UniValue changesettings(const UniValue& params);
extern UniValue clearbanned(const UniValue& params);
extern UniValue currenttime(const UniValue& params);
extern UniValue getaddednodeinfo(const UniValue& params);
extern UniValue getnodeaddresses(const UniValue& params);
extern UniValue getbestblockhash(const UniValue& params);
extern UniValue getblock(const UniValue& params);
extern UniValue getblockbynumber(const UniValue& params);
extern UniValue getblockbymintime(const UniValue& params);
extern UniValue getblocksbatch(const UniValue& params);
extern UniValue getblockchaininfo(const UniValue& params);
extern UniValue getblockcount(const UniValue& params);
extern UniValue setmocktime(const UniValue& params);
extern UniValue getblockhash(const UniValue& params);
extern UniValue getburnreport(const UniValue& params);
extern UniValue getcheckpoint(const UniValue& params);
extern UniValue getconnectioncount(const UniValue& params);
extern UniValue getdifficulty(const UniValue& params);
extern UniValue getinfo(const UniValue& params); // To Be Deprecated --> getblockchaininfo getnetworkinfo getwalletinfo
extern UniValue getnettotals(const UniValue& params);
extern UniValue getclockstats(const UniValue& params);
extern UniValue getnetworkinfo(const UniValue& params);
extern UniValue getpeerinfo(const UniValue& params);
extern UniValue getrawmempool(const UniValue& params);
extern UniValue getmempoolentry(const UniValue& params);
extern UniValue getmempoolinfo(const UniValue& params);
extern UniValue testmempoolaccept(const UniValue& params);
extern UniValue listbanned(const UniValue& params);
extern UniValue networktime(const UniValue& params);
extern UniValue ping(const UniValue& params);
extern UniValue rpc_exportstats(const UniValue& params);
extern UniValue rpc_getrecentblocks(const UniValue& params);
extern UniValue setban(const UniValue& params);
extern UniValue disconnectnode(const UniValue& params);
extern UniValue showblock(const UniValue& params);

// Voting
extern UniValue addpoll(const UniValue& params);
extern UniValue getpollresults(const UniValue& params);
extern UniValue getvotingclaim(const UniValue& params);
extern UniValue listpolls(const UniValue& params);
extern UniValue testpollnotification(const UniValue& params);
extern UniValue vote(const UniValue& params);
extern UniValue votebyid(const UniValue& params);
extern UniValue votedetails(const UniValue& params);

int GetDefaultRPCPort();

// RPCHelpMan accessors — one per converted command. Most return the command's
// file-scope `static const RPCHelpMan` instance. The exception is
// `addpoll_helpman()`, whose help shape depends on the poll-payload version: it
// returns a reference into a function-local `static std::map<uint32_t,
// AddPollBuild>` cache (see `addpoll_build()` in voting.cpp), built lazily per
// version. All are wired into `CRPCCommand::helpman` so the dispatcher can
// render help text and validate arity without invoking the command body.
// Generated by the lift-to-accessor refactor in PR M.
extern const RPCHelpMan& abandontransaction_helpman();
extern const RPCHelpMan& cancelunbroadcasttransaction_helpman();
extern const RPCHelpMan& addkey_helpman();
extern const RPCHelpMan& addmultisigaddress_helpman();
extern const RPCHelpMan& addnode_helpman();
extern const RPCHelpMan& addpoll_helpman();
//! Render addpoll's help for an explicit poll-payload version, bypassing chain
//! state. addpoll_helpman() and the addpoll body both derive their shape from
//! the same per-version build, so this exposes that pure derivation for tests
//! that need to exercise the v2/v3 fork flip deterministically (no live chain).
extern std::string addpoll_help_for_version(uint32_t payload_version);
extern const RPCHelpMan& addredeemscript_helpman();
extern const RPCHelpMan& advertisebeacon_helpman();
extern const RPCHelpMan& advertisebeaconv3_helpman();
extern const RPCHelpMan& approvepool_helpman();
extern const RPCHelpMan& archivelog_helpman();
extern const RPCHelpMan& askforoutstandingblocks_helpman();
extern const RPCHelpMan& auditsnapshotaccrual_helpman();
extern const RPCHelpMan& auditsnapshotaccruals_helpman();
extern const RPCHelpMan& authorizepool_helpman();
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
extern const RPCHelpMan& analyzepsgt_helpman();
extern const RPCHelpMan& combinepsgt_helpman();
extern const RPCHelpMan& converttopsgt_helpman();
extern const RPCHelpMan& createpsgt_helpman();
extern const RPCHelpMan& decodepsgt_helpman();
extern const RPCHelpMan& finalizepsgt_helpman();
extern const RPCHelpMan& inspectwalletstate_helpman();
extern const RPCHelpMan& utxoupdatepsgt_helpman();
extern const RPCHelpMan& walletcreatefundedpsgt_helpman();
extern const RPCHelpMan& walletprocesspsgt_helpman();
extern const RPCHelpMan& submitpsgt_helpman();
extern const RPCHelpMan& listpsgtpool_helpman();
extern const RPCHelpMan& signpsgtinpool_helpman();
extern const RPCHelpMan& removepsgtfrompool_helpman();
extern const RPCHelpMan& getpsgtpoolinfo_helpman();
extern const RPCHelpMan& currenttime_helpman();
extern const RPCHelpMan& debug_helpman();
extern const RPCHelpMan& decoderawtransaction_helpman();
extern const RPCHelpMan& decodescript_helpman();
extern const RPCHelpMan& deletecscrapermanifest_helpman();
extern const RPCHelpMan& dumpcontracts_helpman();
extern const RPCHelpMan& dumpprivkey_helpman();
extern const RPCHelpMan& dumpseedphrase_helpman();
extern const RPCHelpMan& dumpwallet_helpman();
extern const RPCHelpMan& encryptwallet_helpman();
extern const RPCHelpMan& explainmagnitude_helpman();
extern const RPCHelpMan& fundrawtransaction_helpman();
extern const RPCHelpMan& generate_helpman();
extern const RPCHelpMan& generatesuperblock_helpman();
extern const RPCHelpMan& generatetoaddress_helpman();
extern const RPCHelpMan& getaccount_helpman();
extern const RPCHelpMan& getaccountaddress_helpman();
extern const RPCHelpMan& getaddednodeinfo_helpman();
extern const RPCHelpMan& getaddressesbyaccount_helpman();
extern const RPCHelpMan& getaddressesbylabel_helpman();
extern const RPCHelpMan& getautogreylist_helpman();
extern const RPCHelpMan& getbalance_helpman();
extern const RPCHelpMan& getbalancedetail_helpman();
extern const RPCHelpMan& getbestblockhash_helpman();
extern const RPCHelpMan& getblock_helpman();
extern const RPCHelpMan& getblockbymintime_helpman();
extern const RPCHelpMan& getblockbynumber_helpman();
extern const RPCHelpMan& getblockchaininfo_helpman();
extern const RPCHelpMan& getblockcount_helpman();
extern const RPCHelpMan& setmocktime_helpman();
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
extern const RPCHelpMan& getclockstats_helpman();
extern const RPCHelpMan& getnetworkinfo_helpman();
extern const RPCHelpMan& getnewaddress_helpman();
extern const RPCHelpMan& getnewpubkey_helpman();
extern const RPCHelpMan& getnodeaddresses_helpman();
extern const RPCHelpMan& getpeerinfo_helpman();
extern const RPCHelpMan& getpollresults_helpman();
extern const RPCHelpMan& getrawmempool_helpman();
extern const RPCHelpMan& getmempoolentry_helpman();
extern const RPCHelpMan& getmempoolinfo_helpman();
extern const RPCHelpMan& testmempoolaccept_helpman();
extern const RPCHelpMan& getrawprojectstatus_helpman();
extern const RPCHelpMan& getrawtransaction_helpman();
extern const RPCHelpMan& getrawwallettransaction_helpman();
extern const RPCHelpMan& gettxoutproof_helpman();
extern const RPCHelpMan& verifytxoutproof_helpman();
extern const RPCHelpMan& getreceivedbyaccount_helpman();
extern const RPCHelpMan& getreceivedbyaddress_helpman();
extern const RPCHelpMan& getreceivedbylabel_helpman();
extern const RPCHelpMan& getseedphraseinfo_helpman();
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
extern const RPCHelpMan& listlabels_helpman();
extern const RPCHelpMan& listalerts_helpman();
extern const RPCHelpMan& listbanned_helpman();
extern const RPCHelpMan& listmandatorysidestakes_helpman();
extern const RPCHelpMan& listmanifests_helpman();
extern const RPCHelpMan& listpolls_helpman();
extern const RPCHelpMan& listpools_helpman();
extern const RPCHelpMan& listprojects_helpman();
extern const RPCHelpMan& listprotocolentries_helpman();
extern const RPCHelpMan& listreceivedbyaccount_helpman();
extern const RPCHelpMan& listreceivedbyaddress_helpman();
extern const RPCHelpMan& listreceivedbylabel_helpman();
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
extern const RPCHelpMan& makeseedphrase_helpman();
extern const RPCHelpMan& migratelabels_helpman();
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
extern const RPCHelpMan& registerpool_helpman();
extern const RPCHelpMan& removepool_helpman();
extern const RPCHelpMan& repairwallet_helpman();
extern const RPCHelpMan& resendtx_helpman();
extern const RPCHelpMan& reservebalance_helpman();
extern const RPCHelpMan& resetcpids_helpman();
extern const RPCHelpMan& restoreseedphrase_helpman();
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
extern const RPCHelpMan& disconnectnode_helpman();
extern const RPCHelpMan& sethdseed_helpman();
extern const RPCHelpMan& setlabel_helpman();
extern const RPCHelpMan& settxfee_helpman();
extern const RPCHelpMan& showblock_helpman();
extern const RPCHelpMan& signmessage_helpman();
extern const RPCHelpMan& signrawtransaction_helpman();
extern const RPCHelpMan& signrawtransactionwithkey_helpman();
extern const RPCHelpMan& signrawtransactionwithwallet_helpman();
extern const RPCHelpMan& splitunspent_helpman();
extern const RPCHelpMan& stakelimit_helpman();
extern const RPCHelpMan& stop_helpman();
extern const RPCHelpMan& sweepuncoveredcoins_helpman();
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
extern const RPCHelpMan& withdrawpool_helpman();
extern const RPCHelpMan& writedata_helpman();

#endif // BITCOIN_RPC_SERVER_H
