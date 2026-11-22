// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/tablemodelindextests.h"

#include "qt/researcher/projecttablemodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/test/researcherfixture.h"
#include "qt/voting/additionalfieldstablemodel.h"
#include "qt/voting/polltablemodel.h"
#include "qt/voting/votingmodel.h"

#include <QAbstractItemModel>
#include <QModelIndex>

// QModelIndex::isValid() is already false for a negative row or column, so
// those cases check the pointer the index carries instead: it must be null,
// not an address computed from the model's rows.

void TableModelIndexTests::pollTableRefusesAnIndexOutsideItsRows()
{
    PollTableDataModel m;
    m.reload({PollItem{}});

    QVERIFY(m.index(0, 0, {}).isValid());
    QVERIFY(!m.index(1, 0, {}).isValid());
    QVERIFY(!m.index(0, m.columnCount({}), {}).isValid());
    QCOMPARE(m.index(-1, 0, {}).internalPointer(), nullptr);
    QCOMPARE(m.index(0, -1, {}).internalPointer(), nullptr);
    QVERIFY(!m.index(0, 0, m.index(0, 0, {})).isValid());
}

void TableModelIndexTests::additionalFieldsTableRefusesAnIndexOutsideItsRows()
{
    // setPollItem() before refresh(): the constructor does not set the poll
    // item, and refresh() reads it.
    PollItem p;
    AdditionalFieldsTableModel m;
    m.setPollItem(&p);
    m.refresh();
    QVERIFY(!m.sourceModel()->index(0, 0, {}).isValid());

    PollItem q;
    q.m_additional_field_entries.emplace_back(QStringLiteral("n"), QStringLiteral("v"), false);
    AdditionalFieldsTableModel n;
    n.setPollItem(&q);
    n.refresh();
    const QAbstractItemModel* s = n.sourceModel();
    QVERIFY(s->index(0, 0, {}).isValid());
    QVERIFY(n.rowItem(0) != nullptr);
    QVERIFY(!s->index(1, 0, {}).isValid());
    QVERIFY(!s->index(0, s->columnCount({}), {}).isValid());
    QCOMPARE(s->index(-1, 0, {}).internalPointer(), nullptr);
    QCOMPARE(s->index(0, -1, {}).internalPointer(), nullptr);
    QVERIFY(!s->index(0, 0, s->index(0, 0, {})).isValid());
}

void TableModelIndexTests::projectTableRefusesAnIndexOutsideItsRows()
{
    ProjectTableModel e(nullptr, true);
    QVERIFY(!e.index(0, 0, {}).isValid());

    qt_test::ResearcherFixture f;
    f.m_context.m_projects = {interfaces::ResearcherProjectRow{}};
    ProjectTableModel m(&f.m_model, true);
    m.refresh();
    QCOMPARE(m.rowCount({}), 1);
    QVERIFY(m.index(0, 0, {}).isValid());
    QVERIFY(!m.index(1, 0, {}).isValid());
    QVERIFY(!m.index(0, m.columnCount({}), {}).isValid());
    QCOMPARE(m.index(-1, 0, {}).internalPointer(), nullptr);
    QCOMPARE(m.index(0, -1, {}).internalPointer(), nullptr);
    QVERIFY(!m.index(0, 0, m.index(0, 0, {})).isValid());
}
