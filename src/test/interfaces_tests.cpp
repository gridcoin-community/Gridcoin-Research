// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "clientversion.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/node.h"
#include "interfaces/wallet.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "util.h"
#include "wallet/wallet.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

extern CWallet* pwalletMain;

BOOST_AUTO_TEST_SUITE(interfaces_tests)

BOOST_AUTO_TEST_CASE(signal_handler_disconnects)
{
    boost::signals2::signal<void()> sig;
    int calls = 0;

    std::unique_ptr<interfaces::Handler> handler =
        interfaces::MakeSignalHandler(sig.connect([&] { ++calls; }));

    sig();
    BOOST_CHECK_EQUAL(calls, 1);

    handler->disconnect();
    sig();
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(signal_handler_disconnects_on_destruction)
{
    boost::signals2::signal<void()> sig;
    int calls = 0;

    {
        std::unique_ptr<interfaces::Handler> handler =
            interfaces::MakeSignalHandler(sig.connect([&] { ++calls; }));
        sig();
    }

    sig();
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(cleanup_handler_runs_exactly_once)
{
    int cleanups = 0;

    {
        std::unique_ptr<interfaces::Handler> handler =
            interfaces::MakeCleanupHandler([&] { ++cleanups; });
        handler->disconnect();
        BOOST_CHECK_EQUAL(cleanups, 1);
        // Destruction after an explicit disconnect must not run it again.
    }

    BOOST_CHECK_EQUAL(cleanups, 1);
}

BOOST_AUTO_TEST_CASE(cleanup_handler_runs_on_destruction)
{
    int cleanups = 0;

    {
        std::unique_ptr<interfaces::Handler> handler =
            interfaces::MakeCleanupHandler([&] { ++cleanups; });
        // No explicit disconnect: destruction alone must run the cleanup.
    }

    BOOST_CHECK_EQUAL(cleanups, 1);
}

BOOST_AUTO_TEST_CASE(node_wraps_chain_globals)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    BOOST_CHECK_EQUAL(node->getNumBlocks(), WITH_LOCK(cs_main, return nBestHeight));
    BOOST_CHECK(node->getBestBlockHash() == WITH_LOCK(cs_main, return hashBestChain));
    BOOST_CHECK_EQUAL(node->isTestNet(), fTestNet);
    BOOST_CHECK_EQUAL(node->getClientVersion(), FormatFullVersion());
    // No peers in the unit-test environment.
    BOOST_CHECK_EQUAL(node->getNodeCount(), 0);
}

BOOST_AUTO_TEST_CASE(node_handler_bridges_ui_signal)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();
    int calls = 0;

    std::unique_ptr<interfaces::Handler> handler =
        node->handleBannedListChanged([&] { ++calls; });

    uiInterface.BannedListChanged();
    BOOST_CHECK_EQUAL(calls, 1);

    handler->disconnect();
    uiInterface.BannedListChanged();
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(wallet_wraps_cwallet)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    BOOST_CHECK_EQUAL(wallet->getBalance(), pwalletMain->GetBalance());
    BOOST_CHECK_EQUAL(wallet->isCrypted(), pwalletMain->IsCrypted());
    BOOST_CHECK_EQUAL(wallet->isLocked(), pwalletMain->IsLocked());

    int calls = 0;
    uint256 seen_hash;
    std::unique_ptr<interfaces::Handler> handler = wallet->handleTransactionChanged(
        [&](const uint256& tx_hash, ChangeType status) {
            ++calls;
            seen_hash = tx_hash;
            BOOST_CHECK(status == CT_NEW);
        });

    const uint256 probe = uint256S("0xdeadbeef");
    pwalletMain->NotifyTransactionChanged(pwalletMain, probe, CT_NEW);
    BOOST_CHECK_EQUAL(calls, 1);
    BOOST_CHECK(seen_hash == probe);

    handler->disconnect();
    pwalletMain->NotifyTransactionChanged(pwalletMain, probe, CT_NEW);
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(init_hands_out_interfaces)
{
    std::unique_ptr<interfaces::Init> init = interfaces::MakeGridcoinInit();

    BOOST_REQUIRE(init != nullptr);
    BOOST_CHECK(init->makeNode() != nullptr);
    // pwalletMain exists in the unit-test environment, so the monolithic
    // Init must hand out a wallet interface for it.
    BOOST_CHECK(init->makeWallet() != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
