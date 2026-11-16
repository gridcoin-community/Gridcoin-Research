// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "walletpromptqmltests.h"

#include "interfacefakes_qml.h"
#include "modelharness.h"
#include "qmltestutil.h"

#include <amount.h>
#include <chainparams.h>
#include <interfaces/wallet.h>
#include <interfaces/wallet_coin_source.h>
#include <key_io.h>
#include <pubkey.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/quick/clientstatus.h>
#include <qt/quick/clipboard.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/sendadapter.h>
#include <qt/quick/sendrecipientmodel.h>
#include <qt/quick/walletopgate.h>
#include <qt/quick/walletpromptlatch.h>
#include <qt/quick/walletsummary.h>
#include <qt/test/interfacefakes.h>
#include <qt/test/modelfakes.h>
#include <qt/walletmodel.h>
#include <uint256.h>
#include <util.h>

#include <QAbstractItemModel>
#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QFont>
#include <QMetaObject>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QThreadPool>
#include <QTranslator>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using Owner = WalletPromptLatch::Owner;
using Role = SendRecipientModel::Role;

//! The carried root, as gridcoinresearch-qml loads it.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

const std::string PASSPHRASE = "correct horse";
const std::string TXID = "abcdef0123456789";
constexpr qint64 FEE = 123456789;

QString Pass()
{
    return QString::fromStdString(PASSPHRASE);
}

//! A valid address of the selected chain, distinct for each `b`.
QString Address(unsigned char b)
{
    return QString::fromStdString(EncodeDestination(CKeyID(uint160(std::vector<unsigned char>(20, b)))));
}

QString Uri(const QString& address, const QString& query = QString())
{
    return QStringLiteral("gridcoin:") + address + (query.isEmpty() ? QString() : QStringLiteral("?") + query);
}

interfaces::SendCoinsResult Result(interfaces::SendCoinsStatus status, int64_t fee = 0, std::string txid = {})
{
    interfaces::SendCoinsResult result;
    result.status = status;
    result.fee = fee;
    result.txid_hex = std::move(txid);
    return result;
}

interfaces::SendCoinsResult FeeReq(qint64 fee)
{
    return Result(interfaces::SendCoinsStatus::FeeConfirmationRequired, fee);
}

interfaces::SendCoinsResult Ok()
{
    return Result(interfaces::SendCoinsStatus::OK, 0, TXID);
}

QString SendTitle()
{
    return QCoreApplication::translate("SendCoinsDialog", "Send Coins");
}

QString BusyMessage()
{
    return QCoreApplication::translate("SendCoinsDialog", "Another wallet operation is in progress. Please try again when it has finished.");
}

QString DuplicateMessage()
{
    return QCoreApplication::translate("SendCoinsDialog",
                                       "Duplicate address found, can only send to each address once per send operation.");
}

QString UriText()
{
    return QCoreApplication::translate("BitcoinGUI", "URI can not be parsed! This can be caused by an invalid Gridcoin address or malformed URI parameters.");
}

//! A runner whose policy treats no exception as a disconnect.
QmlCallRunner::DisconnectPolicy NoDisconnects()
{
    return QmlCallRunner::DisconnectPolicy{[](const std::string&) { return false; }, [](const std::string&) {}};
}

//! Keeps a closed QML window from quitting the application for the life of a
//! test.
class NoQuitOnLastWindow
{
public:
    NoQuitOnLastWindow() : m_old(qApp->quitOnLastWindowClosed()) { qApp->setQuitOnLastWindowClosed(false); }
    ~NoQuitOnLastWindow() { qApp->setQuitOnLastWindowClosed(m_old); }
    NoQuitOnLastWindow(const NoQuitOnLastWindow&) = delete;
    NoQuitOnLastWindow& operator=(const NoQuitOnLastWindow&) = delete;

private:
    bool m_old;
};

//! Joins the global pool, so no pooled read outlives the harness's fakes.
void ReapPool()
{
    QThreadPool::globalInstance()->waitForDone();
}

//! Posts a no-op on the serial lane and pumps until it is delivered.
bool SerialProbe(QmlCallRunner& runner)
{
    auto delivered = std::make_shared<bool>(false);
    if (!runner.postSerial([] {}, [delivered] { *delivered = true; })) return false;
    return qml_test::PumpUntil([delivered] { return *delivered; }, 2s);
}

//! Forces -messagecontractdisableheight for the life of the object.
class ForcedDisableHeight
{
public:
    explicit ForcedDisableHeight(const std::string& height)
    {
        gArgs.ForceSetArg("-messagecontractdisableheight", height);
    }
    ~ForcedDisableHeight() { gArgs.ClearForcedArg("-messagecontractdisableheight"); }
    ForcedDisableHeight(const ForcedDisableHeight&) = delete;
    ForcedDisableHeight& operator=(const ForcedDisableHeight&) = delete;
};

//! The object named `name` under `parent`, or null.
QObject* Child(QObject* parent, const char* name)
{
    return parent ? parent->findChild<QObject*>(QString::fromLatin1(name)) : nullptr;
}

QString Text(QObject* object)
{
    return object ? object->property("text").toString() : QString();
}

bool Visible(QObject* object)
{
    return object && object->property("visible").toBool();
}

//! Clicks a button by emitting its clicked signal through its meta-object.
bool Click(QObject* button)
{
    return button && QMetaObject::invokeMethod(button, "clicked");
}

//! Clicks the centre of an item with a real mouse press and release on its
//! window.
bool MouseClick(QObject* object)
{
    QQuickItem* item = qobject_cast<QQuickItem*>(object);
    if (!item || !item->window()) return false;
    const QPointF centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    QTest::mouseClick(item->window(), Qt::LeftButton, Qt::NoModifier, centre.toPoint());
    return true;
}

bool OpenPopup(QObject* popup)
{
    return popup && QMetaObject::invokeMethod(popup, "open");
}

bool ClosePopup(QObject* popup)
{
    return popup && QMetaObject::invokeMethod(popup, "close");
}

//! The delegate of `row` in a ListView, once it exists; null after 2 s.
QQuickItem* Delegate(QObject* list, int row)
{
    QQuickItem* item = nullptr;
    qml_test::PumpUntil(
        [&] {
            item = nullptr;
            QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, row));
            return item != nullptr;
        },
        2s);
    return item;
}

QVariant RoleOf(SendAdapter& adapter, int row, int role)
{
    QAbstractItemModel* model = adapter.recipients();
    return model->data(model->index(row, 0), role);
}

//! Sets row `row`'s recipient and amount text.
bool Fill(SendAdapter& adapter, int row, const QString& address, const QString& amount = QStringLiteral("1"))
{
    return adapter.updateRecipient(row, QVariantMap{{QStringLiteral("recipient"), address},
                                                    {QStringLiteral("amount"), amount}});
}

SendAdapter::Outcome OutcomeOf(const QSignalSpy& finished, int index)
{
    return finished.at(index).at(1).value<SendAdapter::Outcome>();
}

//! Hands the adapter the bridge's URIs as QmlFrontEnd::attachModels does.
void Wire(SendAdapter& adapter, QmlCoreBridge& bridge)
{
    adapter.setUriSource([&bridge] { return bridge.takePendingUri(); },
                         [&bridge](const QString& uri) { bridge.handleURI(uri); });
    QObject::connect(&bridge, &QmlCoreBridge::pendingUriChanged, &adapter, &SendAdapter::claimPendingUri);
    adapter.claimPendingUri();
}

//! A wrapper window, `qrc:/qmltest/<name>.qml`, holding `body`. Null, with the
//! errors in `errors`, when it does not load.
std::unique_ptr<QObject> LoadWrapper(QQmlEngine& engine, const QString& name, const QByteArray& body, QString& errors)
{
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    const QByteArray source = "import QtQuick\nimport QtQuick.Controls\nimport Gridcoin.App\n"
                              "ApplicationWindow {\n"
                              "    id: wrapper\n"
                              "    width: 900\n"
                              "    height: 700\n"
                              "    visible: true\n" +
                              body + "\n}\n";
    QQmlComponent component(&engine);
    component.setData(source, QUrl(QStringLiteral("qrc:/qmltest/%1.qml").arg(name)));
    std::unique_ptr<QObject> object(component.create());
    if (!object) errors = component.errorString();
    return object;
}

//! The item that draws a button's label: its contentItem when that has a
//! contentWidth, else the contentItem's child that has one (an IconLabel's
//! text item).
QQuickItem* LabelItem(QObject* button)
{
    QQuickItem* content = button ? button->property("contentItem").value<QQuickItem*>() : nullptr;
    if (!content) return nullptr;
    if (content->metaObject()->indexOfProperty("contentWidth") >= 0) return content;
    for (QQuickItem* child : content->childItems()) {
        if (child->metaObject()->indexOfProperty("contentWidth") >= 0) return child;
    }
    return nullptr;
}

//! Checks that a button's label is drawn as `plain`, with no mnemonic marker:
//! its width equals a Text reading `plain` in the label's font, and differs
//! from one reading `marked`. Empty when it holds; else what failed.
QString CheckLabelWidth(QQmlEngine& engine, QObject* button, const QString& plain, const QString& marked)
{
    QQuickItem* label = LabelItem(button);
    if (!label) return QStringLiteral("no label item under the button");
    const QFont font = label->property("font").value<QFont>();

    QQmlComponent component(&engine);
    component.setData("import QtQuick\nItem {\n    property alias plain: plain\n    property alias marked: marked\n"
                      "    Text { id: plain }\n    Text { id: marked }\n}\n",
                      QUrl(QStringLiteral("qrc:/qmltest/labelreference.qml")));
    std::unique_ptr<QObject> reference(component.create());
    if (!reference) return QStringLiteral("the reference did not load: ") + component.errorString();
    QObject* plain_text = reference->property("plain").value<QObject*>();
    QObject* marked_text = reference->property("marked").value<QObject*>();
    if (!plain_text || !marked_text) return QStringLiteral("the reference texts were not found");
    plain_text->setProperty("font", font);
    plain_text->setProperty("text", plain);
    marked_text->setProperty("font", font);
    marked_text->setProperty("text", marked);
    const qreal plain_width = plain_text->property("contentWidth").toReal();
    const qreal marked_width = marked_text->property("contentWidth").toReal();
    // The precondition: in this font the marker changes the width.
    if (qFuzzyCompare(plain_width, marked_width)) {
        return QStringLiteral("reference precondition: \"%1\" and \"%2\" are both %3 wide")
            .arg(plain, marked)
            .arg(plain_width);
    }
    const qreal width = label->property("contentWidth").toReal();
    if (width <= 0) return QStringLiteral("the label has no width");
    if (!qFuzzyCompare(width, plain_width)) {
        return QStringLiteral("the label is %1 wide; \"%2\" is %3 and \"%4\" is %5")
            .arg(width)
            .arg(plain)
            .arg(plain_width)
            .arg(marked)
            .arg(marked_width);
    }
    return QString();
}

//! A translator that marks the strings of its contexts with "[<context>] ",
//! so a test can tell which context a string was looked up in.
class MarkingTranslator : public QTranslator
{
public:
    explicit MarkingTranslator(QStringList contexts) : m_contexts(std::move(contexts)) {}

    QString translate(const char* context, const char* source_text, const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        const QString name = QString::fromLatin1(context);
        if (!m_contexts.contains(name)) return QString();
        return QStringLiteral("[%1] ").arg(name) + QString::fromUtf8(source_text);
    }

    bool isEmpty() const override { return false; }

private:
    QStringList m_contexts;
};

//! A translator for OptionsDialog's OK and Cancel only, with the mnemonic
//! forms other locales use: a "(&X)" group, once after a space.
class MnemonicTranslator : public QTranslator
{
public:
    QString translate(const char* context, const char* source_text, const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (QByteArray(context) != "OptionsDialog") return QString();
        if (QByteArray(source_text) == "&OK") return QString::fromUtf8("确定(&O)");
        if (QByteArray(source_text) == "&Cancel") return QString::fromUtf8("キャンセル (&C)");
        return QString();
    }

    bool isEmpty() const override { return false; }
};

//! What a wrapper test of the prompts declares before its engine: the
//! scripted wallet and its models, a runner, a provider over a queuing front
//! end (installed), the gate and the adapter attached, and the teardown that
//! drains the runner and then detaches the adapter and the gate.
struct PromptHarness {
    PromptHarness()
    {
        wallet.setPassphrase(PASSPHRASE);
        QmlProvider::install(&provider);
        provider.walletOps().attach(wallet, runner);
        provider.sendAdapter().attach(model, nullptr, runner);
    }
    PromptHarness(const PromptHarness&) = delete;
    PromptHarness& operator=(const PromptHarness&) = delete;

    SendAdapter& adapter() { return provider.sendAdapter(); }

    qt_test::ScriptedWallet wallet;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager side_stakes;
    OptionsModel options{node, side_stakes};
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source{interfaces::MakeSyntheticCoinSource(0, 1)};
    WalletModel model{wallet, tx_source, *coin_source, &options};
    QmlCallRunner runner{NoDisconnects()};
    qml_test::QueuingFrontEnd frontend;
    QmlProvider provider{frontend};
    qml_test::ScopeExit uninstall{[] { QmlProvider::install(nullptr); }};
    qml_test::ScopeExit teardown{[this] {
        runner.drain();
        provider.sendAdapter().detach();
        provider.walletOps().detach();
    }};
};

//! The prompts' wrapper: WalletPrompts, and a function that asks the
//! out-of-sync question with two callbacks recording the answer.
const QByteArray PROMPTS_BODY = "    property string syncAnswer: \"\"\n"
                                "    function askSync() {\n"
                                "        prompts.askOutOfSync(function () { wrapper.syncAnswer = \"yes\" },\n"
                                "                             function () { wrapper.syncAnswer = \"no\" })\n"
                                "    }\n"
                                "    WalletPrompts { id: prompts; objectName: \"walletPrompts\" }";

//! What GuiMain does before its event loop, with the models attached: the
//! splash, the root, the models, then the main window. The core bridge, or
//! null when the root did not load.
QObject* Preamble(QmlFrontEnd& fe, qml_test::ModelHarness& harness)
{
    fe.createSplash();
    QObject* bridge = nullptr;
    try {
        bridge = fe.construct();
    } catch (const std::exception& e) {
        qWarning().noquote() << "construct() threw:" << e.what();
        return nullptr;
    }
    if (!fe.rootObject()) return nullptr;
    fe.attachModels(harness.bundle());
    fe.showMain(false);
    return bridge;
}

//! The object named `name` under the root's main window.
QObject* MainItem(QmlFrontEnd& fe, const char* name)
{
    QObject* root = fe.rootObject();
    QObject* main = root ? root->property("mainWindow").value<QObject*>() : nullptr;
    return Child(main, name);
}

QString LoaderSource(QObject* loader)
{
    return loader ? loader->property("source").toUrl().toString() : QString();
}

int64_t Now()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

//! Puts the harness's node in sync with the network and waits for the
//! front end's status to say so.
bool BringInSync(qml_test::ModelHarness& harness, QmlFrontEnd& fe)
{
    harness.node.m_node_count = 8;
    harness.node.m_last_block_time = Now() - 60;
    emit harness.client.numConnectionsChanged(8);
    emit harness.client.numBlocksChanged(100, 100);
    return qml_test::PumpUntil([&] { return fe.provider().clientStatus().inSync(); }, 2s);
}

//! Switches the tab menu to `path` as a click on its entry does.
bool Select(QObject* menu, const QString& path)
{
    return menu && QMetaObject::invokeMethod(menu, "select", Q_ARG(QVariant, QVariant(path)));
}

} // namespace

void QmlWalletPromptTests::initTestCase()
{
    // The tests that build OptionsModel by hand keep its settings out of the
    // user's store, as the Qt tests' OptionsModelTests does.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName(QStringLiteral("GridcoinTest"));
    QCoreApplication::setApplicationName(QStringLiteral("walletpromptqmltests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());

    SelectParams(CBaseChainParams::REGTEST);
}

void QmlWalletPromptTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void QmlWalletPromptTests::init()
{
    // OptionsModel::Init reaches GetDataDir(false), which creates a datadir:
    // point -datadir at a temporary directory.
    m_prev_datadir_set = gArgs.IsArgSet("-datadir");
    m_prev_datadir = gArgs.GetArg("-datadir", "");
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void QmlWalletPromptTests::cleanup()
{
    // A model harness points the settings at its own directory and cannot
    // put the path back (QSettings has no getter), so this class's path is
    // applied again first.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());
    QSettings().clear();
    if (m_prev_datadir_set) {
        gArgs.ForceSetArg("-datadir", m_prev_datadir);
    } else {
        gArgs.ClearForcedArg("-datadir");
    }
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

void QmlWalletPromptTests::unlockDialogBinds()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::unlockDialogBinds");
    NoQuitOnLastWindow no_quit;
    qt_test::ScriptedWallet wallet;
    qt_test::FakeLatch unlock_latch;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    QmlCallRunner runner(NoDisconnects());
    qml_test::QueuingFrontEnd frontend;
    QmlProvider provider(frontend);
    QmlProvider::install(&provider);
    qml_test::ScopeExit uninstall([] { QmlProvider::install(nullptr); });
    WalletOpGate& gate = provider.walletOps();
    gate.attach(wallet, runner);
    qml_test::ScopeExit teardown([&] {
        runner.drain();
        gate.detach();
    });
    qml_test::ScopeExit unlatch([&] {
        unlock_latch.open = true;
        wallet.m_unlock_latch = nullptr;
    });

    QQmlEngine engine;
    QString errors;
    std::unique_ptr<QObject> window = LoadWrapper(engine, QStringLiteral("unlockwrapper"), "    UnlockDialog { }", errors);
    QVERIFY2(window, qPrintable(errors));
    QQuickWindow* quick_window = qobject_cast<QQuickWindow*>(window.get());
    QVERIFY(quick_window);
    QVERIFY(qml_test::PumpUntil([&] { return quick_window->isExposed(); }, 2s));
    QObject* dialog = Child(window.get(), "unlockDialog");
    QVERIFY(dialog);
    QSignalSpy failed(&gate, &WalletOpGate::unlockFailed);
    QSignalSpy accepted(&gate, &WalletOpGate::unlockAccepted);

    bool granted = false;
    const quint64 id = gate.begin([&granted](quint64) { granted = true; }, [](quint64, WalletOpGate::Result) {});
    QVERIFY(id != 0);
    QVERIFY2(qml_test::PumpUntil([&] { return Visible(dialog); }, 2s), "the request did not open the dialog");
    QObject* text = Child(dialog, "unlockText");
    QObject* field = Child(dialog, "unlockPassphrase");
    QObject* ok = Child(dialog, "unlockOk");
    QObject* failure = Child(dialog, "unlockFailure");
    QVERIFY(text);
    QVERIFY(field);
    QVERIFY(ok);
    QVERIFY(failure);
    QCOMPARE(Text(text),
             QCoreApplication::translate("AskPassphraseDialog", "This operation needs your wallet passphrase. The wallet is unlocked for the operation and returns to its previous state afterwards."));

    // A wrong passphrase: the dialog stays open with the failure, and the
    // field is empty.
    field->setProperty("text", QStringLiteral("wrong"));
    QVERIFY(MouseClick(ok));
    QVERIFY(qml_test::PumpUntil([&] { return failed.count() == 1 || !Visible(dialog); }, 2s));
    QVERIFY2(Visible(dialog), "closed after a wrong passphrase");
    QCOMPARE(Text(failure),
             QCoreApplication::translate("AskPassphraseDialog", "The passphrase entered for the wallet decryption was incorrect."));
    QCOMPARE(Text(field), QString());

    // The right one, its unlock held on the latch: the field is emptied at
    // once and the dialog waits for the result.
    wallet.m_unlock_latch = &unlock_latch;
    field->setProperty("text", Pass());
    QVERIFY(MouseClick(ok));
    QVERIFY(qml_test::PumpUntil([&] { return wallet.unlockCalls() == 2; }, 2s));
    QCOMPARE(Text(field), QString());
    QVERIFY(Visible(dialog));

    unlock_latch.open = true;
    QVERIFY(qml_test::PumpUntil([&] { return accepted.count() == 1; }, 2s));
    QCOMPARE(Text(field), QString());
    QVERIFY2(qml_test::PumpUntil([&] { return !Visible(dialog); }, 2s), "the dialog stayed open after the unlock");
    QVERIFY(granted);
    gate.end(id);
}

void QmlWalletPromptTests::unlockDialogCloseCancels()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::unlockDialogCloseCancels");
    NoQuitOnLastWindow no_quit;
    qt_test::ScriptedWallet wallet;
    wallet.setPassphrase(PASSPHRASE);
    wallet.setLockState(true, true, false);
    QmlCallRunner runner(NoDisconnects());
    qml_test::QueuingFrontEnd frontend;
    QmlProvider provider(frontend);
    QmlProvider::install(&provider);
    qml_test::ScopeExit uninstall([] { QmlProvider::install(nullptr); });
    WalletOpGate& gate = provider.walletOps();
    gate.attach(wallet, runner);
    qml_test::ScopeExit teardown([&] {
        runner.drain();
        gate.detach();
    });

    QQmlEngine engine;
    QString errors;
    std::unique_ptr<QObject> window = LoadWrapper(engine, QStringLiteral("unlockwrapper"), "    UnlockDialog { }", errors);
    QVERIFY2(window, qPrintable(errors));
    QObject* dialog = Child(window.get(), "unlockDialog");
    QVERIFY(dialog);
    QSignalSpy finished(&gate, &WalletOpGate::finished);

    bool refused = false;
    const quint64 id = gate.begin(
        [&gate, &wallet](quint64 granted_id) {
            gate.post(granted_id, [&wallet](interfaces::Wallet&) { wallet.recordOp(); }, [] {});
        },
        [&refused](quint64, WalletOpGate::Result) { refused = true; });
    QVERIFY(id != 0);
    QVERIFY2(qml_test::PumpUntil([&] { return Visible(dialog); }, 2s), "the request did not open the dialog");

    // Closed without Cancel, as Escape or a closed window closes it.
    QVERIFY(ClosePopup(dialog));
    QVERIFY2(qml_test::PumpUntil([&] { return finished.count() == 1; }, 2s), "closing the dialog did not cancel");
    QCOMPARE(finished.at(0).at(0).toULongLong(), id);
    QCOMPARE(finished.at(0).at(1).value<WalletOpGate::Result>(), WalletOpGate::Cancelled);
    QVERIFY(refused);
    QVERIFY(SerialProbe(runner));
    QCOMPARE(wallet.opCalls(), 0);
    QCOMPARE(wallet.unlockCalls(), 0);
}

void QmlWalletPromptTests::promptsAnswerEveryClose()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::promptsAnswerEveryClose");
    NoQuitOnLastWindow no_quit;

    {
        PromptHarness h;
        QQmlEngine engine;
        QString errors;
        std::unique_ptr<QObject> window = LoadWrapper(engine, QStringLiteral("promptswrapper"), PROMPTS_BODY, errors);
        QVERIFY2(window, qPrintable(errors));
        QQuickWindow* quick_window = qobject_cast<QQuickWindow*>(window.get());
        QVERIFY(quick_window);
        QVERIFY(qml_test::PumpUntil([&] { return quick_window->isExposed(); }, 2s));
        QObject* result_prompt = Child(window.get(), "resultPrompt");
        QObject* send_prompt = Child(window.get(), "sendConfirmPrompt");
        QObject* fee_prompt = Child(window.get(), "feePrompt");
        QObject* sync_prompt = Child(window.get(), "outOfSyncPrompt");
        QVERIFY(result_prompt);
        QVERIFY(send_prompt);
        QVERIFY(fee_prompt);
        QVERIFY(sync_prompt);

        // OK is drawn without a mnemonic marker.
        QVERIFY(OpenPopup(result_prompt));
        QVERIFY(qml_test::PumpUntil([&] { return Visible(result_prompt); }, 2s));
        QObject* result_ok = Child(result_prompt, "promptOk");
        QVERIFY(result_ok);
        QCOMPARE(Text(result_ok), QStringLiteral("OK"));
        const QString ok_width = CheckLabelWidth(engine, result_ok, QStringLiteral("OK"), QStringLiteral("&OK"));
        QVERIFY2(ok_width.isEmpty(), qPrintable(ok_width));
        QVERIFY(ClosePopup(result_prompt));
        QVERIFY(qml_test::PumpUntil([&] { return !Visible(result_prompt); }, 2s));

        SendAdapter& adapter = h.adapter();
        QSignalSpy fee_asked(&adapter, &SendAdapter::feeConfirmationRequested);
        QSignalSpy finished(&adapter, &SendAdapter::finished);
        h.wallet.scriptSend({FeeReq(FEE), FeeReq(FEE + 1)});
        QVERIFY(Fill(adapter, 0, Address(1)));

        // The confirmation's Yes, then the fee question's Yes; then the fee
        // question again, closed without an answer, which answers no.
        const quint64 first = adapter.send();
        QVERIFY(first != 0);
        QVERIFY(qml_test::PumpUntil([&] { return Visible(send_prompt); }, 2s));
        QCOMPARE(send_prompt->property("modal").toBool(), true);
        QVERIFY(Click(Child(send_prompt, "promptYes")));
        QVERIFY2(qml_test::PumpUntil([&] { return fee_asked.count() == 1 && Visible(fee_prompt); }, 2s),
                 "the fee question was not shown");
        QVERIFY(Click(Child(fee_prompt, "promptYes")));
        QVERIFY2(qml_test::PumpUntil([&] { return fee_asked.count() == 2 && Visible(fee_prompt); }, 2s),
                 "the fee question's Yes did not send again");
        QVERIFY(ClosePopup(fee_prompt));
        QVERIFY2(qml_test::PumpUntil([&] { return finished.count() == 1; }, 2s), "closing the fee question did not answer it");
        QCOMPARE(finished.at(0).at(0).toULongLong(), first);
        QCOMPARE(OutcomeOf(finished, 0), SendAdapter::Aborted);
        QCOMPARE(h.wallet.sendCalls(), 2);
        const std::vector<qt_test::ScriptedWallet::Call> log = h.wallet.log();
        std::vector<int64_t> fees;
        for (const qt_test::ScriptedWallet::Call& call : log) {
            if (call.method == qt_test::ScriptedWallet::Method::SendCoins) fees.push_back(call.accepted_fee);
        }
        QCOMPARE(fees, (std::vector<int64_t>{0, FEE}));

        // The confirmation closed without an answer answers no. Its negative
        // button reads Cancel, without a mnemonic marker.
        QVERIFY(Fill(adapter, 0, Address(1)));
        const quint64 second = adapter.send();
        QVERIFY(second != 0);
        QVERIFY(qml_test::PumpUntil([&] { return Visible(send_prompt); }, 2s));
        QObject* send_no = Child(send_prompt, "promptNo");
        QVERIFY(send_no);
        QCOMPARE(Text(send_no), QStringLiteral("Cancel"));
        const QString cancel_width =
            CheckLabelWidth(engine, send_no, QStringLiteral("Cancel"), QStringLiteral("&Cancel"));
        QVERIFY2(cancel_width.isEmpty(), qPrintable(cancel_width));
        QVERIFY(ClosePopup(send_prompt));
        QVERIFY2(qml_test::PumpUntil([&] { return finished.count() == 2; }, 2s), "closing the confirmation did not answer it");
        QCOMPARE(finished.at(1).at(0).toULongLong(), second);
        QCOMPARE(OutcomeOf(finished, 1), SendAdapter::Aborted);
        QCOMPARE(h.wallet.sendCalls(), 2);

        // The out-of-sync question reads No, and Return answers it No.
        quick_window->requestActivate();
        qml_test::PumpUntil([&] { return quick_window->isActive(); }, 2s);
        QVERIFY(QMetaObject::invokeMethod(window.get(), "askSync"));
        QVERIFY(qml_test::PumpUntil([&] { return Visible(sync_prompt); }, 2s));
        QObject* sync_no = Child(sync_prompt, "promptNo");
        QVERIFY(sync_no);
        QCOMPARE(Text(sync_no), QStringLiteral("No"));
        QTest::keyClick(quick_window, Qt::Key_Return);
        QVERIFY2(qml_test::PumpUntil([&] { return !window->property("syncAnswer").toString().isEmpty(); }, 2s),
                 "Return did not answer the out-of-sync question");
        QCOMPARE(window->property("syncAnswer").toString(), QStringLiteral("no"));
    }

    // The labels are looked up in OptionsDialog's context.
    {
        MarkingTranslator translator({QStringLiteral("OptionsDialog")});
        QCoreApplication::installTranslator(&translator);
        qml_test::ScopeExit remove_translator([&translator] { QCoreApplication::removeTranslator(&translator); });
        PromptHarness h;
        QQmlEngine engine;
        QString errors;
        std::unique_ptr<QObject> window = LoadWrapper(engine, QStringLiteral("promptswrapper"), PROMPTS_BODY, errors);
        QVERIFY2(window, qPrintable(errors));
        QObject* result_ok = Child(Child(window.get(), "resultPrompt"), "promptOk");
        QObject* send_no = Child(Child(window.get(), "sendConfirmPrompt"), "promptNo");
        QVERIFY(result_ok);
        QVERIFY(send_no);
        QCOMPARE(Text(result_ok), QStringLiteral("[OptionsDialog] OK"));
        QCOMPARE(Text(send_no), QStringLiteral("[OptionsDialog] Cancel"));
    }

    // A translation's "(&X)" group goes, with the space before it.
    {
        MnemonicTranslator translator;
        QCoreApplication::installTranslator(&translator);
        qml_test::ScopeExit remove_translator([&translator] { QCoreApplication::removeTranslator(&translator); });
        PromptHarness h;
        QQmlEngine engine;
        QString errors;
        std::unique_ptr<QObject> window = LoadWrapper(engine, QStringLiteral("promptswrapper"), PROMPTS_BODY, errors);
        QVERIFY2(window, qPrintable(errors));
        QObject* result_ok = Child(Child(window.get(), "resultPrompt"), "promptOk");
        QObject* send_no = Child(Child(window.get(), "sendConfirmPrompt"), "promptNo");
        QVERIFY(result_ok);
        QVERIFY(send_no);
        QCOMPARE(Text(result_ok), QString::fromUtf8("确定"));
        QCOMPARE(Text(send_no), QString::fromUtf8("キャンセル"));
    }
}

void QmlWalletPromptTests::resultPromptOpenRule()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::resultPromptOpenRule");
    NoQuitOnLastWindow no_quit;
    qml_test::QueuingFrontEnd bridge_fe;
    QmlProvider bridge_provider(bridge_fe);
    QmlCoreBridge bridge(bridge_provider.shell());
    PromptHarness h;
    SendAdapter& adapter = h.adapter();
    Wire(adapter, bridge);

    QQmlEngine engine;
    QString errors;
    std::unique_ptr<QObject> window = LoadWrapper(engine, QStringLiteral("promptswrapper"), PROMPTS_BODY, errors);
    QVERIFY2(window, qPrintable(errors));
    QObject* result_prompt = Child(window.get(), "resultPrompt");
    QObject* send_prompt = Child(window.get(), "sendConfirmPrompt");
    QVERIFY(result_prompt);
    QVERIFY(send_prompt);
    QObject* send_yes = Child(send_prompt, "promptYes");
    QObject* send_no = Child(send_prompt, "promptNo");
    QVERIFY(send_yes);
    QVERIFY(send_no);
    QSignalSpy finished(&adapter, &SendAdapter::finished);
    h.wallet.scriptSend({Ok()});

    // Busy, then the rejection of a URI that waited for the send: the second
    // result is shown after the first is closed, not over it.
    {
        WalletPromptLatch& latch = h.provider.walletPromptLatch();
        QVERIFY(latch.tryAcquire(Owner::Security));
        qml_test::ScopeExit release([&latch] { latch.release(Owner::Security); });
        QVERIFY(Fill(adapter, 0, Address(1)));
        QVERIFY(adapter.send() != 0);
        QVERIFY(qml_test::PumpUntil([&] { return Visible(send_prompt); }, 2s));
        bridge.handleURI(QStringLiteral("gridcoin:notanaddress"));
        QVERIFY(Click(send_yes));
        QVERIFY(qml_test::PumpUntil([&] { return Visible(result_prompt); }, 2s));
        QCOMPARE(result_prompt->property("title").toString(), SendTitle());
        QCOMPARE(Text(result_prompt), BusyMessage());
        QVERIFY(ClosePopup(result_prompt));
        QVERIFY(qml_test::PumpUntil([&] { return Visible(result_prompt) && Text(result_prompt) != BusyMessage(); }, 2s));
        QCOMPARE(Text(result_prompt), UriText());
        QVERIFY(ClosePopup(result_prompt));
        QVERIFY(qml_test::PumpUntil([&] { return !Visible(result_prompt); }, 2s));
    }

    // Failed: the message is shown.
    QVERIFY(adapter.addRecipient());
    QVERIFY(Fill(adapter, 1, Address(1)));
    QVERIFY(adapter.send() != 0);
    QVERIFY(qml_test::PumpUntil([&] { return Visible(send_prompt); }, 2s));
    QVERIFY(Click(send_yes));
    QVERIFY(qml_test::PumpUntil([&] { return Visible(result_prompt); }, 2s));
    QCOMPARE(Text(result_prompt), DuplicateMessage());
    QVERIFY(ClosePopup(result_prompt));
    QVERIFY(qml_test::PumpUntil([&] { return !Visible(result_prompt); }, 2s));

    // Sent: nothing is shown.
    QVERIFY(adapter.clearRecipients());
    QVERIFY(Fill(adapter, 0, Address(1)));
    const quint64 sent = adapter.send();
    QVERIFY(sent != 0);
    QVERIFY(qml_test::PumpUntil([&] { return Visible(send_prompt); }, 2s));
    QVERIFY(Click(send_yes));
    QVERIFY(qml_test::PumpUntil(
        [&] { return finished.count() > 0 && finished.at(finished.count() - 1).at(0).toULongLong() == sent; }, 2s));
    QCOMPARE(OutcomeOf(finished, finished.count() - 1), SendAdapter::Sent);
    qml_test::PumpFor(300ms);
    QVERIFY2(!Visible(result_prompt), "a sent transaction opened the result prompt");

    // Aborted: nothing is shown.
    QVERIFY(Fill(adapter, 0, Address(1)));
    const quint64 aborted = adapter.send();
    QVERIFY(aborted != 0);
    QVERIFY(qml_test::PumpUntil([&] { return Visible(send_prompt); }, 2s));
    QVERIFY(Click(send_no));
    QVERIFY(qml_test::PumpUntil(
        [&] { return finished.count() > 0 && finished.at(finished.count() - 1).at(0).toULongLong() == aborted; }, 2s));
    QCOMPARE(OutcomeOf(finished, finished.count() - 1), SendAdapter::Aborted);
    qml_test::PumpFor(300ms);
    QVERIFY2(!Visible(result_prompt), "an aborted send opened the result prompt");
}

void QmlWalletPromptTests::sendViewBinds()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::sendViewBinds");
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness(std::make_unique<qt_test::ScriptedWallet>());
    qml_test::ScopeExit reap(ReapPool);
    qt_test::ScriptedWallet& wallet = static_cast<qt_test::ScriptedWallet&>(harness.wallet());
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    QmlProvider::install(&fe.provider());
    qml_test::ScopeExit uninstall([] { QmlProvider::install(nullptr); });

    QQmlEngine engine;
    QString errors;
    std::unique_ptr<QObject> window =
        LoadWrapper(engine, QStringLiteral("sendviewwrapper"), "    SendView { anchors.fill: parent }", errors);
    QVERIFY2(window, qPrintable(errors));
    SendAdapter& adapter = fe.provider().sendAdapter();
    QObject* list = Child(window.get(), "recipientList");
    QObject* balance = Child(window.get(), "sendBalance");
    QObject* send_button = Child(window.get(), "sendButton");
    QVERIFY(list);
    QVERIFY(balance);
    QVERIFY(send_button);

    // A delegate per row.
    QVERIFY(qml_test::PumpUntil([&] { return list->property("count").toInt() == adapter.recipients()->rowCount(); }, 2s));
    QCOMPARE(list->property("count").toInt(), 1);
    QVERIFY(adapter.addRecipient());
    QVERIFY(qml_test::PumpUntil([&] { return list->property("count").toInt() == 2; }, 2s));
    QQuickItem* row0 = Delegate(list, 0);
    QQuickItem* row1 = Delegate(list, 1);
    QVERIFY(row0);
    QVERIFY(row1);

    // The message field: the first row only, and only while a message can
    // be sent.
    QObject* message0 = Child(row0, "messageField");
    QObject* message1 = Child(row1, "messageField");
    QVERIFY(message0);
    QVERIFY(message1);
    QVERIFY(Visible(message0));
    QVERIFY(!Visible(message1));
    {
        ForcedDisableHeight disable("100");
        adapter.setChainHeight(99);
        QVERIFY(!adapter.messageAllowed());
        QVERIFY(!Visible(message0));
        adapter.setChainHeight(98);
        QVERIFY(adapter.messageAllowed());
        QVERIFY(Visible(message0));
    }

    // The subtract-fee box writes its row's role.
    QObject* subtract = Child(row0, "subtractFeeBox");
    QVERIFY(subtract);
    QVERIFY(subtract->setProperty("checked", true));
    QCOMPARE(RoleOf(adapter, 0, Role::SubtractFeeRole).toBool(), true);

    // The header shows the wallet summary's balance.
    WalletSummary& summary = fe.provider().walletSummary();
    emit harness.wallet_model.balanceChanged(1234 * COIN, 0, 0, 0);
    QVERIFY(qml_test::PumpUntil([&] { return summary.balance() == 1234 * COIN; }, 2s));
    QVERIFY(!summary.balanceText().isEmpty());
    QCOMPARE(Text(balance), summary.balanceText());

    // Send commits a field edited without Return or a focus change.
    QVERIFY(adapter.removeRecipient(1));
    QVERIFY(qml_test::PumpUntil([&] { return list->property("count").toInt() == 1; }, 2s));
    QVERIFY(adapter.updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), Address(1)}}));
    QObject* amount = Child(row0, "amountField");
    QVERIFY(amount);
    QVERIFY(amount->setProperty("text", QStringLiteral("2.5")));
    wallet.scriptSend({Ok()});
    QSignalSpy confirm(&adapter, &SendAdapter::sendConfirmationRequested);
    QSignalSpy finished(&adapter, &SendAdapter::finished);
    QVERIFY(send_button->property("enabled").toBool());
    QVERIFY(Click(send_button));
    QCOMPARE(confirm.count(), 1);
    const QString confirmation = confirm.at(0).at(2).toString();
    QVERIFY2(confirmation.contains(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, 250000000)), qPrintable(confirmation));

    // Disabled while the send waits at its confirmation; enabled again once
    // it has finished.
    QVERIFY(adapter.busy());
    QVERIFY(!send_button->property("enabled").toBool());
    const quint64 id = confirm.at(0).at(0).toULongLong();
    QVERIFY(adapter.confirmSend(id, true));
    QVERIFY(qml_test::PumpUntil([&] { return finished.count() == 1; }, 2s));
    QCOMPARE(OutcomeOf(finished, 0), SendAdapter::Sent);
    QCOMPARE(finished.at(0).at(4).toString(), QString::fromStdString(TXID));
    QVERIFY(send_button->property("enabled").toBool());
}

void QmlWalletPromptTests::navigationGuards()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::navigationGuards");
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness(std::make_unique<qt_test::ScriptedWallet>());
    qml_test::ScopeExit reap(ReapPool);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    QVERIFY2(Preamble(fe, harness), "the root did not load");

    QObject* menu = MainItem(fe, "tabMenu");
    QObject* loader = MainItem(fe, "viewLoader");
    QObject* menu_list = MainItem(fe, "menuList");
    QObject* prompt = MainItem(fe, "outOfSyncPrompt");
    QVERIFY(menu);
    QVERIFY(loader);
    QVERIFY(menu_list);
    QVERIFY(prompt);
    QObject* yes = Child(prompt, "promptYes");
    QObject* no = Child(prompt, "promptNo");
    QVERIFY(yes);
    QVERIFY(no);
    QVERIFY(!fe.provider().clientStatus().inSync());

    // Out of sync: the send page asks first; No stays on the overview.
    QVERIFY(Select(menu, QStringLiteral("SendView.qml")));
    QVERIFY2(qml_test::PumpUntil([&] { return Visible(prompt); }, 2s), "the out-of-sync question was not asked");
    QVERIFY(Click(no));
    QVERIFY(qml_test::PumpUntil([&] { return !Visible(prompt); }, 2s));
    QVERIFY2(LoaderSource(loader).endsWith(QStringLiteral("OverviewView.qml")), qPrintable(LoaderSource(loader)));
    QCOMPARE(menu_list->property("currentIndex").toInt(), 0);

    // Yes opens the send page.
    QVERIFY(Select(menu, QStringLiteral("SendView.qml")));
    QVERIFY(qml_test::PumpUntil([&] { return Visible(prompt); }, 2s));
    QVERIFY(Click(yes));
    QVERIFY2(qml_test::PumpUntil([&] { return LoaderSource(loader).endsWith(QStringLiteral("SendView.qml")); }, 2s),
             qPrintable(LoaderSource(loader)));
    QCOMPARE(menu_list->property("currentIndex").toInt(), 1);

    // In sync, an accepted URI switches to the send page.
    QVERIFY(BringInSync(harness, fe));
    QVERIFY(Select(menu, QStringLiteral("OverviewView.qml")));
    QVERIFY(qml_test::PumpUntil([&] { return LoaderSource(loader).endsWith(QStringLiteral("OverviewView.qml")); }, 2s));
    QCOMPARE(menu_list->property("currentIndex").toInt(), 0);
    emit fe.provider().sendAdapter().uriAccepted();
    QVERIFY2(qml_test::PumpUntil([&] { return LoaderSource(loader).endsWith(QStringLiteral("SendView.qml")); }, 2s),
             qPrintable(LoaderSource(loader)));
    QCOMPARE(menu_list->property("currentIndex").toInt(), 1);
    QObject* root = fe.rootObject();
    QObject* main = root->property("mainWindow").value<QObject*>();
    QVERIFY(main);
    QVERIFY(Visible(main));
}

void QmlWalletPromptTests::bridgeUriReachesSend()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::bridgeUriReachesSend");
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness(std::make_unique<qt_test::ScriptedWallet>());
    qml_test::ScopeExit reap(ReapPool);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    QObject* bridge = Preamble(fe, harness);
    QVERIFY2(bridge, "the root did not load");
    QmlCoreBridge* core_bridge = qobject_cast<QmlCoreBridge*>(bridge);
    QVERIFY(core_bridge);
    QObject* menu_list = MainItem(fe, "menuList");
    QObject* loader = MainItem(fe, "viewLoader");
    QVERIFY(menu_list);
    QVERIFY(loader);
    QVERIFY(BringInSync(harness, fe));
    QVERIFY(LoaderSource(loader).endsWith(QStringLiteral("OverviewView.qml")));

    // A received URI, as the core hands it over.
    const QString uri = Uri(Address(1), QStringLiteral("amount=1"));
    QVERIFY(QMetaObject::invokeMethod(bridge, "handleURI", Qt::DirectConnection, Q_ARG(QString, uri)));
    SendAdapter& adapter = fe.provider().sendAdapter();
    QCOMPARE(RoleOf(adapter, 0, Role::RecipientRole).toString(), Address(1));
    QVERIFY2(qml_test::PumpUntil([&] { return LoaderSource(loader).endsWith(QStringLiteral("SendView.qml")); }, 2s),
             qPrintable(LoaderSource(loader)));
    QCOMPARE(menu_list->property("currentIndex").toInt(), 1);
    QVERIFY(core_bridge->pendingUri().isEmpty());
}

void QmlWalletPromptTests::uriBeforeAttachClaimed()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::uriBeforeAttachClaimed");
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness(std::make_unique<qt_test::ScriptedWallet>());
    qml_test::ScopeExit reap(ReapPool);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });

    // GuiMain's order, with a URI handed over between the root and the
    // models.
    fe.createSplash();
    QObject* bridge = nullptr;
    try {
        bridge = fe.construct();
    } catch (const std::exception& e) {
        QFAIL(e.what());
    }
    QVERIFY2(fe.rootObject(), "the root did not load");
    QmlCoreBridge* core_bridge = qobject_cast<QmlCoreBridge*>(bridge);
    QVERIFY(core_bridge);
    core_bridge->handleURI(Uri(Address(1), QStringLiteral("amount=1")));
    QVERIFY(!core_bridge->pendingUri().isEmpty());
    fe.attachModels(harness.bundle());
    fe.showMain(false);

    SendAdapter& adapter = fe.provider().sendAdapter();
    QCOMPARE(RoleOf(adapter, 0, Role::RecipientRole).toString(), Address(1));
    QVERIFY(core_bridge->pendingUri().isEmpty());
}

void QmlWalletPromptTests::pasteReplacesRecipient()
{
    qml_test::Watchdog watchdog("QmlWalletPromptTests::pasteReplacesRecipient");
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness(std::make_unique<qt_test::ScriptedWallet>());
    qml_test::ScopeExit reap(ReapPool);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    QmlProvider::install(&fe.provider());
    qml_test::ScopeExit uninstall([] { QmlProvider::install(nullptr); });

    QQmlEngine engine;
    QString errors;
    std::unique_ptr<QObject> window =
        LoadWrapper(engine, QStringLiteral("sendviewwrapper"), "    SendView { anchors.fill: parent }", errors);
    QVERIFY2(window, qPrintable(errors));
    SendAdapter& adapter = fe.provider().sendAdapter();
    QObject* list = Child(window.get(), "recipientList");
    QVERIFY(list);
    QQuickItem* row0 = Delegate(list, 0);
    QVERIFY(row0);
    QObject* paste = Child(row0, "recipientPaste");
    QObject* field = Child(row0, "recipientField");
    QVERIFY(paste);
    QVERIFY(field);

    QVERIFY(adapter.updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), QStringLiteral("old")}}));
    QCOMPARE(Text(field), QStringLiteral("old"));
    fe.provider().clipboard().setText(QStringLiteral("addr"));
    QCOMPARE(fe.provider().clipboard().text(), QStringLiteral("addr"));

    QVERIFY(Click(paste));
    QCOMPARE(RoleOf(adapter, 0, Role::RecipientRole).toString(), QStringLiteral("addr"));
    QCOMPARE(Text(field), QStringLiteral("addr"));

    // The field still follows the model.
    QVERIFY(adapter.updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), QStringLiteral("other")}}));
    QCOMPARE(Text(field), QStringLiteral("other"));
}
