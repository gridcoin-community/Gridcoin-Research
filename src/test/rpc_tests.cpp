#include <boost/test/unit_test.hpp>

#include <amount.h>
#include <interfaces/file_transfer.h>
#include <key_io.h>
#include <util.h>

#include <rpc/client.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <net.h>
#include <policy/fees.h>
#include <test/data/entry_point_classes.json.h>
#include <wallet/capability.h>
#include <wallet/wallet.h>

#include "encrypted_wallet.h"
#include "lone_best_block.h"
#include "staking_test_peers.h"

#include <univalue.h>


#include "chain.h"
#include "test/chain_setup.h"
#include <optional>
#include <regex>
#include <set>
#include <tuple>
#include <stdexcept>

using namespace std;

BOOST_AUTO_TEST_SUITE(rpc_tests)

static UniValue
createArgs(int nRequired, const char* address1 = nullptr, const char* address2 = nullptr)
{
    UniValue result(UniValue::VARR);
    result.push_back(nRequired);
    UniValue addresses(UniValue::VARR);
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    result.push_back(addresses);
    return result;
}

BOOST_AUTO_TEST_CASE(rpc_addmultisig)
{
    rpcfn_type addmultisig = tableRPC["addmultisigaddress"]->actor;

    // old, 65-byte-long:
    const char address1Hex[] = "0434e3e09f49ea168c5bbf53f877ff4206923858aab7c7e1df25bc263978107c95e35065a27ef6f1b27222db0ec97e0e895eaca603d3ee0d4c060ce3d8a00286c8";
    // new, compressed:
    const char address2Hex[] = "0388c2037017c62240b6b72ac1a2a5f94da790596ebd06177c8572752922165cb4";

    UniValue v;
    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex)));
    CTxDestination address = DecodeDestination(v.get_str());
    BOOST_CHECK(std::holds_alternative<CScriptID>(address));

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex, address2Hex)));
    address = DecodeDestination(v.get_str());
    BOOST_CHECK(std::holds_alternative<CScriptID>(address));

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(2, address1Hex, address2Hex)));
    address = DecodeDestination(v.get_str());
    BOOST_CHECK(std::holds_alternative<CScriptID>(address));

    BOOST_CHECK_THROW(addmultisig(createArgs(0)), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(1)), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(2, address1Hex)), runtime_error);

    BOOST_CHECK_THROW(addmultisig(createArgs(1, "")), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(1, "NotAValidPubkey")), runtime_error);

    string short1(address1Hex, address1Hex+sizeof(address1Hex)-2); // last byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short1.c_str())), runtime_error);

    string short2(address1Hex+1, address1Hex+sizeof(address1Hex)); // first byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short2.c_str())), runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values)
{
    BOOST_CHECK(ValueFromAmount(0LL).write() == "0.00000000");
    BOOST_CHECK(ValueFromAmount(1LL).write() == "0.00000001");
    BOOST_CHECK(ValueFromAmount(17622195LL).write() == "0.17622195");
    BOOST_CHECK(ValueFromAmount(50000000LL).write() == "0.50000000");
    BOOST_CHECK(ValueFromAmount(89898989LL).write() == "0.89898989");
    BOOST_CHECK(ValueFromAmount(100000000LL).write() == "1.00000000");
    BOOST_CHECK(ValueFromAmount(2099999999999990LL).write() == "20999999.99999990");
    BOOST_CHECK(ValueFromAmount(2099999999999999LL).write() == "20999999.99999999");

    BOOST_CHECK_EQUAL(ValueFromAmount(0).write(), "0.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount((COIN/10000)*123456789).write(), "12345.67890000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-COIN).write(), "-1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-COIN/10).write(), "-0.10000000");

    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*100000000).write(), "100000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*10000000).write(), "10000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*1000000).write(), "1000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*100000).write(), "100000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*10000).write(), "10000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*1000).write(), "1000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*100).write(), "100.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*10).write(), "10.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN).write(), "1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/10).write(), "0.10000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/100).write(), "0.01000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/1000).write(), "0.00100000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/10000).write(), "0.00010000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/100000).write(), "0.00001000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/1000000).write(), "0.00000100");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/10000000).write(), "0.00000010");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/100000000).write(), "0.00000001");
}

static UniValue ValueFromString(const std::string &str)
{
    UniValue value;
    BOOST_CHECK(value.setNumStr(str));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values)
{
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000001")), 1LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.17622195")), 17622195LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.5")), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.50000000")), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.89898989")), 89898989LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("1.00000000")), 100000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.9999999")), 2099999999999990LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.99999999")), 2099999999999999LL);

    // A JSON string amount parses identically to the numeric form. Before the
    // ParseFixedPoint conversion, AmountFromValue called value.get_real() and
    // threw on any string-typed amount; now it accepts either representation.
    BOOST_CHECK_EQUAL(AmountFromValue(UniValue(UniValue::VSTR, "0.5")), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(UniValue(UniValue::VSTR, "1.00000000")), 100000000LL);

    // Exact parsing at the top of the money range (a double would lose the low
    // digits here): MAX_MONEY = 2000000000 * COIN.
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("2000000000.00000000")), 200000000000000000LL);

    // Rejections. JSONRPCError throws a UniValue.
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("0")), UniValue);            // zero (Gridcoin rejects <= 0)
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("0.00000000")), UniValue);   // zero, written out
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("-1")), UniValue);           // negative
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("-0.00000001")), UniValue);  // negative sub-satoshi
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("0.000000001")), UniValue);  // more than 8 decimals (was silently rounded)
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("2000000000.00000001")), UniValue); // just past MAX_MONEY
    BOOST_CHECK_THROW(AmountFromValue(UniValue(UniValue::VSTR, "")), UniValue);    // empty string
    BOOST_CHECK_THROW(AmountFromValue(UniValue(UniValue::VSTR, "1.2.3")), UniValue);   // not a fixed-point number
    BOOST_CHECK_THROW(AmountFromValue(UniValue(UniValue::VSTR, "1a")), UniValue);      // trailing junk
    BOOST_CHECK_THROW(AmountFromValue(UniValue(true)), UniValue);                      // neither number nor string
}

// Regression guard: getaddednodeinfo dns=true must return an ARRAY of per-node
// objects. It previously built a UniValue object and populated it with
// push_back (a no-op on objects), so it always returned {}. g_connman is
// provided by the global TestingSetup fixture.
BOOST_AUTO_TEST_CASE(getaddednodeinfo_dns_true_returns_array)
{
    BOOST_REQUIRE(g_connman);
    const std::string node = "192.0.2.10:32749"; // TEST-NET-1, never a live peer
    BOOST_REQUIRE(g_connman->AddNode(node));

    UniValue params(UniValue::VARR);
    params.push_back(true); // dns=true
    UniValue r = tableRPC["getaddednodeinfo"]->actor(params);

    BOOST_CHECK(r.isArray());
    BOOST_CHECK_EQUAL(r.size(), 1u);
    BOOST_CHECK_EQUAL(r[0]["addednode"].get_str(), node);
    BOOST_CHECK_EQUAL(r[0]["connected"].get_bool(), false);

    g_connman->RemoveAddedNode(node); // vAddedNodes is process-global; clean up
}

//! The HTTP header parse is reachable BEFORE any authentication check, so its
//! bounds are what an unauthenticated client is limited to. Each returns a
//! negative length, which ReadHTTPMessage rejects -- not a status code, which it
//! would read as a body size.
BOOST_AUTO_TEST_CASE(http_headers_bounded)
{
    std::map<std::string, std::string> headers;

    // A normal request parses, and yields its content length.
    {
        std::istringstream in("content-length: 42\r\nconnection: close\r\n\r\n");
        BOOST_CHECK_EQUAL(ReadHTTPHeaders(in, headers), 42);
    }

    // Too many headers.
    {
        std::string raw;
        for (int i = 0; i < 101; ++i) raw += "x-pad-" + ToString(i) + ": v\r\n";
        raw += "\r\n";
        std::istringstream in(raw);
        BOOST_CHECK(ReadHTTPHeaders(in, headers) < 0);
    }

    // One header longer than the per-line bound, with no newline to end it --
    // the shape std::getline would have buffered without limit.
    {
        std::istringstream in("x-pad: " + std::string(9000, 'A'));
        BOOST_CHECK(ReadHTTPHeaders(in, headers) < 0);
    }

    // Within the count and line bounds, but past the aggregate.
    {
        std::string raw;
        for (int i = 0; i < 20; ++i) raw += "x-pad-" + ToString(i) + ": " + std::string(4000, 'A') + "\r\n";
        raw += "\r\n";
        std::istringstream in(raw);
        BOOST_CHECK(ReadHTTPHeaders(in, headers) < 0);
    }

    // A malformed content-length returns rather than throwing: ServiceConnection
    // has no try/catch around this call.
    {
        std::istringstream in("content-length: notanumber\r\n\r\n");
        BOOST_CHECK_NO_THROW(BOOST_CHECK(ReadHTTPHeaders(in, headers) < 0));
    }
}

//! Header names are lower-cased and trimmed, values are trimmed of the same
//! whitespace at both ends and nowhere else, a value may be empty, a line
//! without a colon is skipped, and a repeated header keeps its last value.
BOOST_AUTO_TEST_CASE(http_headers_trimmed_and_last_repeat_kept)
{
    std::map<std::string, std::string> headers;
    std::istringstream in(" X-Mixed-Case \t:  \t inner  spaces kept \t\r\n"
                          "x-repeated: first\r\n"
                          "X-Repeated:\tsecond value\r\n"
                          "x-empty:   \r\n"
                          "x-bare:\r\n"
                          "no colon on this line\r\n"
                          "\r\n");

    BOOST_CHECK_EQUAL(ReadHTTPHeaders(in, headers), 0);

    BOOST_CHECK_EQUAL(headers.size(), 4U);
    BOOST_CHECK_EQUAL(headers["x-mixed-case"], "inner  spaces kept");
    BOOST_CHECK_EQUAL(headers["x-repeated"], "second value");
    BOOST_CHECK_EQUAL(headers["x-empty"], "");
    BOOST_CHECK_EQUAL(headers["x-bare"], "");
}

//! "GET / " splits into three words whose third is empty, and pop_back() on an
//! empty string is undefined. Reachable before authentication.
BOOST_AUTO_TEST_CASE(http_request_line_empty_proto)
{
    std::istringstream in("GET / \r\n");
    int proto = 0;
    std::string method, uri;

    BOOST_CHECK_NO_THROW((void)ReadHTTPRequestLine(in, proto, method, uri));
}

BOOST_AUTO_TEST_CASE(settxfee_refuses_a_rate_above_the_cap)
{
    // nTransactionFee is a global the case changes, so put it back afterwards.
    struct RestoreFee { int64_t saved{nTransactionFee}; ~RestoreFee() { nTransactionFee = saved; } } restore;
    const rpcfn_type settxfee_fn = tableRPC["settxfee"]->actor;

    BOOST_CHECK(settxfee_fn(RPCConvertValues("settxfee", {"0.1"})).get_bool());
    BOOST_CHECK_EQUAL(nTransactionFee, MaxTxFeeRate());

    BOOST_CHECK_EXCEPTION(settxfee_fn(RPCConvertValues("settxfee", {"0.10000001"})), UniValue,
        [](const UniValue& e) {
            return find_value(e, "code").get_int() == RPC_INVALID_PARAMETER
                && find_value(e, "message").get_str().find("above maximum") != std::string::npos;
        });
    BOOST_CHECK_EQUAL(nTransactionFee, MaxTxFeeRate());
}

static bool CodeIs(const UniValue& e, int code)
{
    return find_value(e, "code").get_int() == code;
}

static void RunAs(const char* method, const std::vector<std::string>& args)
{
    tableRPC.execute(method, RPCConvertValues(method, args));
}

//! The dispatcher's refusal of an elevate-class row without the passphrase.
static std::string GateRefusal(const std::string& method)
{
    return method + " needs the wallet passphrase: unlock the wallet fully, or send the passphrase with this request";
}

BOOST_AUTO_TEST_CASE(rpc_rows_carry_their_access_class)
{
    for (const char* name : {"settxfee", "backupwallet", "maintainbackups", "writedata"}) {
        BOOST_CHECK_MESSAGE(tableRPC[name]->access == access_elevate, name);
    }

    BOOST_CHECK(tableRPC["encryptwallet"]->access == access_elevate_unencrypted);

    for (const char* name : {"walletpassphrase", "walletpassphrasechange", "setautounlock", "clearautounlock"}) {
        BOOST_CHECK_MESSAGE(tableRPC[name]->access == access_elevate_inband, name);
    }

    BOOST_CHECK(tableRPC["changesettings"]->access == access_settings);

    for (const char* name : {"setlabel", "setaccount", "sendrawtransaction", "stop", "addnode", "dumpcontracts",
                             "parseaccrualsnapshotfile"}) {
        BOOST_CHECK_MESSAGE(tableRPC[name]->access == access_free, name);
    }

    for (const char* name : {"getbalance", "listsettings", "help"}) {
        BOOST_CHECK_MESSAGE(tableRPC[name]->access == access_read_only, name);
    }

    BOOST_CHECK_EQUAL(std::string(RpcAccessName(access_read_only)), "read_only");
    BOOST_CHECK_EQUAL(std::string(RpcAccessName(access_free)), "free");
    BOOST_CHECK_EQUAL(std::string(RpcAccessName(access_elevate)), "elevate");
    BOOST_CHECK_EQUAL(std::string(RpcAccessName(access_elevate_inband)), "elevate_inband");
    BOOST_CHECK_EQUAL(std::string(RpcAccessName(access_elevate_unencrypted)), "elevate_unencrypted");
    BOOST_CHECK_EQUAL(std::string(RpcAccessName(access_settings)), "settings");
}

BOOST_AUTO_TEST_CASE(every_rpc_row_matches_the_classification_file)
{
    // src/rpc/entry_point_classes.json, embedded at build time. It is one
    // object, so it is read directly rather than through read_json().
    const std::string text(json_tests::entry_point_classes,
                           json_tests::entry_point_classes + sizeof(json_tests::entry_point_classes));

    UniValue classes;
    BOOST_REQUIRE(classes.read(text));
    BOOST_REQUIRE(classes.isObject());

    const UniValue rpc = find_value(classes, "rpc");
    BOOST_REQUIRE(rpc.isObject());

    const std::vector<std::string> commands = tableRPC.listCommands(true);
    BOOST_CHECK_EQUAL(rpc.size(), commands.size());

    for (const std::string& name : commands) {
        const UniValue& entry = find_value(rpc, name);

        BOOST_CHECK_MESSAGE(entry.isObject(), name + " has no entry in the classification file");
        if (!entry.isObject()) continue;

        const UniValue cls = find_value(entry, "class");
        const UniValue authority = find_value(entry, "authority");

        BOOST_CHECK_MESSAGE(cls.isStr(), name + " has no class");
        if (!cls.isStr()) continue;

        // An elevate entry whose method verifies a passphrase argument is the
        // access_elevate_inband row, one that acts only on an unencrypted wallet
        // the access_elevate_unencrypted row; every other class names its column.
        std::string expected = cls.get_str();
        if (expected == "elevate" && authority.isStr() && authority.get_str() == "passphrase_argument") {
            expected = "elevate_inband";
        }
        if (expected == "elevate" && authority.isStr() && authority.get_str() == "unencrypted_only") {
            expected = "elevate_unencrypted";
        }

        const std::string column = RpcAccessName(tableRPC[name]->access);
        BOOST_CHECK_MESSAGE(expected == column,
                            name + ": the classification file gives " + expected + ", the row access_" + column);
    }
}

// The RPCs that take a file path from the caller (the classification file's
// "path": true rows) are exactly the ones the IPC console refuses to dispatch
// and the GUI console runs through the file transfer (ConsoleFileCommands()).
BOOST_AUTO_TEST_CASE(the_path_taking_rows_are_the_console_file_commands)
{
    const std::string text(json_tests::entry_point_classes,
                           json_tests::entry_point_classes + sizeof(json_tests::entry_point_classes));

    UniValue classes;
    BOOST_REQUIRE(classes.read(text));
    const UniValue rpc = find_value(classes, "rpc");
    BOOST_REQUIRE(rpc.isObject());

    std::set<std::string> path_rows;
    for (const std::string& name : rpc.getKeys()) {
        if (find_value(rpc[name], "path").isTrue()) path_rows.insert(name);
    }

    std::set<std::string> file_commands;
    for (const interfaces::ConsoleFileCommand& command : interfaces::ConsoleFileCommands()) {
        file_commands.insert(command.method);
    }

    BOOST_CHECK(!path_rows.empty());
    BOOST_CHECK_EQUAL_COLLECTIONS(path_rows.begin(), path_rows.end(), file_commands.begin(), file_commands.end());
}

BOOST_AUTO_TEST_CASE(every_gated_row_is_refused_without_elevation)
{
    struct RestoreFee { int64_t saved{nTransactionFee}; ~RestoreFee() { nTransactionFee = saved; } } restore;
    grc_test::EncryptedWalletSwap fx;

    size_t gated = 0;

    // Locked first, then unlocked for staking only.
    for (int state = 0; state < 2; ++state) {
        fx.wallet().Lock();

        if (state == 1) {
            BOOST_REQUIRE(fx.wallet().Unlock(grc_test::EncryptedWalletSwap::Passphrase(), UnlockScope::StakingOnly));
        }

        gated = 0;

        for (const std::string& name : tableRPC.listCommands(true)) {
            const CRPCCommand* cmd = tableRPC[name];

            if (cmd->access != access_elevate) continue;

            ++gated;

            // The smallest argument count the row's help declaration accepts; a
            // variadic row opts out of the check and gets none.
            std::vector<std::string> args;

            if (cmd->helpman != nullptr && !cmd->helpman().IsVariadic()) {
                size_t n = 0;

                while (n < 32 && !cmd->helpman().IsValidNumArgs(n)) ++n;

                args.assign(n, "x");
            }

            UniValue params(UniValue::VARR);
            for (const std::string& a : args) params.push_back(a);

            BOOST_CHECK_EXCEPTION(tableRPC.execute(name, params), UniValue,
                [&name](const UniValue& e) {
                    return CodeIs(e, RPC_WALLET_UNLOCK_NEEDED)
                        && find_value(e, "message").get_str() == GateRefusal(name);
                });
        }
    }

    BOOST_CHECK_EQUAL(gated, 47U);
}

BOOST_AUTO_TEST_CASE(elevate_rows_run_with_a_full_unlock)
{
    struct RestoreFee { int64_t saved{nTransactionFee}; ~RestoreFee() { nTransactionFee = saved; } } restore;
    grc_test::EncryptedWalletSwap fx;

    BOOST_REQUIRE(fx.wallet().Unlock(grc_test::EncryptedWalletSwap::Passphrase(), UnlockScope::Full));

    UniValue result;
    BOOST_CHECK_NO_THROW(result = tableRPC.execute("settxfee", RPCConvertValues("settxfee", {"0.002"})));
    BOOST_CHECK(result.get_bool());
    BOOST_CHECK_EQUAL(nTransactionFee, 200000);

    fx.wallet().Lock();
}

BOOST_AUTO_TEST_CASE(rpc_request_capability_admits_one_request)
{
    struct RestoreFee { int64_t saved{nTransactionFee}; ~RestoreFee() { nTransactionFee = saved; } } restore;
    grc_test::EncryptedWalletSwap fx;

    BOOST_REQUIRE(fx.wallet().Unlock(grc_test::EncryptedWalletSwap::Passphrase(), UnlockScope::StakingOnly));

    const std::optional<CapabilityId> id =
        fx.wallet().BeginElevation(grc_test::EncryptedWalletSwap::Passphrase(), CapabilityKind::RpcRequest);
    BOOST_REQUIRE(id.has_value());

    {
        const CapabilityScope scope(fx.wallet(), id, CapabilityKind::RpcRequest);
        BOOST_CHECK_NO_THROW(RunAs("settxfee", {"0.002"}));
    }

    BOOST_CHECK_EXCEPTION(RunAs("settxfee", {"0.003"}), UniValue,
        [](const UniValue& e) { return CodeIs(e, RPC_WALLET_UNLOCK_NEEDED); });
    BOOST_CHECK_EQUAL(nTransactionFee, 200000);

    fx.wallet().EndElevation(*id);
    fx.wallet().Lock();
}

BOOST_AUTO_TEST_CASE(inband_rows_are_not_gated)
{
    grc_test::EncryptedWalletSwap fx;

    // walletpassphrase schedules the relock, so it needs a scheduler.
    grc_test::SchedulerForThisCase scheduler;

    BOOST_REQUIRE(fx.wallet().IsLocked());

    const SecureString secure_passphrase = grc_test::EncryptedWalletSwap::Passphrase();
    const std::string passphrase(secure_passphrase.begin(), secure_passphrase.end());

    BOOST_CHECK_NO_THROW(RunAs("walletpassphrase", {passphrase, "60", "true"}));
    BOOST_CHECK(fx.wallet().IsUnlockedForStakingOnly());

    fx.wallet().Lock();
}

BOOST_AUTO_TEST_CASE(free_rows_run_without_elevation)
{
    grc_test::EncryptedWalletSwap fx;

    BOOST_REQUIRE(fx.wallet().IsLocked());

    BOOST_CHECK_NO_THROW(RunAs("reservebalance", {}));
    BOOST_CHECK_NO_THROW(RunAs("getbalance", {}));
}

BOOST_AUTO_TEST_CASE(elevate_rows_run_with_an_unencrypted_wallet)
{
    struct RestoreFee { int64_t saved{nTransactionFee}; ~RestoreFee() { nTransactionFee = saved; } } restore;

    // The fixture's wallet is not encrypted, so it has no keys to protect.
    BOOST_REQUIRE(pwalletMain != nullptr);
    BOOST_REQUIRE(!pwalletMain->IsCrypted());

    BOOST_CHECK_NO_THROW(RunAs("settxfee", {"0.002"}));
    BOOST_CHECK_EQUAL(nTransactionFee, 200000);
}

BOOST_AUTO_TEST_CASE(elevate_rows_are_refused_with_no_wallet_loaded)
{
    struct RestoreFee { int64_t saved{nTransactionFee}; ~RestoreFee() { nTransactionFee = saved; } } restore;
    struct NoWallet {
        CWallet* saved{pwalletMain};
        NoWallet() { pwalletMain = nullptr; }
        ~NoWallet() { pwalletMain = saved; }
    } no_wallet;

    const int64_t before = nTransactionFee;

    BOOST_CHECK_EXCEPTION(RunAs("settxfee", {"0.003"}), UniValue,
        [](const UniValue& e) {
            return CodeIs(e, RPC_WALLET_UNLOCK_NEEDED)
                && find_value(e, "message").get_str() == "Error: no wallet is loaded.";
        });
    BOOST_CHECK_EQUAL(nTransactionFee, before);

    // A row of another class runs as before.
    BOOST_CHECK_NO_THROW(RunAs("getblockcount", {}));
}

BOOST_AUTO_TEST_CASE(the_access_gate_follows_the_arity_check)
{
    grc_test::EncryptedWalletSwap fx;

    BOOST_REQUIRE(fx.wallet().IsLocked());

    BOOST_CHECK_THROW(tableRPC.execute("settxfee", UniValue(UniValue::VARR)), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(the_access_gate_names_the_method_and_both_ways_to_proceed)
{
    struct RestoreFee { int64_t saved{nTransactionFee}; ~RestoreFee() { nTransactionFee = saved; } } restore;
    grc_test::EncryptedWalletSwap fx;

    BOOST_REQUIRE(fx.wallet().Unlock(grc_test::EncryptedWalletSwap::Passphrase(), UnlockScope::StakingOnly));

    const int64_t before = nTransactionFee;

    BOOST_CHECK_EXCEPTION(RunAs("settxfee", {"0.003"}), UniValue,
        [](const UniValue& e) {
            return CodeIs(e, RPC_WALLET_UNLOCK_NEEDED)
                && find_value(e, "message").get_str()
                       == "settxfee needs the wallet passphrase: unlock the wallet fully, "
                          "or send the passphrase with this request";
        });
    BOOST_CHECK_EQUAL(nTransactionFee, before);

    fx.wallet().Lock();
}

BOOST_AUTO_TEST_CASE(the_help_of_an_elevate_row_says_it_needs_the_passphrase)
{
    const std::string line = "\nNeeds the wallet passphrase.\n";

    const auto count = [&line](const std::string& text) {
        size_t n = 0;
        for (size_t pos = text.find(line); pos != std::string::npos; pos = text.find(line, pos + 1)) ++n;
        return n;
    };

    BOOST_CHECK_EQUAL(count(tableRPC.help("settxfee", cat_null)), 1U);
    BOOST_CHECK_EQUAL(count(tableRPC.help("getblockcount", cat_null)), 0U);

    // Every row with help says it exactly when it is an elevate-class row.
    for (const std::string& name : tableRPC.listCommands(true)) {
        const CRPCCommand* cmd = tableRPC[name];

        if (cmd->category == cat_null) continue;

        const size_t expected = cmd->access == access_elevate ? 1 : 0;
        BOOST_CHECK_MESSAGE(count(tableRPC.help(name, cat_null)) == expected, name);
    }

    // So does the help the dispatcher returns for a call with the wrong number
    // of arguments, which it checks before the access class.
    try {
        tableRPC.execute("settxfee", UniValue(UniValue::VARR));
        BOOST_FAIL("settxfee with no arguments did not throw");
    } catch (const std::runtime_error& e) {
        BOOST_CHECK_EQUAL(count(e.what()), 1U);
    }
}

// dumpcontracts refuses a low_height or high_height that is not on the chain with
// RPC_INVALID_PARAMETER, naming the height and the range, and writes no file.
BOOST_AUTO_TEST_CASE(dumpcontracts_refuses_a_height_off_the_chain)
{
    const grc_test::LoneBestBlock tip; // one block, at height 0

    const fs::path path = GetDataDir() / "rpc_tests_dumpcontracts.txt";
    boost::system::error_code ec;
    fs::remove(path, ec);

    // (low_height, high_height or none, the height named). A high_height above the tip
    // comes last: before the fix it crashed the process.
    const std::vector<std::tuple<int, std::optional<int>, std::string>> cases{
        {1, std::nullopt, "low_height 1"},
        {-1, std::nullopt, "low_height -1"},
        {-4, -2, "low_height -4"},
        {0, 100, "high_height 100"},
    };
    for (const auto& [low, high, named] : cases) {
        UniValue params(UniValue::VARR);
        params.push_back("*");
        params.push_back(path.string());
        params.push_back(true);
        params.push_back(low);
        if (high) params.push_back(*high);

        try {
            dumpcontracts(params);
            BOOST_ERROR("dumpcontracts accepted " << named);
        } catch (const UniValue& error) {
            BOOST_CHECK_EQUAL(find_value(error, "code").get_int(), RPC_INVALID_PARAMETER);
            const std::string message = find_value(error, "message").get_str();
            BOOST_CHECK_MESSAGE(message.find(named) != std::string::npos, message);
            BOOST_CHECK_MESSAGE(message.find("0 to 0") != std::string::npos, message);
        }
        BOOST_CHECK_MESSAGE(!fs::exists(path), named);
        fs::remove(path, ec);
    }
}

// The message for a missing or unusable rpcpassword names the configuration file and says
// which settings to add. It suggests no password: the message goes to the log, and a
// password suggested there is one anybody who reads the log knows.
BOOST_AUTO_TEST_CASE(missing_password_message_has_no_password)
{
    const fs::path conf = fs::path("etc") / "gridcoin" / "gridcoinresearch.conf";

    const std::string message = MissingRpcPasswordMessage(conf);
    BOOST_TEST_MESSAGE(message);

    BOOST_CHECK(message.find(conf.string()) != std::string::npos);
    BOOST_CHECK(message.find("rpcuser") != std::string::npos);
    BOOST_CHECK(message.find("rpcpassword") != std::string::npos);

    // A base58 password as GenerateRpcPassword makes one is 43 or 44 characters.
    static const std::regex generated{"rpcpassword=[1-9A-HJ-NP-Za-km-z]{40,}"};
    BOOST_CHECK(!std::regex_search(message, generated));
}

BOOST_AUTO_TEST_SUITE_END()

// dumpcontracts walks pprev from the chain tip to the requested heights. A height that is not on the chain used
// to leave that pointer null, and the dereference that followed crashed the node. These cases run against the
// regtest chain, which has only the genesis block, so the valid heights are exactly 0 to 0.
BOOST_AUTO_TEST_SUITE(rpc_dumpcontracts_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

static UniValue DumpContractsArgs(const fs::path& file, int low_height, int high_height)
{
    UniValue params(UniValue::VARR);
    params.push_back("*");
    params.push_back(file.string());
    params.push_back(true);
    params.push_back(low_height);
    params.push_back(high_height);
    return params;
}

static void CheckDumpContractsRefused(const fs::path& file, int low_height, int high_height, const std::string& expected)
{
    fs::remove(file);

    try {
        tableRPC["dumpcontracts"]->actor(DumpContractsArgs(file, low_height, high_height));
        BOOST_ERROR("dumpcontracts did not refuse low_height " << low_height << ", high_height " << high_height);
    } catch (const UniValue& objError) {
        BOOST_CHECK_EQUAL(find_value(objError, "code").get_int(), RPC_INVALID_PARAMETER);
        BOOST_CHECK_EQUAL(find_value(objError, "message").get_str(), expected);
    }

    BOOST_CHECK(!fs::exists(file));
}

BOOST_AUTO_TEST_CASE(dumpcontracts_refuses_heights_off_the_chain)
{
    const fs::path file = GetDataDir() / "dumpcontracts_height_test.txt";

    // high_height above the tip.
    CheckDumpContractsRefused(file, 0, 103, "high_height 103 is not on the chain: heights run from 0 to 0");

    // low_height above the tip, with high_height left at its default.
    CheckDumpContractsRefused(file, 5, 0, "low_height 5 is not on the chain: heights run from 0 to 0");

    // Negative low_height.
    CheckDumpContractsRefused(file, -1, 0, "low_height -1 is not on the chain: heights run from 0 to 0");

    // Negative high_height. 0 means the tip, but a negative value is refused rather than taken as 0.
    CheckDumpContractsRefused(file, 0, -1, "high_height -1 is not on the chain: heights run from 0 to 0");
}

BOOST_AUTO_TEST_CASE(dumpcontracts_accepts_heights_on_the_chain)
{
    const fs::path file = GetDataDir() / "dumpcontracts_height_test.txt";
    fs::remove(file);

    UniValue result;
    BOOST_CHECK_NO_THROW(result = tableRPC["dumpcontracts"]->actor(DumpContractsArgs(file, 0, 0)));
    BOOST_CHECK(fs::exists(file));
    BOOST_CHECK_EQUAL(find_value(result, "high_height").get_int(), 0);

    fs::remove(file);
}

BOOST_AUTO_TEST_SUITE_END()
