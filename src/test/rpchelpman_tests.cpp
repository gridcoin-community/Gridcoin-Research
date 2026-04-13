// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/util.h>

#include <tinyformat.h>
#include <univalue.h>

#include <boost/test/unit_test.hpp>

#include <stdexcept>
#include <string>

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

BOOST_AUTO_TEST_SUITE_END()
