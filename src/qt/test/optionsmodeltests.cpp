// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/optionsmodeltests.h"

#include "qt/guiutil.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "util.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDataWidgetMapper>
#include <QLineEdit>
#include <QModelIndex>
#include <QVariant>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr qint64 COIN_SAT = 100000000;

using SettingsBatch = std::vector<std::pair<std::string, std::string>>;

bool SubmitReserve(OptionsModel& model, qint64 sat)
{
    return model.setData(model.index(OptionsModel::ReserveBalance), QVariant(sat), Qt::EditRole);
}

//! What the Options dialog's widget hands back to setData for a field the user
//! did not touch: a checkbox submits its checked state as a bool, a line edit its
//! text. The mapper loaded the widget from data(), so this is data() passed
//! through the widget's user property.
QVariant UntouchedSubmit(const OptionsModel& model, OptionsModel::OptionID row, bool is_line_edit)
{
    const QVariant shown = model.data(model.index(row), Qt::EditRole);
    return is_line_edit ? QVariant(shown.toString()) : QVariant(shown.toBool());
}

//! The eight core settings the dialog maps, with the kind of widget each uses.
//! The seed is a config-file-like value for the key; FakeNode::getSettingInt is
//! strict, so numeric seeds are canonical integers. The changed value is what a
//! line edit submits after an edit (checkbox rows flip the displayed state).
struct CoreRow {
    OptionsModel::OptionID row;
    const char* key;
    bool is_line_edit;
    const char* seed;
    const char* changed;
};

const std::vector<CoreRow>& CoreRows()
{
    static const std::vector<CoreRow> rows{
        {OptionsModel::MapPortUPnP, "upnp", false, "1", ""},
        {OptionsModel::DisableUpdateCheck, "disableupdatecheck", false, "1", ""},
        {OptionsModel::EnableStaking, "staking", false, "0", ""},
        {OptionsModel::EnableStakeSplit, "enablestakesplit", false, "1", ""},
        {OptionsModel::EnableSideStaking, "enablesidestaking", false, "1", ""},
        {OptionsModel::StakingEfficiency, "stakingefficiency", true, "95", "96"},
        {OptionsModel::MinStakeSplitValue, "minstakesplitvalue", true, "1000", "1200"},
        {OptionsModel::ContractChangeToInput, "contractchangetoinputaddress", false, "1", ""},
    };
    return rows;
}

void AddCoreRowColumns()
{
    QTest::addColumn<int>("row");
    QTest::addColumn<QString>("key");
    QTest::addColumn<bool>("isLineEdit");
    QTest::addColumn<QString>("seed");
    QTest::addColumn<QString>("changed");
}

//! Submit every proxy field untouched, as one OK of the dialog does.
bool SubmitProxyUntouched(OptionsModel& model)
{
    return model.setData(model.index(OptionsModel::ProxyUse), UntouchedSubmit(model, OptionsModel::ProxyUse, false), Qt::EditRole)
        && model.setData(model.index(OptionsModel::ProxyIP), UntouchedSubmit(model, OptionsModel::ProxyIP, true), Qt::EditRole)
        && model.setData(model.index(OptionsModel::ProxyPort), UntouchedSubmit(model, OptionsModel::ProxyPort, true), Qt::EditRole);
}
} // anonymous namespace

OptionsModelTests::OptionsModelTests() = default;
OptionsModelTests::~OptionsModelTests() = default;

void OptionsModelTests::initTestCase()
{
    // The proxy fields are GUI state in QSettings. The test binary sets no
    // organization or application name, so it cannot reach the real Gridcoin-Qt
    // store; the redirect below keeps these tests' writes out of whatever fallback
    // store Qt would otherwise use, so a run cannot leave keys behind that change
    // the next run's result. IniFormat is set as the default so that the model's
    // default-constructed QSettings uses the redirected path on every platform
    // (NativeFormat is the registry on Windows and a plist on macOS).
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("optionsmodeltests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());
}

void OptionsModelTests::cleanupTestCase()
{
    // The IniFormat user path keeps pointing at the (now removed) temporary
    // directory: Qt has no getter to restore the previous one. Nothing later in
    // this binary opens an IniFormat QSettings.
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void OptionsModelTests::init()
{
    // GUIUtil::nodeSettingsKey() calls GetDataDir(false), which creates the
    // default datadir on first use. Point -datadir at a temporary directory so a
    // test run never creates ~/.GridcoinResearch on the host.
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void OptionsModelTests::cleanup()
{
    QSettings().clear();
    gArgs.ForceSetArg("-datadir", "");
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

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
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"reservebalance", "7.00000000"}}));

    // A zero chosen over a set reserve is stored as zero, not erased, so it holds
    // against a -reservebalance in the config file.
    QVERIFY(SubmitReserve(model, 0));
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"reservebalance", "0.00000000"}}));
}

void OptionsModelTests::unchangedCoreSettingsWriteNothing_data()
{
    AddCoreRowColumns();
    for (const CoreRow& r : CoreRows()) {
        // Unset: the dialog shows the node's default. Seeded: it shows a value
        // from the config file, the case a write would pin.
        QTest::newRow(qPrintable(QString("%1/unset").arg(r.key)))
            << int{r.row} << QString(r.key) << r.is_line_edit << QString() << QString(r.changed);
        QTest::newRow(qPrintable(QString("%1/seeded").arg(r.key)))
            << int{r.row} << QString(r.key) << r.is_line_edit << QString(r.seed) << QString(r.changed);
    }
}

void OptionsModelTests::unchangedCoreSettingsWriteNothing()
{
    QFETCH(int, row);
    QFETCH(QString, key);
    QFETCH(bool, isLineEdit);
    QFETCH(QString, seed);

    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    if (!seed.isEmpty()) node.m_settings[key.toStdString()] = seed.toStdString();
    const std::map<std::string, std::string> before = node.m_settings;

    const auto id = static_cast<OptionsModel::OptionID>(row);
    QVERIFY(model.setData(model.index(id), UntouchedSubmit(model, id, isLineEdit), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{0});
    QVERIFY(node.m_settings == before);
}

void OptionsModelTests::changedCoreSettingIsStored_data()
{
    AddCoreRowColumns();
    for (const CoreRow& r : CoreRows()) {
        QTest::newRow(r.key)
            << int{r.row} << QString(r.key) << r.is_line_edit << QString(r.seed) << QString(r.changed);
    }
}

void OptionsModelTests::changedCoreSettingIsStored()
{
    QFETCH(int, row);
    QFETCH(QString, key);
    QFETCH(bool, isLineEdit);
    QFETCH(QString, changed);

    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    const auto id = static_cast<OptionsModel::OptionID>(row);
    const QVariant shown = model.data(model.index(id), Qt::EditRole);

    // A checkbox edit flips what was shown; the expected stored form follows from
    // the submitted state (the default build's UPnP default is not visible here).
    QVariant submitted;
    std::string expected;
    if (isLineEdit) {
        QVERIFY(changed != shown.toString());
        submitted = QVariant(changed);
        expected = changed.toStdString();
    } else {
        const bool flipped = !shown.toBool();
        submitted = QVariant(flipped);
        expected = flipped ? "1" : "0";
    }

    QVERIFY(model.setData(model.index(id), submitted, Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{1});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{key.toStdString(), expected}}));
}

void OptionsModelTests::mapperRoundTripWritesNothing()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    node.m_settings["stakingefficiency"] = "95";

    // Configured as the OptionsDialog constructor configures its mapper (setMapper
    // only adds the mappings): the model is a one-column list with the options as
    // rows, so the mapper runs vertically.
    QCheckBox staking;
    QLineEdit efficiency;
    QLineEdit split;
    QDataWidgetMapper mapper;
    mapper.setModel(&model);
    mapper.setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper.setOrientation(Qt::Vertical);
    mapper.addMapping(&staking, OptionsModel::EnableStaking);
    mapper.addMapping(&efficiency, OptionsModel::StakingEfficiency);
    mapper.addMapping(&split, OptionsModel::MinStakeSplitValue);
    mapper.toFirst();

    // The widgets show what data() reports...
    QCOMPARE(efficiency.text(), QString("95"));
    QCOMPARE(split.text(), QString("800"));
    QCOMPARE(staking.isChecked(), model.data(model.index(OptionsModel::EnableStaking), Qt::EditRole).toBool());

    // ...and submitting them untouched writes nothing.
    QVERIFY(mapper.submit());
    QCOMPARE(node.m_change_calls.size(), size_t{0});
}

void OptionsModelTests::unchangedProxyWritesNothing_unset_data()
{
    QTest::addColumn<int>("row");
    QTest::addColumn<bool>("isLineEdit");
    QTest::newRow("ProxyUse") << int{OptionsModel::ProxyUse} << false;
    QTest::newRow("ProxyIP") << int{OptionsModel::ProxyIP} << true;
    QTest::newRow("ProxyPort") << int{OptionsModel::ProxyPort} << true;
}

void OptionsModelTests::unchangedProxyWritesNothing_unset()
{
    QFETCH(int, row);
    QFETCH(bool, isLineEdit);

    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    const auto id = static_cast<OptionsModel::OptionID>(row);
    QVERIFY(model.setData(model.index(id), UntouchedSubmit(model, id, isLineEdit), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{0});

    QSettings settings;
    QVERIFY(!settings.contains(GUIUtil::nodeSettingsKey("fUseProxy")));
    QVERIFY(!settings.contains(GUIUtil::nodeSettingsKey("addrProxy")));
}

void OptionsModelTests::unchangedProxyKeepsNodeProxy()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    // A node-side proxy the dialog does not show (set through changesettings, for
    // example). The dialog shows the proxy disabled; an untouched OK used to push
    // the empty effective proxy, which erases the node-side one.
    node.m_settings["proxy"] = "10.0.0.1:9050";

    QVERIFY(SubmitProxyUntouched(model));
    QCOMPARE(node.m_change_calls.size(), size_t{0});
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("10.0.0.1:9050"));
}

void OptionsModelTests::unchangedProxyTypedReadBack()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    // Stored as a settings file reads them back: text, not a bool or an int. The
    // untouched submit is a bool and two strings, so the comparison has to
    // normalise types.
    {
        QSettings settings;
        settings.setValue(GUIUtil::nodeSettingsKey("fUseProxy"), QStringLiteral("true"));
        settings.setValue(GUIUtil::nodeSettingsKey("addrProxy"), QStringLiteral("10.1.2.3:9150"));
    }

    QVERIFY(SubmitProxyUntouched(model));
    QCOMPARE(node.m_change_calls.size(), size_t{0});
}

void OptionsModelTests::changedProxyIsPushed()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    QVERIFY(model.setData(model.index(OptionsModel::ProxyUse), QVariant(true), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{1});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"proxy", "127.0.0.1:9050"}}));

    QVERIFY(model.setData(model.index(OptionsModel::ProxyPort), QVariant(QString("9150")), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"proxy", "127.0.0.1:9150"}}));
}
