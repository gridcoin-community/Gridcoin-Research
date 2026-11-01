// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/bitcoinguitests.h"

#include "qt/bitcoingui.h"

#include <QString>

namespace {
// Neither digit string occurs inside the other, so a check for one cannot be
// satisfied by the other.
constexpr double COIN_WEIGHT = 4321.0;
constexpr double NET_WEIGHT = 98765000.0;

// One week: 52.14 stakes per year, shown as "4.35 times per month".
constexpr unsigned int ETTS_SECONDS = 7 * 24 * 3600;
const QString FREQUENCY = QStringLiteral("4.35 times per month");

const QString MASK = QStringLiteral("#.##");
const QString WARNINGS = QStringLiteral("No mature coins");
} // anonymous namespace

void BitcoinGUITests::stakingToolTipUnmaskedShowsWeightAndFrequency()
{
    const QString tip = BitcoinGUI::StakingToolTip(true, NET_WEIGHT, COIN_WEIGHT, ETTS_SECONDS, false, WARNINGS);

    QVERIFY2(tip.contains("4321"), qPrintable(tip));
    QVERIFY2(tip.contains("98765000"), qPrintable(tip));
    QVERIFY2(tip.contains(FREQUENCY), qPrintable(tip));
    QCOMPARE(tip.count(MASK), 0);

    const QString not_staking = BitcoinGUI::StakingToolTip(false, NET_WEIGHT, COIN_WEIGHT, ETTS_SECONDS, false, WARNINGS);

    QVERIFY2(not_staking.contains(WARNINGS), qPrintable(not_staking));
    QVERIFY2(not_staking.contains(FREQUENCY), qPrintable(not_staking));
    QCOMPARE(not_staking.count(MASK), 0);
}

void BitcoinGUITests::stakingToolTipMaskedHidesWeightAndFrequency()
{
    const QString tip = BitcoinGUI::StakingToolTip(true, NET_WEIGHT, COIN_WEIGHT, ETTS_SECONDS, true, WARNINGS);

    QVERIFY2(!tip.contains("4321"), qPrintable(tip));
    QVERIFY2(!tip.contains("4.35"), qPrintable(tip));
    QVERIFY2(!tip.contains("times per"), qPrintable(tip));
    QVERIFY2(tip.contains("98765000"), qPrintable(tip));
    QCOMPARE(tip.count(MASK), 2);
}

void BitcoinGUITests::notStakingToolTipMaskedHidesFrequency()
{
    const QString tip = BitcoinGUI::StakingToolTip(false, NET_WEIGHT, COIN_WEIGHT, ETTS_SECONDS, true, WARNINGS);

    QVERIFY2(!tip.contains("4.35"), qPrintable(tip));
    QVERIFY2(!tip.contains("times per"), qPrintable(tip));
    QVERIFY2(tip.contains(WARNINGS), qPrintable(tip));
    QCOMPARE(tip.count(MASK), 1);
}

void BitcoinGUITests::unableToStakeToolTipIgnoresMask()
{
    const QString unmasked = BitcoinGUI::StakingToolTip(false, NET_WEIGHT, 0.0, ETTS_SECONDS, false, WARNINGS);
    const QString masked = BitcoinGUI::StakingToolTip(false, NET_WEIGHT, 0.0, ETTS_SECONDS, true, WARNINGS);

    QCOMPARE(unmasked, QStringLiteral("Unable to stake: No mature coins"));
    QCOMPARE(masked, unmasked);
}
