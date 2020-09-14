#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "script.h"
#include "key.h"

using namespace std;

// Helpers:
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s.begin(), s.end());
    return sSerialized;
}

BOOST_AUTO_TEST_SUITE(sigopcount_tests)

BOOST_AUTO_TEST_CASE(GetSigOpCount)
{
    // Test CScript::GetSigOpCount()
    CScript s1;
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(false), 0);
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(true), 0);

    uint160 dummy;
    s1 << OP_1 << dummy << dummy << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(true), 2);
    s1 << OP_IF << OP_CHECKSIG << OP_ENDIF;
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(true), 3);
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(false), 21);

    CScript p2sh;
    p2sh.SetDestination(s1.GetID());
    CScript scriptSig;
    scriptSig << OP_0 << Serialize(s1);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(scriptSig), 3);

    std::vector<CKey> keys;
    for (int i = 0; i < 3; i++)
    {
        CKey k;
        k.MakeNewKey(true);
        keys.push_back(k);
    }
    CScript s2;
    s2.SetMultisig(1, keys);
    BOOST_CHECK_EQUAL(s2.GetSigOpCount(true), 3);
    BOOST_CHECK_EQUAL(s2.GetSigOpCount(false), 20);

    p2sh.SetDestination(s2.GetID());
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(true), 0);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(false), 0);
    CScript scriptSig2;
    scriptSig2 << OP_1 << dummy << dummy << Serialize(s2);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(scriptSig2), 3);
}

BOOST_AUTO_TEST_SUITE_END()
