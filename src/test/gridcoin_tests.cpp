#include "global_objects_noui.hpp"
#include "util.h"

#include <boost/test/unit_test.hpp>
#include <map>
#include <string>

extern StructCPID GetInitializedStructCPID2(std::string name,std::map<std::string, StructCPID>& vRef);
extern double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime, double& total_owed, double block_magnitude);
extern std::map<std::string, StructCPID> mvDPOR;

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

BOOST_AUTO_TEST_SUITE(gridcoin_tests)

BOOST_AUTO_TEST_CASE(gridcoin_GetOutstandingAmountOwedShouldReturnCorrectSum)
{
   // Update the CPID earliest pay time to 24 hours ago.
   StructCPID cpid = GetInitializedStructCPID2(TEST_CPID, mvMagnitudes);
   cpid.EarliestPaymentTime = GetAdjustedTime() - 24 * 3600;
   mvMagnitudes[TEST_CPID] = cpid;

   double total_owed;
   double magnitude = 190;
   double outstanding = GetOutstandingAmountOwed(cpid, TEST_CPID, GetAdjustedTime(), total_owed, magnitude);

   // Value taken from GetOutstandingAmountOwed prior to refactoring given
   // the setup in this test and the GridcoinTestsConfig constructor.
   BOOST_CHECK_CLOSE(total_owed, 140.625, 0.00001);
   BOOST_CHECK_CLOSE(outstanding, total_owed - cpid.payments, 0.000001);
}

BOOST_AUTO_TEST_SUITE_END()

