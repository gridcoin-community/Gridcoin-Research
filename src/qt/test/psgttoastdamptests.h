// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_TEST_PSGTTOASTDAMPTESTS_H
#define GRIDCOIN_QT_TEST_PSGTTOASTDAMPTESTS_H

#include <QObject>
#include <QTest>

class PSGTToastDampTests : public QObject
{
    Q_OBJECT

private slots:
    void oneArrangementAnnouncesOnceAcrossItsRevisions();
    void separateArrangementsEachAnnounce();
    void anUnknownRevisionIsAnnouncedNotSwallowed();
    void removalForgetsOnlyTheEntriesThatLeft();
    void aResubmittedArrangementAnnouncesAgain();
};

#endif // GRIDCOIN_QT_TEST_PSGTTOASTDAMPTESTS_H
