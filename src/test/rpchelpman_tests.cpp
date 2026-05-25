// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/util.h>

#include <rpc/protocol.h>
#include <rpc/server.h>   // PR M2: bring in the extern <cmd>_helpman() accessor declarations
#include <tinyformat.h>
#include <univalue.h>

#include <boost/test/unit_test.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Forward declarations of the Tier 1a blockchain-core and Tier 1b
// researcher/beacon/MRC commands under test.
// Must live in the global namespace; placing them inside BOOST_AUTO_TEST_SUITE
// captures them into the suite's namespace and breaks linkage. The tier
// help-rendering tests below call each command's RPCHelpMan accessor (not the
// body), so no fixture is needed.
UniValue getbestblockhash(const UniValue& params);
UniValue getblockcount(const UniValue& params);
UniValue getdifficulty(const UniValue& params);
UniValue getblockhash(const UniValue& params);
UniValue getblock(const UniValue& params);
UniValue getblockbynumber(const UniValue& params);
UniValue getblockbymintime(const UniValue& params);
UniValue getblocksbatch(const UniValue& params);
UniValue showblock(const UniValue& params);
UniValue getrawmempool(const UniValue& params);
UniValue getblockchaininfo(const UniValue& params);
UniValue getcheckpoint(const UniValue& params);
UniValue getburnreport(const UniValue& params);
UniValue rpc_reorganize(const UniValue& params);
UniValue currenttime(const UniValue& params);
UniValue networktime(const UniValue& params);
UniValue network(const UniValue& params);
UniValue sendblock(const UniValue& params);
UniValue askforoutstandingblocks(const UniValue& params);
UniValue debug(const UniValue& params);
UniValue versionreport(const UniValue& params);
UniValue advertisebeacon(const UniValue& params);
UniValue advertisebeaconv3(const UniValue& params);
UniValue beaconauth(const UniValue& params);
UniValue revokebeacon(const UniValue& params);
UniValue beaconreport(const UniValue& params);
UniValue beaconconvergence(const UniValue& params);
UniValue pendingbeaconreport(const UniValue& params);
UniValue beaconstatus(const UniValue& params);
UniValue beaconaudit(const UniValue& params);
UniValue getmrcinfo(const UniValue& params);
UniValue createmrcrequest(const UniValue& params);
UniValue magnitude(const UniValue& params);
UniValue explainmagnitude(const UniValue& params);
UniValue lifetime(const UniValue& params);
UniValue resetcpids(const UniValue& params);
UniValue rainbymagnitude(const UniValue& params);
UniValue currentcontractaverage(const UniValue& params);

// Forward declarations of the Tier 2 commands under test. These must live in
// the global namespace; if placed inside BOOST_AUTO_TEST_SUITE(...) they get
// captured into the suite's namespace and fail to link against the
// definitions in src/rpc/blockchain.cpp, src/rpc/net.cpp, src/wallet/rpcwallet.cpp.
// Each command's converted body throws for fHelp=true before touching any
// globals (vNodes, g_banman, pwalletMain, mapBlockIndex, locks), so calling
// these with fHelp=true and an empty params array is safe in unit tests.
UniValue settxfee(const UniValue& params);
UniValue addnode(const UniValue& params);
UniValue setban(const UniValue& params);
UniValue listsinceblock(const UniValue& params);

// Tier 1c forward declarations (snapshots, registries, generic-data, dumpcontracts).
// Same global-namespace placement requirement as the Tier 2 block above.
UniValue superblocks(const UniValue& params);
UniValue superblockage(const UniValue& params);
UniValue superblockaverage(const UniValue& params);
UniValue parselegacysb(const UniValue& params);
UniValue listscrapers(const UniValue& params);
UniValue listprojects(const UniValue& params);
UniValue projects(const UniValue& params);
UniValue getautogreylist(const UniValue& params);
UniValue listsidestakes(const UniValue& params);
UniValue listmandatorysidestakes(const UniValue& params);
UniValue listprotocolentries(const UniValue& params);
UniValue readdata(const UniValue& params);
UniValue writedata(const UniValue& params);
UniValue dumpcontracts(const UniValue& params);
// Tier 1 PR D1: src/rpc/server.cpp + src/rpc/misc.cpp + src/rpc/dataacq.cpp.
// Each function's converted body throws via help.ToString() before touching
// globals (cs_main, gArgs, log instance, file IO), so calling these with
// fHelp=true and an empty params array is safe in unit tests.
UniValue help(const UniValue& params);
UniValue stop(const UniValue& params);
UniValue logging(const UniValue& params);
UniValue listsettings(const UniValue& params);
UniValue changesettings(const UniValue& params);
UniValue rpc_getblockstats(const UniValue& params);
UniValue rpc_exportstats(const UniValue& params);
UniValue rpc_getrecentblocks(const UniValue& params);
// Tier 1 PR D2: remaining src/rpc/net.cpp commands. Each function's converted
// body throws via help.ToString() before touching any globals (vNodes,
// g_banman, mapAlerts, addrman, cs_main, cs_vNodes), so calling these with
// fHelp=true and an empty params array is safe in unit tests.
UniValue getconnectioncount(const UniValue& params);
UniValue getnodeaddresses(const UniValue& params);
UniValue getaddednodeinfo(const UniValue& params);
UniValue listbanned(const UniValue& params);
UniValue clearbanned(const UniValue& params);
UniValue ping(const UniValue& params);
UniValue getpeerinfo(const UniValue& params);
UniValue getnettotals(const UniValue& params);
UniValue listalerts(const UniValue& params);
UniValue sendalert(const UniValue& params);
UniValue sendalert2(const UniValue& params);
UniValue getnetworkinfo(const UniValue& params);
// Tier 1 PR D3: src/rpc/voting.cpp non-deprecated commands. Each function's
// converted body throws via help.ToString() before touching globals (the
// poll registry, cs_main, pwalletMain), so calling these with fHelp=true
// and an empty params array is safe in unit tests.
UniValue listpolls(const UniValue& params);
UniValue getpollresults(const UniValue& params);
UniValue getvotingclaim(const UniValue& params);
UniValue votebyid(const UniValue& params);
UniValue votedetails(const UniValue& params);
UniValue testpollnotification(const UniValue& params);
// Tier 1 PR E1: src/rpc/mining.cpp commands. Each function's converted body
// throws via help.ToString() before touching globals (g_miner_status,
// cs_main, pwalletMain, snapshot files), so calling these with fHelp=true
// and an empty params array is safe in unit tests.
UniValue getstakinginfo(const UniValue& params);
UniValue getlaststake(const UniValue& params);
UniValue auditsnapshotaccrual(const UniValue& params);
UniValue auditsnapshotaccruals(const UniValue& params);
UniValue listresearcheraccounts(const UniValue& params);
UniValue inspectaccrualsnapshot(const UniValue& params);
UniValue parseaccrualsnapshotfile(const UniValue& params);
// Tier 1 PR E2: src/gridcoin/scraper/scraper.cpp + scraper_net.cpp commands.
// Each function's converted body throws via help.ToString() before touching
// any scraper globals or locks.
UniValue sendscraperfilemanifest(const UniValue& params);
UniValue savescraperfilemanifest(const UniValue& params);
UniValue deletecscrapermanifest(const UniValue& params);
UniValue archivelog(const UniValue& params);
UniValue convergencereport(const UniValue& params);
UniValue testnewsb(const UniValue& params);
UniValue scraperreport(const UniValue& params);
UniValue listmanifests(const UniValue& params);
UniValue getmpart(const UniValue& params);
// Tier 1 PR E3: src/rpc/rawtransaction.cpp + src/rpc/htlc.cpp commands.
// Each function's converted body throws via help.ToString() before touching
// any globals (cs_main, pwalletMain, mempool).
UniValue getrawtransaction(const UniValue& params);
UniValue listunspent(const UniValue& params);
UniValue consolidateunspent(const UniValue& params);
UniValue consolidatemsunspent(const UniValue& params);
UniValue scanforunspent(const UniValue& params);
UniValue createrawtransaction(const UniValue& params);
UniValue fundrawtransaction(const UniValue& params);
UniValue decoderawtransaction(const UniValue& params);
UniValue decodescript(const UniValue& params);
UniValue signrawtransactionwithkey(const UniValue& params);
UniValue signrawtransactionwithwallet(const UniValue& params);
UniValue signrawtransaction(const UniValue& params);
UniValue sendrawtransaction(const UniValue& params);
UniValue createhtlc(const UniValue& params);
UniValue claimhtlc(const UniValue& params);
UniValue refundhtlc(const UniValue& params);
// Tier 1 PR F1: src/wallet/rpcdump.cpp commands. Each function's converted
// body throws via help.ToString() before touching pwalletMain or file IO.
UniValue importprivkey(const UniValue& params);
UniValue importwallet(const UniValue& params);
UniValue dumpprivkey(const UniValue& params);
UniValue dumpwallet(const UniValue& params);
// Tier 1 deprecated batch: the deprecated-accounts wallet RPCs plus the
// deprecated `vote` RPC. Each function's converted body throws via
// help.ToString() before touching pwalletMain or the poll registry.
UniValue getaccountaddress(const UniValue& params);
UniValue setaccount(const UniValue& params);
UniValue getaccount(const UniValue& params);
UniValue getaddressesbyaccount(const UniValue& params);
UniValue getreceivedbyaccount(const UniValue& params);
UniValue listreceivedbyaccount(const UniValue& params);
UniValue listaccounts(const UniValue& params);
UniValue movecmd(const UniValue& params);  // dispatched as "move"
UniValue vote(const UniValue& params);
// Forward declarations of the Tier 1 F2 wallet-keys commands under test.
UniValue getnewpubkey(const UniValue& params);
UniValue getnewaddress(const UniValue& params);
UniValue signmessage(const UniValue& params);
UniValue verifymessage(const UniValue& params);
UniValue addmultisigaddress(const UniValue& params);
UniValue addredeemscript(const UniValue& params);
UniValue validateaddress(const UniValue& params);
UniValue validatepubkey(const UniValue& params);
UniValue makekeypair(const UniValue& params);
UniValue sethdseed(const UniValue& params);

// Forward declarations of the Tier 1 F3 wallet-query commands under test.
UniValue getinfo(const UniValue& params);
UniValue getwalletinfo(const UniValue& params);
UniValue listaddressgroupings(const UniValue& params);
UniValue getreceivedbyaddress(const UniValue& params);
UniValue getbalance(const UniValue& params);
UniValue getbalancedetail(const UniValue& params);
UniValue getunconfirmedbalance(const UniValue& params);
UniValue listreceivedbyaddress(const UniValue& params);
UniValue listtransactions(const UniValue& params);
UniValue liststakes(const UniValue& params);
UniValue gettransaction(const UniValue& params);
UniValue getrawwallettransaction(const UniValue& params);

// Forward declarations of the Tier 1 F4 wallet management/send commands under test.
UniValue sendtoaddress(const UniValue& params);
UniValue sendfrom(const UniValue& params);
UniValue sendmany(const UniValue& params);
UniValue backupwallet(const UniValue& params);
UniValue keypoolrefill(const UniValue& params);
UniValue walletdiagnose(const UniValue& params);
UniValue encryptwallet(const UniValue& params);
UniValue reservebalance(const UniValue& params);
UniValue checkwallet(const UniValue& params);
UniValue repairwallet(const UniValue& params);
UniValue resendtx(const UniValue& params);
UniValue burn(const UniValue& params);
UniValue upgradewallet(const UniValue& params);
// Forward declarations of the wallet transaction-state debug commands under test.
UniValue abandontransaction(const UniValue& params);
UniValue inspectwalletstate(const UniValue& params);

// Forward declarations of the PSGT (Partially Signed Gridcoin Transaction) commands under test.
UniValue createpsgt(const UniValue& params);
UniValue decodepsgt(const UniValue& params);
UniValue combinepsgt(const UniValue& params);
UniValue finalizepsgt(const UniValue& params);
UniValue walletprocesspsgt(const UniValue& params);
UniValue utxoupdatepsgt(const UniValue& params);
UniValue converttopsgt(const UniValue& params);
UniValue walletcreatefundedpsgt(const UniValue& params);

BOOST_AUTO_TEST_SUITE(rpchelpman_tests)

// Helper: build a minimal RPCHelpMan with one required string arg and one result.
static RPCHelpMan MakeTrivial()
{
    return RPCHelpMan{
        "trivialcmd",
        "A trivial command used to exercise RPCHelpMan::ToString.",
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The address to look up."},
        },
        RPCResult{RPCResult::Type::STR_HEX, "", "The hex-encoded result."},
        RPCExamples{
            HelpExampleCli("trivialcmd", "\"S1Example\"") +
            HelpExampleRpc("trivialcmd", "\"S1Example\"")},
    };
}

BOOST_AUTO_TEST_CASE(tostring_trivial)
{
    const RPCHelpMan help = MakeTrivial();
    const std::string out = help.ToString();

    // Expected structural markers from the upstream renderer.
    BOOST_CHECK(out.find("trivialcmd") != std::string::npos);
    BOOST_CHECK(out.find("A trivial command") != std::string::npos);
    BOOST_CHECK(out.find("Arguments:") != std::string::npos);
    BOOST_CHECK(out.find("1. address") != std::string::npos);
    BOOST_CHECK(out.find("required") != std::string::npos);
    BOOST_CHECK(out.find("Result:") != std::string::npos);
    BOOST_CHECK(out.find("\"hex\"") != std::string::npos);
    BOOST_CHECK(out.find("Examples:") != std::string::npos);
    BOOST_CHECK(out.find("gridcoinresearch") != std::string::npos);
    BOOST_CHECK(out.find("curl") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(isvalidnumargs_boundaries)
{
    // Zero args (no-arg command).
    const RPCHelpMan zero{"zero", "No args.", {},
                         RPCResult{RPCResult::Type::NONE, "", ""},
                         RPCExamples{""}};
    BOOST_CHECK(zero.IsValidNumArgs(0));
    BOOST_CHECK(!zero.IsValidNumArgs(1));

    // One required + two optional.
    const RPCHelpMan mixed{
        "mixed", "Mixed args.",
        {
            {"a", RPCArg::Type::STR, RPCArg::Optional::NO, "required"},
            {"b", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "optional"},
            {"c", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED, "optional"},
        },
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{""}};
    BOOST_CHECK(!mixed.IsValidNumArgs(0)); // missing required
    BOOST_CHECK(mixed.IsValidNumArgs(1));
    BOOST_CHECK(mixed.IsValidNumArgs(2));
    BOOST_CHECK(mixed.IsValidNumArgs(3));
    BOOST_CHECK(!mixed.IsValidNumArgs(4)); // too many

    // All required.
    const RPCHelpMan all_required{
        "ar", "All required.",
        {
            {"x", RPCArg::Type::STR, RPCArg::Optional::NO, "a"},
            {"y", RPCArg::Type::STR, RPCArg::Optional::NO, "b"},
        },
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{""}};
    BOOST_CHECK(!all_required.IsValidNumArgs(0));
    BOOST_CHECK(!all_required.IsValidNumArgs(1));
    BOOST_CHECK(all_required.IsValidNumArgs(2));
    BOOST_CHECK(!all_required.IsValidNumArgs(3));
}

// PR M2: variadic marker opts a command out of the dispatcher's
// IsValidNumArgs upper-bound pre-check. The body is then responsible
// for whatever real arity the command accepts. IsValidNumArgs itself
// stays unchanged — the dispatcher gates on IsVariadic() before
// invoking IsValidNumArgs at all.
BOOST_AUTO_TEST_CASE(markvariadic_default_and_marker)
{
    const RPCHelpMan plain{"plain", "Not variadic.",
                          {{"a", RPCArg::Type::STR, RPCArg::Optional::NO, "required"}},
                          RPCResult{RPCResult::Type::NONE, "", ""},
                          RPCExamples{""}};
    BOOST_CHECK(!plain.IsVariadic());

    const RPCHelpMan marked = RPCHelpMan{"marked", "Variadic.",
                                       {{"a", RPCArg::Type::STR, RPCArg::Optional::NO, "required"}},
                                       RPCResult{RPCResult::Type::NONE, "", ""},
                                       RPCExamples{""}}.MarkVariadic();
    BOOST_CHECK(marked.IsVariadic());
    // The marker does not relax IsValidNumArgs itself — the dispatcher
    // is the one that consults IsVariadic before calling IsValidNumArgs.
    BOOST_CHECK(!marked.IsValidNumArgs(0)); // still rejects missing required
    BOOST_CHECK(marked.IsValidNumArgs(1));
    BOOST_CHECK(!marked.IsValidNumArgs(2)); // upper-bound logic unchanged
}

// Each of the four commands that needed lower-bound-only arity semantics
// must report IsVariadic via its accessor so the dispatcher pre-check
// skips them. Their bodies retain their own arity guards.
BOOST_AUTO_TEST_CASE(variadic_command_accessors_report_variadic)
{
    BOOST_CHECK(votebyid_helpman().IsVariadic());
    BOOST_CHECK(changesettings_helpman().IsVariadic());
    BOOST_CHECK(sendalert_helpman().IsVariadic());
    BOOST_CHECK(parselegacysb_helpman().IsVariadic());

    // Spot-check a non-variadic command for the converse — a typical
    // converted RPC should not be variadic by default.
    BOOST_CHECK(!getbestblockhash_helpman().IsVariadic());
}

// Pin the dispatcher's arity gate. CRPCTable::execute (src/rpc/server.cpp)
// rejects a converted command when `!IsVariadic() && !IsValidNumArgs(n)`.
// The accessor-reports-variadic test above proves the flag is set; this test
// proves the flag is *load-bearing* — i.e. that a variadic command's
// over-the-declared-max arg count is NOT rejected before the body runs,
// while the same over-count on a non-variadic command IS. We assert the gate
// predicate directly rather than calling CRPCTable::execute, which would
// require a live node/wallet to invoke the command body.
BOOST_AUTO_TEST_CASE(variadic_dispatcher_precheck_skips_overmax_args)
{
    // Mirror of the dispatcher gate in CRPCTable::execute.
    const auto dispatcher_rejects = [](const RPCHelpMan& help, size_t n) {
        return !help.IsVariadic() && !help.IsValidNumArgs(n);
    };

    // votebyid declares 2 fixed args; the multichoice form passes 3+.
    const RPCHelpMan& votebyid = votebyid_helpman();
    BOOST_CHECK(votebyid.IsValidNumArgs(2));         // the declared shape
    BOOST_CHECK(!votebyid.IsValidNumArgs(3));        // over the declared max...
    BOOST_CHECK(!dispatcher_rejects(votebyid, 3));   // ...but the gate lets it through
    BOOST_CHECK(!dispatcher_rejects(votebyid, 9));   // arbitrarily many choices

    // changesettings declares 1 fixed arg; the multi-setting form passes 2+.
    const RPCHelpMan& changesettings = changesettings_helpman();
    BOOST_CHECK(changesettings.IsValidNumArgs(1));
    BOOST_CHECK(!changesettings.IsValidNumArgs(2));
    BOOST_CHECK(!dispatcher_rejects(changesettings, 2));
    BOOST_CHECK(!dispatcher_rejects(changesettings, 5));

    // Converse: a non-variadic converted command IS rejected by the gate once
    // it exceeds its declared arity — proving the marker is what spares the
    // variadic commands above.
    const RPCHelpMan& nonvariadic = getbestblockhash_helpman();
    const size_t overmax = nonvariadic.GetArgNames().size() + 1;
    BOOST_CHECK(dispatcher_rejects(nonvariadic, overmax));
}

BOOST_AUTO_TEST_CASE(getargnames_mixed)
{
    const RPCHelpMan help{
        "names", "Name list test.",
        {
            {"first", RPCArg::Type::STR, RPCArg::Optional::NO, "first arg"},
            {"second|alias", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "alias arg"},
        },
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{""}};

    const auto names = help.GetArgNames();
    BOOST_CHECK_EQUAL(names.size(), 2u);
    BOOST_CHECK_EQUAL(names[0].first, "first");
    BOOST_CHECK(!names[0].second); // not named-only
    BOOST_CHECK_EQUAL(names[1].first, "second|alias");
    BOOST_CHECK(!names[1].second);
}

BOOST_AUTO_TEST_CASE(helpexample_cli_formatting)
{
    const std::string cli = HelpExampleCli("somecmd", "\"arg1\" 42");
    BOOST_CHECK_EQUAL(cli, "> gridcoinresearch somecmd \"arg1\" 42\n");

    const std::string empty_args = HelpExampleCli("nopargs", "");
    BOOST_CHECK_EQUAL(empty_args, "> gridcoinresearch nopargs \n");
}

BOOST_AUTO_TEST_CASE(helpexample_cli_named_formatting)
{
    RPCArgList args;
    UniValue v_str;
    v_str.setStr("hello world"); // contains a space -> should be shell-quoted
    args.emplace_back("greeting", v_str);
    UniValue v_num;
    v_num.setInt(42);
    args.emplace_back("count", v_num);

    const std::string cli = HelpExampleCliNamed("somecmd", args);
    BOOST_CHECK(cli.find("> gridcoinresearch -named somecmd") != std::string::npos);
    BOOST_CHECK(cli.find("greeting='hello world'") != std::string::npos);
    BOOST_CHECK(cli.find("count=42") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(helpexample_rpc_formatting)
{
    const std::string rpc = HelpExampleRpc("somecmd", "\"arg1\", 42");
    BOOST_CHECK(rpc.find("curl --user myusername") != std::string::npos);
    BOOST_CHECK(rpc.find("\"method\": \"somecmd\"") != std::string::npos);
    BOOST_CHECK(rpc.find("\"params\": [\"arg1\", 42]") != std::string::npos);
    BOOST_CHECK(rpc.find("127.0.0.1:9332") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(rpcarg_tostring_oneline_every_scalar_type)
{
    // STR
    {
        const RPCArg a{"addr", RPCArg::Type::STR, RPCArg::Optional::NO, "desc"};
        BOOST_CHECK_EQUAL(a.ToString(/*oneline=*/true), "\"addr\"");
    }
    // STR_HEX
    {
        const RPCArg a{"hash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "desc"};
        BOOST_CHECK_EQUAL(a.ToString(/*oneline=*/true), "\"hash\"");
    }
    // NUM / AMOUNT / RANGE / BOOL: rendered by their name only
    {
        const RPCArg n{"count", RPCArg::Type::NUM, RPCArg::Optional::NO, "desc"};
        BOOST_CHECK_EQUAL(n.ToString(/*oneline=*/true), "count");

        const RPCArg amt{"amount", RPCArg::Type::AMOUNT, RPCArg::Optional::NO, "desc"};
        BOOST_CHECK_EQUAL(amt.ToString(/*oneline=*/true), "amount");

        const RPCArg r{"height", RPCArg::Type::RANGE, RPCArg::Optional::NO, "desc"};
        BOOST_CHECK_EQUAL(r.ToString(/*oneline=*/true), "height");

        const RPCArg b{"verbose", RPCArg::Type::BOOL, RPCArg::Optional::NO, "desc"};
        BOOST_CHECK_EQUAL(b.ToString(/*oneline=*/true), "verbose");
    }
}

BOOST_AUTO_TEST_CASE(rpcresult_description_various_types)
{
    // STR_HEX top-level
    {
        const RPCResults r{RPCResult{RPCResult::Type::STR_HEX, "", "the hex encoded hash"}};
        const std::string desc = r.ToDescriptionString();
        BOOST_CHECK(desc.find("Result:") != std::string::npos);
        BOOST_CHECK(desc.find("\"hex\"") != std::string::npos);
        BOOST_CHECK(desc.find("the hex encoded hash") != std::string::npos);
    }
    // OBJ with two fields
    {
        const RPCResults r{RPCResult{
            RPCResult::Type::OBJ, "", "an object",
            {
                {RPCResult::Type::STR, "name", "the name"},
                {RPCResult::Type::NUM, "height", "the height"},
            }}};
        const std::string desc = r.ToDescriptionString();
        BOOST_CHECK(desc.find("{") != std::string::npos);
        BOOST_CHECK(desc.find("\"name\"") != std::string::npos);
        BOOST_CHECK(desc.find("\"height\"") != std::string::npos);
        BOOST_CHECK(desc.find("}") != std::string::npos);
    }
    // ARR with inner STR
    {
        const RPCResults r{RPCResult{
            RPCResult::Type::ARR, "", "an array",
            {
                {RPCResult::Type::STR, "", "an element"},
            }}};
        const std::string desc = r.ToDescriptionString();
        BOOST_CHECK(desc.find("[") != std::string::npos);
        BOOST_CHECK(desc.find("]") != std::string::npos);
    }
    // OBJ_DYN with a single inner
    {
        const RPCResults r{RPCResult{
            RPCResult::Type::OBJ_DYN, "", "dynamic keys",
            {
                {RPCResult::Type::NUM, "key", "value for this key"},
            }}};
        const std::string desc = r.ToDescriptionString();
        BOOST_CHECK(desc.find("{") != std::string::npos);
        BOOST_CHECK(desc.find("...") != std::string::npos); // dynamic continuation marker
    }
}

// Per-call (non-static) construction with runtime-determined description and
// arg list. This validates the pattern that Tier 3 commands like addkey/addpoll
// will use when their help text depends on protocol version or node state.
BOOST_AUTO_TEST_CASE(percall_construction_dynamic_help)
{
    for (int version : {1, 2}) {
        std::vector<RPCArg> args{
            {"base_arg", RPCArg::Type::STR, RPCArg::Optional::NO, "always present"},
        };
        std::string description = strprintf("Command supporting protocol version %d.", version);
        if (version >= 2) {
            args.push_back({"v2_only", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "only valid at v2+"});
        }

        const RPCHelpMan help{
            "dyncmd", description, std::move(args),
            RPCResult{RPCResult::Type::NONE, "", ""},
            RPCExamples{""}};

        const std::string out = help.ToString();
        BOOST_CHECK(out.find("dyncmd") != std::string::npos);
        BOOST_CHECK(out.find(strprintf("protocol version %d", version)) != std::string::npos);

        if (version == 1) {
            BOOST_CHECK(help.IsValidNumArgs(1));
            BOOST_CHECK(!help.IsValidNumArgs(2));
            BOOST_CHECK(out.find("v2_only") == std::string::npos);
        } else {
            BOOST_CHECK(help.IsValidNumArgs(1)); // v2_only is optional
            BOOST_CHECK(help.IsValidNumArgs(2));
            BOOST_CHECK(!help.IsValidNumArgs(3));
            BOOST_CHECK(out.find("v2_only") != std::string::npos);
        }
    }
}

// Exercises the OBJ-typed RPCArg constructor: full RPCHelpMan rendering with
// an object argument containing two scalar inner fields. Verifies that
// Sections::Push recurses into m_inner, that the rendered help reaches the
// "Arguments:" section for both the OBJ wrapper and its scalars, and that
// RPCArg::ToStringObj() handles scalar inners. Does *not* nest object inside
// object — that path intentionally trips CHECK_NONFATAL(false) (see TODO at
// RPCArg::ToStringObj definition).
BOOST_AUTO_TEST_CASE(rpcarg_obj_with_scalar_inner_renders)
{
    const RPCHelpMan help{
        "objcmd",
        "A command that takes an options object.",
        {
            {"options", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED,
                "Options for the command.",
                {
                    {"feerate", RPCArg::Type::AMOUNT, RPCArg::Optional::OMITTED,
                        "Fee rate, GRC/kB."},
                    {"replaceable", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
                        "Whether to mark the transaction as BIP125-replaceable."},
                }},
        },
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{HelpExampleCli("objcmd", "")},
    };

    const std::string out = help.ToString();

    // The OBJ wrapper appears in the Arguments: section by name.
    BOOST_CHECK(out.find("Arguments:") != std::string::npos);
    BOOST_CHECK(out.find("options") != std::string::npos);
    // Both scalar inner fields appear by name in the rendered help.
    BOOST_CHECK(out.find("feerate") != std::string::npos);
    BOOST_CHECK(out.find("replaceable") != std::string::npos);

    // GetArgNames returns just the top-level wrapper, not its inner fields.
    const auto names = help.GetArgNames();
    BOOST_CHECK_EQUAL(names.size(), 1u);
    BOOST_CHECK_EQUAL(names[0].first, "options");
}

// Verify that throwing ToString() via the typical Tier 1 call pattern produces
// a runtime_error whose message contains the rendered help.
BOOST_AUTO_TEST_CASE(tier1_throw_pattern)
{
    const RPCHelpMan help = MakeTrivial();
    const bool fHelp = true;
    const UniValue params(UniValue::VARR);

    BOOST_CHECK_THROW(
        {
            if (fHelp || !help.IsValidNumArgs(params.size()))
                throw std::runtime_error(help.ToString());
        },
        std::runtime_error);

    try {
        if (fHelp || !help.IsValidNumArgs(params.size()))
            throw std::runtime_error(help.ToString());
    } catch (const std::runtime_error& e) {
        const std::string what{e.what()};
        BOOST_CHECK(what.find("trivialcmd") != std::string::npos);
        BOOST_CHECK(what.find("address") != std::string::npos);
    }
}

// Each individual help-renders test now invokes its command's RPCHelpMan
// accessor directly and verifies the rendered text. With PR M2 in place,
// the command body no longer throws on fHelp=true — the dispatcher is the
// authoritative help-rendering site — so the old throw-and-catch pattern
// is obsolete. Calling the accessor is also free of side effects and
// touches no globals, so this stays fixture-free.

BOOST_AUTO_TEST_CASE(settxfee_help_renders)
{
    const std::string what = settxfee_helpman().ToString();
    BOOST_CHECK(what.find("settxfee") != std::string::npos);
    BOOST_CHECK(what.find("amount") != std::string::npos);
    BOOST_CHECK(what.find("Examples:") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(addnode_help_renders)
{
    const std::string what = addnode_helpman().ToString();
    BOOST_CHECK(what.find("addnode") != std::string::npos);
    BOOST_CHECK(what.find("node") != std::string::npos);
    BOOST_CHECK(what.find("onetry") != std::string::npos);
    BOOST_CHECK(what.find("Examples:") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(setban_help_renders)
{
    const std::string what = setban_helpman().ToString();
    BOOST_CHECK(what.find("setban") != std::string::npos);
    BOOST_CHECK(what.find("subnet") != std::string::npos);
    BOOST_CHECK(what.find("bantime") != std::string::npos);
    BOOST_CHECK(what.find("Examples:") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(listsinceblock_help_renders)
{
    const std::string what = listsinceblock_helpman().ToString();
    BOOST_CHECK(what.find("listsinceblock") != std::string::npos);
    BOOST_CHECK(what.find("blockhash") != std::string::npos);
    BOOST_CHECK(what.find("target_confirmations") != std::string::npos);
    BOOST_CHECK(what.find("transactions") != std::string::npos);
    BOOST_CHECK(what.find("Examples:") != std::string::npos);
}

// Invalid-subcommand path for addnode: the JSONRPCError throw at net.cpp:62-63
// fires before any LOCK(cs_vAddedNodes) or vNodes/ConnectNode access, so this
// is safe to exercise without a node/net fixture.
BOOST_AUTO_TEST_CASE(addnode_invalid_subcommand_throws_structured_error)
{
    UniValue params(UniValue::VARR);
    params.push_back("x");
    params.push_back("bogus");
    try {
        addnode(params);
        BOOST_FAIL("expected UniValue JSON-RPC error");
    } catch (const UniValue& e) {
        BOOST_CHECK_EQUAL(e["code"].get_int(), RPC_INVALID_PARAMETER);
        const std::string message = e["message"].get_str();
        BOOST_CHECK(message.find("command must be one of") != std::string::npos);
        BOOST_CHECK(message.find("add") != std::string::npos);
        BOOST_CHECK(message.find("remove") != std::string::npos);
        BOOST_CHECK(message.find("onetry") != std::string::npos);
    }
}

// Invalid-subcommand path for setban: the JSONRPCError throw at net.cpp:258-259
// fires before the g_banman null check and any lock acquisition, so this is
// safe to exercise without a banman/net fixture.
BOOST_AUTO_TEST_CASE(setban_invalid_subcommand_throws_structured_error)
{
    UniValue params(UniValue::VARR);
    params.push_back("1.2.3.4");
    params.push_back("bogus");
    try {
        setban(params);
        BOOST_FAIL("expected UniValue JSON-RPC error");
    } catch (const UniValue& e) {
        BOOST_CHECK_EQUAL(e["code"].get_int(), RPC_INVALID_PARAMETER);
        const std::string message = e["message"].get_str();
        BOOST_CHECK(message.find("command must be") != std::string::npos);
        BOOST_CHECK(message.find("add") != std::string::npos);
        BOOST_CHECK(message.find("remove") != std::string::npos);
    }
}

// Shared helper for tier-level help-rendering tests. Each tier passes a list
// of (rpc_name, helpman_accessor) pairs; the helper calls each accessor's
// ToString() and verifies the rendered text contains the command name plus
// the structural "Examples:" marker. With PR M2 in place the dispatcher is
// the authoritative help-rendering site, so verifying the accessor output
// directly is both more robust (no body invocation) and a closer match to
// the actual production help path (`pcmd->helpman().ToString()`).
namespace {
using HelpmanAccessor = const RPCHelpMan& (*)();

void check_help_renders(const std::vector<std::pair<const char*, HelpmanAccessor>>& cases,
                        bool require_deprecated_banner = false)
{
    for (const auto& [rpc_name, accessor] : cases) {
        BOOST_TEST_CONTEXT(rpc_name) {
            const std::string what = accessor().ToString();
            BOOST_CHECK_MESSAGE(what.find(rpc_name) != std::string::npos,
                rpc_name << ": help text missing command name; got: " << what);
            BOOST_CHECK_MESSAGE(what.find("Examples:") != std::string::npos,
                rpc_name << ": help text missing 'Examples:' section");
            if (require_deprecated_banner) {
                BOOST_CHECK_MESSAGE(what.find("DEPRECATED") != std::string::npos,
                    rpc_name << ": help text missing DEPRECATED banner; got: " << what);
            }
        }
    }
}
} // namespace

// Tier 1a — blockchain-core (21 commands).
BOOST_AUTO_TEST_CASE(tier1a_blockchain_core_help_renders)
{
    check_help_renders({
        {"getbestblockhash",        &getbestblockhash_helpman},
        {"getblockcount",           &getblockcount_helpman},
        {"getdifficulty",           &getdifficulty_helpman},
        {"getblockhash",            &getblockhash_helpman},
        {"getblock",                &getblock_helpman},
        {"getblockbynumber",        &getblockbynumber_helpman},
        {"getblockbymintime",       &getblockbymintime_helpman},
        {"getblocksbatch",          &getblocksbatch_helpman},
        {"showblock",               &showblock_helpman},
        {"getrawmempool",           &getrawmempool_helpman},
        {"getblockchaininfo",       &getblockchaininfo_helpman},
        {"getcheckpoint",           &getcheckpoint_helpman},
        {"getburnreport",           &getburnreport_helpman},
        {"reorganize",              &rpc_reorganize_helpman}, // C++ symbol differs from RPC name
        {"currenttime",             &currenttime_helpman},
        {"networktime",             &networktime_helpman},
        {"network",                 &network_helpman},
        {"sendblock",               &sendblock_helpman},
        {"askforoutstandingblocks", &askforoutstandingblocks_helpman},
        {"debug",                   &debug_helpman},
        {"versionreport",           &versionreport_helpman},
    });
}

// Tier 1c — snapshots / registries / generic-data (14 commands).
BOOST_AUTO_TEST_CASE(tier1c_snapshots_registries_help_renders)
{
    check_help_renders({
        {"superblocks",              &superblocks_helpman},
        {"superblockage",            &superblockage_helpman},
        {"superblockaverage",        &superblockaverage_helpman},
        {"parselegacysb",            &parselegacysb_helpman},
        {"listscrapers",             &listscrapers_helpman},
        {"listprojects",             &listprojects_helpman},
        {"projects",                 &projects_helpman},
        {"getautogreylist",          &getautogreylist_helpman},
        {"listsidestakes",           &listsidestakes_helpman},
        {"listmandatorysidestakes",  &listmandatorysidestakes_helpman},
        {"listprotocolentries",      &listprotocolentries_helpman},
        {"readdata",                 &readdata_helpman},
        {"writedata",                &writedata_helpman},
        {"dumpcontracts",            &dumpcontracts_helpman},
    });
}

// Tier 1 D1 — server / misc / dataacq (8 commands).
BOOST_AUTO_TEST_CASE(tier1_d1_server_misc_dataacq_help_renders)
{
    check_help_renders({
        {"help",            &help_helpman},
        {"stop",            &stop_helpman},
        {"logging",         &logging_helpman},
        {"listsettings",    &listsettings_helpman},
        {"changesettings",  &changesettings_helpman},
        {"getblockstats",   &rpc_getblockstats_helpman},
        {"exportstats1",    &rpc_exportstats_helpman},
        {"getrecentblocks", &rpc_getrecentblocks_helpman},
    });
}

// Tier 1 D2 — net.cpp remaining (12 commands).
BOOST_AUTO_TEST_CASE(tier1_d2_net_remaining_help_renders)
{
    check_help_renders({
        {"getconnectioncount", &getconnectioncount_helpman},
        {"getnodeaddresses",   &getnodeaddresses_helpman},
        {"getaddednodeinfo",   &getaddednodeinfo_helpman},
        {"listbanned",         &listbanned_helpman},
        {"clearbanned",        &clearbanned_helpman},
        {"ping",               &ping_helpman},
        {"getpeerinfo",        &getpeerinfo_helpman},
        {"getnettotals",       &getnettotals_helpman},
        {"listalerts",         &listalerts_helpman},
        {"sendalert",          &sendalert_helpman},
        {"sendalert2",         &sendalert2_helpman},
        {"getnetworkinfo",     &getnetworkinfo_helpman},
    });
}

// Tier 1 D3 — voting.cpp (6 commands).
BOOST_AUTO_TEST_CASE(tier1_d3_voting_help_renders)
{
    check_help_renders({
        {"listpolls",            &listpolls_helpman},
        {"getpollresults",       &getpollresults_helpman},
        {"getvotingclaim",       &getvotingclaim_helpman},
        {"votebyid",             &votebyid_helpman},
        {"votedetails",          &votedetails_helpman},
        {"testpollnotification", &testpollnotification_helpman},
    });
}

// Tier 1 E1 — mining.cpp (7 commands).
BOOST_AUTO_TEST_CASE(tier1_e1_mining_help_renders)
{
    check_help_renders({
        {"getstakinginfo",          &getstakinginfo_helpman},
        {"getlaststake",            &getlaststake_helpman},
        {"auditsnapshotaccrual",    &auditsnapshotaccrual_helpman},
        {"auditsnapshotaccruals",   &auditsnapshotaccruals_helpman},
        {"listresearcheraccounts",  &listresearcheraccounts_helpman},
        {"inspectaccrualsnapshot",  &inspectaccrualsnapshot_helpman},
        {"parseaccrualsnapshotfile", &parseaccrualsnapshotfile_helpman},
    });
}

// Tier 1 E2 — scrapers (9 commands).
BOOST_AUTO_TEST_CASE(tier1_e2_scrapers_help_renders)
{
    check_help_renders({
        {"sendscraperfilemanifest", &sendscraperfilemanifest_helpman},
        {"savescraperfilemanifest", &savescraperfilemanifest_helpman},
        {"deletecscrapermanifest",  &deletecscrapermanifest_helpman},
        {"archivelog",              &archivelog_helpman},
        {"convergencereport",       &convergencereport_helpman},
        {"testnewsb",               &testnewsb_helpman},
        {"scraperreport",           &scraperreport_helpman},
        {"listmanifests",           &listmanifests_helpman},
        {"getmpart",                &getmpart_helpman},
    });
}

// Tier 1 E3 — rawtransaction + htlc (16 commands).
BOOST_AUTO_TEST_CASE(tier1_e3_rawtx_htlc_help_renders)
{
    check_help_renders({
        {"getrawtransaction",           &getrawtransaction_helpman},
        {"listunspent",                 &listunspent_helpman},
        {"consolidateunspent",          &consolidateunspent_helpman},
        {"consolidatemsunspent",        &consolidatemsunspent_helpman},
        {"scanforunspent",              &scanforunspent_helpman},
        {"createrawtransaction",        &createrawtransaction_helpman},
        {"fundrawtransaction",          &fundrawtransaction_helpman},
        {"decoderawtransaction",        &decoderawtransaction_helpman},
        {"decodescript",                &decodescript_helpman},
        {"signrawtransactionwithkey",   &signrawtransactionwithkey_helpman},
        {"signrawtransactionwithwallet", &signrawtransactionwithwallet_helpman},
        {"signrawtransaction",          &signrawtransaction_helpman},
        {"sendrawtransaction",          &sendrawtransaction_helpman},
        {"createhtlc",                  &createhtlc_helpman},
        {"claimhtlc",                   &claimhtlc_helpman},
        {"refundhtlc",                  &refundhtlc_helpman},
    });
}

// Tier 1 F1 — rpcdump (4 commands).
BOOST_AUTO_TEST_CASE(tier1_f1_rpcdump_help_renders)
{
    check_help_renders({
        {"importprivkey", &importprivkey_helpman},
        {"importwallet",  &importwallet_helpman},
        {"dumpprivkey",   &dumpprivkey_helpman},
        {"dumpwallet",    &dumpwallet_helpman},
    });
}

// Tier 1 Deprecated — wallet account-system commands + `vote` wrapper (9 commands).
// In addition to the standard name/Examples checks, each command's help text
// must contain the literal "DEPRECATED" banner per the conversion convention.
BOOST_AUTO_TEST_CASE(tier1_deprecated_batch_help_renders)
{
    check_help_renders({
        {"getaccountaddress",     &getaccountaddress_helpman},
        {"setaccount",            &setaccount_helpman},
        {"getaccount",            &getaccount_helpman},
        {"getaddressesbyaccount", &getaddressesbyaccount_helpman},
        {"getreceivedbyaccount",  &getreceivedbyaccount_helpman},
        {"listreceivedbyaccount", &listreceivedbyaccount_helpman},
        {"listaccounts",          &listaccounts_helpman},
        // Dispatched RPC name is "move", but the C++ function symbol is movecmd.
        {"move",                  &movecmd_helpman},
        {"vote",                  &vote_helpman},
    }, /*require_deprecated_banner=*/true);
}
// Tier 1 F2 — wallet keys/addresses/signing (10 commands).
BOOST_AUTO_TEST_CASE(tier1_f2_wallet_keys_help_renders)
{
    check_help_renders({
        {"getnewpubkey",       &getnewpubkey_helpman},
        {"getnewaddress",      &getnewaddress_helpman},
        {"signmessage",        &signmessage_helpman},
        {"verifymessage",      &verifymessage_helpman},
        {"addmultisigaddress", &addmultisigaddress_helpman},
        {"addredeemscript",    &addredeemscript_helpman},
        {"validateaddress",    &validateaddress_helpman},
        {"validatepubkey",     &validatepubkey_helpman},
        {"makekeypair",        &makekeypair_helpman},
        {"sethdseed",          &sethdseed_helpman},
    });
}

// Tier 1b — researcher/beacon/MRC (17 commands).
BOOST_AUTO_TEST_CASE(tier1b_researcher_help_renders)
{
    check_help_renders({
        {"advertisebeacon",        &advertisebeacon_helpman},
        {"advertisebeaconv3",      &advertisebeaconv3_helpman},
        {"beaconauth",             &beaconauth_helpman},
        {"revokebeacon",           &revokebeacon_helpman},
        {"beaconreport",           &beaconreport_helpman},
        {"beaconconvergence",      &beaconconvergence_helpman},
        {"pendingbeaconreport",    &pendingbeaconreport_helpman},
        {"beaconstatus",           &beaconstatus_helpman},
        {"beaconaudit",            &beaconaudit_helpman},
        {"getmrcinfo",             &getmrcinfo_helpman},
        {"createmrcrequest",       &createmrcrequest_helpman},
        {"magnitude",              &magnitude_helpman},
        {"explainmagnitude",       &explainmagnitude_helpman},
        {"lifetime",               &lifetime_helpman},
        {"resetcpids",             &resetcpids_helpman},
        {"rainbymagnitude",        &rainbymagnitude_helpman},
        {"currentcontractaverage", &currentcontractaverage_helpman},
    });
}

// Tier 1 F3 — wallet read-only queries (12 commands).
BOOST_AUTO_TEST_CASE(tier1_f3_wallet_queries_help_renders)
{
    check_help_renders({
        {"getinfo",                 &getinfo_helpman},
        {"getwalletinfo",           &getwalletinfo_helpman},
        {"listaddressgroupings",    &listaddressgroupings_helpman},
        {"getreceivedbyaddress",    &getreceivedbyaddress_helpman},
        {"getbalance",              &getbalance_helpman},
        {"getbalancedetail",        &getbalancedetail_helpman},
        {"getunconfirmedbalance",   &getunconfirmedbalance_helpman},
        {"listreceivedbyaddress",   &listreceivedbyaddress_helpman},
        {"listtransactions",        &listtransactions_helpman},
        {"liststakes",              &liststakes_helpman},
        {"gettransaction",          &gettransaction_helpman},
        {"getrawwallettransaction", &getrawwallettransaction_helpman},
    });
}

// Tier 1 F4 — wallet mgmt + send (13 commands).
BOOST_AUTO_TEST_CASE(tier1_f4_wallet_mgmt_send_help_renders)
{
    check_help_renders({
        {"sendtoaddress",   &sendtoaddress_helpman},
        {"sendfrom",        &sendfrom_helpman},
        {"sendmany",        &sendmany_helpman},
        {"backupwallet",    &backupwallet_helpman},
        {"keypoolrefill",   &keypoolrefill_helpman},
        {"walletdiagnose",  &walletdiagnose_helpman},
        {"encryptwallet",   &encryptwallet_helpman},
        {"reservebalance",  &reservebalance_helpman},
        {"checkwallet",     &checkwallet_helpman},
        {"repairwallet",    &repairwallet_helpman},
        {"resendtx",        &resendtx_helpman},
        {"burn",            &burn_helpman},
        {"upgradewallet",   &upgradewallet_helpman},
    });
}

// Wallet transaction-state debug commands (introduced in #2839 issue-1157-2,
// lifted to helpman accessors via the straggler integration in PR M).
BOOST_AUTO_TEST_CASE(wallet_tx_state_debug_help_renders)
{
    check_help_renders({
        {"abandontransaction", &abandontransaction_helpman},
        {"inspectwalletstate", &inspectwalletstate_helpman},
    });
}

// PSGT commands (Partially Signed Gridcoin Transaction, lifted to helpman
// accessors via the straggler integration in PR M).
BOOST_AUTO_TEST_CASE(psgt_commands_help_renders)
{
    check_help_renders({
        {"createpsgt",             &createpsgt_helpman},
        {"decodepsgt",             &decodepsgt_helpman},
        {"combinepsgt",            &combinepsgt_helpman},
        {"finalizepsgt",           &finalizepsgt_helpman},
        {"walletprocesspsgt",      &walletprocesspsgt_helpman},
        {"utxoupdatepsgt",         &utxoupdatepsgt_helpman},
        {"converttopsgt",          &converttopsgt_helpman},
        {"walletcreatefundedpsgt", &walletcreatefundedpsgt_helpman},
    });
}

BOOST_AUTO_TEST_SUITE_END()
