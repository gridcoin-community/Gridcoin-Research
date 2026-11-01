// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_BITCOINGUITESTS_H
#define BITCOIN_QT_TEST_BITCOINGUITESTS_H

#include <QObject>
#include <QTest>

class BitcoinGUITests : public QObject
{
    Q_OBJECT

private slots:
    void stakingToolTipUnmaskedShowsWeightAndFrequency();
    void stakingToolTipMaskedHidesWeightAndFrequency();
    void notStakingToolTipMaskedHidesFrequency();
    void unableToStakeToolTipIgnoresMask();
};

#endif // BITCOIN_QT_TEST_BITCOINGUITESTS_H
