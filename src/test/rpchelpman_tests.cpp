// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/util.h>

#include <rpc/protocol.h>
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
// captures them into the suite's namespace and breaks linkage. After conversion
// each function throws std::runtime_error for fHelp=true before touching any
// node globals or acquiring locks, so calling them with empty params is safe.
UniValue getbestblockhash(const UniValue& params, bool fHelp);
UniValue getblockcount(const UniValue& params, bool fHelp);
UniValue getdifficulty(const UniValue& params, bool fHelp);
UniValue getblockhash(const UniValue& params, bool fHelp);
UniValue getblock(const UniValue& params, bool fHelp);
UniValue getblockbynumber(const UniValue& params, bool fHelp);
UniValue getblockbymintime(const UniValue& params, bool fHelp);
UniValue getblocksbatch(const UniValue& params, bool fHelp);
UniValue showblock(const UniValue& params, bool fHelp);
UniValue getrawmempool(const UniValue& params, bool fHelp);
UniValue getblockchaininfo(const UniValue& params, bool fHelp);
UniValue getcheckpoint(const UniValue& params, bool fHelp);
UniValue getburnreport(const UniValue& params, bool fHelp);
UniValue rpc_reorganize(const UniValue& params, bool fHelp);
UniValue currenttime(const UniValue& params, bool fHelp);
UniValue networktime(const UniValue& params, bool fHelp);
UniValue network(const UniValue& params, bool fHelp);
UniValue sendblock(const UniValue& params, bool fHelp);
UniValue askforoutstandingblocks(const UniValue& params, bool fHelp);
UniValue debug(const UniValue& params, bool fHelp);
UniValue versionreport(const UniValue& params, bool fHelp);
UniValue advertisebeacon(const UniValue& params, bool fHelp);
UniValue advertisebeaconv3(const UniValue& params, bool fHelp);
UniValue beaconauth(const UniValue& params, bool fHelp);
UniValue revokebeacon(const UniValue& params, bool fHelp);
UniValue beaconreport(const UniValue& params, bool fHelp);
UniValue beaconconvergence(const UniValue& params, bool fHelp);
UniValue pendingbeaconreport(const UniValue& params, bool fHelp);
UniValue beaconstatus(const UniValue& params, bool fHelp);
UniValue beaconaudit(const UniValue& params, bool fHelp);
UniValue getmrcinfo(const UniValue& params, bool fHelp);
UniValue createmrcrequest(const UniValue& params, const bool fHelp);
UniValue magnitude(const UniValue& params, bool fHelp);
UniValue explainmagnitude(const UniValue& params, bool fHelp);
UniValue lifetime(const UniValue& params, bool fHelp);
UniValue resetcpids(const UniValue& params, bool fHelp);
UniValue rainbymagnitude(const UniValue& params, bool fHelp);
UniValue currentcontractaverage(const UniValue& params, bool fHelp);
#include <utility>
#include <vector>

// Forward declarations of the Tier 2 commands under test. These must live in
// the global namespace; if placed inside BOOST_AUTO_TEST_SUITE(...) they get
// captured into the suite's namespace and fail to link against the
// definitions in src/rpc/blockchain.cpp, src/rpc/net.cpp, src/wallet/rpcwallet.cpp.
// Each command's converted body throws for fHelp=true before touching any
// globals (vNodes, g_banman, pwalletMain, mapBlockIndex, locks), so calling
// these with fHelp=true and an empty params array is safe in unit tests.
UniValue settxfee(const UniValue& params, bool fHelp);
UniValue addnode(const UniValue& params, bool fHelp);
UniValue setban(const UniValue& params, bool fHelp);
UniValue listsinceblock(const UniValue& params, bool fHelp);

// Tier 1c forward declarations (snapshots, registries, generic-data, dumpcontracts).
// Same global-namespace placement requirement as the Tier 2 block above.
UniValue superblocks(const UniValue& params, bool fHelp);
UniValue superblockage(const UniValue& params, bool fHelp);
UniValue superblockaverage(const UniValue& params, bool fHelp);
UniValue parselegacysb(const UniValue& params, bool fHelp);
UniValue listscrapers(const UniValue& params, bool fHelp);
UniValue listprojects(const UniValue& params, bool fHelp);
UniValue projects(const UniValue& params, bool fHelp);
UniValue getautogreylist(const UniValue& params, bool fHelp);
UniValue listsidestakes(const UniValue& params, bool fHelp);
UniValue listmandatorysidestakes(const UniValue& params, bool fHelp);
UniValue listprotocolentries(const UniValue& params, bool fHelp);
UniValue readdata(const UniValue& params, bool fHelp);
UniValue writedata(const UniValue& params, bool fHelp);
UniValue dumpcontracts(const UniValue& params, bool fHelp);

// Tier 1 PR D1: src/rpc/server.cpp + src/rpc/misc.cpp + src/rpc/dataacq.cpp.
// Each function's converted body throws via help.ToString() before touching
// globals (cs_main, gArgs, log instance, file IO), so calling these with
// fHelp=true and an empty params array is safe in unit tests.
UniValue help(const UniValue& params, bool fHelp);
UniValue stop(const UniValue& params, bool fHelp);
UniValue logging(const UniValue& params, bool fHelp);
UniValue listsettings(const UniValue& params, bool fHelp);
UniValue changesettings(const UniValue& params, bool fHelp);
UniValue rpc_getblockstats(const UniValue& params, bool fHelp);
UniValue rpc_exportstats(const UniValue& params, bool fHelp);
UniValue rpc_getrecentblocks(const UniValue& params, bool fHelp);

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

BOOST_AUTO_TEST_CASE(settxfee_help_renders)
{
    const UniValue params(UniValue::VARR);
    try {
        settxfee(params, /*fHelp=*/true);
        BOOST_FAIL("expected runtime_error");
    } catch (const std::runtime_error& e) {
        const std::string what{e.what()};
        BOOST_CHECK(what.find("settxfee") != std::string::npos);
        BOOST_CHECK(what.find("amount") != std::string::npos);
        BOOST_CHECK(what.find("Examples:") != std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(addnode_help_renders)
{
    const UniValue params(UniValue::VARR);
    try {
        addnode(params, /*fHelp=*/true);
        BOOST_FAIL("expected runtime_error");
    } catch (const std::runtime_error& e) {
        const std::string what{e.what()};
        BOOST_CHECK(what.find("addnode") != std::string::npos);
        BOOST_CHECK(what.find("node") != std::string::npos);
        BOOST_CHECK(what.find("onetry") != std::string::npos);
        BOOST_CHECK(what.find("Examples:") != std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(setban_help_renders)
{
    const UniValue params(UniValue::VARR);
    try {
        setban(params, /*fHelp=*/true);
        BOOST_FAIL("expected runtime_error");
    } catch (const std::runtime_error& e) {
        const std::string what{e.what()};
        BOOST_CHECK(what.find("setban") != std::string::npos);
        BOOST_CHECK(what.find("subnet") != std::string::npos);
        BOOST_CHECK(what.find("bantime") != std::string::npos);
        BOOST_CHECK(what.find("Examples:") != std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(listsinceblock_help_renders)
{
    const UniValue params(UniValue::VARR);
    try {
        listsinceblock(params, /*fHelp=*/true);
        BOOST_FAIL("expected runtime_error");
    } catch (const std::runtime_error& e) {
        const std::string what{e.what()};
        BOOST_CHECK(what.find("listsinceblock") != std::string::npos);
        BOOST_CHECK(what.find("blockhash") != std::string::npos);
        BOOST_CHECK(what.find("target_confirmations") != std::string::npos);
        BOOST_CHECK(what.find("transactions") != std::string::npos);
        BOOST_CHECK(what.find("Examples:") != std::string::npos);
    }
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
        addnode(params, /*fHelp=*/false);
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
        setban(params, /*fHelp=*/false);
        BOOST_FAIL("expected UniValue JSON-RPC error");
    } catch (const UniValue& e) {
        BOOST_CHECK_EQUAL(e["code"].get_int(), RPC_INVALID_PARAMETER);
        const std::string message = e["message"].get_str();
        BOOST_CHECK(message.find("command must be") != std::string::npos);
        BOOST_CHECK(message.find("add") != std::string::npos);
        BOOST_CHECK(message.find("remove") != std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(tier1_d1_server_misc_dataacq_help_renders)
{
    const UniValue empty(UniValue::VARR);
    using HelpFn = UniValue (*)(const UniValue&, bool);
    const std::vector<std::pair<const char*, HelpFn>> cases{
        {"help", &help},
        {"stop", &stop},
        {"logging", &logging},
        {"listsettings", &listsettings},
        {"changesettings", &changesettings},
        {"getblockstats", &rpc_getblockstats},
        {"exportstats1", &rpc_exportstats},
        {"getrecentblocks", &rpc_getrecentblocks},
    };

    for (const auto& [rpc_name, fn] : cases) {
        BOOST_TEST_CONTEXT(rpc_name) {
            bool threw = false;
            try {
                fn(empty, /*fHelp=*/true);
            } catch (const std::runtime_error& e) {
                threw = true;
                const std::string what{e.what()};
                BOOST_CHECK_MESSAGE(what.find(rpc_name) != std::string::npos,
                    rpc_name << ": help text missing command name; got: " << what);
                BOOST_CHECK_MESSAGE(what.find("Examples:") != std::string::npos,
                    rpc_name << ": help text missing 'Examples:' section");
            }
            BOOST_CHECK_MESSAGE(threw, rpc_name << ": expected runtime_error for fHelp=true");
        }
    }
}

// Tier 1a coverage: each converted blockchain-core command throws a
// runtime_error containing its name and the "Examples:" marker when called
// with fHelp=true. The check confirms RPCHelpMan is wired (renders) and the
// help-gate fires correctly. The throw happens before any global state is
// touched, so the test runs fixture-free.
BOOST_AUTO_TEST_CASE(tier1a_blockchain_core_help_renders)
{
    const UniValue empty(UniValue::VARR);

    using HelpFn = UniValue (*)(const UniValue&, bool);
    const std::vector<std::pair<const char*, HelpFn>> cases{
        {"getbestblockhash",        &getbestblockhash},
        {"getblockcount",           &getblockcount},
        {"getdifficulty",           &getdifficulty},
        {"getblockhash",            &getblockhash},
        {"getblock",                &getblock},
        {"getblockbynumber",        &getblockbynumber},
        {"getblockbymintime",       &getblockbymintime},
        {"getblocksbatch",          &getblocksbatch},
        {"showblock",               &showblock},
        {"getrawmempool",           &getrawmempool},
        {"getblockchaininfo",       &getblockchaininfo},
        {"getcheckpoint",           &getcheckpoint},
        {"getburnreport",           &getburnreport},
        {"reorganize",              &rpc_reorganize}, // C++ symbol differs from RPC name
        {"currenttime",             &currenttime},
        {"networktime",             &networktime},
        {"network",                 &network},
        {"sendblock",               &sendblock},
        {"askforoutstandingblocks", &askforoutstandingblocks},
        {"debug",                   &debug},
        {"versionreport",           &versionreport},
    };

BOOST_AUTO_TEST_CASE(tier1_d1_server_misc_dataacq_help_renders)
{
    const UniValue empty(UniValue::VARR);
    using HelpFn = UniValue (*)(const UniValue&, bool);
    const std::vector<std::pair<const char*, HelpFn>> cases{
        {"help", &help},
        {"stop", &stop},
        {"logging", &logging},
        {"listsettings", &listsettings},
        {"changesettings", &changesettings},
        {"getblockstats", &rpc_getblockstats},
        {"exportstats1", &rpc_exportstats},
        {"getrecentblocks", &rpc_getrecentblocks},
    };
    for (const auto& [rpc_name, fn] : cases) {
        BOOST_TEST_CONTEXT(rpc_name) {
            bool threw = false;
            try {
                fn(empty, /*fHelp=*/true);
            } catch (const std::runtime_error& e) {
                threw = true;
                const std::string what{e.what()};
                BOOST_CHECK_MESSAGE(what.find(rpc_name) != std::string::npos,
                    rpc_name << ": help text missing command name; got: " << what);
                BOOST_CHECK_MESSAGE(what.find("Examples:") != std::string::npos,
                    rpc_name << ": help text missing 'Examples:' section");
            }
            BOOST_CHECK_MESSAGE(threw, rpc_name << ": expected runtime_error for fHelp=true");
        }
    }
}

// Tier 1b coverage: each converted researcher/beacon/MRC command throws a
// runtime_error containing its name and the "Examples:" marker when called
// with fHelp=true. Runs fixture-free — the help-gate fires before any global
// state is touched.
BOOST_AUTO_TEST_CASE(tier1b_researcher_help_renders)
{
    const UniValue empty(UniValue::VARR);

    using HelpFn = UniValue (*)(const UniValue&, bool);
    const std::vector<std::pair<const char*, HelpFn>> cases{
        {"advertisebeacon",        &advertisebeacon},
        {"advertisebeaconv3",      &advertisebeaconv3},
        {"beaconauth",             &beaconauth},
        {"revokebeacon",           &revokebeacon},
        {"beaconreport",           &beaconreport},
        {"beaconconvergence",      &beaconconvergence},
        {"pendingbeaconreport",    &pendingbeaconreport},
        {"beaconstatus",           &beaconstatus},
        {"beaconaudit",            &beaconaudit},
        {"getmrcinfo",             &getmrcinfo},
        {"createmrcrequest",       &createmrcrequest},
        {"magnitude",              &magnitude},
        {"explainmagnitude",       &explainmagnitude},
        {"lifetime",               &lifetime},
        {"resetcpids",             &resetcpids},
        {"rainbymagnitude",        &rainbymagnitude},
        {"currentcontractaverage", &currentcontractaverage},
    };

    for (const auto& [rpc_name, fn] : cases) {
        BOOST_TEST_CONTEXT(rpc_name) {
            bool threw = false;
            try {
                fn(empty, /*fHelp=*/true);
            } catch (const std::runtime_error& e) {
                threw = true;
                const std::string what{e.what()};
                BOOST_CHECK_MESSAGE(what.find(rpc_name) != std::string::npos,
                    rpc_name << ": help text missing command name; got: " << what);
                BOOST_CHECK_MESSAGE(what.find("Examples:") != std::string::npos,
                    rpc_name << ": help text missing 'Examples:' section");
            }
            BOOST_CHECK_MESSAGE(threw, rpc_name << ": expected runtime_error for fHelp=true");
                                    "help text missing command name: " << what);
                BOOST_CHECK_MESSAGE(what.find("Examples:") != std::string::npos,
                                    "help text missing Examples section: " << what);
            }
            BOOST_CHECK_MESSAGE(threw, "expected runtime_error from " << rpc_name);
        }
    }
}

// Help-rendering coverage for the 14 Tier 1c snapshots / registries / generic-data
// commands. Each fHelp=true throw happens before any globals are touched, so this
// runs fixture-free like the other Tier 1 / Tier 2 help-rendering cases.
BOOST_AUTO_TEST_CASE(tier1c_snapshots_registries_help_renders)
{
    const UniValue empty(UniValue::VARR);
    using HelpFn = UniValue (*)(const UniValue&, bool);
    const std::vector<std::pair<const char*, HelpFn>> cases{
        {"superblocks", superblocks},
        {"superblockage", superblockage},
        {"superblockaverage", superblockaverage},
        {"parselegacysb", parselegacysb},
        {"listscrapers", listscrapers},
        {"listprojects", listprojects},
        {"projects", projects},
        {"getautogreylist", getautogreylist},
        {"listsidestakes", listsidestakes},
        {"listmandatorysidestakes", listmandatorysidestakes},
        {"listprotocolentries", listprotocolentries},
        {"readdata", readdata},
        {"writedata", writedata},
        {"dumpcontracts", dumpcontracts},
    };

    for (const auto& [rpc_name, fn] : cases) {
        BOOST_TEST_CONTEXT(rpc_name) {
            bool threw = false;
            try {
                fn(empty, /*fHelp=*/true);
            } catch (const std::runtime_error& e) {
                threw = true;
                const std::string what{e.what()};
                BOOST_CHECK_MESSAGE(what.find(rpc_name) != std::string::npos,
                    "help message missing command name");
                BOOST_CHECK_MESSAGE(what.find("Examples:") != std::string::npos,
                    "help message missing Examples section");
            }
            BOOST_CHECK_MESSAGE(threw, "expected std::runtime_error to be thrown");
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
