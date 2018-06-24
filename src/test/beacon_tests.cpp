#include "global_objects_noui.hpp"
#include "uint256.h"
#include "util.h"
#include "main.h"
#include "beacon.h"
#include "appcache.h"

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>
#include <map>
#include <string>
namespace utf = boost::unit_test;

namespace
{
   // Arbitrary random characters generated with Python UUID.
   const std::string TEST_CPID("17c65330c0924259b2f93c31d25b03ac");

   struct GridcoinTestsConfig
   {
      GridcoinTestsConfig()
      {
         // Create a fake CPID
         StructCPID cpid = GetInitializedStructCPID2(TEST_CPID, mvMagnitudes);
         cpid.projectname = "My Simple Test Project";
         cpid.entries = 1;
         cpid.cpid = TEST_CPID;
         cpid.payments = 123;
         //cpid.EarliestPaymentTime = 1490260000;
         mvMagnitudes[TEST_CPID] = cpid;

         // Create a fake DPOR
         StructCPID dpor = GetInitializedStructCPID2(TEST_CPID, mvDPOR);
         dpor.Magnitude = 125;
         mvDPOR[TEST_CPID] = dpor;
      }

      ~GridcoinTestsConfig()
      {
         // Clean up.
         mvMagnitudes.erase(TEST_CPID);
         mvDPOR.erase(TEST_CPID);
      }
   };
}

BOOST_GLOBAL_FIXTURE(GridcoinTestsConfig);

BOOST_AUTO_TEST_SUITE(gridcoin_beacon_tests)

/* Test requirements:
 * beacon.cpp/h
    * n:GenerateBeaconKeys [Integ]
    * n:GetStoredBeaconPrivateKey [Integ]
    * p: GetBeaconElements, GetBeaconPublicKey, BeaconTimeStamp, HasActiveBeacon [ClearSky]
    * p: VerifyBeaconContractTx
 * contract
    * SignMessage
    * Verify signature
    * SignBlockWithCPID
 * rpcblockchain ImportBeaconKeysFromConfig
 * rpcblockchain AdvertiseBeacon
 * main GridcoinServices? timer that calls import
 * rpcdump importprivkey
*/

BOOST_AUTO_TEST_CASE(GetBeaconElements_good)
{
    const std::string sBeacon= "ZjVjY2U0NThmYjM1MzJjMzhkZDE3NTdiOGZiZTllY2FjOWM5OTc0MTM5M2FjYjk3OWM5OWM4Mzc2ZDY3M2ZjNDNiNjczYzZiOTc2NDY4NjdjYWM1OWVjNTllYzg5NzM0NjYzNjMxMzQzOTNhMmY2Nzc2NmQ2NTYyNDE2ODZlNzkyZjY1NjY7MDI1NmY2NTRjYzE5MTBkY2E4Y2I5NzMxMWY0MGMxYjkyYjAxNGYwNzdlYzc5ZGM3Y2U2ZGM1MDg2MTU4ODE2NTttc3pzRTlKaGN3dGJ6cHJoMUdhNTVndVFvWUNBcmpvYzd6OzA0NmNlNGQ4YjUzZmFlMTkwOGY0ZTMyZDlmNWY0MDc0OTA4MDUwMzZhYmUxYWEwNTEyNzA5YmFiYTgyZjU0M2I4YWNiZjA2NGFhYTg2Mzk0OTBiZDgzYTMyY2YzZTM1ZDc5NGRkYjJhNjljYmZjZWZkZjdiMzMyOGQ1MzFjMTIzYjA=";
    std::string out_cpid, out_address, out_publickey;
    GetBeaconElements(sBeacon, out_cpid, out_address, out_publickey);
    /* out_cpid is in (deprecated) "CPIDv2" format, that has a cpid in front and some unknown data attached to end. We
     * do not want to check the extra data, so justcheck the first 32 chars and ignore the rest. */
    BOOST_CHECK_EQUAL(out_cpid.substr(0,32), "f5cce458fb3532c38dd1757b8fbe9eca");
    BOOST_CHECK_EQUAL(out_address, "mszsE9Jhcwtbzprh1Ga55guQoYCArjoc7z");
    BOOST_CHECK_EQUAL(out_publickey, "046ce4d8b53fae1908f4e32d9f5f407490805036abe1aa0512709baba82f543b8acbf064aaa8639490bd83a32cf3e35d794ddb2a69cbfcefdf7b3328d531c123b0");
}

struct GridcoinBeaconTestsFixture
{
    /* mock data
"b67846ea8547cedd2d61ce492fe14102": "YjY3ODQ2ZWE4NTQ3Y2VkZDJkNjFjZTQ5MmZlMTQxMDIzNTM4MzgzZDlhOWEzZWM5OTZjYTk3Mzk0MDM5OTUzNmM3NmM2NjZjMzQ5NDQxYzkzZTlkOTM2OTZkM2M0MjZiNzk2MjZlNmE2ZjZlNzA0MTcwNmU2ZjZhNzU2NjY0NjkyZjZmNjY3NTs3MmQzMDlhMDI5OGJmNjc0NTI2ZmI1NDI3NjMxY2NjZWIyNGQzMDA4NjVlOTM0Mjg1NjJiODIwZGMxNTA2MGQyO21tejQ4UFBBcGhWNk15ZmFxRGlZaTd6OUMzTkVnWGFTajU7MDRhMTQzODgxZjQwYWJiMzRiOTI4ZWZhZGEyODczZjY1NzFkYmY3NTgzYjMzMzU5ZWIxMjhiZWVlMzA5YWZmMjViNGUwY2E1ZTlhMWQ4MzUwNzkxNzA1MzkyY2I5NDQ1ZDIzY2ZlY2NlODRjYmYxMDEzNDRmNGYxY2QxZjg3N2Y0Yg==",
"bc0621a4ac4610ffa400a0d298c02e23": "YmMwNjIxYTRhYzQ2MTBmZmE0MDBhMGQyOThjMDJlMjNjYWMyOWEzZGM1OTc2N2M4OTk5OWM0M2M0MjNjYzM0MzM5NzAzYjk4NjgzMmM4YzgzODk5Njk5Nzk3M2U2NTk4NmI2MjZlNjY3NDcwNzg2NjZmNzQ0MTcwNzE3NTcwNmY2ZDZhNmY2NjJmNmY2Njc1OzBlNWQwODg1ODllYmU3MTBjODBlOWM2YjU1YjgwYWQzOTZkOTk2ZWUxNzBlMGVkNjQyMmVhMWI1ZTk5NDVlYjg7bW42Mm9rNzl2RmFOc3N6ZUVxZGUybldWUXRKVmdaUHo2RjswNGYyMTU1OWViY2Q2NzhmYTg3N2RiZThhYjAwOWE4YjZlNWNkODA3ZWE3Y2FlMzAxNDNiMjU1YTYyNjRmYmMxMjUzMmM2NGZjZGYyMjcxOTk2ODk2ZDQ3NTIzNGYzN2E5YjhjOWE4YTQzMjlhMDQyNGVlMjg2M2NiY2NlMWY1MmRm",
"c5c0a595d39ff3a00a2aca7485f1754f": "YzVjMGE1OTVkMzlmZjNhMDBhMmFjYTc0ODVmMTc1NGY5N2M2MzUzNjM5M2FjYTM4Mzc5MTY1NmIzZDM5NjY5Njk4M2MzNWM4OTYzZDk3MzU2OTljYzY2Yjk2M2UzZTQwNjMzMTc1NDE3NDY2N2I2ZjYyNmUyZjY0N2I7YjE2NDdhMTFmYzIzOWI5NTU2ODAyYzE2YjhhNWNhODY0M2U4MDBkMDE5NTE5MDdkMGU2NzI5NGU0MjMyNzNjYTttclZTNkVRVzVoNEcxRnM1QTlFTWFnajFLNXllTkRNSGdrOzA0ODNjY2YyZTA3MWI3Yjk3OTU4ZGM4MzI0YmNlNDA2NDQzODk2ZDEyYjk5MmNjZjQ5ZDA3YjIxZTgzMzU2NGVkODg4NzU3YzhlOGNjMWJmMjVhNWM0NzQ1MDIzOTc2N2M2MDIxZWJjNjAyYzY3MjcwMDY0MGYyODZjNzBlNTQwMGM=",
"d0cea85e7a4a3e30af709e672dace08b": "ZDBjZWE4NWU3YTRhM2UzMGFmNzA5ZTY3MmRhY2UwOGI5OGM4M2Q5NDY2Yzc2NjNhOWI2YjNlNjc5Nzk5NjdjNzllY2EzOTYzNzA5MzllOTY5OWM4MzY5Yjk4NmI2M2M1NzQ2OTZlNzA3MDY4NmQ2NjcwNzQ3NjZjNjI2ZTZhNDE2ODZlNjI2YTZkMmY2NDcwNmU7OTQ0MmI4OWRlMzcyMzE0MWEyZDQ5YWNhZTIxN2QyYTNjYzRhNmQ4NjFlMzQ3YTZiZGZiZTYwY2JjNmMwZDc1Yzttb0FhYmFzRGlMRjd5YURKSHZQS0REc0RvdUYyUXhNb0J5OzA0MDU3ZWU2MzhhNTY3NzcwM2YyMWNjMzk5MmI1MDc0ZmI3YjA0MWUxYTI3MDg2NWYzZTAwNGJhNWFlNGNkN2IxMWZkZmY1YTE0ZWU4MDk3ODBjODJlZDhjNTZkMmM5N2FjOTgyMzJhMzAwZTU4ZjZiZmQ0ZTM3YjZkZjQzOTI0ZmE=",
    */
    const int64_t beacon1_time;
    GridcoinBeaconTestsFixture()
        : beacon1_time(pindexBest->nTime - 600 /* 10 minutes ago */)
    {
        // setup function
        /* Add cpid beacon, so it is valid (age 10 minutes)
         * pk: 04a143881f40abb34b928efada2873f6571dbf7583b33359eb128beee309aff25b4e0ca5e9a1d8350791705392cb9445d23cfecce84cbf101344f4f1cd1f877f4b */
        WriteCache("beacon","b67846ea8547cedd2d61ce492fe14102",
            "YjY3ODQ2ZWE4NTQ3Y2VkZDJkNjFjZTQ5MmZlMTQxMDIzNTM4MzgzZDlhOWEzZWM5OTZjYTk3Mzk0MDM5OTUzNmM3NmM2NjZjMzQ5NDQxYzkzZTlkOTM2OTZkM2M0MjZiNzk2MjZlNmE2ZjZlNzA0MTcwNmU2ZjZhNzU2NjY0NjkyZjZmNjY3NTs3MmQzMDlhMDI5OGJmNjc0NTI2ZmI1NDI3NjMxY2NjZWIyNGQzMDA4NjVlOTM0Mjg1NjJiODIwZGMxNTA2MGQyO21tejQ4UFBBcGhWNk15ZmFxRGlZaTd6OUMzTkVnWGFTajU7MDRhMTQzODgxZjQwYWJiMzRiOTI4ZWZhZGEyODczZjY1NzFkYmY3NTgzYjMzMzU5ZWIxMjhiZWVlMzA5YWZmMjViNGUwY2E1ZTlhMWQ4MzUwNzkxNzA1MzkyY2I5NDQ1ZDIzY2ZlY2NlODRjYmYxMDEzNDRmNGYxY2QxZjg3N2Y0Yg==",
            beacon1_time);
    }
    ~GridcoinBeaconTestsFixture()
    {     // teardown function
    }
};

BOOST_FIXTURE_TEST_CASE(GetBeaconPublicKey_clearsky, GridcoinBeaconTestsFixture)
{
    std::string pk= GetBeaconPublicKey("b67846ea8547cedd2d61ce492fe14102", false);
    BOOST_CHECK_EQUAL( pk , "04a143881f40abb34b928efada2873f6571dbf7583b33359eb128beee309aff25b4e0ca5e9a1d8350791705392cb9445d23cfecce84cbf101344f4f1cd1f877f4b");
}

BOOST_FIXTURE_TEST_CASE(GetBeaconPublicKey_Advertising_clearsky, GridcoinBeaconTestsFixture)
{
    std::string pk= GetBeaconPublicKey("b67846ea8547cedd2d61ce492fe14102", true);
    BOOST_CHECK_EQUAL( pk , "04a143881f40abb34b928efada2873f6571dbf7583b33359eb128beee309aff25b4e0ca5e9a1d8350791705392cb9445d23cfecce84cbf101344f4f1cd1f877f4b");
}

BOOST_FIXTURE_TEST_CASE(BeaconTimeStamp_clearsky, GridcoinBeaconTestsFixture)
{
    int64_t tstamp= BeaconTimeStamp("b67846ea8547cedd2d61ce492fe14102", false);
    BOOST_CHECK_EQUAL( tstamp , beacon1_time);
}

BOOST_FIXTURE_TEST_CASE(HasActiveBeacon_clearsky, GridcoinBeaconTestsFixture)
{
    BOOST_CHECK_EQUAL( true, HasActiveBeacon("b67846ea8547cedd2d61ce492fe14102") );
}


BOOST_AUTO_TEST_CASE(Generate_Retrieve_integration)
{
    CKey generatedKey, retrievedKey;
    BOOST_CHECK( GenerateBeaconKeys(TEST_CPID, generatedKey) );

    //Simulate that a beacon was accepted
    WriteCache("beacon",TEST_CPID,
        EncodeBase64(TEST_CPID+";stuff1;stuff2;"+HexStr(generatedKey.GetPubKey().Raw())),
        pindexBest->nTime - 600);

    BOOST_CHECK( GetStoredBeaconPrivateKey(TEST_CPID, retrievedKey) );
    BOOST_CHECK( retrievedKey.IsValid() );
    BOOST_CHECK( !retrievedKey.GetPrivKey().empty() );
    bool bool1,bool2;
    BOOST_CHECK( retrievedKey.GetSecret(bool1) == generatedKey.GetSecret(bool2));
}


BOOST_AUTO_TEST_SUITE_END()
