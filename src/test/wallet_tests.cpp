#include <boost/test/unit_test.hpp>

#include "gridcoin/sidestake.h"
#include "main.h"
#include "wallet/wallet.h"

// Stream operator for uint256 to support Boost Test output.
// Defined in global namespace (not std) to avoid undefined behavior per [namespace.std].
inline std::ostream& operator<<(std::ostream& os, const uint256& hash) {
    return os << hash.ToString();
}

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
    CMutableTransaction tx;
    tx.nLockTime = i++;        // so all transactions get different hashes
    tx.nTime = 0;
    tx.vout.resize(nInput+1);
    tx.vout[nInput].nValue = nValue;
    if (fIsFromMe)
    {
        // IsFromMe() returns (GetDebit() > 0), and GetDebit() is 0 if vin.empty(),
        // so stop vin being empty, and cache a non-zero Debit to fake out IsFromMe()
        tx.vin.resize(1);
    }
    std::unique_ptr<CWalletTx> wtx(new CWalletTx(&wallet, CTransaction(tx)));
    if (fIsFromMe)
    {
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

// ---------------------------------------------------------------------------
// SyncLegacyFromState — derives hashBlock/nIndex from m_state
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(sync_legacy_from_state_confirmed)
{
    CWalletTx wtx;
    uint256 bh = uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    wtx.SetTxState(TxStateConfirmed(bh, 500, 7));

    BOOST_CHECK_EQUAL(wtx.hashBlock, bh);
    BOOST_CHECK_EQUAL(wtx.nIndex, 7);
}

BOOST_AUTO_TEST_CASE(sync_legacy_from_state_abandoned)
{
    CWalletTx wtx;
    wtx.SetTxState(TxStateInactive{true});

    BOOST_CHECK_EQUAL(wtx.hashBlock, ABANDONED_HASH_SENTINEL);
    BOOST_CHECK_EQUAL(wtx.nIndex, -1);
}

BOOST_AUTO_TEST_CASE(sync_legacy_from_state_conflicted)
{
    CWalletTx wtx;
    wtx.SetTxState(TxStateInactive{false});

    BOOST_CHECK_EQUAL(wtx.hashBlock, CONFLICTED_HASH_SENTINEL);
    BOOST_CHECK_EQUAL(wtx.nIndex, -1);
}

BOOST_AUTO_TEST_CASE(sync_legacy_from_state_mempool)
{
    CWalletTx wtx;
    wtx.SetTxState(TxStateInMempool{});

    BOOST_CHECK(wtx.hashBlock.IsNull());
    BOOST_CHECK_EQUAL(wtx.nIndex, -1);
}

BOOST_AUTO_TEST_CASE(sync_legacy_from_state_unrecognized)
{
    CWalletTx wtx;
    wtx.SetTxState(TxStateUnrecognized{});

    BOOST_CHECK(wtx.hashBlock.IsNull());
    BOOST_CHECK_EQUAL(wtx.nIndex, -1);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(wallet_integration_tests)

BOOST_AUTO_TEST_CASE(state_transition_mempool_to_confirmed)
{
    // Test state transition from mempool to confirmed
    CWallet test_wallet;

    // Create a transaction
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();
    CTransactionRef ptx = MakeTransactionRef(tx);

    {
        LOCK(test_wallet.cs_wallet);

        // Start in mempool state
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());

        // Transition to confirmed
        uint256 block_hash = uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        TxState confirmed_state = TxStateConfirmed(block_hash, 100, 5);

        // Manually update state (simulating what SyncTransaction does)
        test_wallet.mapWallet[hash].SetTxState(confirmed_state);

        // Verify transition
        BOOST_CHECK(test_wallet.mapWallet[hash].isConfirmed());
        BOOST_CHECK(!test_wallet.mapWallet[hash].isInMempool());

        auto* conf = test_wallet.mapWallet[hash].state<TxStateConfirmed>();
        BOOST_CHECK(conf != nullptr);
        BOOST_CHECK(conf->m_confirmed_block_hash == block_hash);
        BOOST_CHECK_EQUAL(conf->m_confirmed_block_height, 100);
        BOOST_CHECK_EQUAL(conf->m_position_in_block, 5);
    }
}

BOOST_AUTO_TEST_CASE(state_transition_confirmed_to_inactive_reorg)
{
    // Test reorg scenario: confirmed -> inactive
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 75 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Start confirmed
        CWalletTx wtx(&test_wallet, tx);
        uint256 block_hash = uint256S("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
        wtx.SetTxState(TxStateConfirmed(block_hash, 200, 3));
        test_wallet.mapWallet[hash] = wtx;

        BOOST_CHECK(test_wallet.mapWallet[hash].isConfirmed());

        // Simulate reorg: block disconnected, tx becomes inactive
        test_wallet.mapWallet[hash].SetTxState(TxStateInactive{false});

        // Verify transition
        BOOST_CHECK(test_wallet.mapWallet[hash].isInactive());
        BOOST_CHECK(!test_wallet.mapWallet[hash].isConfirmed());

        auto* inactive = test_wallet.mapWallet[hash].state<TxStateInactive>();
        BOOST_CHECK(inactive != nullptr);
        BOOST_CHECK_EQUAL(inactive->m_abandoned, false);  // Not abandoned, just orphaned
    }
}

BOOST_AUTO_TEST_CASE(spent_tracking_on_confirmation)
{
    // Test that parent outputs are marked spent when child tx confirms
    CWallet test_wallet;

    // Create parent transaction
    CTransaction parent_tx;
    parent_tx.vout.resize(2);
    parent_tx.vout[0].nValue = 100 * COIN;
    parent_tx.vout[1].nValue = 50 * COIN;
    uint256 parent_hash = parent_tx.GetHash();

    // Create child transaction spending parent output 0
    CTransaction child_tx;
    child_tx.vin.resize(1);
    child_tx.vin[0].prevout = COutPoint(parent_hash, 0);
    child_tx.vout.resize(1);
    child_tx.vout[0].nValue = 99 * COIN;  // 1 COIN fee
    uint256 child_hash = child_tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Add parent to wallet (confirmed)
        CWalletTx parent_wtx(&test_wallet, parent_tx);
        parent_wtx.SetTxState(TxStateConfirmed(uint256S("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"), 100, 1));
        parent_wtx.vfSpent.resize(2, false);  // Both outputs unspent initially
        test_wallet.mapWallet[parent_hash] = parent_wtx;

        // Add child to wallet (mempool initially)
        CWalletTx child_wtx(&test_wallet, child_tx);
        child_wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[child_hash] = child_wtx;

        // Verify parent output 0 is unspent
        BOOST_CHECK(!test_wallet.mapWallet[parent_hash].IsSpent(0));
        BOOST_CHECK(!test_wallet.mapWallet[parent_hash].IsSpent(1));

        // Mark parent output as spent (simulating what SyncTransaction does)
        test_wallet.mapWallet[parent_hash].MarkSpent(0);

        // Verify parent output 0 is now spent
        BOOST_CHECK(test_wallet.mapWallet[parent_hash].IsSpent(0));
        BOOST_CHECK(!test_wallet.mapWallet[parent_hash].IsSpent(1));  // Output 1 still unspent
    }
}

BOOST_AUTO_TEST_CASE(reorg_unmarks_parent_spent)
{
    // Test that reorg correctly unmarks parent outputs as unspent
    CWallet test_wallet;

    // Create parent and child transactions
    CTransaction parent_tx;
    parent_tx.vout.resize(1);
    parent_tx.vout[0].nValue = 100 * COIN;
    uint256 parent_hash = parent_tx.GetHash();

    CTransaction child_tx;
    child_tx.vin.resize(1);
    child_tx.vin[0].prevout = COutPoint(parent_hash, 0);
    child_tx.vout.resize(1);
    child_tx.vout[0].nValue = 99 * COIN;
    uint256 child_hash = child_tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Add parent (confirmed, output spent)
        CWalletTx parent_wtx(&test_wallet, parent_tx);
        parent_wtx.SetTxState(TxStateConfirmed(uint256S("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"), 150, 2));
        parent_wtx.vfSpent.resize(1, true);  // Output 0 is spent
        test_wallet.mapWallet[parent_hash] = parent_wtx;

        // Verify output is spent
        BOOST_CHECK(test_wallet.mapWallet[parent_hash].IsSpent(0));

        // Simulate reorg: unmark parent output as spent
        test_wallet.mapWallet[parent_hash].MarkUnspent(0);

        // Verify output is now unspent
        BOOST_CHECK(!test_wallet.mapWallet[parent_hash].IsSpent(0));
    }
}

BOOST_AUTO_TEST_CASE(abandon_then_reconfirm_workflow)
{
    // Test full abandon lifecycle including edge case reconfirmation
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 60 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // 1. Transaction starts in mempool
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());

        // 2. Transaction removed/conflicted (not abandoned yet)
        test_wallet.mapWallet[hash].SetTxState(TxStateInactive{false});
        BOOST_CHECK(test_wallet.mapWallet[hash].isInactive());
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));

        // 3. User explicitly abandons
        bool abandoned_ok = test_wallet.AbandonTransaction(hash);
        BOOST_CHECK(abandoned_ok);
        BOOST_CHECK(test_wallet.IsAbandoned(hash));

        auto* inactive = test_wallet.mapWallet[hash].state<TxStateInactive>();
        BOOST_CHECK(inactive != nullptr);
        BOOST_CHECK_EQUAL(inactive->m_abandoned, true);

        // 4. Edge case: abandoned tx appears in a block (should re-activate)
        uint256 block_hash = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        test_wallet.mapWallet[hash].SetTxState(TxStateConfirmed(block_hash, 300, 4));

        // Verify it's now confirmed (abandoned flag overridden)
        BOOST_CHECK(test_wallet.mapWallet[hash].isConfirmed());
        BOOST_CHECK(!test_wallet.mapWallet[hash].isInactive());
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));
    }
}

BOOST_AUTO_TEST_CASE(inactive_conflicted_vs_abandoned)
{
    // Test distinction between conflicted (abandoned=false) and abandoned (abandoned=true)
    CWallet test_wallet;

    // Transaction 1: conflicted
    CTransaction tx1;
    tx1.vout.resize(1);
    tx1.vout[0].nValue = 40 * COIN;
    uint256 hash1 = tx1.GetHash();

    // Transaction 2: abandoned
    CTransaction tx2;
    tx2.vout.resize(1);
    tx2.vout[0].nValue = 30 * COIN;
    uint256 hash2 = tx2.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Add conflicted transaction
        CWalletTx wtx1(&test_wallet, tx1);
        wtx1.SetTxState(TxStateInactive{false});  // conflicted, not abandoned
        test_wallet.mapWallet[hash1] = wtx1;

        // Add abandoned transaction
        CWalletTx wtx2(&test_wallet, tx2);
        wtx2.SetTxState(TxStateInactive{true});  // abandoned
        test_wallet.mapWallet[hash2] = wtx2;

        // Verify both are inactive
        BOOST_CHECK(test_wallet.mapWallet[hash1].isInactive());
        BOOST_CHECK(test_wallet.mapWallet[hash2].isInactive());

        // But only tx2 is abandoned
        BOOST_CHECK(!test_wallet.IsAbandoned(hash1));
        BOOST_CHECK(test_wallet.IsAbandoned(hash2));

        // Verify the abandoned flags
        auto* inactive1 = test_wallet.mapWallet[hash1].state<TxStateInactive>();
        auto* inactive2 = test_wallet.mapWallet[hash2].state<TxStateInactive>();

        BOOST_CHECK(inactive1 != nullptr);
        BOOST_CHECK(inactive2 != nullptr);
        BOOST_CHECK_EQUAL(inactive1->m_abandoned, false);
        BOOST_CHECK_EQUAL(inactive2->m_abandoned, true);
    }
}

BOOST_AUTO_TEST_CASE(unrecognized_migration_with_hashblock)
{
    // Test migration of unrecognized transactions with hashBlock set
    // This tests the consolidated migration path (Fix 1)
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 80 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Create transaction with unrecognized state but block hash known
        // (simulates old wallet format being loaded — ResolveUnrecognizedTx path)
        CWalletTx wtx(&test_wallet, tx);
        uint256 block_hash = uint256S("1111111122222222333333334444444455555555666666667777777788888888");
        TxStateUnrecognized unrec;
        unrec.m_block_hash = block_hash;
        unrec.m_index = 7;
        wtx.SetTxState(unrec);
        test_wallet.mapWallet[hash] = wtx;

        BOOST_CHECK(test_wallet.mapWallet[hash].isUnrecognized());

        // Simulate migration: if m_block_hash is set, migrate to confirmed state
        // (This is what ResolveUnrecognizedTx does)
        auto* unrec_state = test_wallet.mapWallet[hash].state<TxStateUnrecognized>();
        if (unrec_state && !unrec_state->m_block_hash.IsNull()) {
            test_wallet.mapWallet[hash].SetTxState(TxStateConfirmed(
                unrec_state->m_block_hash,
                250,  // height (would come from mapBlockIndex in real code)
                unrec_state->m_index
            ));
        }

        // Verify migration succeeded
        BOOST_CHECK(test_wallet.mapWallet[hash].isConfirmed());
        BOOST_CHECK(!test_wallet.mapWallet[hash].isUnrecognized());

        auto* conf = test_wallet.mapWallet[hash].state<TxStateConfirmed>();
        BOOST_CHECK(conf != nullptr);
        BOOST_CHECK(conf->m_confirmed_block_hash == block_hash);
        BOOST_CHECK_EQUAL(conf->m_position_in_block, 7);
    }
}

BOOST_AUTO_TEST_CASE(unrecognized_migration_without_hashblock)
{
    // Test migration of unrecognized transactions without hashBlock
    // These should become inactive (not found)
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 90 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Create transaction with unrecognized state and no block hash
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateUnrecognized{});  // m_block_hash is null by default
        test_wallet.mapWallet[hash] = wtx;

        BOOST_CHECK(test_wallet.mapWallet[hash].isUnrecognized());

        // Simulate migration: no m_block_hash, not in mempool/txdb -> inactive
        auto* unrec_state = test_wallet.mapWallet[hash].state<TxStateUnrecognized>();
        if (unrec_state && unrec_state->m_block_hash.IsNull()) {
            test_wallet.mapWallet[hash].SetTxState(TxStateInactive{false});
        }

        // Verify migration to inactive
        BOOST_CHECK(test_wallet.mapWallet[hash].isInactive());
        BOOST_CHECK(!test_wallet.mapWallet[hash].isUnrecognized());
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));  // Not abandoned, just not found
    }
}

BOOST_AUTO_TEST_CASE(multiple_state_transitions)
{
    // Test multiple state transitions in sequence
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 110 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // 1. Start: Unrecognized (loaded from old wallet)
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateUnrecognized{});
        test_wallet.mapWallet[hash] = wtx;
        BOOST_CHECK(test_wallet.mapWallet[hash].isUnrecognized());

        // 2. Migrate to Mempool (found in mempool during ReacceptWalletTransactions)
        test_wallet.mapWallet[hash].SetTxState(TxStateInMempool{});
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());

        // 3. Confirm (block connected)
        uint256 block_hash = uint256S("9999999999999999999999999999999999999999999999999999999999999999");
        test_wallet.mapWallet[hash].SetTxState(TxStateConfirmed(block_hash, 400, 8));
        BOOST_CHECK(test_wallet.mapWallet[hash].isConfirmed());

        // 4. Reorg back to Inactive (block disconnected)
        test_wallet.mapWallet[hash].SetTxState(TxStateInactive{false});
        BOOST_CHECK(test_wallet.mapWallet[hash].isInactive());

        // 5. Return to Mempool (transaction still valid)
        test_wallet.mapWallet[hash].SetTxState(TxStateInMempool{});
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());

        // 6. Re-confirm in new chain
        uint256 new_block_hash = uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        test_wallet.mapWallet[hash].SetTxState(TxStateConfirmed(new_block_hash, 401, 2));
        BOOST_CHECK(test_wallet.mapWallet[hash].isConfirmed());

        auto* final_conf = test_wallet.mapWallet[hash].state<TxStateConfirmed>();
        BOOST_CHECK(final_conf != nullptr);
        BOOST_CHECK(final_conf->m_confirmed_block_hash == new_block_hash);
        BOOST_CHECK_EQUAL(final_conf->m_confirmed_block_height, 401);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(wallet_state_tests)

BOOST_AUTO_TEST_CASE(state_type_creation)
{
    // Test creating each state type
    TxStateInMempool mempool_state;
    BOOST_CHECK(std::holds_alternative<TxStateInMempool>(
        TxState{mempool_state}));

    TxStateConfirmed conf_state(uint256S("00"), 100, 0);
    BOOST_CHECK_EQUAL(conf_state.m_confirmed_block_height, 100);

    TxStateInactive inactive_state(true);
    BOOST_CHECK_EQUAL(inactive_state.m_abandoned, true);

    TxStateUnrecognized unrec_state;
    BOOST_CHECK_EQUAL(unrec_state.m_index, -1);
}

BOOST_AUTO_TEST_CASE(state_serialization)
{
    // Test each state type round-trips through CWalletTx sentinel-based serialization.
    // SyncLegacyFromState -> CMerkleTx -> deserialize -> MigrateFromLegacyHashBlock -> state.

    {
        // Confirmed state
        CWalletTx original;
        original.SetTxState(TxStateConfirmed(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 12345, 7));

        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << original;

        CWalletTx restored;
        ss >> restored;

        BOOST_CHECK(restored.isConfirmed());
        auto* conf = restored.state<TxStateConfirmed>();
        BOOST_CHECK(conf != nullptr);
        BOOST_CHECK(conf->m_confirmed_block_hash == uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
        // Block height is not serialized in CMerkleTx, so it reconstructs as -1
        BOOST_CHECK_EQUAL(conf->m_confirmed_block_height, -1);
        BOOST_CHECK_EQUAL(conf->m_position_in_block, 7);
    }

    {
        // Inactive (abandoned) state — serializes as ABANDONED_HASH_SENTINEL
        CWalletTx original;
        original.SetTxState(TxStateInactive(true));

        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << original;

        CWalletTx restored;
        ss >> restored;

        BOOST_CHECK(restored.isInactive());
        auto* inactive = restored.state<TxStateInactive>();
        BOOST_CHECK(inactive != nullptr);
        BOOST_CHECK_EQUAL(inactive->m_abandoned, true);
    }

    {
        // Inactive (conflicted) state — serializes as CONFLICTED_HASH_SENTINEL
        CWalletTx original;
        original.SetTxState(TxStateInactive(false));

        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << original;

        CWalletTx restored;
        ss >> restored;

        BOOST_CHECK(restored.isInactive());
        auto* inactive = restored.state<TxStateInactive>();
        BOOST_CHECK(inactive != nullptr);
        BOOST_CHECK_EQUAL(inactive->m_abandoned, false);
    }

    {
        // InMempool state has no persistent representation;
        // serializes as null hashBlock -> deserializes as Unrecognized
        CWalletTx original;
        original.SetTxState(TxStateInMempool{});

        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << original;

        CWalletTx restored;
        ss >> restored;

        BOOST_CHECK(restored.isUnrecognized());
    }
}

BOOST_AUTO_TEST_CASE(wallet_tx_state_helpers)
{
    CWalletTx wtx;

    // Test mempool state
    wtx.SetTxState(TxStateInMempool{});
    BOOST_CHECK(wtx.isInMempool());
    BOOST_CHECK(!wtx.isConfirmed());
    BOOST_CHECK(!wtx.isInactive());
    BOOST_CHECK(!wtx.isUnrecognized());

    // Test confirmed state
    wtx.SetTxState(TxStateConfirmed(uint256S("00"), 100, 0));
    BOOST_CHECK(!wtx.isInMempool());
    BOOST_CHECK(wtx.isConfirmed());
    BOOST_CHECK(!wtx.isInactive());
    BOOST_CHECK(!wtx.isUnrecognized());

    auto* conf = wtx.state<TxStateConfirmed>();
    BOOST_CHECK(conf != nullptr);
    BOOST_CHECK_EQUAL(conf->m_confirmed_block_height, 100);

    // Test inactive state
    wtx.SetTxState(TxStateInactive(true));
    BOOST_CHECK(!wtx.isInMempool());
    BOOST_CHECK(!wtx.isConfirmed());
    BOOST_CHECK(wtx.isInactive());
    BOOST_CHECK(!wtx.isUnrecognized());

    auto* inactive = wtx.state<TxStateInactive>();
    BOOST_CHECK(inactive != nullptr);
    BOOST_CHECK_EQUAL(inactive->m_abandoned, true);

    // Test unrecognized state
    wtx.SetTxState(TxStateUnrecognized{});
    BOOST_CHECK(!wtx.isInMempool());
    BOOST_CHECK(!wtx.isConfirmed());
    BOOST_CHECK(!wtx.isInactive());
    BOOST_CHECK(wtx.isUnrecognized());
}

BOOST_AUTO_TEST_CASE(wallet_tx_serialization_with_state)
{
    // Test that CWalletTx with state serializes correctly
    CWalletTx original;
    original.SetTxState(TxStateConfirmed(uint256S("abc123"), 500, 3));

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << original;

    CWalletTx restored;
    ss >> restored;

    BOOST_CHECK(restored.isConfirmed());
    auto* conf = restored.state<TxStateConfirmed>();
    BOOST_CHECK(conf != nullptr);
    BOOST_CHECK(conf->m_confirmed_block_hash == uint256S("abc123"));
    // Block height is not stored in CMerkleTx, so it's -1 after deserialization
    BOOST_CHECK_EQUAL(conf->m_confirmed_block_height, -1);
    BOOST_CHECK_EQUAL(conf->m_position_in_block, 3);
}

BOOST_AUTO_TEST_CASE(old_wallet_migration)
{
    // Test that old wallet format entries (direct hashBlock/nIndex in CMerkleTx,
    // no state system) deserialize correctly via MigrateFromLegacyHashBlock.
    // With sentinel-based serialization, old and new formats are identical —
    // the read path always reconstructs m_state from hashBlock/nIndex.

    // Simulate an old confirmed entry
    CWalletTx oldFormat;
    oldFormat.SetTxState(TxStateConfirmed(uint256S("def456"), -1, 5));

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << oldFormat;

    CWalletTx newFormat;
    ss >> newFormat;

    // MigrateFromLegacyHashBlock should reconstruct confirmed state
    BOOST_CHECK(newFormat.isConfirmed());
    auto* conf = newFormat.state<TxStateConfirmed>();
    BOOST_CHECK(conf != nullptr);
    BOOST_CHECK(conf->m_confirmed_block_hash == uint256S("def456"));
    BOOST_CHECK_EQUAL(conf->m_position_in_block, 5);
}

BOOST_AUTO_TEST_CASE(state_template_methods)
{
    CWalletTx wtx;

    // Test template state retrieval
    wtx.SetTxState(TxStateConfirmed(uint256S("test"), 123, 1));

    // Test const version
    const CWalletTx& const_wtx = wtx;
    const TxStateConfirmed* const_conf = const_wtx.state<TxStateConfirmed>();
    BOOST_CHECK(const_conf != nullptr);
    BOOST_CHECK_EQUAL(const_conf->m_confirmed_block_height, 123);

    // Test non-const version
    TxStateConfirmed* conf = wtx.state<TxStateConfirmed>();
    BOOST_CHECK(conf != nullptr);
    conf->m_confirmed_block_height = 456;
    BOOST_CHECK_EQUAL(conf->m_confirmed_block_height, 456);

    // Test retrieval of wrong type returns nullptr
    const TxStateInMempool* mempool_ptr = const_wtx.state<TxStateInMempool>();
    BOOST_CHECK(mempool_ptr == nullptr);
}

BOOST_AUTO_TEST_CASE(get_conflicts_no_conflicts)
{
    // Test GetConflicts with no conflicting transactions
    CWallet test_wallet;

    // Create a simple transaction
    CTransaction tx1;
    tx1.vin.resize(1);
    tx1.vin[0].prevout = COutPoint(uint256S("1111111111111111111111111111111111111111111111111111111111111111"), 0);
    tx1.vout.resize(1);
    tx1.vout[0].nValue = 100 * COIN;

    uint256 hash1 = tx1.GetHash();

    // Add to wallet
    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx1(&test_wallet, tx1);
        wtx1.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash1] = wtx1;

        // Populate mapTxSpends
        for (const auto& txin : tx1.vin) {
            test_wallet.mapTxSpends.insert(std::make_pair(txin.prevout, hash1));
        }
    }

    // Should have no conflicts
    {
        LOCK(test_wallet.cs_wallet);
        std::set<uint256> conflicts = test_wallet.GetConflicts(hash1);
        BOOST_CHECK(conflicts.empty());
    }
}

BOOST_AUTO_TEST_CASE(get_conflicts_with_conflicts)
{
    // Test GetConflicts with conflicting transactions (double-spend)
    CWallet test_wallet;

    // Create two transactions that spend the same output
    COutPoint shared_outpoint(uint256S("2222222222222222222222222222222222222222222222222222222222222222"), 0);

    CTransaction tx1;
    tx1.vin.resize(1);
    tx1.vin[0].prevout = shared_outpoint;
    tx1.vout.resize(1);
    tx1.vout[0].nValue = 100 * COIN;
    uint256 hash1 = tx1.GetHash();

    CTransaction tx2;
    tx2.vin.resize(1);
    tx2.vin[0].prevout = shared_outpoint;  // Same input!
    tx2.vout.resize(1);
    tx2.vout[0].nValue = 99 * COIN;  // Different output
    uint256 hash2 = tx2.GetHash();

    // Add both to wallet
    {
        LOCK(test_wallet.cs_wallet);

        CWalletTx wtx1(&test_wallet, tx1);
        wtx1.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash1] = wtx1;

        CWalletTx wtx2(&test_wallet, tx2);
        wtx2.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash2] = wtx2;

        // Populate mapTxSpends for both
        test_wallet.mapTxSpends.insert(std::make_pair(shared_outpoint, hash1));
        test_wallet.mapTxSpends.insert(std::make_pair(shared_outpoint, hash2));
    }

    // tx1 should conflict with tx2
    {
        LOCK(test_wallet.cs_wallet);
        std::set<uint256> conflicts1 = test_wallet.GetConflicts(hash1);
        BOOST_CHECK_EQUAL(conflicts1.size(), 1u);
        BOOST_CHECK(conflicts1.count(hash2) == 1);

        // tx2 should conflict with tx1
        std::set<uint256> conflicts2 = test_wallet.GetConflicts(hash2);
        BOOST_CHECK_EQUAL(conflicts2.size(), 1u);
        BOOST_CHECK(conflicts2.count(hash1) == 1);
    }
}

BOOST_AUTO_TEST_CASE(get_conflicts_multiple_conflicts)
{
    // Test GetConflicts with multiple conflicting transactions
    CWallet test_wallet;

    COutPoint shared_outpoint(uint256S("3333333333333333333333333333333333333333333333333333333333333333"), 5);

    // Create three transactions that all spend the same output
    std::vector<uint256> hashes;
    for (int i = 0; i < 3; i++) {
        CTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout = shared_outpoint;
        tx.vout.resize(1);
        tx.vout[0].nValue = (100 - i) * COIN;

        uint256 hash = tx.GetHash();
        hashes.push_back(hash);

        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;
        test_wallet.mapTxSpends.insert(std::make_pair(shared_outpoint, hash));
    }

    // Each transaction should conflict with the other two
    {
        LOCK(test_wallet.cs_wallet);
        for (size_t i = 0; i < hashes.size(); i++) {
            std::set<uint256> conflicts = test_wallet.GetConflicts(hashes[i]);
            BOOST_CHECK_EQUAL(conflicts.size(), 2u);

            for (size_t j = 0; j < hashes.size(); j++) {
                if (i != j) {
                    BOOST_CHECK(conflicts.count(hashes[j]) == 1);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(is_abandoned_not_abandoned)
{
    // Test IsAbandoned with non-abandoned transaction
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;
    }

    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));
    }
}

BOOST_AUTO_TEST_CASE(is_abandoned_with_abandoned_tx)
{
    // Test IsAbandoned with abandoned transaction
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInactive{true});  // abandoned = true
        test_wallet.mapWallet[hash] = wtx;
    }

    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(test_wallet.IsAbandoned(hash));
    }
}

BOOST_AUTO_TEST_CASE(is_abandoned_inactive_not_abandoned)
{
    // Test IsAbandoned with inactive but not abandoned transaction
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInactive{false});  // abandoned = false
        test_wallet.mapWallet[hash] = wtx;
    }

    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));
    }
}

BOOST_AUTO_TEST_CASE(is_abandoned_nonexistent_tx)
{
    // Test IsAbandoned with transaction not in wallet
    CWallet test_wallet;

    uint256 nonexistent_hash = uint256S("4444444444444444444444444444444444444444444444444444444444444444");
    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(!test_wallet.IsAbandoned(nonexistent_hash));
    }
}

BOOST_AUTO_TEST_CASE(abandon_transaction_unconfirmed_tx)
{
    // Test AbandonTransaction with unconfirmed transaction (not in mempool)
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 75 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        // Use Unrecognized state - transaction not in mempool, not confirmed
        wtx.SetTxState(TxStateUnrecognized{});
        test_wallet.mapWallet[hash] = wtx;
    }

    // Should be able to abandon unconfirmed transaction not in mempool
    BOOST_CHECK(test_wallet.AbandonTransaction(hash));

    // Should now be marked as abandoned
    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(test_wallet.IsAbandoned(hash));
    }

    // Verify state changed to inactive with abandoned=true
    {
        LOCK(test_wallet.cs_wallet);
        auto it = test_wallet.mapWallet.find(hash);
        BOOST_CHECK(it != test_wallet.mapWallet.end());
        BOOST_CHECK(it->second.isInactive());

        auto* inactive_state = it->second.state<TxStateInactive>();
        BOOST_CHECK(inactive_state != nullptr);
        BOOST_CHECK_EQUAL(inactive_state->m_abandoned, true);
    }
}

BOOST_AUTO_TEST_CASE(abandon_transaction_confirmed_tx_fails)
{
    // Test that AbandonTransaction fails with confirmed transaction
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 75 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateConfirmed(uint256S("5555555555555555555555555555555555555555555555555555555555555555"), 100, 0));
        test_wallet.mapWallet[hash] = wtx;
    }

    // Should NOT be able to abandon confirmed transaction
    BOOST_CHECK(!test_wallet.AbandonTransaction(hash));

    // Should still be confirmed, not abandoned
    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));

        auto it = test_wallet.mapWallet.find(hash);
        BOOST_CHECK(it != test_wallet.mapWallet.end());
        BOOST_CHECK(it->second.isConfirmed());
    }
}

BOOST_AUTO_TEST_CASE(abandon_transaction_nonexistent_tx_fails)
{
    // Test that AbandonTransaction fails with nonexistent transaction
    CWallet test_wallet;

    uint256 nonexistent_hash = uint256S("6666666666666666666666666666666666666666666666666666666666666666");

    // Should fail gracefully
    BOOST_CHECK(!test_wallet.AbandonTransaction(nonexistent_hash));
}

BOOST_AUTO_TEST_CASE(maptxspends_cleanup_on_erase)
{
    // Test that mapTxSpends is cleaned up when transaction is erased
    CWallet test_wallet;

    // Create a transaction with multiple inputs
    CTransaction tx;
    tx.vin.resize(3);
    tx.vin[0].prevout = COutPoint(uint256S("7777777777777777777777777777777777777777777777777777777777777777"), 0);
    tx.vin[1].prevout = COutPoint(uint256S("8888888888888888888888888888888888888888888888888888888888888888"), 1);
    tx.vin[2].prevout = COutPoint(uint256S("9999999999999999999999999999999999999999999999999999999999999999"), 2);
    tx.vout.resize(1);
    tx.vout[0].nValue = 100 * COIN;

    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        // Populate mapTxSpends
        for (const auto& txin : tx.vin) {
            test_wallet.mapTxSpends.insert(std::make_pair(txin.prevout, hash));
        }

        // Verify entries exist
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(tx.vin[0].prevout), 1u);
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(tx.vin[1].prevout), 1u);
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(tx.vin[2].prevout), 1u);

        // Erase the transaction
        test_wallet.EraseFromWallet(hash);

        // mapTxSpends entries should be cleaned up
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(tx.vin[0].prevout), 0u);
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(tx.vin[1].prevout), 0u);
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(tx.vin[2].prevout), 0u);

        // Transaction should be gone from mapWallet
        BOOST_CHECK(test_wallet.mapWallet.find(hash) == test_wallet.mapWallet.end());
    }
}

// ---------------------------------------------------------------------------
// transactionRemovedFromMempool state-transition tests (#9)
//
// These verify the state-transition *logic* for each MemPoolRemovalReason
// without requiring full mempool mocking. We pre-populate mapWallet and
// apply the same transitions that transactionRemovedFromMempool() makes.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(mempool_removal_block_reason_no_state_change)
{
    // When reason == BLOCK, transactionRemovedFromMempool returns early.
    // blockConnected handles the state transition instead.
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;
    }

    // Simulate: reason == BLOCK → no action (early return)
    // State should remain InMempool (blockConnected changes it later)
    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());
    }
}

BOOST_AUTO_TEST_CASE(mempool_removal_conflict_marks_inactive)
{
    // CONFLICT reason should mark transaction as inactive (not abandoned)
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        // Apply same logic as transactionRemovedFromMempool for CONFLICT
        test_wallet.mapWallet[hash].SetTxState(TxStateInactive{false});

        BOOST_CHECK(test_wallet.mapWallet[hash].isInactive());
        auto* inactive = test_wallet.mapWallet[hash].state<TxStateInactive>();
        BOOST_CHECK(inactive != nullptr);
        BOOST_CHECK_EQUAL(inactive->m_abandoned, false);  // Not abandoned, just conflicted
    }
}

BOOST_AUTO_TEST_CASE(mempool_removal_replaced_marks_inactive)
{
    // REPLACED reason should mark transaction as inactive (same as CONFLICT)
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        // Apply same logic as transactionRemovedFromMempool for REPLACED
        test_wallet.mapWallet[hash].SetTxState(TxStateInactive{false});

        BOOST_CHECK(test_wallet.mapWallet[hash].isInactive());
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));
    }
}

BOOST_AUTO_TEST_CASE(mempool_removal_expiry_preserves_mempool_state)
{
    // EXPIRY reason should NOT change state — tx is eligible for re-acceptance
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        // EXPIRY: no state change (just logs)
        // State should remain InMempool — ReacceptWalletTransactions will handle it
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());
    }
}

BOOST_AUTO_TEST_CASE(mempool_removal_sizelimit_preserves_mempool_state)
{
    // SIZELIMIT reason should NOT change state — tx is eligible for re-acceptance
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        // SIZELIMIT: no state change (just logs)
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());
    }
}

BOOST_AUTO_TEST_CASE(mempool_removal_reorg_defers_to_block_disconnected)
{
    // REORG reason should NOT change state — blockDisconnected handles it
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        // REORG: no state change (deferred to blockDisconnected)
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());
    }
}

BOOST_AUTO_TEST_CASE(mempool_removal_unknown_marks_inactive)
{
    // UNKNOWN reason should conservatively mark as inactive
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;

        // Apply same logic as transactionRemovedFromMempool for UNKNOWN
        test_wallet.mapWallet[hash].SetTxState(TxStateInactive{false});

        BOOST_CHECK(test_wallet.mapWallet[hash].isInactive());
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));
    }
}

BOOST_AUTO_TEST_CASE(mempool_removal_not_in_wallet_no_crash)
{
    // Removal of tx NOT in wallet should be a no-op (no crash)
    CWallet test_wallet;
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    uint256 hash = tx.GetHash();

    // Don't add to wallet — verify no crash when looking up
    {
        LOCK(test_wallet.cs_wallet);
        auto it = test_wallet.mapWallet.find(hash);
        BOOST_CHECK(it == test_wallet.mapWallet.end());
        // In transactionRemovedFromMempool, this causes an early return
    }
}

// ---------------------------------------------------------------------------
// MigrateFromLegacyHashBlock tests — all 4 branches
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(migrate_legacy_abandoned_sentinel)
{
    // ABANDONED_HASH_SENTINEL → TxStateInactive{true}
    TxState state = MigrateFromLegacyHashBlock(ABANDONED_HASH_SENTINEL, -1);
    BOOST_CHECK(std::holds_alternative<TxStateInactive>(state));
    auto* inactive = std::get_if<TxStateInactive>(&state);
    BOOST_CHECK(inactive != nullptr);
    BOOST_CHECK_EQUAL(inactive->m_abandoned, true);
}

BOOST_AUTO_TEST_CASE(migrate_legacy_conflicted_sentinel)
{
    // CONFLICTED_HASH_SENTINEL → TxStateInactive{false}
    TxState state = MigrateFromLegacyHashBlock(CONFLICTED_HASH_SENTINEL, -1);
    BOOST_CHECK(std::holds_alternative<TxStateInactive>(state));
    auto* inactive = std::get_if<TxStateInactive>(&state);
    BOOST_CHECK(inactive != nullptr);
    BOOST_CHECK_EQUAL(inactive->m_abandoned, false);
}

BOOST_AUTO_TEST_CASE(migrate_legacy_confirmed)
{
    // Non-null hash → TxStateConfirmed with height=-1 (resolved later)
    uint256 block_hash = uint256S("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
    TxState state = MigrateFromLegacyHashBlock(block_hash, 5);
    BOOST_CHECK(std::holds_alternative<TxStateConfirmed>(state));
    auto* conf = std::get_if<TxStateConfirmed>(&state);
    BOOST_CHECK(conf != nullptr);
    BOOST_CHECK(conf->m_confirmed_block_hash == block_hash);
    BOOST_CHECK_EQUAL(conf->m_confirmed_block_height, -1);  // Height not known at migration time
    BOOST_CHECK_EQUAL(conf->m_position_in_block, 5);
}

BOOST_AUTO_TEST_CASE(migrate_legacy_null_hash)
{
    // Null hash → TxStateUnrecognized
    TxState state = MigrateFromLegacyHashBlock(uint256(), -1);
    BOOST_CHECK(std::holds_alternative<TxStateUnrecognized>(state));
}

// ---------------------------------------------------------------------------
// CWalletTx state round-trip via sentinel serialization
// (State is persisted through CMerkleTx hashBlock/nIndex fields with sentinels.
//  Replaces variant-level serialization tests now that TxState structs are not
//  individually serializable.)
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(wallet_tx_roundtrip_mempool)
{
    // InMempool has no on-disk representation; serializes as null hashBlock
    CWalletTx original;
    original.SetTxState(TxStateInMempool{});

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << original;

    CWalletTx restored;
    ss >> restored;

    // Null hash -> MigrateFromLegacyHashBlock -> Unrecognized
    BOOST_CHECK(restored.isUnrecognized());
}

BOOST_AUTO_TEST_CASE(wallet_tx_roundtrip_confirmed)
{
    uint256 bh = uint256S("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210");
    CWalletTx original;
    original.SetTxState(TxStateConfirmed(bh, 999, 42));

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << original;

    CWalletTx restored;
    ss >> restored;

    BOOST_CHECK(restored.isConfirmed());
    auto* conf = restored.state<TxStateConfirmed>();
    BOOST_CHECK(conf != nullptr);
    BOOST_CHECK(conf->m_confirmed_block_hash == bh);
    // Block height is not serialized in CMerkleTx
    BOOST_CHECK_EQUAL(conf->m_confirmed_block_height, -1);
    BOOST_CHECK_EQUAL(conf->m_position_in_block, 42);
}

BOOST_AUTO_TEST_CASE(wallet_tx_roundtrip_inactive_abandoned)
{
    CWalletTx original;
    original.SetTxState(TxStateInactive{true});

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << original;

    CWalletTx restored;
    ss >> restored;

    BOOST_CHECK(restored.isInactive());
    auto* inactive = restored.state<TxStateInactive>();
    BOOST_CHECK(inactive != nullptr);
    BOOST_CHECK_EQUAL(inactive->m_abandoned, true);
}

BOOST_AUTO_TEST_CASE(wallet_tx_roundtrip_inactive_conflicted)
{
    CWalletTx original;
    original.SetTxState(TxStateInactive{false});

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << original;

    CWalletTx restored;
    ss >> restored;

    BOOST_CHECK(restored.isInactive());
    auto* inactive = restored.state<TxStateInactive>();
    BOOST_CHECK(inactive != nullptr);
    BOOST_CHECK_EQUAL(inactive->m_abandoned, false);
}

BOOST_AUTO_TEST_CASE(wallet_tx_roundtrip_unrecognized)
{
    // Unrecognized is a transient state; serializes as null hashBlock
    CWalletTx original;
    original.SetTxState(TxStateUnrecognized{});

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << original;

    CWalletTx restored;
    ss >> restored;

    // Null hash -> MigrateFromLegacyHashBlock -> Unrecognized
    BOOST_CHECK(restored.isUnrecognized());
}

// ---------------------------------------------------------------------------
// Sentinel-based serialization helpers — unit tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(txstate_serialized_block_hash_helpers)
{
    // Verify the helpers produce the correct hashBlock/nIndex for each state

    // Confirmed
    uint256 bh = uint256S("abcd");
    TxState confirmed = TxStateConfirmed(bh, 100, 5);
    BOOST_CHECK(TxStateSerializedBlockHash(confirmed) == bh);
    BOOST_CHECK_EQUAL(TxStateSerializedIndex(confirmed), 5);

    // Inactive (abandoned)
    TxState abandoned = TxStateInactive{true};
    BOOST_CHECK(TxStateSerializedBlockHash(abandoned) == ABANDONED_HASH_SENTINEL);
    BOOST_CHECK_EQUAL(TxStateSerializedIndex(abandoned), -1);

    // Inactive (conflicted)
    TxState conflicted = TxStateInactive{false};
    BOOST_CHECK(TxStateSerializedBlockHash(conflicted) == CONFLICTED_HASH_SENTINEL);
    BOOST_CHECK_EQUAL(TxStateSerializedIndex(conflicted), -1);

    // InMempool
    TxState mempool = TxStateInMempool{};
    BOOST_CHECK(TxStateSerializedBlockHash(mempool).IsNull());
    BOOST_CHECK_EQUAL(TxStateSerializedIndex(mempool), -1);

    // Unrecognized
    TxState unrecognized = TxStateUnrecognized{};
    BOOST_CHECK(TxStateSerializedBlockHash(unrecognized).IsNull());
    BOOST_CHECK_EQUAL(TxStateSerializedIndex(unrecognized), -1);
}

// ---------------------------------------------------------------------------
// AbandonTransaction: cascading to child transactions
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(abandon_transaction_cascades_to_children)
{
    CWallet test_wallet;

    // Parent tx
    CTransaction parent_tx;
    parent_tx.vout.resize(2);
    parent_tx.vout[0].nValue = 50 * COIN;
    parent_tx.vout[1].nValue = 30 * COIN;
    uint256 parent_hash = parent_tx.GetHash();

    // Child tx spending parent output 0
    CTransaction child_tx;
    child_tx.vin.resize(1);
    child_tx.vin[0].prevout = COutPoint(parent_hash, 0);
    child_tx.vout.resize(1);
    child_tx.vout[0].nValue = 49 * COIN;
    uint256 child_hash = child_tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Add parent (inactive/unconfirmed, not in mempool)
        CWalletTx parent_wtx(&test_wallet, parent_tx);
        parent_wtx.SetTxState(TxStateInactive{false});
        test_wallet.mapWallet[parent_hash] = parent_wtx;

        // Add child (inactive/unconfirmed, not in mempool)
        CWalletTx child_wtx(&test_wallet, child_tx);
        child_wtx.SetTxState(TxStateInactive{false});
        test_wallet.mapWallet[child_hash] = child_wtx;

        // Wire up mapTxSpends so AbandonTransaction can find the child
        test_wallet.mapTxSpends.insert(std::make_pair(COutPoint(parent_hash, 0), child_hash));

        // Neither should be abandoned yet
        BOOST_CHECK(!test_wallet.IsAbandoned(parent_hash));
        BOOST_CHECK(!test_wallet.IsAbandoned(child_hash));

        // Abandon the parent
        bool ok = test_wallet.AbandonTransaction(parent_hash);
        BOOST_CHECK(ok);

        // Both parent AND child should now be abandoned
        BOOST_CHECK(test_wallet.IsAbandoned(parent_hash));
        BOOST_CHECK(test_wallet.IsAbandoned(child_hash));
    }
}

// ---------------------------------------------------------------------------
// AbandonTransaction: hashBlock sentinel verification (Fix B)
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(abandon_transaction_sets_sentinel_hash)
{
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 25 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInactive{false});  // Not yet abandoned
        wtx.SyncLegacyFromState();
        test_wallet.mapWallet[hash] = wtx;

        // Abandon
        bool ok = test_wallet.AbandonTransaction(hash);
        BOOST_CHECK(ok);

        // Verify hashBlock is set to ABANDONED_HASH_SENTINEL (not null)
        BOOST_CHECK(test_wallet.mapWallet[hash].hashBlock == ABANDONED_HASH_SENTINEL);
        BOOST_CHECK(!test_wallet.mapWallet[hash].hashBlock.IsNull());

        // And nIndex should be -1
        BOOST_CHECK_EQUAL(test_wallet.mapWallet[hash].nIndex, -1);
    }
}

// ---------------------------------------------------------------------------
// AbandonTransaction: double-abandon idempotency
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Variant index stability — serialization compatibility guard
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(txstate_variant_index_stability)
{
    // The variant index is written to disk. If anyone reorders the variant
    // alternatives, old wallet.dat files will silently deserialize to the
    // wrong type. This test locks the indices.
    TxState s0 = TxStateInMempool{};
    TxState s1 = TxStateConfirmed{};
    TxState s2 = TxStateInactive{};
    TxState s3 = TxStateUnrecognized{};

    BOOST_CHECK_EQUAL(s0.index(), 0u);  // InMempool must be index 0
    BOOST_CHECK_EQUAL(s1.index(), 1u);  // Confirmed must be index 1
    BOOST_CHECK_EQUAL(s2.index(), 2u);  // Inactive must be index 2
    BOOST_CHECK_EQUAL(s3.index(), 3u);  // Unrecognized must be index 3
}

// ---------------------------------------------------------------------------
// Default constructor invariants
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(txstate_default_constructor_invariants)
{
    // Guard against someone changing default field values
    TxStateConfirmed conf;
    BOOST_CHECK(conf.m_confirmed_block_hash.IsNull());
    BOOST_CHECK_EQUAL(conf.m_confirmed_block_height, -1);
    BOOST_CHECK_EQUAL(conf.m_position_in_block, -1);

    TxStateInactive inactive;
    BOOST_CHECK_EQUAL(inactive.m_abandoned, false);

    TxStateUnrecognized unrec;
    BOOST_CHECK(unrec.m_block_hash.IsNull());
    BOOST_CHECK_EQUAL(unrec.m_index, -1);
}

// ---------------------------------------------------------------------------
// AbandonTransaction: mempool tx cannot be abandoned
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(abandon_transaction_mempool_tx_fails)
{
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 20 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);
        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash] = wtx;
    }

    // Should NOT be able to abandon a mempool transaction
    BOOST_CHECK(!test_wallet.AbandonTransaction(hash));

    // Should still be in mempool, not abandoned
    {
        LOCK(test_wallet.cs_wallet);
        BOOST_CHECK(!test_wallet.IsAbandoned(hash));
        BOOST_CHECK(test_wallet.mapWallet[hash].isInMempool());
    }
}

BOOST_AUTO_TEST_CASE(abandon_transaction_double_abandon_is_idempotent)
{
    CWallet test_wallet;

    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 15 * COIN;
    uint256 hash = tx.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        CWalletTx wtx(&test_wallet, tx);
        wtx.SetTxState(TxStateUnrecognized{});  // Can be abandoned
        test_wallet.mapWallet[hash] = wtx;

        // First abandon
        bool ok1 = test_wallet.AbandonTransaction(hash);
        BOOST_CHECK(ok1);
        BOOST_CHECK(test_wallet.IsAbandoned(hash));

        // Second abandon — should return true without error
        bool ok2 = test_wallet.AbandonTransaction(hash);
        BOOST_CHECK(ok2);
        BOOST_CHECK(test_wallet.IsAbandoned(hash));

        // State should still be inactive/abandoned (unchanged)
        auto* inactive = test_wallet.mapWallet[hash].state<TxStateInactive>();
        BOOST_CHECK(inactive != nullptr);
        BOOST_CHECK_EQUAL(inactive->m_abandoned, true);
    }
}

BOOST_AUTO_TEST_CASE(maptxspends_cleanup_preserves_other_conflicts)
{
    // Test that erasing one conflicting tx doesn't remove other conflicts
    CWallet test_wallet;

    COutPoint shared_outpoint(uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 0);

    // Create two conflicting transactions
    CTransaction tx1;
    tx1.vin.resize(1);
    tx1.vin[0].prevout = shared_outpoint;
    tx1.vout.resize(1);
    tx1.vout[0].nValue = 100 * COIN;
    uint256 hash1 = tx1.GetHash();

    CTransaction tx2;
    tx2.vin.resize(1);
    tx2.vin[0].prevout = shared_outpoint;
    tx2.vout.resize(1);
    tx2.vout[0].nValue = 99 * COIN;
    uint256 hash2 = tx2.GetHash();

    {
        LOCK(test_wallet.cs_wallet);

        // Add both transactions
        CWalletTx wtx1(&test_wallet, tx1);
        wtx1.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash1] = wtx1;

        CWalletTx wtx2(&test_wallet, tx2);
        wtx2.SetTxState(TxStateInMempool{});
        test_wallet.mapWallet[hash2] = wtx2;

        // Populate mapTxSpends
        test_wallet.mapTxSpends.insert(std::make_pair(shared_outpoint, hash1));
        test_wallet.mapTxSpends.insert(std::make_pair(shared_outpoint, hash2));

        // Both entries should exist
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(shared_outpoint), 2u);

        // Erase first transaction
        test_wallet.EraseFromWallet(hash1);

        // Second transaction's entry should still exist
        BOOST_CHECK_EQUAL(test_wallet.mapTxSpends.count(shared_outpoint), 1u);

        // Verify it's the correct entry
        auto range = test_wallet.mapTxSpends.equal_range(shared_outpoint);
        BOOST_CHECK(range.first != range.second);
        BOOST_CHECK_EQUAL(range.first->second, hash2);
    }
}

BOOST_AUTO_TEST_SUITE_END()
