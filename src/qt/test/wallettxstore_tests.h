// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_WALLETTXSTORE_TESTS_H
#define BITCOIN_QT_TEST_WALLETTXSTORE_TESTS_H

#include <QObject>
#include <QTest>

//! Coverage for the PR2.5 store-worker / double-queue: producers enqueue
//! (O(1)) and a single worker thread drains the intake queue and runs the O(N)
//! store maintenance off the core locks, pushing the position-stamped events.
//! These exercise the worker's drain/ordering/concurrency and clean shutdown.
//! The rebuild barrier (reloadAndSnapshot quiescing the worker) needs a live
//! CWallet, so it is covered by the ASan-GUI mesh soak + isolated-testnet
//! validation, exactly as the PR2 store proper was.
class WalletTxStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void workerDrainsAllInsertsInOrder();
    void workerHandlesInterleavedInsertRemove();
    void workerPreservesAllUnderConcurrentProducers();
    void dtorWithPendingIntakeIsClean();
    //! getRowDetail (windowed-model PR5-C): the null-wallet-safe early-return
    //! paths — an unknown hash and a known hash with a non-matching idx both
    //! return an empty QString under cs_store BEFORE any wallet dereference. The
    //! matching-key -> toHTML path needs a live CWallet/cs_main and is covered by
    //! the GUI mesh soak.
    void getRowDetailUnknownHashReturnsEmpty();
    void getRowDetailWrongIdxReturnsEmpty();
};

#endif // BITCOIN_QT_TEST_WALLETTXSTORE_TESTS_H
