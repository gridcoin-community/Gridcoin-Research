// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_GUIFRONTENDTESTS_H
#define BITCOIN_QT_TEST_GUIFRONTENDTESTS_H

#include <QObject>
#include <QTest>

class GUIFrontEndTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void detachSequence();
    void guardRunsDetachOnThrow();
    void bridgeSlotSignatures();
};

#endif // BITCOIN_QT_TEST_GUIFRONTENDTESTS_H
