// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/optionsmodeltests.h"

#include "interfaces/wallet_coin_source.h"
#include "logging.h"
#include "qt/editsidestakedialog.h"
#include "qt/guiutil.h"
#include "qt/optionsdialog.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"
#include "util.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDataWidgetMapper>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QStringList>
#include <QTableView>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
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

//! A Settings capability the fake prompt began: the fake it was given accepts
//! changes that need the passphrase while this lives.
class FakeElevation
{
public:
    explicit FakeElevation(bool& elevated) : m_elevated(elevated) { m_elevated = true; }
    ~FakeElevation() { m_elevated = false; }

    FakeElevation(const FakeElevation&) = delete;
    FakeElevation& operator=(const FakeElevation&) = delete;

private:
    bool& m_elevated;
};

//! Stand in for the wallet model's passphrase prompt: count each prompt in
//! \p asks and, when \p grant, begin a FakeElevation over \p elevated.
void InstallPrompt(OptionsModel& model, int& asks, bool grant, bool& elevated)
{
    model.setSettingsElevationRequest([&asks, grant, &elevated]() -> std::shared_ptr<void> {
        ++asks;
        if (!grant) return nullptr;
        return std::make_shared<FakeElevation>(elevated);
    });
}

bool SubmitSideStaking(OptionsModel& model, bool enabled)
{
    return model.setData(model.index(OptionsModel::EnableSideStaking), QVariant(enabled), Qt::EditRole);
}

// The messages the dialogs show when the passphrase was not given.
const QString PASSPHRASE_NEEDED_MESSAGE = QStringLiteral(
    "Changing UPnP, the proxy or side staking needs your wallet passphrase. These changes were not saved.");
const QString SIDE_STAKE_PASSPHRASE_MESSAGE = QStringLiteral(
    "Changing side stakes needs your wallet passphrase. The change was not saved.");

//! The UPnP value the node uses when nothing sets it, as the Options dialog
//! shows it: off, unless the build turns UPnP on by default.
bool UnsetUpnp()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    return model.data(model.index(OptionsModel::MapPortUPnP, 0), Qt::EditRole).toBool();
}

//! The upnp setting a legacy UPnP value other than the node's moves as.
std::string MovedUpnp()
{
    return UnsetUpnp() ? "0" : "1";
}

//! Put the settings an earlier version kept in Gridcoin-Qt.conf at the root of
//! QSettings: the two the core takes without the passphrase and, when
//! \p network, the proxy and UPnP ones, each different from what the node uses.
void SeedLegacySettings(bool network)
{
    QSettings settings;
    settings.setValue("nReserveBalance", 5 * COIN_SAT);
    settings.setValue("fDisableUpdateCheck", true);
    if (network) {
        settings.setValue("fUseProxy", true);
        settings.setValue("addrProxy", QStringLiteral("1.2.3.4:9050"));
        settings.setValue("fUseUPnP", !UnsetUpnp());
    }
}

//! A migration's elevation: counts each call in \p calls and, when \p grant,
//! runs apply under a FakeElevation over \p elevated.
OptionsModel::WithElevation CountingElevation(int& calls, bool grant, bool& elevated)
{
    return [&calls, grant, &elevated](const std::function<void()>& apply) {
        ++calls;
        if (!grant) return false;
        FakeElevation elevation(elevated);
        apply();
        return true;
    };
}

//! Keeps the lines logged while it lives.
class LogCapture
{
public:
    LogCapture()
        : m_callback(LogInstance().PushBackCallback([this](const std::string& line) { m_lines.push_back(line); }))
    {
    }
    ~LogCapture() { LogInstance().DeleteCallback(m_callback); }

    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;

    //! How many lines contain \p text.
    int Count(const std::string& text) const
    {
        int count = 0;
        for (const std::string& line : m_lines) {
            if (line.find(text) != std::string::npos) ++count;
        }
        return count;
    }

private:
    std::vector<std::string> m_lines;
    std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

//! Closes each message box shown while it lives with its default button, and
//! keeps its text, so that a test can run a slot that shows one.
class MessageBoxCloser : public QObject
{
public:
    MessageBoxCloser()
    {
        connect(&m_timer, &QTimer::timeout, this, [this] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                QMessageBox* box = qobject_cast<QMessageBox*>(widget);
                if (!box || !box->isVisible()) continue;

                m_texts << box->text();

                if (QAbstractButton* button = box->defaultButton()) {
                    button->click();
                } else if (QAbstractButton* escape = box->escapeButton()) {
                    escape->click();
                } else {
                    box->close();
                }
            }
        });
        m_timer.start(10);
    }

    QStringList m_texts;

private:
    QTimer m_timer;
};

interfaces::WalletLockState LockState(bool crypted, bool locked, bool staking_only)
{
    interfaces::WalletLockState state;
    state.crypted = crypted;
    state.locked = locked;
    state.unlocked_for_staking_only = staking_only;
    return state;
}

//! A WalletModel over fakes. Constructed, it installs its passphrase request
//! on the options model as it does in the application. The prompt it raises
//! (requireUnlock) is answered here: each is counted, with the number of
//! changes \p node had been sent by then, and when \p grant the capability is
//! begun on the fake wallet, which sets \p capability_flag while it lives.
class PromptedWallet
{
public:
    PromptedWallet(OptionsModel& model, const qt_test::FakeNode* node, interfaces::WalletLockState state,
                   bool grant, bool* capability_flag)
        : m_wallet_model(m_wallet, m_tx_source, *m_coin_source, &model)
    {
        m_wallet.m_lock_state = state;
        m_wallet.m_capability_flag = capability_flag;

        QObject::connect(&m_wallet_model, &WalletModel::requireUnlock, &m_wallet_model,
                         [this, node, grant](CapabilityKind kind) {
            m_changes_sent_at_prompt.push_back(node ? node->m_change_calls.size() : 0);
            m_prompt_kinds.push_back(kind);
            if (grant && m_wallet.beginElevation(SecureString(), kind)) {
                m_wallet_model.setElevationBegun();
            }
        });
    }

    int Prompts() const { return static_cast<int>(m_prompt_kinds.size()); }

    qt_test::FakeLockStateWallet m_wallet;
    std::vector<size_t> m_changes_sent_at_prompt;
    std::vector<CapabilityKind> m_prompt_kinds;

private:
    qt_test::FakeWalletTxSource m_tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> m_coin_source{interfaces::MakeSyntheticCoinSource(1, 1)};
    WalletModel m_wallet_model;
};

template <typename Widget>
Widget* Find(QWidget& parent, const char* name)
{
    return parent.findChild<Widget*>(QString::fromLatin1(name));
}

void Flip(QCheckBox* box)
{
    box->setChecked(!box->isChecked());
}

//! The dialog's widgets the tests below drive.
struct DialogWidgets
{
    explicit DialogWidgets(QWidget& dialog)
        : upnp(Find<QCheckBox>(dialog, "mapPortUpnp"))
        , proxy(Find<QCheckBox>(dialog, "connectSocks"))
        , proxy_host(Find<QLineEdit>(dialog, "proxyIp"))
        , staking(Find<QCheckBox>(dialog, "enableStaking"))
        , side_staking(Find<QCheckBox>(dialog, "enableSideStaking"))
        , new_side_stake(Find<QPushButton>(dialog, "pushButtonNewSideStake"))
        , ok(Find<QPushButton>(dialog, "okButton"))
        , apply(Find<QPushButton>(dialog, "applyButton"))
    {
    }

    bool AllFound() const
    {
        return upnp && proxy && proxy_host && staking && side_staking && new_side_stake && ok && apply;
    }

    QCheckBox* upnp;
    QCheckBox* proxy;
    QLineEdit* proxy_host;
    QCheckBox* staking;
    QCheckBox* side_staking;
    QPushButton* new_side_stake;
    QPushButton* ok;
    QPushButton* apply;
};

//! The settings the dialog writes that need the wallet passphrase.
const std::set<std::string> ELEVATE_SETTINGS{"proxy", "upnp", "enablesidestaking"};

OptionsModel::OptionID OptionByName(const QString& name)
{
    static const std::map<QString, OptionsModel::OptionID> ids{
        {QStringLiteral("MapPortUPnP"), OptionsModel::MapPortUPnP},
        {QStringLiteral("ProxyUse"), OptionsModel::ProxyUse},
        {QStringLiteral("ProxyIP"), OptionsModel::ProxyIP},
        {QStringLiteral("ProxyPort"), OptionsModel::ProxyPort},
        {QStringLiteral("EnableSideStaking"), OptionsModel::EnableSideStaking},
        {QStringLiteral("EnableStaking"), OptionsModel::EnableStaking},
    };
    return ids.at(name);
}

interfaces::SideStakeEntry LocalSideStake(const std::string& address)
{
    interfaces::SideStakeEntry entry;
    entry.address = address;
    entry.allocation_percent = 10.0;
    entry.description = "d";
    entry.status = "Active";
    return entry;
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

void OptionsModelTests::proxyFailedEnableRetries()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    node.m_fail_changes = true;
    QVERIFY(!model.setData(model.index(OptionsModel::ProxyUse), QVariant(true), Qt::EditRole));
    QVERIFY(!QSettings().contains(GUIUtil::nodeSettingsKey("fUseProxy")));
    QCOMPARE(node.m_change_calls.size(), size_t{1});

    // An identical resubmit must push again.
    node.m_fail_changes = false;
    QVERIFY(model.setData(model.index(OptionsModel::ProxyUse), QVariant(true), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"proxy", "127.0.0.1:9050"}}));
    QVERIFY(QSettings().value(GUIUtil::nodeSettingsKey("fUseProxy"), false).toBool());
}

void OptionsModelTests::proxyFailedHostRetries()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    {
        QSettings settings;
        settings.setValue(GUIUtil::nodeSettingsKey("fUseProxy"), true);
        settings.setValue(GUIUtil::nodeSettingsKey("addrProxy"), QStringLiteral("127.0.0.1:9050"));
    }

    node.m_fail_changes = true;
    QVERIFY(!model.setData(model.index(OptionsModel::ProxyIP), QVariant(QStringLiteral("10.0.0.2")), Qt::EditRole));
    QCOMPARE(QSettings().value(GUIUtil::nodeSettingsKey("addrProxy")).toString(), QStringLiteral("127.0.0.1:9050"));
    QCOMPARE(node.m_change_calls.size(), size_t{1});

    node.m_fail_changes = false;
    QVERIFY(model.setData(model.index(OptionsModel::ProxyIP), QVariant(QStringLiteral("10.0.0.2")), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"proxy", "10.0.0.2:9050"}}));
    QCOMPARE(QSettings().value(GUIUtil::nodeSettingsKey("addrProxy")).toString(), QStringLiteral("10.0.0.2:9050"));
}

void OptionsModelTests::proxyFailedPortRetries()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    {
        QSettings settings;
        settings.setValue(GUIUtil::nodeSettingsKey("fUseProxy"), true);
        settings.setValue(GUIUtil::nodeSettingsKey("addrProxy"), QStringLiteral("127.0.0.1:9050"));
    }

    node.m_fail_changes = true;
    QVERIFY(!model.setData(model.index(OptionsModel::ProxyPort), QVariant(QString("9150")), Qt::EditRole));
    QCOMPARE(QSettings().value(GUIUtil::nodeSettingsKey("addrProxy")).toString(), QStringLiteral("127.0.0.1:9050"));
    QCOMPARE(node.m_change_calls.size(), size_t{1});

    node.m_fail_changes = false;
    QVERIFY(model.setData(model.index(OptionsModel::ProxyPort), QVariant(QString("9150")), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"proxy", "127.0.0.1:9150"}}));
    QCOMPARE(QSettings().value(GUIUtil::nodeSettingsKey("addrProxy")).toString(), QStringLiteral("127.0.0.1:9150"));
}

void OptionsModelTests::proxyFailedEnableThenValidHost()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);

    // A stale address the node would reject; the proxy is disabled.
    {
        QSettings settings;
        settings.setValue(GUIUtil::nodeSettingsKey("addrProxy"), QStringLiteral("invalid-proxy-address"));
    }

    // Enabling pushes the stale address, which fails, so the enable is undone.
    node.m_fail_changes = true;
    QVERIFY(!model.setData(model.index(OptionsModel::ProxyUse), QVariant(true), Qt::EditRole));
    QVERIFY(!QSettings().contains(GUIUtil::nodeSettingsKey("fUseProxy")));

    // A valid host later in the same submit is stored, and pushed with the proxy
    // still disabled.
    node.m_fail_changes = false;
    QVERIFY(model.setData(model.index(OptionsModel::ProxyIP), QVariant(QStringLiteral("10.0.0.2")), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"proxy", ""}}));
    QCOMPARE(QSettings().value(GUIUtil::nodeSettingsKey("addrProxy")).toString(), QStringLiteral("10.0.0.2:9050"));

    // Re-ticking the box applies the host.
    QVERIFY(model.setData(model.index(OptionsModel::ProxyUse), QVariant(true), Qt::EditRole));
    QCOMPARE(node.m_change_calls.size(), size_t{3});
    QCOMPARE(node.m_change_calls.back(), (SettingsBatch{{"proxy", "10.0.0.2:9050"}}));
}

void OptionsModelTests::elevationRefusalAsksAndRetriesOnce()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    node.m_needs_elevation = {"enablesidestaking"};

    int asks = 0;
    InstallPrompt(model, asks, true, node.m_elevated);

    // Refused, asked for the passphrase once, and sent once more under it.
    QVERIFY(SubmitSideStaking(model, true));
    QCOMPARE(asks, 1);
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.getSettingStr("enablesidestaking", ""), std::string("1"));

    // The capability is held until control returns to the event loop.
    QVERIFY(node.m_elevated);
    QCoreApplication::processEvents();
    QVERIFY(!node.m_elevated);
}

void OptionsModelTests::declinedElevationIsAskedOncePerPass()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    node.m_needs_elevation = {"enablesidestaking"};

    int asks = 0;
    InstallPrompt(model, asks, false, node.m_elevated);

    // Declined: the change fails, is not sent again, and stores nothing.
    QVERIFY(!SubmitSideStaking(model, true));
    QCOMPARE(asks, 1);
    QCOMPARE(node.m_change_calls.size(), size_t{1});
    QVERIFY(!node.isSettingSet("enablesidestaking"));

    // A second refusal before control returns to the event loop does not ask again.
    QVERIFY(!SubmitSideStaking(model, true));
    QCOMPARE(asks, 1);
    QCOMPARE(node.m_change_calls.size(), size_t{2});

    // A later submit asks again.
    QCoreApplication::processEvents();
    QVERIFY(!SubmitSideStaking(model, true));
    QCOMPARE(asks, 2);
}

void OptionsModelTests::oneSubmitAsksOnceForSeveralElevatedFields()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    node.m_needs_elevation = {"proxy", "enablesidestaking"};

    int asks = 0;
    InstallPrompt(model, asks, true, node.m_elevated);

    // Enabling the proxy, changing its host and enabling side staking in one
    // submit writes the proxy twice and side staking once, and asks once.
    QVERIFY(model.setData(model.index(OptionsModel::ProxyUse), QVariant(true), Qt::EditRole));
    QVERIFY(model.setData(model.index(OptionsModel::ProxyIP), QVariant(QStringLiteral("10.0.0.1")), Qt::EditRole));
    QVERIFY(SubmitSideStaking(model, true));
    QCOMPARE(asks, 1);
    QCOMPARE(node.m_change_calls.size(), size_t{4});
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("10.0.0.1:9050"));
    QCOMPARE(node.getSettingStr("enablesidestaking", ""), std::string("1"));

    // The next submit asks again.
    QCoreApplication::processEvents();
    QVERIFY(SubmitSideStaking(model, false));
    QCOMPARE(asks, 2);
}

void OptionsModelTests::elevationRefusalWithoutAWalletModelFails()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    node.m_needs_elevation = {"enablesidestaking"};

    QVERIFY(!SubmitSideStaking(model, true));
    QCOMPARE(node.m_change_calls.size(), size_t{1});
    QVERIFY(!node.isSettingSet("enablesidestaking"));
}

void OptionsModelTests::sideStakeEditRefusalAsksAndRetriesOnce()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    sidestakes.m_needs_elevation = true;
    sidestakes.m_edit_result.status = interfaces::SideStakeEditStatus::OK;
    sidestakes.m_edit_result.address = "encodedAddr";
    OptionsModel model(node, sidestakes);
    SideStakeTableModel* table = model.getSideStakeTableModel();

    // Declined: the add fails with the elevation status and is not sent again.
    int declined_asks = 0;
    InstallPrompt(model, declined_asks, false, sidestakes.m_elevated);
    QVERIFY(table->addRow("rawAddr", "10", "d").isEmpty());
    QCOMPARE(declined_asks, 1);
    QCOMPARE(sidestakes.m_edit_calls, 1);
    QCOMPARE(table->getEditStatus(), SideStakeTableModel::ELEVATION_REQUIRED);
    QCoreApplication::processEvents();

    // Given: sent once more under the capability, which stays held for the
    // rest of the pass, so the next edit is not asked again.
    int asks = 0;
    InstallPrompt(model, asks, true, sidestakes.m_elevated);
    QCOMPARE(table->addRow("rawAddr", "10", "d"), QString("encodedAddr"));
    QCOMPARE(asks, 1);
    QCOMPARE(sidestakes.m_edit_calls, 3);
    QCOMPARE(table->getEditStatus(), SideStakeTableModel::OK);
    QVERIFY(sidestakes.m_elevated);

    QCoreApplication::processEvents();
    QVERIFY(!sidestakes.m_elevated);
}

void OptionsModelTests::sideStakeInvalidInputDoesNotAsk()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    sidestakes.m_edit_result.status = interfaces::SideStakeEditStatus::INVALID_ADDRESS;
    OptionsModel model(node, sidestakes);
    SideStakeTableModel* table = model.getSideStakeTableModel();

    int asks = 0;
    InstallPrompt(model, asks, true, sidestakes.m_elevated);

    // The node answers bad input with its own status, before it would ask for
    // the passphrase, and the model shows that answer without a prompt.
    QVERIFY(table->addRow("rawAddr", "10", "d").isEmpty());
    QCOMPARE(table->getEditStatus(), SideStakeTableModel::INVALID_ADDRESS);
    QCOMPARE(asks, 0);
    QCOMPARE(sidestakes.m_edit_calls, 1);
}

void OptionsModelTests::migrationMovesOnlyFreeSettingsWithoutElevation()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(true);
    OptionsModel model(node, sidestakes);

    model.migrateCoreSettings();

    QCOMPARE(node.getSettingStr("reservebalance", ""), std::string("5.00000000"));
    QCOMPARE(node.getSettingStr("disableupdatecheck", ""), std::string("1"));
    QVERIFY(!node.isSettingSet("proxy"));
    QVERIFY(!node.isSettingSet("upnp"));

    // Proxy and UPnP are left for migrateElevatedCoreSettings: its flag is not
    // set, and the old values are still there for it.
    QSettings settings;
    QVERIFY(settings.value("coreSettingsMigrated", false).toBool());
    QVERIFY(!settings.value("coreElevatedSettingsMigrated", false).toBool());
    QVERIFY(settings.contains("addrProxy"));
    QVERIFY(settings.contains("fUseUPnP"));
}

void OptionsModelTests::elevatedMigrationAppliesProxyAndUpnpWhenElevated()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(true);
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();

    int calls = 0;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));

    QCOMPARE(calls, 1);
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("1.2.3.4:9050"));
    QCOMPARE(node.getSettingStr("upnp", ""), MovedUpnp());
    QVERIFY(QSettings().value("coreElevatedSettingsMigrated", false).toBool());
}

void OptionsModelTests::declinedElevatedMigrationDiscardsLegacyValuesOnce()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(true);
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();

    int calls = 0;
    {
        LogCapture log;
        model.migrateElevatedCoreSettings(CountingElevation(calls, false, node.m_elevated));
        QCOMPARE(log.Count("Discarded the proxy and UPnP settings kept by an earlier version "
                           "(proxy=1.2.3.4:9050, upnp=" + MovedUpnp() + "): the wallet passphrase was not given"),
                 1);
    }

    QCOMPARE(calls, 1);
    QVERIFY(!node.isSettingSet("proxy"));
    QVERIFY(!node.isSettingSet("upnp"));

    // The old values are gone, so the dialog no longer shows a proxy the node
    // does not use.
    QSettings settings;
    QVERIFY(!settings.contains("fUseProxy"));
    QVERIFY(!settings.contains("addrProxy"));
    QVERIFY(!settings.contains("fUseUPnP"));
    QVERIFY(!model.data(model.index(OptionsModel::ProxyUse, 0), Qt::EditRole).toBool());
    QVERIFY(settings.value("coreElevatedSettingsMigrated", false).toBool());

    // Done: a later start does not ask again.
    model.migrateElevatedCoreSettings(CountingElevation(calls, false, node.m_elevated));
    QCOMPARE(calls, 1);
}

void OptionsModelTests::elevatedMigrationSkippedAfterAnEarlierMigration()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(true);

    // An earlier build moved every setting, proxy and UPnP included, and set
    // only this flag.
    QSettings().setValue("coreSettingsMigrated", true);

    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();
    QCOMPARE(node.m_change_calls.size(), size_t{0});

    int calls = 0;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 0);
    QCOMPARE(node.m_change_calls.size(), size_t{0});
    QVERIFY(QSettings().value("coreElevatedSettingsMigrated", false).toBool());
}

void OptionsModelTests::failedElevatedMigrationStoreRetries()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(true);
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();

    // Given the passphrase, but the node could not store the values.
    int calls = 0;
    node.m_fail_changes = true;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 1);
    QVERIFY(!QSettings().value("coreElevatedSettingsMigrated", false).toBool());
    QVERIFY(QSettings().contains("addrProxy"));

    node.m_fail_changes = false;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 2);
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("1.2.3.4:9050"));
    QVERIFY(QSettings().value("coreElevatedSettingsMigrated", false).toBool());
}

void OptionsModelTests::elevatedMigrationRetriedAtTheNextStart()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(true);

    // A start that moves the free settings and ends before the main window
    // shows, so the proxy and UPnP move never runs...
    {
        OptionsModel model(node, sidestakes);
        model.migrateCoreSettings();
    }

    // ...is not read at the next start as an earlier build's complete move.
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();

    int calls = 0;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 1);
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("1.2.3.4:9050"));
    QCOMPARE(node.getSettingStr("upnp", ""), MovedUpnp());
}

void OptionsModelTests::elevatedMigrationWithoutLegacyValuesDoesNotAsk()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(false);
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();

    int calls = 0;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 0);
    QVERIFY(QSettings().value("coreElevatedSettingsMigrated", false).toBool());

    // Nor when the node already has its own proxy and UPnP settings: the old
    // ones are not moved over them.
    QSettings().remove("coreElevatedSettingsMigrated");
    SeedLegacySettings(true);
    node.m_settings["proxy"] = "10.0.0.1:9050";
    node.m_settings["upnp"] = "0";
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 0);
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("10.0.0.1:9050"));
    QVERIFY(QSettings().value("coreElevatedSettingsMigrated", false).toBool());
}

void OptionsModelTests::elevatedMigrationDropsAUpnpValueTheNodeUses()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(false);
    QSettings().setValue("fUseUPnP", UnsetUpnp());
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();
    const size_t free_change_calls = node.m_change_calls.size();

    // Moving it would change nothing, so nothing is asked or sent, and the old
    // value is removed.
    int calls = 0;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 0);
    QCOMPARE(node.m_change_calls.size(), free_change_calls);
    QVERIFY(!node.isSettingSet("upnp"));

    QSettings settings;
    QVERIFY(!settings.contains("fUseUPnP"));
    QVERIFY(settings.value("coreElevatedSettingsMigrated", false).toBool());
}

void OptionsModelTests::elevatedMigrationAsksForAUpnpValueThatDiffers()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(false);
    QSettings().setValue("fUseUPnP", !UnsetUpnp());
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();

    int calls = 0;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 1);
    QCOMPARE(node.getSettingStr("upnp", ""), MovedUpnp());
    QVERIFY(QSettings().value("coreElevatedSettingsMigrated", false).toBool());
}

void OptionsModelTests::elevatedMigrationMovesOnlyTheValuesThatDiffer()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    SeedLegacySettings(true);
    QSettings().setValue("fUseUPnP", UnsetUpnp());
    OptionsModel model(node, sidestakes);
    model.migrateCoreSettings();

    // The proxy differs, so the user is asked, and only the proxy is sent.
    int calls = 0;
    model.migrateElevatedCoreSettings(CountingElevation(calls, true, node.m_elevated));
    QCOMPARE(calls, 1);
    QVERIFY(!node.m_change_calls.empty());
    const std::vector<std::pair<std::string, std::string>> proxy_only{{"proxy", "1.2.3.4:9050"}};
    QVERIFY(node.m_change_calls.back() == proxy_only);
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("1.2.3.4:9050"));
    QVERIFY(!node.isSettingSet("upnp"));

    QSettings settings;
    QVERIFY(!settings.contains("fUseUPnP"));
    QVERIFY(settings.value("coreElevatedSettingsMigrated", false).toBool());
}

void OptionsModelTests::needsSettingsElevationOnlyForChangedNetworkAndSideStakeFields_data()
{
    QTest::addColumn<bool>("proxyStoredOn");
    QTest::addColumn<bool>("onlyChanges");
    QTest::addColumn<QVariantMap>("changes");
    QTest::addColumn<bool>("expected");

    const QVariant host = QStringLiteral("10.0.0.1");

    QTest::newRow("all five unchanged") << false << false << QVariantMap{} << false;
    QTest::newRow("UPnP on") << false << false << QVariantMap{{"MapPortUPnP", true}} << true;
    QTest::newRow("proxy on") << false << false << QVariantMap{{"ProxyUse", true}} << true;
    QTest::newRow("host, proxy off") << false << false << QVariantMap{{"ProxyIP", host}, {"ProxyUse", false}} << false;
    QTest::newRow("host, proxy on") << false << false << QVariantMap{{"ProxyIP", host}, {"ProxyUse", true}} << true;
    QTest::newRow("side staking on") << false << false << QVariantMap{{"EnableSideStaking", true}} << true;
    QTest::newRow("staking only") << false << true << QVariantMap{{"EnableStaking", false}} << false;
    QTest::newRow("enabled proxy unchanged") << true << false << QVariantMap{} << false;
    QTest::newRow("enabled proxy, host") << true << false << QVariantMap{{"ProxyIP", host}} << true;
    QTest::newRow("enabled proxy, port")
        << true << false << QVariantMap{{"ProxyPort", QStringLiteral("9150")}} << true;
    QTest::newRow("enabled proxy turned off") << true << false << QVariantMap{{"ProxyUse", false}} << true;
}

void OptionsModelTests::needsSettingsElevationOnlyForChangedNetworkAndSideStakeFields()
{
    QFETCH(bool, proxyStoredOn);
    QFETCH(bool, onlyChanges);
    QFETCH(QVariantMap, changes);
    QFETCH(bool, expected);

    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_settings["upnp"] = "0";
    node.m_settings["enablesidestaking"] = "0";
    {
        QSettings settings;
        settings.setValue(GUIUtil::nodeSettingsKey("fUseProxy"), proxyStoredOn);
        settings.setValue(GUIUtil::nodeSettingsKey("addrProxy"), QStringLiteral("127.0.0.1:9050"));
    }
    OptionsModel model(node, sidestakes);

    // The five fields as the dialog hands them over untouched, then the edits.
    QMap<OptionsModel::OptionID, QVariant> submitted;
    if (!onlyChanges) {
        submitted[OptionsModel::MapPortUPnP] = UntouchedSubmit(model, OptionsModel::MapPortUPnP, false);
        submitted[OptionsModel::ProxyUse] = UntouchedSubmit(model, OptionsModel::ProxyUse, false);
        submitted[OptionsModel::ProxyIP] = UntouchedSubmit(model, OptionsModel::ProxyIP, true);
        submitted[OptionsModel::ProxyPort] = UntouchedSubmit(model, OptionsModel::ProxyPort, true);
        submitted[OptionsModel::EnableSideStaking] = UntouchedSubmit(model, OptionsModel::EnableSideStaking, false);
    }
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        submitted[OptionByName(it.key())] = it.value();
    }

    QCOMPARE(model.needsSettingsElevation(submitted), expected);

    // Asking writes nothing.
    QCOMPARE(node.m_change_calls.size(), size_t{0});
}

void OptionsModelTests::dialogSubmitOfElevatedChangesAsksOnce()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(true, false, true), true, &node.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    // UPnP, the proxy and its host in one submit: the proxy alone is written
    // twice.
    const bool upnp_was = widgets.upnp->isChecked();
    Flip(widgets.upnp);
    widgets.proxy->setChecked(true);
    widgets.proxy_host->setText(QStringLiteral("10.0.0.1"));

    MessageBoxCloser boxes;
    widgets.ok->click();

    // Asked once, for the Settings capability, before anything was written.
    QCOMPARE(wallet.Prompts(), 1);
    QVERIFY(wallet.m_prompt_kinds.front() == CapabilityKind::Settings);
    QCOMPARE(wallet.m_changes_sent_at_prompt, std::vector<size_t>{0});
    QVERIFY(boxes.m_texts.isEmpty());
    QCOMPARE(node.getSettingStr("upnp", ""), std::string(upnp_was ? "0" : "1"));
    QCOMPARE(node.getSettingStr("proxy", ""), std::string("10.0.0.1:9050"));
    QVERIFY(!dialog.isVisible());

    // The capability ends once control returns to the event loop.
    QCOMPARE(wallet.m_wallet.m_end_elevation_calls, 0);
    QCoreApplication::processEvents();
    QCOMPARE(wallet.m_wallet.m_begin_elevation_calls, 1);
    QCOMPARE(wallet.m_wallet.m_end_elevation_calls, 1);
    QVERIFY(!node.m_elevated);
}

void OptionsModelTests::dialogSubmitOfFreeChangesDoesNotAsk()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(true, false, true), true, &node.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    const bool staking_was = widgets.staking->isChecked();
    Flip(widgets.staking);

    MessageBoxCloser boxes;
    widgets.ok->click();

    QCOMPARE(wallet.Prompts(), 0);
    QVERIFY(boxes.m_texts.isEmpty());
    QCOMPARE(node.getSettingStr("staking", ""), std::string(staking_was ? "0" : "1"));
    QVERIFY(!dialog.isVisible());
}

void OptionsModelTests::dialogDoesNotAskAWalletThatNeedsNoPassphrase_data()
{
    QTest::addColumn<bool>("crypted");
    QTest::newRow("unencrypted") << false;
    QTest::newRow("fully unlocked") << true;
}

void OptionsModelTests::dialogDoesNotAskAWalletThatNeedsNoPassphrase()
{
    QFETCH(bool, crypted);

    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    // The wallet's keys are at hand, so the node takes these changes as they are.
    node.m_elevated = true;
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(crypted, false, false), true, nullptr);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    const bool upnp_was = widgets.upnp->isChecked();
    Flip(widgets.upnp);

    MessageBoxCloser boxes;
    widgets.ok->click();

    QCOMPARE(wallet.Prompts(), 0);
    QCOMPARE(wallet.m_wallet.m_begin_elevation_calls, 0);
    QVERIFY(boxes.m_texts.isEmpty());
    QCOMPARE(node.getSettingStr("upnp", ""), std::string(upnp_was ? "0" : "1"));
    QVERIFY(!dialog.isVisible());
}

void OptionsModelTests::dialogHeldElevationIsNotAskedAgain()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    OptionsModel model(node, sidestakes);
    // The prompt succeeds, but the node does not see the capability, so it
    // refuses the change as if none were held.
    PromptedWallet wallet(model, &node, LockState(true, false, true), true, nullptr);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    Flip(widgets.upnp);

    MessageBoxCloser boxes;
    widgets.ok->click();

    // The refusal is not asked for a second time while the first answer holds.
    QCOMPARE(wallet.Prompts(), 1);
    QCOMPARE(node.m_change_calls.size(), size_t{1});
    QVERIFY(!node.isSettingSet("upnp"));
}

void OptionsModelTests::dialogUnexpectedRefusalAsksOnce()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    // A setting the dialog does not expect to need the passphrase, refused for
    // want of it all the same: the options model asks after the refusal.
    node.m_needs_elevation = {"staking"};
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(true, false, true), true, &node.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    const bool staking_was = widgets.staking->isChecked();
    Flip(widgets.staking);

    MessageBoxCloser boxes;
    widgets.ok->click();

    QCOMPARE(wallet.Prompts(), 1);
    QCOMPARE(wallet.m_changes_sent_at_prompt, std::vector<size_t>{1});
    QCOMPARE(node.m_change_calls.size(), size_t{2});
    QCOMPARE(node.getSettingStr("staking", ""), std::string(staking_was ? "0" : "1"));
}

void OptionsModelTests::dialogDeclinedPassphraseSavesNothing()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(true, false, true), false, &node.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    // A free change goes with the refused one: nothing of the submit is saved.
    Flip(widgets.upnp);
    Flip(widgets.staking);

    MessageBoxCloser boxes;
    widgets.ok->click();

    QCOMPARE(wallet.Prompts(), 1);
    QCOMPARE(wallet.m_changes_sent_at_prompt, std::vector<size_t>{0});
    QCOMPARE(boxes.m_texts, QStringList{PASSPHRASE_NEEDED_MESSAGE});
    QCOMPARE(node.m_change_calls.size(), size_t{0});
    QVERIFY(dialog.isVisible());
}

void OptionsModelTests::dialogWithoutAWalletModelSavesNothing()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    OptionsModel model(node, sidestakes);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    Flip(widgets.upnp);
    QVERIFY(widgets.apply->isEnabled());

    MessageBoxCloser boxes;
    widgets.apply->click();

    QCOMPARE(boxes.m_texts, QStringList{PASSPHRASE_NEEDED_MESSAGE});
    QCOMPARE(node.m_change_calls.size(), size_t{0});
    QVERIFY(widgets.apply->isEnabled());
    QVERIFY(dialog.isVisible());
}

void OptionsModelTests::dialogSideStakingToggleAsksOnce()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    node.m_settings["enablesidestaking"] = "0";
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(true, false, true), true, &node.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());
    QVERIFY(!widgets.side_staking->isChecked());

    // The toggle submits the dialog at once.
    MessageBoxCloser boxes;
    widgets.side_staking->setChecked(true);

    QCOMPARE(wallet.Prompts(), 1);
    QCOMPARE(wallet.m_changes_sent_at_prompt, std::vector<size_t>{0});
    QVERIFY(boxes.m_texts.isEmpty());
    QCOMPARE(node.getSettingStr("enablesidestaking", ""), std::string("1"));
    QVERIFY(widgets.side_staking->isChecked());
}

void OptionsModelTests::dialogDeclinedSideStakingToggleReverts()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    node.m_settings["enablesidestaking"] = "0";
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(true, false, true), false, &node.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    MessageBoxCloser boxes;
    widgets.side_staking->setChecked(true);

    // Not saved, so the box shows side staking off again, with the side-stake
    // buttons that go with that, and the revert submits nothing.
    QCOMPARE(wallet.Prompts(), 1);
    QCOMPARE(boxes.m_texts, QStringList{PASSPHRASE_NEEDED_MESSAGE});
    QCOMPARE(node.m_change_calls.size(), size_t{0});
    QVERIFY(!widgets.side_staking->isChecked());
    QVERIFY(widgets.new_side_stake->isHidden());
    QVERIFY(dialog.isVisible());
}

void OptionsModelTests::dialogProxyHostEditWithTheProxyOffDoesNotAsk()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    node.m_needs_elevation = ELEVATE_SETTINGS;
    OptionsModel model(node, sidestakes);
    PromptedWallet wallet(model, &node, LockState(true, false, true), true, &node.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    DialogWidgets widgets(dialog);
    QVERIFY(widgets.AllFound());

    // The host can be edited only with the box ticked: tick, edit, untick.
    widgets.proxy->setChecked(true);
    widgets.proxy_host->setText(QStringLiteral("10.0.0.1"));
    widgets.proxy->setChecked(false);

    MessageBoxCloser boxes;
    widgets.ok->click();

    // The node's proxy stays unset: the empty proxy sent for the host edit
    // erases nothing, and needs no passphrase.
    QCOMPARE(wallet.Prompts(), 0);
    QVERIFY(boxes.m_texts.isEmpty());
    QCOMPARE(node.m_change_calls, (std::vector<SettingsBatch>{{{"proxy", ""}}}));
    QVERIFY(!node.isSettingSet("proxy"));
    QCOMPARE(QSettings().value(GUIUtil::nodeSettingsKey("addrProxy")).toString(), QStringLiteral("10.0.0.1:9050"));
    QVERIFY(!dialog.isVisible());
}

void OptionsModelTests::editSideStakeDeclinedShowsMessageAndStaysOpen_data()
{
    QTest::addColumn<bool>("editExisting");
    QTest::newRow("new") << false;
    QTest::newRow("edit") << true;
}

void OptionsModelTests::editSideStakeDeclinedShowsMessageAndStaysOpen()
{
    QFETCH(bool, editExisting);

    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    sidestakes.m_needs_elevation = true;
    sidestakes.m_edit_result.status = interfaces::SideStakeEditStatus::OK;
    sidestakes.m_edit_result.address = "encodedAddr";
    if (editExisting) sidestakes.m_snapshot.entries.push_back(LocalSideStake("rawAddr"));
    OptionsModel model(node, sidestakes);
    SideStakeTableModel* table = model.getSideStakeTableModel();
    table->refresh();

    int asks = 0;
    InstallPrompt(model, asks, false, sidestakes.m_elevated);

    EditSideStakeDialog dialog(editExisting ? EditSideStakeDialog::EditSideStake : EditSideStakeDialog::NewSideStake);
    dialog.setModel(table);
    QLineEdit* address = Find<QLineEdit>(dialog, "addressLineEdit");
    QLineEdit* allocation = Find<QLineEdit>(dialog, "allocationLineEdit");
    QLineEdit* description = Find<QLineEdit>(dialog, "descriptionLineEdit");
    QVERIFY(address && allocation && description);
    if (editExisting) {
        dialog.loadRow(0);
    } else {
        address->setText(QStringLiteral("rawAddr"));
    }
    allocation->setText(QStringLiteral("20"));
    description->setText(QStringLiteral("d"));
    dialog.show();

    MessageBoxCloser boxes;
    dialog.accept();

    QCOMPARE(asks, 1);
    QCOMPARE(sidestakes.m_edit_calls, 1);
    QCOMPARE(table->getEditStatus(), SideStakeTableModel::ELEVATION_REQUIRED);
    QCOMPARE(boxes.m_texts, QStringList{SIDE_STAKE_PASSPHRASE_MESSAGE});
    QVERIFY(dialog.isVisible());
}

void OptionsModelTests::deleteSideStakeDeclinedRemovesNothing()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    sidestakes.m_needs_elevation = true;
    sidestakes.m_edit_result.status = interfaces::SideStakeEditStatus::OK;
    sidestakes.m_snapshot.entries.push_back(LocalSideStake("rawAddr"));
    OptionsModel model(node, sidestakes);

    int asks = 0;
    InstallPrompt(model, asks, false, sidestakes.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    QTableView* view = Find<QTableView>(dialog, "sidestakingTableView");
    QPushButton* remove = Find<QPushButton>(dialog, "pushButtonDeleteSideStake");
    QVERIFY(view && remove);
    QCOMPARE(view->model()->rowCount(), 1);

    view->selectRow(0);
    QVERIFY(remove->isEnabled());

    MessageBoxCloser boxes;
    remove->click();

    // Refused once, asked once, not sent again, and the row is still there.
    QCOMPARE(asks, 1);
    QCOMPARE(sidestakes.m_edit_calls, 1);
    QCOMPARE(boxes.m_texts, QStringList{SIDE_STAKE_PASSPHRASE_MESSAGE});
    QCOMPARE(view->model()->rowCount(), 1);
    QVERIFY(dialog.isVisible());
}

void OptionsModelTests::tableSideStakeEditDeclinedShowsTheMessage()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    sidestakes.m_needs_elevation = true;
    sidestakes.m_edit_result.status = interfaces::SideStakeEditStatus::OK;
    sidestakes.m_snapshot.entries.push_back(LocalSideStake("rawAddr"));
    OptionsModel model(node, sidestakes);

    int asks = 0;
    InstallPrompt(model, asks, false, sidestakes.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    QTableView* view = Find<QTableView>(dialog, "sidestakingTableView");
    QVERIFY(view);

    // Edit the allocation in the table, as a double click does, and press Enter.
    const QModelIndex allocation = view->model()->index(0, SideStakeTableModel::Allocation);
    view->setCurrentIndex(allocation);
    view->edit(allocation);
    QLineEdit* editor = qobject_cast<QLineEdit*>(view->indexWidget(allocation));
    QVERIFY(editor);
    editor->setText(QStringLiteral("20"));

    MessageBoxCloser boxes;
    QTest::keyClick(editor, Qt::Key_Return);

    // Shown once the editor has closed, and once.
    QTRY_VERIFY(!boxes.m_texts.isEmpty());
    QTest::qWait(100);
    QCOMPARE(boxes.m_texts, QStringList{SIDE_STAKE_PASSPHRASE_MESSAGE});
    QCOMPARE(asks, 1);
    QCOMPARE(sidestakes.m_edit_calls, 1);
    QVERIFY(view->indexWidget(allocation) == nullptr);
    QVERIFY(dialog.isVisible());
}

void OptionsModelTests::tableSideStakeEditRefusedTwiceShowsTheMessageOnce()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    sidestakes.m_needs_elevation = true;
    sidestakes.m_edit_result.status = interfaces::SideStakeEditStatus::OK;
    sidestakes.m_snapshot.entries.push_back(LocalSideStake("rawAddr"));
    OptionsModel model(node, sidestakes);

    int asks = 0;
    InstallPrompt(model, asks, false, sidestakes.m_elevated);

    OptionsDialog dialog;
    dialog.setModel(&model);
    dialog.show();
    QTableView* view = Find<QTableView>(dialog, "sidestakingTableView");
    QVERIFY(view);

    const QModelIndex allocation = view->model()->index(0, SideStakeTableModel::Allocation);
    view->setCurrentIndex(allocation);
    view->edit(allocation);
    QLineEdit* editor = qobject_cast<QLineEdit*>(view->indexWidget(allocation));
    QVERIFY(editor);
    editor->setText(QStringLiteral("20"));

    MessageBoxCloser boxes;

    // The delegate commits the editor twice before the event loop runs, as
    // Enter and the editor losing focus can, and each commit is refused.
    Q_EMIT view->itemDelegate()->commitData(editor);
    Q_EMIT view->itemDelegate()->commitData(editor);
    QCOMPARE(sidestakes.m_edit_calls, 2);

    QTRY_VERIFY(!boxes.m_texts.isEmpty());
    QTest::qWait(100);
    QCOMPARE(boxes.m_texts, QStringList{SIDE_STAKE_PASSPHRASE_MESSAGE});
    QVERIFY(dialog.isVisible());
}
