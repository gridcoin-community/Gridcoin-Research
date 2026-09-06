// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_PSGTTOASTDAMPTESTS_H
#define BITCOIN_QT_TEST_PSGTTOASTDAMPTESTS_H

#include <QObject>
#include <QTest>

class PSGTToastDampTests : public QObject
{
    Q_OBJECT

private slots:
    void oneSpendAnnouncesOnceAcrossItsRevisions();
    void separateSpendsEachAnnounce();
    void anUnknownRevisionIsAnnouncedNotSwallowed();
    void removalForgetsOnlyTheEntriesThatLeft();
    void aResubmittedSpendAnnouncesAgain();
    void aSupersedeUnderTheSameImageAnnouncesAgain();
    void aFirstRevisionAnnouncesRegardlessOfDeliveryOrder();
};

#endif // BITCOIN_QT_TEST_PSGTTOASTDAMPTESTS_H
