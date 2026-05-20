// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_WALLET_EVENT_QUEUE_TESTS_H
#define BITCOIN_QT_TEST_WALLET_EVENT_QUEUE_TESTS_H

#include <QObject>
#include <QTest>

class WalletEventQueueTests : public QObject
{
    Q_OBJECT

private slots:
    void emptyDrainIsEmpty();
    void singlePushDrainsOne();
    void seqnoMonotonicWithinSingleProducer();
    void allPayloadVariantsRoundTrip();
    void drainPartialBatch();
    void multiProducerSeqnosAreUniqueAndDense();
};

#endif // BITCOIN_QT_TEST_WALLET_EVENT_QUEUE_TESTS_H
