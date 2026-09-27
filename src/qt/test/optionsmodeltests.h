// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_OPTIONSMODELTESTS_H
#define BITCOIN_QT_TEST_OPTIONSMODELTESTS_H

#include <QObject>
#include <QTest>

//! OptionsModel driven by a fake interfaces::Node (qt_test::FakeNode). The
//! Options dialog's mapper submits every mapped field on OK, edited or not, so
//! these tests pin what a submit of the reserve balance writes through
//! changeSettings, the only path by which OptionsModel::setData's ReserveBalance
//! case writes gridcoinsettings.json.
class OptionsModelTests : public QObject
{
    Q_OBJECT

private slots:
    void unchangedReserveWritesNothing();
    void changedReserveIsStored();
};

#endif // BITCOIN_QT_TEST_OPTIONSMODELTESTS_H
