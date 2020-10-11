//
// Unit tests for block-chain checkpoints
//
#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "../chainparams.h"
#include "../checkpoints.h"
#include "../util.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    SelectParams(CBaseChainParams::MAIN);
    uint256 p40 = uint256S("0x0000002c305541bceb763e6f7fce2f111cb752acf9777e64c6f02fab5ef4778c");
    uint256 p500000 = uint256S("0x3916b53eaa0eb392ce5d7e4eaf7db4f745187743f167539ffa4dc1a30c06acbd");
    BOOST_CHECK(Checkpoints::CheckHardened(40, p40));
    BOOST_CHECK(Checkpoints::CheckHardened(500000, p500000));

    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckHardened(40, p500000));
    BOOST_CHECK(!Checkpoints::CheckHardened(500000, p40));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckHardened(40+1, p500000));
    BOOST_CHECK(Checkpoints::CheckHardened(500000+1, p40));

    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 500000);
}

BOOST_AUTO_TEST_SUITE_END()
