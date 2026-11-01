// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/guiutiltests.h"

#include "qt/guiutil.h"

#include <QString>

void GUIUtilTests::extractFirstSuffixFromFilter_data()
{
    QTest::addColumn<QString>("filter");
    QTest::addColumn<QString>("expected");

    QTest::newRow("csv") << QString("Comma separated file (*.csv)") << QString("csv");
    QTest::newRow("png") << QString("PNG Image (*.png)") << QString("png");
    QTest::newRow("psbt") << QString("Partially Signed Transaction (Binary) (*.psbt)") << QString("psbt");
    QTest::newRow("multi") << QString("Images (*.png *.jpg)") << QString("png");
    QTest::newRow("all") << QString("All files (*)") << QString();
    QTest::newRow("empty") << QString() << QString();
}

void GUIUtilTests::extractFirstSuffixFromFilter()
{
    QFETCH(QString, filter);
    QFETCH(QString, expected);

    QCOMPARE(GUIUtil::ExtractFirstSuffixFromFilter(filter), expected);
}
