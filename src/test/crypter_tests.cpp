#include "crypter.h"

#include <boost/test/unit_test.hpp>
#include <string>
#include <vector>
#include <iostream>

namespace
{
    const std::string PLAINTEXT("0434e3e09f49ea168c5bbf53f877ff4206923858aab7c7e1df25bc");
    const std::string SALT("jfjdslkfjslk4");
}

BOOST_AUTO_TEST_SUITE(crypter_tests)

BOOST_AUTO_TEST_CASE(crypter_GridEncryptShouldProduceCorrectOutput)
{
    const std::vector<unsigned char> plaintext(PLAINTEXT.begin(), PLAINTEXT.end());
    std::vector<unsigned char> encrypted;

    BOOST_CHECK(GridEncrypt(plaintext, encrypted));

    // Convert encrypted message to Base64 for easier verification.
    std::string encrypted_message(encrypted.begin(), encrypted.end());
    BOOST_CHECK_EQUAL(EncodeBase64(encrypted_message),
                      "sdENub7Ll+MeRKvGrzOPYuOTQ+Eqlbc1reeD+tdnQ9PcxubYwL883Anxbc7POgmm+wL8AfYEqa4MyeQbPRdL1A==");
}

BOOST_AUTO_TEST_CASE(crypter_GridDecryptShouldDecryptValidInput)
{
    const std::vector<unsigned char> encrypted =
            DecodeBase64("sdENub7Ll+MeRKvGrzOPYuOTQ+Eqlbc1reeD+tdnQ9PcxubYwL883Anxbc7POgmm+wL8AfYEqa4MyeQbPRdL1A==");

    // GridEncrypt allocates memory in destination while GridDecrypt doesn't.
    std::vector<unsigned char> plaintext(encrypted.size());
    BOOST_CHECK(GridDecrypt(encrypted, plaintext));
    const std::string decrypted_message(plaintext.begin(), plaintext.end());
    BOOST_CHECK_EQUAL(PLAINTEXT, decrypted_message);
}

BOOST_AUTO_TEST_CASE(crypter_GridEncryptWithSaltShouldProduceCorrectOutput)
{
    const std::vector<unsigned char> plaintext(PLAINTEXT.begin(), PLAINTEXT.end());
    std::vector<unsigned char> encrypted;

    BOOST_CHECK(GridEncryptWithSalt(plaintext, encrypted, SALT));

    // Convert encrypted message to Base64 for easier verification.
    std::string encrypted_message(encrypted.begin(), encrypted.end());
    BOOST_CHECK_EQUAL(EncodeBase64(encrypted_message),
                      "bFCPuKw6yUO2tSMsRlAfaza/bBrYFJFA4ke4S0lEbvH1gMwayuRzbBJ7ZFzAawTIXWpXe+JTvuMQHI6H0kDg6A==");
}

BOOST_AUTO_TEST_CASE(crypter_GridDecryptWithSaltShouldDecryptValidInput)
{
    const std::vector<unsigned char> encrypted =
            DecodeBase64("bFCPuKw6yUO2tSMsRlAfaza/bBrYFJFA4ke4S0lEbvH1gMwayuRzbBJ7ZFzAawTIXWpXe+JTvuMQHI6H0kDg6A==");

    // GridEncrypt allocates memory in destination while GridDecrypt doesn't.
    std::vector<unsigned char> plaintext(encrypted.size());
    BOOST_CHECK(GridDecryptWithSalt(encrypted, plaintext, SALT));
    const std::string decrypted_message(plaintext.begin(), plaintext.end());
    BOOST_CHECK_EQUAL(PLAINTEXT, decrypted_message);
}

BOOST_AUTO_TEST_CASE(crypter_AdvancedDecryptWithSaltShouldNotCrash)
{
    const std::string boinchash_encrypted("HOVtyXamA5H5IWJl6TtwJr9iD5GGSOClvyb9l08ZYCAG2OkS22sGEH6jUt8NlrDQVto/8eBMz1TxPqWCv3bA+o38H25ysTEGHOijlPby2A1VhzQTjFzNYSNaXC4kIaHMgvwgoHCU/Io1LsCBgVK+atiZRuhXDSpbJLHpLmjHokAon0cELZGP3X2g0kQXhImh");
    const std::string salt("\235\002\353\071A\244*\303\b\274\271\221");
    const std::string result = AdvancedDecryptWithSalt(boinchash_encrypted, salt);
}


BOOST_AUTO_TEST_SUITE_END()
