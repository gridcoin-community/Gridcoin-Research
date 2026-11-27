#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

#include "hash.h"
#include "key.h"
#include <key_io.h>
#include "test/signature_encodings.h"
#include "uint256.h"
#include "util.h"

using namespace std;


static const std::string strSecret1 ("7NUfsJVtS5TruQrAFpni4yqqvoQg2fNk5JHRh1vsJPfm7Sg8cbY");
static const std::string strSecret2 ("7PiDb7gCaHw7P7Jfayc3d6BioSj8NyzxDzNb94vYe7gGYq2ytYF");
static const std::string strSecret1C("V7o8Z2EW1ghZGLFyrRvkGZtaxTqVGLNy7yHVyd6m1m45gzwsUEci");
static const std::string strSecret2C("VDEvZaU25wjqAapyJxqgaHtKVzYwa2ankLEHgcKLepUKd4F7czq6");
static const std::string addr1 ("SMDEt5x56GHokFwbpaYLNykBo1JwA3tC4E");
static const std::string addr2 ("SC3N7xZ7NnoshVxFhAM6eFWq4YCSGMKkzn");
static const std::string addr1C("SKkhuYMjyZueN4wGpY6pMMW5soPiEDhXe8");
static const std::string addr2C("S9P852TD2PFqh9otrRYUL7rTheRddigULh");

static const string strAddressBad("1HV9Lc3sNHZxwj4Zk6fB38tEmBryq2cBiF");


BOOST_AUTO_TEST_SUITE(key_tests)

BOOST_AUTO_TEST_CASE(key_test1)
{
    CKey key1  = DecodeSecret(strSecret1);
    BOOST_CHECK(key1.IsValid() && !key1.IsCompressed());
    CKey key2  = DecodeSecret(strSecret2);
    BOOST_CHECK(key2.IsValid() && !key2.IsCompressed());
    CKey key1C = DecodeSecret(strSecret1C);
    BOOST_CHECK(key1C.IsValid() && key1C.IsCompressed());
    CKey key2C = DecodeSecret(strSecret2C);
    BOOST_CHECK(key2C.IsValid() && key2C.IsCompressed());
    CKey bad_key = DecodeSecret(strAddressBad);
    BOOST_CHECK(!bad_key.IsValid());

    CPubKey pubkey1  = key1. GetPubKey();
    CPubKey pubkey2  = key2. GetPubKey();
    CPubKey pubkey1C = key1C.GetPubKey();
    CPubKey pubkey2C = key2C.GetPubKey();

    BOOST_CHECK(DecodeDestination(addr1)  == CTxDestination(pubkey1.GetID()));
    BOOST_CHECK(DecodeDestination(addr2)  == CTxDestination(pubkey2.GetID()));
    BOOST_CHECK(DecodeDestination(addr1C) == CTxDestination(pubkey1C.GetID()));
    BOOST_CHECK(DecodeDestination(addr2C) == CTxDestination(pubkey2C.GetID()));

    for (int n=0; n<16; n++)
    {
        string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg);

        // normal signatures

        vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.Sign (hashMsg, sign1));
        BOOST_CHECK(key2.Sign (hashMsg, sign2));
        BOOST_CHECK(key1C.Sign(hashMsg, sign1C));
        BOOST_CHECK(key2C.Sign(hashMsg, sign2C));

        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2C));

        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)

        vector<unsigned char> csign1, csign2, csign1C, csign2C;

        BOOST_CHECK(key1.SignCompact (hashMsg, csign1));
        BOOST_CHECK(key2.SignCompact (hashMsg, csign2));
        BOOST_CHECK(key1C.SignCompact(hashMsg, csign1C));
        BOOST_CHECK(key2C.SignCompact(hashMsg, csign2C));

        CPubKey rkey1, rkey2, rkey1C, rkey2C;

        BOOST_CHECK(rkey1.RecoverCompact (hashMsg, csign1));
        BOOST_CHECK(rkey2.RecoverCompact (hashMsg, csign2));
        BOOST_CHECK(rkey1C.RecoverCompact(hashMsg, csign1C));
        BOOST_CHECK(rkey2C.RecoverCompact(hashMsg, csign2C));

        BOOST_CHECK(rkey1  == pubkey1);
        BOOST_CHECK(rkey2  == pubkey2);
        BOOST_CHECK(rkey1C == pubkey1C);
        BOOST_CHECK(rkey2C == pubkey2C);
    }
}

//! Every signature that CKey::Sign() writes passes CPubKey::CheckSignatureForm(). Among the signatures are a
//! first integer of 33 bytes and, in each place, an integer of under 32 bytes.
BOOST_AUTO_TEST_CASE(a_signature_as_signed_has_the_form_that_sign_writes)
{
    bool first_integer_of_33_bytes_seen = false;
    bool first_integer_under_32_bytes_seen = false;
    bool second_integer_under_32_bytes_seen = false;

    for (const std::string& secret : {strSecret1, strSecret2, strSecret1C, strSecret2C}) {
        const CKey key = DecodeSecret(secret);

        BOOST_REQUIRE(key.IsValid());

        for (unsigned int n = 0; n < 256; ++n) {
            const uint256 hash = Hash(strprintf("signature encoding %u", n));

            for (const bool grind : {true, false}) {
                std::vector<unsigned char> signature;

                BOOST_REQUIRE(key.Sign(hash, signature, grind));
                BOOST_REQUIRE(key.GetPubKey().Verify(hash, signature));
                BOOST_CHECK(CPubKey::CheckSignatureForm(signature));

                if (signature[3] == 33) first_integer_of_33_bytes_seen = true;
                if (signature[3] < 32) first_integer_under_32_bytes_seen = true;
                if (signature[5 + signature[3]] < 32) second_integer_under_32_bytes_seen = true;
            }
        }
    }

    BOOST_CHECK(first_integer_of_33_bytes_seen);
    BOOST_CHECK(first_integer_under_32_bytes_seen);
    BOOST_CHECK(second_integer_under_32_bytes_seen);
}

//! A byte string that does not have the form CKey::Sign() writes does not pass CPubKey::CheckSignatureForm().
BOOST_AUTO_TEST_CASE(only_the_form_that_sign_writes_passes_the_signature_form_check)
{
    const CKey key = DecodeSecret(strSecret1C);

    BOOST_REQUIRE(key.IsValid());

    for (unsigned int n = 0; n < 8; ++n) {
        const uint256 hash = Hash(strprintf("signature encoding %u", n));

        for (const bool grind : {true, false}) {
            std::vector<unsigned char> signature;

            BOOST_REQUIRE(key.Sign(hash, signature, grind));
            BOOST_REQUIRE(CPubKey::CheckSignatureForm(signature));

            for (const auto& [name, encoding] : grc_test::OtherEncodingsOf(signature)) {
                BOOST_CHECK_MESSAGE(!CPubKey::CheckSignatureForm(encoding), name);
            }

            const std::vector<unsigned char> short_by_a_byte(signature.begin(), signature.end() - 1);

            BOOST_CHECK(!CPubKey::CheckSignatureForm(short_by_a_byte));
        }
    }

    BOOST_CHECK(!CPubKey::CheckSignatureForm({}));
}

BOOST_AUTO_TEST_SUITE_END()
