// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/sidestakemodeltests.h"

#include "interfaces/sidestake.h"
#include "qt/sidestaketablemodel.h"
#include "qt/test/interfacefakes.h"

#include <QModelIndex>
#include <QString>

#include <string>

namespace {
//! Build a value row the fake serves as if it came from the node side.
interfaces::SideStakeEntry MakeEntry(const std::string& address,
                                     double allocation_percent,
                                     const std::string& description,
                                     const std::string& status,
                                     bool mandatory)
{
    interfaces::SideStakeEntry entry;
    entry.address = address;
    entry.allocation_percent = allocation_percent;
    entry.description = description;
    entry.status = status;
    entry.is_mandatory = mandatory;
    return entry;
}

QVariant CellDisplay(const SideStakeTableModel& model, int row, SideStakeTableModel::ColumnIndex column)
{
    return model.data(model.index(row, column, QModelIndex()), Qt::DisplayRole);
}
} // anonymous namespace

void SideStakeModelTests::emptyUntilRefresh()
{
    qt_test::FakeSideStakeManager fake;
    fake.m_snapshot.entries.push_back(MakeEntry("addr1", 10.0, "d", "Active", false));

    SideStakeTableModel model(fake);

    // The model defers its first fetch until refresh() (the sidestake registry
    // and final chain params are not ready at construction), so it starts empty
    // and has not touched the interface.
    QCOMPARE(model.rowCount(QModelIndex()), 0);
    QCOMPARE(model.columnCount(QModelIndex()), 4);
    QCOMPARE(fake.m_entries_calls, 0);
}

void SideStakeModelTests::mapsSnapshotToTable()
{
    qt_test::FakeSideStakeManager fake;
    fake.m_snapshot.entries.push_back(MakeEntry("mandAddr", 25.0, "mand desc", "Mandatory", true));
    fake.m_snapshot.entries.push_back(MakeEntry("localAddr", 50.5, "local desc", "Active", false));

    SideStakeTableModel model(fake);
    model.refresh();

    QCOMPARE(model.rowCount(QModelIndex()), 2);

    // Row 0 (mandatory): every column maps from the interface value row.
    QCOMPARE(CellDisplay(model, 0, SideStakeTableModel::Address).toString(), QString("mandAddr"));
    QCOMPARE(CellDisplay(model, 0, SideStakeTableModel::Allocation).toString(), QString("25.00%"));
    QCOMPARE(CellDisplay(model, 0, SideStakeTableModel::Description).toString(), QString("mand desc"));
    QCOMPARE(CellDisplay(model, 0, SideStakeTableModel::Status).toString(), QString("Mandatory"));
    QVERIFY(model.isMandatory(0));

    // Row 1 (local): allocation is formatted to two decimals; not mandatory.
    QCOMPARE(CellDisplay(model, 1, SideStakeTableModel::Allocation).toString(), QString("50.50%"));
    QVERIFY(!model.isMandatory(1));

    // Out-of-range row is not mandatory (defensive path).
    QVERIFY(!model.isMandatory(5));
}

void SideStakeModelTests::addRowDelegatesToInterface()
{
    qt_test::FakeSideStakeManager fake;
    fake.m_edit_result.status = interfaces::SideStakeEditStatus::OK;
    fake.m_edit_result.address = "encodedAddr";
    fake.m_edit_result.local_revision = 7;

    SideStakeTableModel model(fake);
    const QString added = model.addRow("rawAddr", "42.50", "the description");

    // The model parses the allocation GUI-side and delegates to the interface
    // with the parsed value; the encoded address comes back from the interface.
    QCOMPARE(QString::fromStdString(fake.m_last_add_address), QString("rawAddr"));
    QCOMPARE(fake.m_last_add_allocation, 42.5);
    QCOMPARE(QString::fromStdString(fake.m_last_add_description), QString("the description"));
    QCOMPARE(added, QString("encodedAddr"));
    QCOMPARE(model.getEditStatus(), SideStakeTableModel::OK);
}

void SideStakeModelTests::addRowInvalidAllocationSkipsInterface()
{
    qt_test::FakeSideStakeManager fake;

    SideStakeTableModel model(fake);
    const QString added = model.addRow("rawAddr", "not-a-number", "d");

    // An unparseable allocation is rejected GUI-side; the interface is not called.
    QVERIFY(added.isEmpty());
    QCOMPARE(model.getEditStatus(), SideStakeTableModel::INVALID_ALLOCATION);
    QVERIFY(fake.m_last_add_address.empty());
}

void SideStakeModelTests::removeRowDelegatesToInterface()
{
    qt_test::FakeSideStakeManager fake;
    fake.m_snapshot.entries.push_back(MakeEntry("delAddr", 10.0, "d", "Active", false));
    fake.m_edit_result.status = interfaces::SideStakeEditStatus::OK;

    SideStakeTableModel model(fake);
    model.refresh();

    QVERIFY(model.removeRows(0, 1, QModelIndex()));
    QCOMPARE(QString::fromStdString(fake.m_last_deleted_address), QString("delAddr"));
}

void SideStakeModelTests::removeRowRefusesMandatory()
{
    qt_test::FakeSideStakeManager fake;
    fake.m_snapshot.entries.push_back(MakeEntry("mandAddr", 25.0, "d", "Mandatory", true));
    fake.m_edit_result.status = interfaces::SideStakeEditStatus::OK;

    SideStakeTableModel model(fake);
    model.refresh();

    // A mandatory (contract) entry cannot be deleted: removeRows refuses it and
    // never reaches the interface, so no deleteLocal command is issued.
    QVERIFY(!model.removeRows(0, 1, QModelIndex()));
    QVERIFY(fake.m_last_deleted_address.empty());
}
