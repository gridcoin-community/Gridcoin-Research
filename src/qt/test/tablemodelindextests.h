// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_TABLEMODELINDEXTESTS_H
#define BITCOIN_QT_TEST_TABLEMODELINDEXTESTS_H

#include <QObject>
#include <QTest>

//! The poll, additional-fields and project table models hand out an index
//! only for a row and a column inside their own counts, and none under a
//! parent: none of the three has children.
class TableModelIndexTests : public QObject
{
    Q_OBJECT

private slots:
    void pollTableRefusesAnIndexOutsideItsRows();
    void additionalFieldsTableRefusesAnIndexOutsideItsRows();
    void projectTableRefusesAnIndexOutsideItsRows();
};

#endif // BITCOIN_QT_TEST_TABLEMODELINDEXTESTS_H
