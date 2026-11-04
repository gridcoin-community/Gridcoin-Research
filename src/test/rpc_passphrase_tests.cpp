// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/client.h>
#include <rpc/passphrase_params.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <test/staking_test_peers.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(rpc_passphrase_tests)

BOOST_AUTO_TEST_CASE(read_secure_line_lines)
{
    std::istringstream in("a\nb\n");
    SecureString line;

    BOOST_CHECK(ReadSecureLine(in, line));
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), "a");
    BOOST_CHECK(ReadSecureLine(in, line));
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), "b");
    BOOST_CHECK(!ReadSecureLine(in, line));
}

BOOST_AUTO_TEST_CASE(read_secure_line_strips_one_carriage_return_only)
{
    std::istringstream in("pw\r\n");
    SecureString line;

    BOOST_CHECK(ReadSecureLine(in, line));
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), "pw");

    std::istringstream two("pw\r\r\n");
    BOOST_CHECK(ReadSecureLine(two, line));
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), "pw\r");
}

BOOST_AUTO_TEST_CASE(read_secure_line_keeps_whitespace_and_non_ascii)
{
    std::istringstream in("  p\xc3\xa4 ss w\xc3\xb6rd  \n");
    SecureString line;

    BOOST_CHECK(ReadSecureLine(in, line));
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), "  p\xc3\xa4 ss w\xc3\xb6rd  ");
}

BOOST_AUTO_TEST_CASE(read_secure_line_empty_and_eof)
{
    SecureString line{"stale"};

    std::istringstream blank("\n");
    BOOST_CHECK(ReadSecureLine(blank, line));
    BOOST_CHECK(line.empty());

    std::istringstream none("");
    line = "stale";
    BOOST_CHECK(!ReadSecureLine(none, line));
    BOOST_CHECK(line.empty());

    std::istringstream unterminated("last");
    BOOST_CHECK(ReadSecureLine(unterminated, line));
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), "last");
    BOOST_CHECK(!ReadSecureLine(unterminated, line));
}

BOOST_AUTO_TEST_CASE(read_secure_line_keeps_nul)
{
    std::istringstream in(std::string("a\0b\n", 4));
    SecureString line;

    BOOST_CHECK(ReadSecureLine(in, line));
    BOOST_CHECK_EQUAL(line.size(), 3u);
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), std::string("a\0b", 3));
}

// A short passphrase must live in the secure allocator's memory, not inside the string object.
BOOST_AUTO_TEST_CASE(read_secure_line_reserves_secure_capacity)
{
    std::istringstream in("pw\n");
    SecureString line;

    BOOST_CHECK(ReadSecureLine(in, line));
    BOOST_CHECK_EQUAL(std::string(line.begin(), line.end()), "pw");
    BOOST_CHECK_GE(line.capacity(), 100u);

    SecureString kept;
    kept.reserve(500);
    std::istringstream again("x\n");
    BOOST_CHECK(ReadSecureLine(again, kept));
    BOOST_CHECK_GE(kept.capacity(), 500u);
}

BOOST_AUTO_TEST_CASE(passphrase_prompt_falls_back)
{
    const std::vector<std::string> one{"Old> "};

    BOOST_CHECK_EQUAL(PassphrasePromptAt(one, 0), "Old> ");
    BOOST_CHECK_EQUAL(PassphrasePromptAt(one, 1), "Wallet passphrase> ");
    BOOST_CHECK_EQUAL(PassphrasePromptAt({}, 0), "Wallet passphrase> ");
}

BOOST_AUTO_TEST_CASE(build_params_with_secrets)
{
    UniValue p = BuildParamsWithSecrets("walletpassphrase", {"60", "true"}, {SecureString{"pw"}});
    BOOST_REQUIRE_EQUAL(p.size(), 3u);
    BOOST_CHECK(p[0].isStr());
    BOOST_CHECK_EQUAL(p[0].get_str(), "pw");
    BOOST_CHECK(p[1].isNum());
    BOOST_CHECK_EQUAL(p[1].get_int(), 60);
    BOOST_CHECK(p[2].isBool());
    BOOST_CHECK(p[2].get_bool());

    p = BuildParamsWithSecrets("walletpassphrasechange", {}, {SecureString{"a"}, SecureString{"b"}});
    BOOST_REQUIRE_EQUAL(p.size(), 2u);
    BOOST_CHECK_EQUAL(p[0].get_str(), "a");
    BOOST_CHECK_EQUAL(p[1].get_str(), "b");

    p = BuildParamsWithSecrets("encryptwallet", {}, {SecureString{"x"}});
    BOOST_REQUIRE_EQUAL(p.size(), 1u);
    BOOST_CHECK_EQUAL(p[0].get_str(), "x");

    BOOST_CHECK_THROW(BuildParamsWithSecrets("walletpassphrase", {"60"}, {}), std::runtime_error);
    BOOST_CHECK_THROW(BuildParamsWithSecrets("getblockcount", {}, {SecureString{"x"}}), std::runtime_error);
}

// The header value is the passphrase in base64, standard alphabet, padded.
BOOST_AUTO_TEST_CASE(encode_elevation_header)
{
    for (const std::string pass : {"correct horse", "p\xc3\xa4ssw\xc3\xb6rd", "tab\tinside", " lead", "trail ",
                                   "cr\rin"}) {
        const std::optional<std::string> value = EncodeElevationHeaderValue(SecureString{pass.begin(), pass.end()});
        BOOST_REQUIRE(value.has_value());
        BOOST_CHECK_EQUAL(*value, EncodeBase64(pass));
    }

    // A NUL travels intact inside the encoding.
    const std::string with_nul("n\0l", 3);
    const std::optional<std::string> nul_value = EncodeElevationHeaderValue(SecureString{with_nul.begin(), with_nul.end()});
    BOOST_REQUIRE(nul_value.has_value());
    BOOST_CHECK_EQUAL(*nul_value, EncodeBase64(with_nul));

    BOOST_CHECK(!EncodeElevationHeaderValue(SecureString{}).has_value());

    // The limit is on the encoded value: 6000 bytes encode to exactly 8000.
    BOOST_CHECK_EQUAL(MAX_ELEVATION_HEADER_VALUE_BYTES, 8000u);
    BOOST_CHECK(EncodeElevationHeaderValue(SecureString(6000, 'a')).has_value());
    BOOST_CHECK(!EncodeElevationHeaderValue(SecureString(6001, 'a')).has_value());
}

BOOST_AUTO_TEST_CASE(cleanse_passphrase_params)
{
    UniValue p(UniValue::VARR);
    p.push_back(std::string("secret"));
    p.push_back(60);
    CleansePassphraseParams("walletpassphrase", p);
    BOOST_CHECK_EQUAL(p[0].get_str(), std::string(6, '\0'));
    BOOST_CHECK_EQUAL(p[1].get_int(), 60);

    UniValue other(UniValue::VARR);
    other.push_back(std::string("secret"));
    CleansePassphraseParams("getblockcount", other);
    BOOST_CHECK_EQUAL(other[0].get_str(), "secret");

    // A non-string at a passphrase index and a missing index are left alone.
    UniValue odd(UniValue::VARR);
    odd.push_back(7);
    CleansePassphraseParams("walletpassphrasechange", odd);
    BOOST_CHECK_EQUAL(odd[0].get_int(), 7);
    BOOST_CHECK_EQUAL(odd.size(), 1u);
}

BOOST_AUTO_TEST_CASE(passphrase_params_cleanser_cleanses_on_destruction)
{
    UniValue p(UniValue::VARR);
    p.push_back(std::string("old"));
    p.push_back(std::string("new"));
    {
        PassphraseParamsCleanser guard("walletpassphrasechange", p);
        BOOST_CHECK_EQUAL(p[0].get_str(), "old");
    }
    BOOST_CHECK_EQUAL(p[0].get_str(), std::string(3, '\0'));
    BOOST_CHECK_EQUAL(p[1].get_str(), std::string(3, '\0'));
}

BOOST_AUTO_TEST_CASE(passphrase_param_indexes)
{
    BOOST_REQUIRE(PassphraseParamIndexes("walletpassphrase") != nullptr);
    BOOST_CHECK((*PassphraseParamIndexes("walletpassphrase") == std::vector<int>{0}));
    BOOST_CHECK((*PassphraseParamIndexes("walletpassphrasechange") == std::vector<int>{0, 1}));
    BOOST_CHECK((*PassphraseParamIndexes("encryptwallet") == std::vector<int>{0}));
    BOOST_CHECK(PassphraseParamIndexes("getblockcount") == nullptr);
}

BOOST_AUTO_TEST_CASE(new_passphrase_indexes)
{
    BOOST_CHECK((NewPassphraseIndexes("encryptwallet") == std::vector<int>{0}));
    BOOST_CHECK((NewPassphraseIndexes("walletpassphrasechange") == std::vector<int>{1}));
    BOOST_CHECK(NewPassphraseIndexes("walletpassphrase").empty());
}

BOOST_AUTO_TEST_CASE(execute_passphrase_command_change)
{
    grc_test::EncryptedWallet fx;
    const SecureString old_pass = grc_test::EncryptedWallet::PASSPHRASE;

    // The passphrase positions of params are ignored; the SecureStrings carry them.
    UniValue params(UniValue::VARR);
    params.push_back("");
    params.push_back("");

    const std::vector<SecureString> passphrases{old_pass, SecureString("new")};
    BOOST_CHECK_NO_THROW(ExecutePassphraseCommand("walletpassphrasechange", passphrases, params));

    BOOST_CHECK(fx.wallet.Unlock(SecureString("new")));
    fx.wallet.Lock();
    BOOST_CHECK(!fx.wallet.Unlock(old_pass));
}

BOOST_AUTO_TEST_CASE(execute_passphrase_command_wrong_count)
{
    grc_test::EncryptedWallet fx;

    UniValue params(UniValue::VARR);
    params.push_back("");
    params.push_back("");

    const std::vector<SecureString> one{SecureString("only")};

    try {
        ExecutePassphraseCommand("walletpassphrasechange", one, params);
        BOOST_ERROR("no exception for a wrong passphrase count");
    } catch (const UniValue& error) {
        BOOST_CHECK_EQUAL(find_value(error, "code").get_int(), RPC_INVALID_PARAMETER);
    }
}

BOOST_AUTO_TEST_SUITE_END()
