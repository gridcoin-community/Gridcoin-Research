#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "util.h"
#include "data/base58_encode_decode.json.h"
#include "data/base58_keys_valid.json.h"
#include "data/base58_keys_invalid.json.h"

#include <univalue.h>

extern UniValue read_json(std::string& jsondata);

BOOST_AUTO_TEST_SUITE(base58_tests)

// Goal: test low-level base58 encoding functionality
BOOST_AUTO_TEST_CASE(base58_EncodeBase58)
{
    UniValue tests = read_json(json_tests::base58_encode_decode);
    for (unsigned int idx = 0; idx < tests.size(); idx++)
    {
        UniValue test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 2) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::vector<unsigned char> sourcedata = ParseHex(test[0].get_str());
        std::string base58string = test[1].get_str();
        BOOST_CHECK_MESSAGE(
                    EncodeBase58(&sourcedata[0], &sourcedata[sourcedata.size()]) == base58string,
                    strTest);
    }
}

// Goal: test low-level base58 decoding functionality
BOOST_AUTO_TEST_CASE(base58_DecodeBase58)
{
    UniValue tests = read_json(json_tests::base58_encode_decode);
    std::vector<unsigned char> result;

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        std::string strTest = test.write();

        if (test.size() < 2) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::vector<unsigned char> expected = ParseHex(test[0].get_str());
        std::string base58string = test[1].get_str();
        BOOST_CHECK_MESSAGE(DecodeBase58(base58string, result), strTest);
        BOOST_CHECK_MESSAGE(result.size() == expected.size() && std::equal(result.begin(), result.end(), expected.begin()), strTest);
    }

    BOOST_CHECK(!DecodeBase58("invalid", result));
}

// Visitor to check address type
class TestAddrTypeVisitor : public boost::static_visitor<bool>
{
private:
    std::string exp_addrType;
public:
    TestAddrTypeVisitor(const std::string &exp_addrType) : exp_addrType(exp_addrType) { }
    bool operator()(const CKeyID &id) const
    {
        return (exp_addrType == "pubkey");
    }
    bool operator()(const CScriptID &id) const
    {
        return (exp_addrType == "script");
    }
    bool operator()(const CNoDestination &no) const
    {
        return (exp_addrType == "none");
    }
};

// Visitor to check address payload
class TestPayloadVisitor : public boost::static_visitor<bool>
{
private:
    std::vector<unsigned char> exp_payload;
public:
    TestPayloadVisitor(std::vector<unsigned char> &exp_payload) : exp_payload(exp_payload) { }
    bool operator()(const CKeyID &id) const
    {
        uint160 exp_key(exp_payload);
        return exp_key == id;
    }
    bool operator()(const CScriptID &id) const
    {
        uint160 exp_key(exp_payload);
        return exp_key == id;
    }
    bool operator()(const CNoDestination &no) const
    {
        return exp_payload.size() == 0;
    }
};

// Goal: check that parsed keys match test payload
BOOST_AUTO_TEST_CASE(base58_keys_valid_parse)
{
    UniValue tests = read_json(json_tests::base58_keys_valid);
    std::vector<unsigned char> result;
    CBitcoinSecret secret;
    CBitcoinAddress addr;
    // Save global state
    bool fTestNet_stored = fTestNet;
    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
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
        bool isTestnet = find_value(metadata, "isTestnet").get_bool();
        fTestNet = isTestnet; // Override testnet flag
        if(isPrivkey)
        {
            bool isCompressed = find_value(metadata, "isCompressed").get_bool();
            // Must be valid private key
            // Note: CBitcoinSecret::SetString tests isValid, whereas CBitcoinAddress does not!
            BOOST_CHECK_MESSAGE(secret.SetString(exp_base58string), "!SetString:"+ strTest);
            BOOST_CHECK_MESSAGE(secret.IsValid(), "!IsValid:" + strTest);
            bool fCompressedOut = false;
            CSecret privkey = secret.GetSecret(fCompressedOut);
            BOOST_CHECK_MESSAGE(fCompressedOut == isCompressed, "compressed mismatch:" + strTest);
            BOOST_CHECK_MESSAGE(privkey.size() == exp_payload.size() && std::equal(privkey.begin(), privkey.end(), exp_payload.begin()), "key mismatch:" + strTest);

            // Private key must be invalid public key
            addr.SetString(exp_base58string);
            BOOST_CHECK_MESSAGE(!addr.IsValid(), "IsValid privkey as pubkey:" + strTest);
        }
        else
        {
            std::string exp_addrType = find_value(metadata, "addrType").get_str(); // "script" or "pubkey"
            // Must be valid public key
            BOOST_CHECK_MESSAGE(addr.SetString(exp_base58string), "SetString:" + strTest);
            BOOST_CHECK_MESSAGE(addr.IsValid(), "!IsValid:" + strTest);
            BOOST_CHECK_MESSAGE(addr.IsScript() == (exp_addrType == "script"), "isScript mismatch" + strTest);
            CTxDestination dest = addr.Get();
            BOOST_CHECK_MESSAGE(boost::apply_visitor(TestAddrTypeVisitor(exp_addrType), dest), "addrType mismatch" + strTest);

            // Public key must be invalid private key
            secret.SetString(exp_base58string);
            BOOST_CHECK_MESSAGE(!secret.IsValid(), "IsValid pubkey as privkey:" + strTest);
        }
    }
    // Restore global state
    fTestNet = fTestNet_stored;
}

// Goal: check that generated keys match test vectors
BOOST_AUTO_TEST_CASE(base58_keys_valid_gen)
{
    UniValue tests = read_json(json_tests::base58_keys_valid);
    std::vector<unsigned char> result;
    // Save global state
    bool fTestNet_stored = fTestNet;
    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
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
        bool isTestnet = find_value(metadata, "isTestnet").get_bool();
        fTestNet = isTestnet; // Override testnet flag
        if(isPrivkey)
        {
            bool isCompressed = find_value(metadata, "isCompressed").get_bool();
            CBitcoinSecret secret;
            secret.SetSecret(CSecret(exp_payload.begin(), exp_payload.end()), isCompressed);
            BOOST_CHECK_MESSAGE(secret.ToString() == exp_base58string, "result mismatch: " + strTest);
        }
        else
        {
            std::string exp_addrType = find_value(metadata, "addrType").get_str();
            CTxDestination dest;
            if(exp_addrType == "pubkey")
            {
                dest = CKeyID(uint160(exp_payload));
            }
            else if(exp_addrType == "script")
            {
                dest = CScriptID(uint160(exp_payload));
            }
            else if(exp_addrType == "none")
            {
                dest = CNoDestination();
            }
            else
            {
                BOOST_ERROR("Bad addrtype: " << strTest);
                continue;
            }
            CBitcoinAddress addrOut;
            BOOST_CHECK_MESSAGE(boost::apply_visitor(CBitcoinAddressVisitor(&addrOut), dest), "encode dest: " + strTest);
            BOOST_CHECK_MESSAGE(addrOut.ToString() == exp_base58string, "mismatch: " + strTest + addrOut.ToString());
        }
    }

    // Visiting a CNoDestination must fail
    CBitcoinAddress dummyAddr;
    CTxDestination nodest = CNoDestination();
    BOOST_CHECK(!boost::apply_visitor(CBitcoinAddressVisitor(&dummyAddr), nodest));

    // Restore global state
    fTestNet = fTestNet_stored;
}

// Goal: check that base58 parsing code is robust against a variety of corrupted data
BOOST_AUTO_TEST_CASE(base58_keys_invalid)
{
    UniValue tests = read_json(json_tests::base58_keys_invalid);
    std::vector<unsigned char> result;
    CBitcoinSecret secret;
    CBitcoinAddress addr;
    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();

        // must be invalid as public and as private key
        addr.SetString(exp_base58string);
        BOOST_CHECK_MESSAGE(!addr.IsValid(), "IsValid pubkey:" + strTest);
        secret.SetString(exp_base58string);
        BOOST_CHECK_MESSAGE(!secret.IsValid(), "IsValid privkey:" + strTest);
    }
}


BOOST_AUTO_TEST_SUITE_END()
