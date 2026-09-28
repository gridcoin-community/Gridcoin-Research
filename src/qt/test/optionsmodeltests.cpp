// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/optionsmodeltests.h"

#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"

#include <QModelIndex>
#include <QVariant>

#include <string>
#include <utility>
#include <vector>

namespace {
constexpr qint64 COIN_SAT = 100000000;

bool SubmitReserve(OptionsModel& model, qint64 sat)
{
    return model.setData(model.index(OptionsModel::ReserveBalance), QVariant(sat), Qt::EditRole);
}
} // anonymous namespace

void OptionsModelTests::unchangedReserveWritesNothing()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    // Never set: the dialog shows zero, and OK submits that zero back.
    QVERIFY(SubmitReserve(model, 0));
    QCOMPARE(node.m_change_calls.size(), size_t{0});
    QVERIFY(!node.isSettingSet("reservebalance"));

    // Set, here to 5 GRC as a config file would: OK submits 5 GRC back.
    node.m_settings["reservebalance"] = "5.00000000";
    QVERIFY(SubmitReserve(model, 5 * COIN_SAT));
    QCOMPARE(node.m_change_calls.size(), size_t{0});
}

void OptionsModelTests::changedReserveIsStored()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    QVERIFY(SubmitReserve(model, 7 * COIN_SAT));
    QCOMPARE(node.m_change_calls.size(), size_t{1});
    QCOMPARE(node.m_change_calls.back(),
             (std::vector<std::pair<std::string, std::string>>{{"reservebalance", "7.00000000"}}));

    // A zero chosen over a set reserve is stored as zero, not erased, so it holds
    // against a -reservebalance in the config file.
    QVERIFY(SubmitReserve(model, 0));
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.m_change_calls.back(),
             (std::vector<std::pair<std::string, std::string>>{{"reservebalance", "0.00000000"}}));
}
