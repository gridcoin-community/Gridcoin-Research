// Copyright (c) 2011-2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <test/data/key_io_invalid.json.h>
#include <test/data/key_io_valid.json.h>

#include <chainparams.h>
#include <key.h>
#include <key_io.h>
#include <script.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

UniValue read_json(const std::string& jsondata);

namespace {
//! Restores the selected chain when a case exits, however it exits.
//!
//! These cases switch chain params per vector, and the params are a process
//! global that the module fixture sets once (BOOST_GLOBAL_FIXTURE, not
//! per-case). This suite runs near the head of the binary, so whatever it
//! leaves selected is what nearly every later suite sees, and plenty of those
//! read Params() without selecting first. The cases used to end with an
//! explicit SelectParams(MAIN), which any throw skips -- get_str() on a
//! malformed vector, for one. Scope-based restoration cannot be skipped.
//!
//! Attached with BOOST_FIXTURE_TEST_SUITE rather than declared per case, so a
//! case added later is covered without anyone having to remember.
struct ChainParamsRestorer
{
    const std::string m_original{Params().NetworkIDString()};

    ~ChainParamsRestorer() { SelectParams(m_original); }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(key_io_tests, ChainParamsRestorer)

// Goal: check that parsed keys match test payload
BOOST_AUTO_TEST_CASE(key_io_valid_parse)
{
    UniValue tests = read_json(std::string(json_tests::key_io_valid, json_tests::key_io_valid + sizeof(json_tests::key_io_valid)));
    CKey privkey;
    CTxDestination destination;
    SelectParams(CBaseChainParams::MAIN);

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 3) { // Allow for extra stuff (useful for comments)
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        const std::vector<unsigned char> exp_payload{ParseHex(test[1].get_str())};
        const UniValue &metadata = test[2].get_obj();
        bool isPrivkey = find_value(metadata, "isPrivkey").get_bool();
        SelectParams(find_value(metadata, "chain").get_str());
        bool try_case_flip = find_value(metadata, "tryCaseFlip").isNull() ? false : find_value(metadata, "tryCaseFlip").get_bool();
        if (isPrivkey) {
            bool isCompressed = find_value(metadata, "isCompressed").get_bool();
            // Must be valid private key
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(privkey.IsValid(), "!IsValid:" + strTest);
            BOOST_CHECK_MESSAGE(privkey.IsCompressed() == isCompressed, "compressed mismatch:" + strTest);
            BOOST_CHECK_MESSAGE(Span{privkey} == MakeByteSpan(exp_payload), "key mismatch:" + strTest);

            // Private key must be invalid public key
            destination = DecodeDestination(exp_base58string);
            BOOST_CHECK_MESSAGE(!IsValidDestination(destination), "IsValid privkey as pubkey:" + strTest);
        } else {
            // Must be valid public key
            destination = DecodeDestination(exp_base58string);
            CScript script;
            script.SetDestination(destination);
            BOOST_CHECK_MESSAGE(IsValidDestination(destination), "!IsValid:" + strTest);
            BOOST_CHECK_EQUAL(HexStr(script), HexStr(exp_payload));

            // Try flipped case version
            for (char& c : exp_base58string) {
                if (c >= 'a' && c <= 'z') {
                    c = (c - 'a') + 'A';
                } else if (c >= 'A' && c <= 'Z') {
                    c = (c - 'A') + 'a';
                }
            }
            destination = DecodeDestination(exp_base58string);
            BOOST_CHECK_MESSAGE(IsValidDestination(destination) == try_case_flip, "!IsValid case flipped:" + strTest);
            if (IsValidDestination(destination)) {
                script.SetDestination(destination);
                BOOST_CHECK_EQUAL(HexStr(script), HexStr(exp_payload));
            }

            // Public key must be invalid private key
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(!privkey.IsValid(), "IsValid pubkey as privkey:" + strTest);
        }
    }
}

// Goal: check that generated keys match test vectors
BOOST_AUTO_TEST_CASE(key_io_valid_gen)
{
    UniValue tests = read_json(std::string(json_tests::key_io_valid, json_tests::key_io_valid + sizeof(json_tests::key_io_valid)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 3) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        std::vector<unsigned char> exp_payload = ParseHex(test[1].get_str());
        const UniValue &metadata = test[2].get_obj();
        bool isPrivkey = find_value(metadata, "isPrivkey").get_bool();
        SelectParams(find_value(metadata, "chain").get_str());
        if (isPrivkey) {
            bool isCompressed = find_value(metadata, "isCompressed").get_bool();
            CKey key;
            key.Set(exp_payload.begin(), exp_payload.end(), isCompressed);
            assert(key.IsValid());
            BOOST_CHECK_MESSAGE(EncodeSecret(key) == exp_base58string, "result mismatch: " + strTest);
        } else {
            CTxDestination dest;
            CScript exp_script(exp_payload.begin(), exp_payload.end());
            BOOST_CHECK(ExtractDestination(exp_script, dest));
            std::string address = EncodeDestination(dest);

            BOOST_CHECK_EQUAL(address, exp_base58string);
        }
    }
}


// Goal: check that base58 parsing code is robust against a variety of corrupted data
BOOST_AUTO_TEST_CASE(key_io_invalid)
{
    UniValue tests = read_json(std::string(json_tests::key_io_invalid, json_tests::key_io_invalid + sizeof(json_tests::key_io_invalid))); // Negative testcases
    CKey privkey;
    CTxDestination destination;

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();

        // must be invalid as public and as private key
        for (const auto& chain : { CBaseChainParams::MAIN, CBaseChainParams::TESTNET }) {
            SelectParams(chain);
            destination = DecodeDestination(exp_base58string);
            BOOST_CHECK_MESSAGE(!IsValidDestination(destination), "IsValid pubkey in mainnet:" + strTest);
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(!privkey.IsValid(), "IsValid privkey in mainnet:" + strTest);
        }
    }
}

// Goal: a direct regression test on the restorer above.
//
// The suite fixture restores the chain around every case, so correctness no
// longer depends on where this sits. It is declared last only because it can
// only observe a leak from a case that ran before it: with the restorer
// neutered, key_io_invalid ends its loop on testnet and this fails
// "test != main".
BOOST_AUTO_TEST_CASE(key_io_leaves_chain_params_unchanged)
{
    BOOST_CHECK_EQUAL(Params().NetworkIDString(), CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_SUITE_END()
