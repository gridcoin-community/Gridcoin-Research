#include <boost/test/unit_test.hpp>

#include "gridcoin/sidestake.h"
#include "main.h"
#include "wallet/wallet.h"

// how many times to run all the tests to have a chance to catch errors that only show up with particular random shuffles
#define RUN_TESTS 100

// some tests fail 1% of the time due to bad luck.
// we repeat those tests this many times and only complain if all iterations of the test fail
#define RANDOM_REPEATS 5

using namespace std;

std::vector<std::unique_ptr<CWalletTx>> wtxn;

typedef set<pair<const CWalletTx*,unsigned int> > CoinSet;

BOOST_AUTO_TEST_SUITE(wallet_tests)

static CWallet wallet;
static vector<COutput> vCoins;

static void add_coin(int64_t nValue, int nAge = 6*24, bool fIsFromMe = false, int nInput=0)
{
    static int i;
    CTransaction tx;
    tx.nLockTime = i++;        // so all transactions get different hashes
    tx.nTime = 0;
    tx.vout.resize(nInput+1);
    tx.vout[nInput].nValue = nValue;
    std::unique_ptr<CWalletTx> wtx(new CWalletTx(&wallet, tx));
    if (fIsFromMe)
    {
        // IsFromMe() returns (GetDebit() > 0), and GetDebit() is 0 if vin.empty(),
        // so stop vin being empty, and cache a non-zero Debit to fake out IsFromMe()
        wtx->vin.resize(1);
        wtx->fDebitCached = true;
        wtx->nDebitCached = 1;
    }
    COutput output(wtx.get(), nInput, nAge);
    vCoins.push_back(output);
    wtxn.emplace_back(std::move(wtx));
}

static void empty_wallet()
{
    vCoins.clear();
    wtxn.clear();
}

static bool equal_sets(CoinSet a, CoinSet b)
{
    pair<CoinSet::iterator, CoinSet::iterator> ret = mismatch(a.begin(), a.end(), b.begin());
    return ret.first == a.end() && ret.second == b.end();
}

BOOST_AUTO_TEST_CASE(coin_selection_tests)
{
    static CoinSet setCoinsRet, setCoinsRet2;
    static int64_t nValueRet;
    int64_t spendTime = GetTime();

    // test multiple times to allow for differences in the shuffle order
    for (int i = 0; i < RUN_TESTS; i++)
    {
        empty_wallet();

        // with an empty wallet we can't even pay one cent
        BOOST_CHECK(!wallet.SelectCoinsMinConf(CENT, spendTime, 1, 6, vCoins, setCoinsRet, nValueRet));

        add_coin(CENT, 4);        // add a new 1 cent coin

        // with a new 1 cent coin, we still can't find a mature 1 cent
        BOOST_CHECK(!wallet.SelectCoinsMinConf(CENT, spendTime, 1, 6, vCoins, setCoinsRet, nValueRet));

        // but we can find a new 1 cent
        BOOST_CHECK(wallet.SelectCoinsMinConf(CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, CENT);

        add_coin(2 * CENT);           // add a mature 2 cent coin

        // we can't make 3 cents of mature coins
        BOOST_CHECK(!wallet.SelectCoinsMinConf(3 * CENT, spendTime, 1, 6, vCoins, setCoinsRet, nValueRet));

        // we can make 3 cents of new coins
        BOOST_CHECK(wallet.SelectCoinsMinConf(3 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 3 * CENT);

        add_coin(5 * CENT);           // add a mature 5 cent coin,
        add_coin(10 * CENT, 3, true); // a new 10 cent coin sent from one of our own addresses
        add_coin(20 * CENT);          // and a mature 20 cent coin

        // now we have new: 1+10=11 (of which 10 was self-sent), and mature: 2+5+20=27.  total = 38

        // we can't make 38 cents only if we disallow new coins:
        BOOST_CHECK(!wallet.SelectCoinsMinConf(38 * CENT, spendTime, 1, 6, vCoins, setCoinsRet, nValueRet));
        // we can't even make 37 cents if we don't allow new coins even if they're from us
        BOOST_CHECK(!wallet.SelectCoinsMinConf(38 * CENT, spendTime, 6, 6, vCoins, setCoinsRet, nValueRet));
        // but we can make 37 cents if we accept new coins from ourself
        BOOST_CHECK(wallet.SelectCoinsMinConf(37 * CENT, spendTime, 1, 6, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 37 * CENT);
        // and we can make 38 cents if we accept all new coins
        BOOST_CHECK(wallet.SelectCoinsMinConf(38 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 38 * CENT);

        // try making 34 cents from 1,2,5,10,20 - we can't do it exactly
        BOOST_CHECK(wallet.SelectCoinsMinConf(34 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_GT(nValueRet, 34 * CENT); // but should get more than 34 cents
        // the best should be 20+10+5.  it's incredibly unlikely the 1 or 2 got included (but possible)
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 3);

        // when we try making 7 cents, the smaller coins (1,2,5) are enough.  We should see just 2+5
        BOOST_CHECK(wallet.SelectCoinsMinConf(7 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 7 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 2);

        // when we try making 8 cents, the smaller coins (1,2,5) are exactly enough.
        BOOST_CHECK(wallet.SelectCoinsMinConf(8 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK(nValueRet == 8 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 3);

        // when we try making 9 cents, no subset of smaller coins is enough, and we get the next bigger coin (10)
        BOOST_CHECK(wallet.SelectCoinsMinConf(9 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 10 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 1);

        // now clear out the wallet and start again to test choosing between subsets of smaller coins and the next biggest coin
        empty_wallet();

        add_coin(6 * CENT);
        add_coin(7 * CENT);
        add_coin(8 * CENT);
        add_coin(20 * CENT);
        add_coin(30 * CENT); // now we have 6+7+8+20+30 = 71 cents total

        // check that we have 71 and not 72
        BOOST_CHECK(wallet.SelectCoinsMinConf(71 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK(!wallet.SelectCoinsMinConf(72 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));

        // now try making 16 cents.  the best smaller coins can do is 6+7+8 = 21; not as good at the next biggest coin, 20
        BOOST_CHECK(wallet.SelectCoinsMinConf(16 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 20 * CENT); // we should get 20 in one coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 1);

        add_coin(5 * CENT); // now we have 5+6+7+8+20+30 = 75 cents total

        // now if we try making 16 cents again, the smaller coins can make 5+6+7 = 18 cents, better than the next biggest coin, 20
        BOOST_CHECK(wallet.SelectCoinsMinConf(16 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 18 * CENT); // we should get 18 in 3 coins
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 3);

        add_coin(18 * CENT); // now we have 5+6+7+8+18+20+30

        // and now if we try making 16 cents again, the smaller coins can make 5+6+7 = 18 cents, the same as the next biggest coin, 18
        BOOST_CHECK(wallet.SelectCoinsMinConf(16 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 18 * CENT);  // we should get 18 in 1 coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 1); // because in the event of a tie, the biggest coin wins

        // now try making 11 cents.  we should get 5+6
        BOOST_CHECK(wallet.SelectCoinsMinConf(11 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 11 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 2);

        // check that the smallest bigger coin is used
        add_coin(1 * COIN);
        add_coin(2 * COIN);
        add_coin(3 * COIN);
        add_coin(4 * COIN); // now we have 5+6+7+8+18+20+30+100+200+300+400 = 1094 cents
        BOOST_CHECK(wallet.SelectCoinsMinConf(95 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 1 * COIN);  // we should get 1 BTC in 1 coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 1);

        BOOST_CHECK(wallet.SelectCoinsMinConf(195 * CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 2 * COIN);  // we should get 2 BTC in 1 coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 1);

        // empty the wallet and start again, now with fractions of a cent, to test sub-cent change avoidance
        empty_wallet();
        add_coin((GRC::Allocation(1, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(2, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(3, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(4, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(5, 10, true) * CENT).ToCAmount());

        // try making 1 cent from 0.1 + 0.2 + 0.3 + 0.4 + 0.5 = 1.5 cents
        // we'll get sub-cent change whatever happens, so can expect 1.0 exactly
        BOOST_CHECK(wallet.SelectCoinsMinConf(CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, CENT);

        // but if we add a bigger coin, making it possible to avoid sub-cent change, things change:
        add_coin(1111 * CENT);

        // try making 1 cent from 0.1 + 0.2 + 0.3 + 0.4 + 0.5 + 1111 = 1112.5 cents
        BOOST_CHECK(wallet.SelectCoinsMinConf(CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, CENT); // we should get the exact amount
        // if we add more sub-cent coins:
        add_coin((GRC::Allocation(6, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(7, 10, true) * CENT).ToCAmount());

        // and try again to make 1.0 cents, we can still make 1.0 cents
        BOOST_CHECK(wallet.SelectCoinsMinConf(CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, CENT); // we should get the exact amount

        // run the 'mtgox' test (see http://blockexplorer.com/tx/29a3efd3ef04f9153d47a990bd7b048a4b2d213daaa5fb8ed670fb85f13bdbcf)
        // they tried to consolidate 10 50k coins into one 500k coin, and ended up with 50k in change
        empty_wallet();
        for (int i = 0; i < 20; i++)
            add_coin(50000 * COIN);

        BOOST_CHECK(wallet.SelectCoinsMinConf(500000 * COIN, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 500000 * COIN); // we should get the exact amount
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 10); // in ten coins

        // if there's not enough in the smaller coins to make at least 1 cent change (0.5+0.6+0.7 < 1.0+1.0),
        // we need to try finding an exact subset anyway

        // sometimes it will fail, and so we use the next biggest coin:
        empty_wallet();
        add_coin((GRC::Allocation(5, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(6, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(7, 10, true) * CENT).ToCAmount());
        add_coin(1111 * CENT);
        BOOST_CHECK(wallet.SelectCoinsMinConf(CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, 1111 * CENT); // we get the bigger coin
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 1);

        // but sometimes it's possible, and we use an exact subset (0.4 + 0.6 = 1.0)
        empty_wallet();
        add_coin((GRC::Allocation(4, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(6, 10, true) * CENT).ToCAmount());
        add_coin((GRC::Allocation(8, 10, true) * CENT).ToCAmount());
        add_coin(1111 * CENT);
        BOOST_CHECK(wallet.SelectCoinsMinConf(CENT, spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        BOOST_CHECK_EQUAL(nValueRet, CENT);   // we should get the exact amount
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 2); // in two coins 0.4+0.6

        // test avoiding sub-cent change
        empty_wallet();
        // Use rational arithmetic because the floating point has problems with GCC13 on 32 bit architecture x86.
        add_coin((GRC::Allocation(5, 10000, true) * COIN).ToCAmount());
        add_coin((GRC::Allocation(1, 100, true) * COIN).ToCAmount());
        add_coin(COIN);

        // trying to make 1.0001 from these three coins
        BOOST_CHECK(wallet.SelectCoinsMinConf((GRC::Allocation(10001, 10000, true) * COIN).ToCAmount(),
                                              spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        // we should get all coins
        BOOST_CHECK(nValueRet == (GRC::Allocation(10105, 10000, true) * COIN).ToCAmount());
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 3);

        // but if we try to make 0.999, we should take the bigger of the two small coins to avoid sub-cent change
        BOOST_CHECK(wallet.SelectCoinsMinConf((GRC::Allocation(999, 1000, true) * COIN).ToCAmount(),
                                              spendTime, 1, 1, vCoins, setCoinsRet, nValueRet));
        // we should get 1 + 0.01
        BOOST_CHECK(nValueRet == (GRC::Allocation(101, 100, true) * COIN).ToCAmount());
        BOOST_CHECK_EQUAL(setCoinsRet.size(), (size_t) 2);

        // test randomness
        {
            empty_wallet();
            for (int i2 = 0; i2 < 100; i2++)
                add_coin(COIN);

            // picking 50 from 100 coins doesn't depend on the shuffle,
            // but does depend on randomness in the stochastic approximation code
            BOOST_CHECK(wallet.SelectCoinsMinConf(50 * COIN, spendTime, 1, 6, vCoins, setCoinsRet , nValueRet));
            BOOST_CHECK(wallet.SelectCoinsMinConf(50 * COIN, spendTime, 1, 6, vCoins, setCoinsRet2, nValueRet));
            BOOST_CHECK(!equal_sets(setCoinsRet, setCoinsRet2));

            int fails = 0;
            for (int i = 0; i < RANDOM_REPEATS; i++)
            {
                // selecting 1 from 100 identical coins depends on the shuffle; this test will fail 1% of the time
                // run the test RANDOM_REPEATS times and only complain if all of them fail
                BOOST_CHECK(wallet.SelectCoinsMinConf(COIN, spendTime, 1, 6, vCoins, setCoinsRet , nValueRet));
                BOOST_CHECK(wallet.SelectCoinsMinConf(COIN, spendTime, 1, 6, vCoins, setCoinsRet2, nValueRet));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            BOOST_CHECK_NE(fails, RANDOM_REPEATS);

            // add 75 cents in small change.  not enough to make 90 cents,
            // then try making 90 cents.  there are multiple competing "smallest bigger" coins,
            // one of which should be picked at random
            add_coin(5 * CENT); add_coin(10 * CENT); add_coin(15 * CENT); add_coin(20 * CENT); add_coin(25 * CENT);

            fails = 0;
            for (int i = 0; i < RANDOM_REPEATS; i++)
            {
                // selecting 1 from 100 identical coins depends on the shuffle; this test will fail 1% of the time
                // run the test RANDOM_REPEATS times and only complain if all of them fail
                BOOST_CHECK(wallet.SelectCoinsMinConf(90 * CENT, spendTime, 1, 6, vCoins, setCoinsRet , nValueRet));
                BOOST_CHECK(wallet.SelectCoinsMinConf(90 * CENT, spendTime, 1, 6, vCoins, setCoinsRet2, nValueRet));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            BOOST_CHECK_NE(fails, RANDOM_REPEATS);
        }
    }
    empty_wallet();
}

BOOST_AUTO_TEST_SUITE_END()
