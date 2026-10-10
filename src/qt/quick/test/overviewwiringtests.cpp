// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "overviewwiringtests.h"

#include "interfacefakes_qml.h"
#include "modelharness.h"
#include "qmltestutil.h"

#include <amount.h>
#include <clientversion.h>
#include <interfaces/wallet_tx_channel.h>
#include <interfaces/wallet_tx_record.h>
#include <qt/guiipcinfo.h>
#include <qt/quick/aboutinfo.h>
#include <qt/quick/clientstatus.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/walletsummary.h>
#include <util.h>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QList>
#include <QMetaMethod>
#include <QMetaObject>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QThreadPool>
#include <QTranslator>
#include <QUrl>
#include <QVariant>
#include <QWindow>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;

//! The carried root, as gridcoinresearch-qml loads it.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

//! Text.PlainText: QQuickText::PlainText has Qt::PlainText's value.
constexpr int PLAIN_TEXT = Qt::PlainText;

//! Every message Qt's message handler receives while CollectMessage is
//! installed; the previous handler still gets each message.
QStringList g_messages;
QtMessageHandler g_previous_handler = nullptr;

void CollectMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    g_messages << message;
    if (g_previous_handler) g_previous_handler(type, context, message);
}

//! The collected messages that name one of `files` under the root's module.
QStringList MessagesNaming(const QStringList& files)
{
    QStringList problems;
    for (const QString& message : std::as_const(g_messages)) {
        for (const QString& file : files) {
            if (message.contains(QStringLiteral("qrc:/qt/qml/Gridcoin/App/") + file)) problems << message;
        }
    }
    return problems;
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

//! What GuiMain does before its event loop, with the models attached: the
//! splash, the root, the models, then the main window. False when the root
//! did not load.
bool Preamble(QmlFrontEnd& fe, qml_test::ModelHarness& harness)
{
    fe.createSplash();
    try {
        fe.construct();
    } catch (const std::exception& e) {
        qWarning().noquote() << "construct() threw:" << e.what();
        return false;
    }
    if (!fe.rootObject()) return false;
    fe.attachModels(harness.bundle());
    fe.showMain(false);
    return true;
}

//! An object the root holds in the QML property `name`.
QObject* RootProperty(QmlFrontEnd& fe, const char* name)
{
    QObject* root = fe.rootObject();
    return root ? root->property(name).value<QObject*>() : nullptr;
}

//! The object named `name` under the main window.
QObject* MainItem(QmlFrontEnd& fe, const char* name)
{
    QObject* main = RootProperty(fe, "mainWindow");
    return main ? main->findChild<QObject*>(QString::fromLatin1(name)) : nullptr;
}

//! The object named `name` under the About window.
QObject* AboutItem(QmlFrontEnd& fe, const char* name)
{
    QObject* about = RootProperty(fe, "aboutWindow");
    return about ? about->findChild<QObject*>(QString::fromLatin1(name)) : nullptr;
}

//! The version windows a window holds as children.
QList<QObject*> VersionWindows(QObject* window)
{
    return window ? window->findChildren<QObject*>(QStringLiteral("versionInfoWindow")) : QList<QObject*>();
}

QString Text(QObject* object)
{
    return object ? object->property("text").toString() : QString();
}

bool Visible(QObject* object)
{
    return object && object->property("visible").toBool();
}

QString ImageSource(QObject* object)
{
    return object ? object->property("imageSource").toUrl().toString() : QString();
}

//! Posts a no-op on the serial lane and pumps until it is delivered.
bool SerialProbe(QmlCallRunner& runner)
{
    auto delivered = std::make_shared<bool>(false);
    if (!runner.postSerial([] {}, [delivered] { *delivered = true; })) return false;
    return qml_test::PumpUntil([delivered] { return *delivered; }, 2s);
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

//! Joins the global pool, so no pooled read outlives the harness's fakes.
void ReapPool()
{
    QThreadPool::globalInstance()->waitForDone();
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

} // namespace

void QmlOverviewWiringTests::overviewShowsAdapters()
{
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    std::vector<TransactionRecord> rows(2);
    rows[0].address = "first";
    rows[0].credit = COIN;
    rows[1].address = "second";
    rows[1].credit = 2 * COIN;
    harness.tx_source.setRows(GRC::VIEW_OVERVIEW, rows);
    harness.researcher_context.m_snapshot.accrual_near_limit = 100;
    harness.researcher_context.m_snapshot.accrual = 150;
    harness.researcher.refresh();

    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    QVERIFY2(Preamble(fe, harness), "the root did not load");

    emit harness.wallet_model.balanceChanged(1 * COIN, 2 * COIN, 3 * COIN, 4 * COIN);
    QObject* available = MainItem(fe, "availableValue");
    QObject* immature = MainItem(fe, "immatureValue");
    QObject* total = MainItem(fe, "totalValue");
    QVERIFY(available);
    QVERIFY(immature);
    QVERIFY(total);
    QCOMPARE(Text(available), QStringLiteral("1.00 GRC"));
    QVERIFY(Visible(immature));
    QCOMPARE(Text(immature), QStringLiteral("4.00 GRC"));
    QCOMPARE(Text(total), QStringLiteral("10.00 GRC"));

    // No connections, so not in sync.
    emit harness.client.numBlocksChanged(90, 100);
    QObject* blocks = MainItem(fe, "blocksValue");
    QVERIFY(blocks);
    QVERIFY2(qml_test::PumpUntil([&] { return Text(blocks) == QStringLiteral("90 of 100 (90%)"); }, 2s),
             qPrintable(Text(blocks)));

    QObject* list = MainItem(fe, "recentTransactionList");
    QVERIFY(list);
    QVERIFY(qml_test::PumpUntil([&] { return list->property("count").toInt() == 2; }, 2s));

    QObject* near_limit = MainItem(fe, "nearLimitIcon");
    QVERIFY(near_limit);
    QVERIFY(Visible(near_limit));

    // Masked: the list is hidden behind the privacy text.
    fe.provider().walletSummary().setMaskValues(true);
    QObject* nothing_yet = MainItem(fe, "nothingYet");
    QVERIFY(nothing_yet);
    QVERIFY(!Visible(list));
    QVERIFY(Visible(nothing_yet));
    bool privacy_text = false;
    for (QObject* child : nothing_yet->findChildren<QObject*>()) {
        if (child->property("text").toString() == QStringLiteral("Privacy Enabled...")) privacy_text = true;
    }
    QVERIFY2(privacy_text, "no child of nothingYet reads \"Privacy Enabled...\"");
}

void QmlOverviewWiringTests::untrustedTextIsPlain()
{
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    harness.voting_manager.setCurrentPollTitle("<img src='http://127.0.0.1:1/x'>");
    std::vector<TransactionRecord> rows(1);
    rows[0].address = "<b>a</b>";
    rows[0].credit = COIN;
    harness.tx_source.setRows(GRC::VIEW_OVERVIEW, rows);
    harness.node.setWarnings("<b>w</b>");

    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    QVERIFY2(Preamble(fe, harness), "the root did not load");

    QObject* polls = MainItem(fe, "pollsInfo");
    QObject* footer = MainItem(fe, "footerMessage");
    QObject* list = MainItem(fe, "recentTransactionList");
    QVERIFY(polls);
    QVERIFY(footer);
    QVERIFY(list);
    QQuickItem* delegate = nullptr;
    QVERIFY2(qml_test::PumpUntil(
                 [&] {
                     delegate = nullptr;
                     QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, delegate), Q_ARG(int, 0));
                     return Text(polls).contains(QStringLiteral("<img")) &&
                            Text(footer).contains(QStringLiteral("<b>w</b>")) && delegate;
                 },
                 2s),
             "the poll title, the footer warnings or the first delegate did not arrive");
    QObject* address = delegate->findChild<QObject*>(QStringLiteral("addressLabel"));
    QVERIFY(address);
    QVERIFY2(Text(address).contains(QStringLiteral("<b>a</b>")), qPrintable(Text(address)));

    QCOMPARE(polls->property("textFormat").toInt(), PLAIN_TEXT);
    QCOMPARE(address->property("textFormat").toInt(), PLAIN_TEXT);
    QCOMPARE(footer->property("textFormat").toInt(), PLAIN_TEXT);
}

void QmlOverviewWiringTests::footerFollowsState()
{
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.m_testnet = true;
        harness.wallet().m_lock_state.crypted = true;
        harness.wallet().m_lock_state.locked = true;
        harness.researcher_context.m_snapshot.configured_for_noncruncher_mode = true;
        harness.researcher.refresh();
        harness.node.setWarnings("W");
        harness.staking.m_staking = false;
        harness.staking.setErrors("E");

        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");

        QObject* testnet = MainItem(fe, "testNetMarker");
        QObject* connection = MainItem(fe, "connectionIcon");
        QObject* lock = MainItem(fe, "lockIcon");
        QObject* beacon = MainItem(fe, "beaconIcon");
        QObject* message = MainItem(fe, "footerMessage");
        QVERIFY(testnet);
        QVERIFY(connection);
        QVERIFY(lock);
        QVERIFY(beacon);
        QVERIFY(message);

        // The hydrate's delivery.
        QVERIFY2(qml_test::PumpUntil([&] { return Text(message).contains(QStringLiteral("Miner: E")); }, 2s),
                 qPrintable(Text(message)));
        QVERIFY(Visible(testnet));
        QVERIFY2(ImageSource(connection).endsWith(QStringLiteral("ic_connection_na_light.svg")) ||
                     ImageSource(connection).endsWith(QStringLiteral("ic_connection_na_dark.svg")),
                 qPrintable(ImageSource(connection)));
        QCOMPARE(Text(connection), fe.provider().clientStatus().connectionToolTip());
        QVERIFY2(qml_test::PumpUntil(
                     [&] {
                         return ImageSource(lock).endsWith(QStringLiteral("ic_locked_light.svg")) ||
                                ImageSource(lock).endsWith(QStringLiteral("ic_locked_dark.svg"));
                     },
                     2s),
                 qPrintable(ImageSource(lock)));
        QVERIFY(!Visible(beacon));
        // A literal from ClientModel's formula, not the bound property.
        QVERIFY2(Text(message).startsWith(QStringLiteral("W")), qPrintable(Text(message)));

        // A status read with no connections still updates the warnings.
        QVERIFY(SerialProbe(fe.callRunner()));
        harness.node.setWarnings("W2");
        emit harness.client.numConnectionsChanged(0);
        QVERIFY2(qml_test::PumpUntil([&] { return Text(message).startsWith(QStringLiteral("W2")); }, 2s),
                 qPrintable(Text(message)));
    }

    // Unknown until the lock state is read.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        qt_test::FakeLatch latch;
        qml_test::ScopeExit unlatch([&] {
            latch.open = true;
            harness.wallet().m_lock_state_latch = nullptr;
        });
        harness.wallet().m_lock_state_latch = &latch;
        QVERIFY2(Preamble(fe, harness), "the root did not load");

        QObject* lock = MainItem(fe, "lockIcon");
        QVERIFY(lock);
        QCOMPARE(ImageSource(lock), QString());
        QCOMPARE(Text(lock), QStringLiteral("The wallet's lock state is not known yet."));
        latch.open = true;
        // The fake wallet is unencrypted: the alert image, unlike Widgets,
        // which shows the open lock an unlocked wallet shows.
        QVERIFY2(qml_test::PumpUntil(
                     [&] { return ImageSource(lock) == QStringLiteral("qrc:/icons/generic/ic_alert.svg"); }, 2s),
                 qPrintable(ImageSource(lock)));
        // A fully unlocked wallet keeps the open lock.
        emit harness.wallet_model.encryptionStatusChanged(WalletModel::Unlocked);
        QVERIFY2(qml_test::PumpUntil([&] { return ImageSource(lock).endsWith(QStringLiteral("ic_unlocked.svg")); }, 2s),
                 qPrintable(ImageSource(lock)));
    }
}

void QmlOverviewWiringTests::buildBannerFromShellState()
{
    GuiIpcInfo mismatch;
    mismatch.active = true;
    mismatch.git_commit_mismatch = true;
    mismatch.gui_version = QStringLiteral("aaa");
    mismatch.node_version = QStringLiteral("bbb");

    // Without the mismatch the banner is hidden.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        QObject* banner = MainItem(fe, "buildBanner");
        QVERIFY(banner);
        QVERIFY(!Visible(banner));
    }

    // Set before showMain, as GuiMain sets it: shown, then dismissed.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.createSplash();
        fe.construct();
        QVERIFY2(fe.rootObject(), "the root did not load");
        fe.setIpcConnectionInfo(mismatch);
        fe.attachModels(harness.bundle());
        fe.showMain(false);

        QObject* banner = MainItem(fe, "buildBanner");
        QObject* text = MainItem(fe, "buildBannerText");
        QObject* dismiss = MainItem(fe, "buildBannerDismiss");
        QVERIFY(banner);
        QVERIFY(text);
        QVERIFY(dismiss);
        QVERIFY(Visible(banner));
        QCOMPARE(Text(text), QStringLiteral("The GUI (aaa) and the Gridcoin daemon (bbb) were built from different "
                                            "commits. Mixed builds can behave unexpectedly."));
        QVERIFY(QMetaObject::invokeMethod(dismiss, "clicked"));
        QVERIFY(qml_test::PumpUntil([&] { return !Visible(banner); }, 2s));
    }

    // The strings are looked up in the Widgets twins' contexts.
    {
        NoQuitOnLastWindow no_quit;
        MarkingTranslator translator({QStringLiteral("BitcoinGUI"), QStringLiteral("OverviewPage")});
        QCoreApplication::installTranslator(&translator);
        qml_test::ScopeExit remove_translator([&translator] { QCoreApplication::removeTranslator(&translator); });
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.createSplash();
        fe.construct();
        QVERIFY2(fe.rootObject(), "the root did not load");
        fe.setIpcConnectionInfo(mismatch);
        fe.attachModels(harness.bundle());
        fe.showMain(false);

        QObject* text = MainItem(fe, "buildBannerText");
        QObject* dismiss = MainItem(fe, "buildBannerDismiss");
        QVERIFY(text);
        QVERIFY(dismiss);
        QCOMPARE(Text(text), QStringLiteral("[BitcoinGUI] The GUI (aaa) and the Gridcoin daemon (bbb) were built "
                                            "from different commits. Mixed builds can behave unexpectedly."));
        QCOMPARE(Text(dismiss), QStringLiteral("[BitcoinGUI] Dismiss"));

        QObject* wallet_out_of_sync = MainItem(fe, "walletOutOfSync");
        QObject* transactions_out_of_sync = MainItem(fe, "transactionsOutOfSync");
        QObject* immature_label = MainItem(fe, "immatureLabel");
        QVERIFY(wallet_out_of_sync);
        QVERIFY(transactions_out_of_sync);
        QVERIFY(immature_label);
        QVERIFY(qml_test::PumpUntil([&] { return Visible(wallet_out_of_sync); }, 2s));
        QCOMPARE(Text(wallet_out_of_sync), QStringLiteral("[OverviewPage] Out of Sync"));
        QCOMPARE(Text(transactions_out_of_sync), QStringLiteral("[OverviewPage] Out of Sync"));
        QCOMPARE(Text(immature_label), QStringLiteral("[OverviewPage] Immature:"));

        QObject* overview = MainItem(fe, "overviewView");
        QVERIFY(overview);
        const QList<QObject*> copy_items = overview->findChildren<QObject*>(QStringLiteral("copyAmountItem"));
        QVERIFY2(!copy_items.isEmpty(), "no copyAmountItem under overviewView");
        QCOMPARE(Text(copy_items.first()), QStringLiteral("[OverviewPage] Copy amount"));
    }
}

void QmlOverviewWiringTests::pollTitleNavigates()
{
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    harness.voting_manager.setCurrentPollTitle("A poll");
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    QVERIFY2(Preamble(fe, harness), "the root did not load");

    QObject* polls = MainItem(fe, "pollsInfo");
    QObject* loader = MainItem(fe, "viewLoader");
    QObject* menu = MainItem(fe, "tabMenu");
    QObject* menu_list = MainItem(fe, "menuList");
    QVERIFY(polls);
    QVERIFY(loader);
    QVERIFY(menu);
    QVERIFY(menu_list);
    QVERIFY(qml_test::PumpUntil([&] { return Text(polls) == QStringLiteral("A poll"); }, 2s));

    // Offscreen on Qt 6.4.2 the main window is 1 pixel wide, so a real click
    // on the poll title lands outside it: the area's clicked signal is
    // emitted through its meta-object instead (the handler reads no event).
    QObject* area = MainItem(fe, "pollTitleArea");
    QVERIFY2(area, "pollTitleArea was not found");
    const int clicked_index = area->metaObject()->indexOfSignal("clicked(QQuickMouseEvent*)");
    QVERIFY(clicked_index >= 0);
    // The menu is a QML object, so its signal has no member pointer: the spy
    // takes the signal's meta-method.
    const int menu_clicked_index = menu->metaObject()->indexOfSignal("menuButtonClicked(QString)");
    QVERIFY(menu_clicked_index >= 0);
    QSignalSpy clicked(menu, menu->metaObject()->method(menu_clicked_index));
    QVERIFY(clicked.isValid());
    void* no_event = nullptr;
    QVERIFY(area->metaObject()->method(clicked_index).invoke(area, Qt::DirectConnection,
                                                             QGenericArgument("QQuickMouseEvent*", &no_event)));
    QVERIFY2(qml_test::PumpUntil(
                 [&] { return loader->property("source").toUrl().toString().endsWith(QStringLiteral("PollView.qml")); },
                 2s),
             qPrintable(loader->property("source").toUrl().toString()));
    QCOMPARE(menu_list->property("currentIndex").toInt(), 5);
    QCOMPARE(clicked.count(), 1);
}

void QmlOverviewWiringTests::aboutWindowBindings()
{
    const QString year = QString::fromLatin1(COPYRIGHT_YEAR);

    // The version, the copyright, the disabled button and the multiprocess
    // section, on a node that is not on mainnet.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.setClientVersion("v1.0");
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        GuiIpcInfo info;
        info.active = true;
        info.node_identity = QStringLiteral("<b>x</b>");
        fe.setIpcConnectionInfo(info);

        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY(qml_test::PumpUntil([&] { return !Text(AboutItem(fe, "versionText")).isEmpty(); }, 2s));
        QCOMPARE(Text(AboutItem(fe, "versionText")), QStringLiteral("v1.0"));
        QVERIFY2(Text(AboutItem(fe, "copyrightText")).contains(year), qPrintable(Text(AboutItem(fe, "copyrightText"))));
        QObject* button = AboutItem(fe, "versionInfoButton");
        QVERIFY(button);
        QVERIFY(!button->property("enabled").toBool());
        QObject* ipc = AboutItem(fe, "ipcSection");
        QVERIFY(ipc);
        QVERIFY(Visible(ipc));
        QCOMPARE(ipc->property("textFormat").toInt(), PLAIN_TEXT);
        QVERIFY2(Text(ipc).contains(QStringLiteral("<b>x</b>")), qPrintable(Text(ipc)));
    }

    // Mainnet: the button's check opens a version window with the result; a
    // result with another id opens none.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.m_mainnet = true;
        harness.node.setClientVersion("v1.0");
        harness.node.setLatestVersion(interfaces::LatestVersionInfo{"v9.9", "d<b>", 2});
        g_messages.clear();
        g_previous_handler = qInstallMessageHandler(CollectMessage);
        qml_test::ScopeExit restore_handler([] { qInstallMessageHandler(g_previous_handler); });
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");

        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY(qml_test::PumpUntil([&] { QObject* b = AboutItem(fe, "versionInfoButton"); return b && b->property("enabled").toBool(); }, 2s));
        QSignalSpy finished(&fe.provider().aboutInfo(), &AboutInfo::updateCheckFinished);
        QObject* button = AboutItem(fe, "versionInfoButton");
        QQuickWindow* about_window = qobject_cast<QQuickWindow*>(RootProperty(fe, "aboutWindow"));
        QVERIFY(about_window);
        QVERIFY(QTest::qWaitForWindowExposed(about_window));
        QVERIFY(MouseClick(button));
        ReapPool();
        QVERIFY2(qml_test::PumpUntil(
                     [&] {
                         const QList<QObject*> windows = VersionWindows(about_window);
                         return finished.count() == 1 && windows.size() == 1 && Visible(windows.first());
                     },
                     2s),
                 "the result opened no visible version window");
        QObject* version_window = VersionWindows(about_window).first();
        QCOMPARE(Text(version_window->findChild<QObject*>(QStringLiteral("versionInfoVersion"))), QStringLiteral("v9.9"));
        QObject* details = version_window->findChild<QObject*>(QStringLiteral("versionInfoDetails"));
        QVERIFY(details);
        QVERIFY(Text(details).contains(QStringLiteral("d<b>")));
        QCOMPARE(details->property("textFormat").toInt(), PLAIN_TEXT);

        const quint64 id = finished.at(0).at(0).toULongLong();
        emit fe.provider().aboutInfo().updateCheckFinished(id + 1, QStringLiteral("v0.0"), QString(), 0);
        qml_test::PumpFor(100ms);
        QCOMPARE(VersionWindows(about_window).size(), qsizetype(1));
        QCOMPARE(Text(VersionWindows(about_window).first()->findChild<QObject*>(QStringLiteral("versionInfoVersion"))),
                 QStringLiteral("v9.9"));
        const QStringList problems = MessagesNaming({QStringLiteral("AboutWindow.qml"), QStringLiteral("VersionInfoWindow.qml")});
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    }

    // A result that arrives after its About window closed opens nothing there.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.m_mainnet = true;
        harness.node.setClientVersion("v1.0");
        harness.node.setLatestVersion(interfaces::LatestVersionInfo{"v9.9", "d<b>", 2});
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        qml_test::Latch latch;
        qml_test::ScopeExit unlatch([&] {
            latch.open = true;
            ReapPool();
            harness.node.m_update_check_latch = nullptr;
        });
        QVERIFY2(Preamble(fe, harness), "the root did not load");

        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY(qml_test::PumpUntil([&] { QObject* b = AboutItem(fe, "versionInfoButton"); return b && b->property("enabled").toBool(); }, 2s));
        QSignalSpy finished(&fe.provider().aboutInfo(), &AboutInfo::updateCheckFinished);
        harness.node.m_update_check_latch = &latch;
        QPointer<QQuickWindow> closed = qobject_cast<QQuickWindow*>(RootProperty(fe, "aboutWindow"));
        QVERIFY(closed);
        QVERIFY(QTest::qWaitForWindowExposed(closed));
        QVERIFY(MouseClick(AboutItem(fe, "versionInfoButton")));
        closed->close();
        latch.open = true;
        ReapPool();
        QVERIFY(qml_test::PumpUntil([&] { return finished.count() == 1; }, 2s));
        qml_test::PumpFor(100ms);
        QVERIFY(closed);
        QCOMPARE(VersionWindows(closed).size(), qsizetype(0));
    }

    // The About window's strings are looked up in AboutDialog's context.
    {
        NoQuitOnLastWindow no_quit;
        MarkingTranslator translator({QStringLiteral("AboutDialog")});
        QCoreApplication::installTranslator(&translator);
        qml_test::ScopeExit remove_translator([&translator] { QCoreApplication::removeTranslator(&translator); });
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        GuiIpcInfo info;
        info.active = true;
        info.gui_version = QStringLiteral("aaa");
        fe.setIpcConnectionInfo(info);

        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY(qml_test::PumpUntil([&] { return AboutItem(fe, "versionInfoButton") != nullptr; }, 2s));
        QCOMPARE(Text(AboutItem(fe, "versionInfoButton")), QStringLiteral("[AboutDialog] Version Information"));
        QCOMPARE(Text(AboutItem(fe, "ipcHeader")), QStringLiteral("[AboutDialog] <b>Multiprocess connection</b>"));
        QVERIFY2(Text(AboutItem(fe, "ipcSection")).contains(QStringLiteral("[AboutDialog] GUI version: aaa")),
                 qPrintable(Text(AboutItem(fe, "ipcSection"))));
    }

    // Each open re-reads the flags: a flag cleared while About was closed
    // enables the button of the next window.
    {
        gArgs.ForceSetArg("-disableupdatecheck", "1");
        qml_test::ScopeExit clear_arg([] { gArgs.ClearForcedArg("-disableupdatecheck"); });
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.m_mainnet = true;
        harness.node.setClientVersion("v1.0");
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");

        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY(qml_test::PumpUntil([&] { return Text(AboutItem(fe, "versionText")) == QStringLiteral("v1.0"); }, 2s));
        QVERIFY(!AboutItem(fe, "versionInfoButton")->property("enabled").toBool());
        QVERIFY(SerialProbe(fe.callRunner()));
        qobject_cast<QQuickWindow*>(RootProperty(fe, "aboutWindow"))->close();
        QVERIFY(qml_test::PumpUntil([&] { return RootProperty(fe, "aboutWindow") == nullptr; }, 2s));
        gArgs.ClearForcedArg("-disableupdatecheck");
        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY2(qml_test::PumpUntil(
                     [&] { QObject* b = AboutItem(fe, "versionInfoButton"); return b && b->property("enabled").toBool(); }, 2s),
                 "the reopened About window did not re-read the flag");
    }

    // A reopened About window adopts a check a closed one started.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.m_mainnet = true;
        harness.node.setClientVersion("v1.0");
        harness.node.setLatestVersion(interfaces::LatestVersionInfo{"v9.9", "d<b>", 2});
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        qml_test::Latch latch;
        qml_test::ScopeExit unlatch([&] {
            latch.open = true;
            ReapPool();
            harness.node.m_update_check_latch = nullptr;
        });
        harness.node.m_update_check_latch = &latch;
        QVERIFY2(Preamble(fe, harness), "the root did not load");

        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY(qml_test::PumpUntil([&] { QObject* b = AboutItem(fe, "versionInfoButton"); return b && b->property("enabled").toBool(); }, 2s));
        QPointer<QQuickWindow> closed = qobject_cast<QQuickWindow*>(RootProperty(fe, "aboutWindow"));
        QVERIFY(closed);
        QVERIFY(QTest::qWaitForWindowExposed(closed));
        QVERIFY(MouseClick(AboutItem(fe, "versionInfoButton")));
        closed->close();
        QVERIFY(qml_test::PumpUntil([&] { return RootProperty(fe, "aboutWindow") == nullptr; }, 2s));

        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY(qml_test::PumpUntil([&] { QObject* w = RootProperty(fe, "aboutWindow"); return w && Visible(w); }, 2s));
        QObject* reopened = RootProperty(fe, "aboutWindow");
        QVERIFY(reopened != closed.data());
        // The check is still held, so it is still running.
        QVERIFY(!AboutItem(fe, "versionInfoButton")->property("enabled").toBool());
        latch.open = true;
        ReapPool();
        QVERIFY2(qml_test::PumpUntil([&] { return VersionWindows(reopened).size() == 1; }, 2s),
                 "the reopened About window did not adopt the running check");
        QVERIFY(closed);
        QCOMPARE(VersionWindows(closed).size(), qsizetype(0));
    }
}

void QmlOverviewWiringTests::footerIconsFollowState()
{
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    // A convergence time and no current event: the scraper icon starts in its
    // waiting state, and every other state comes from an event.
    interfaces::ScraperConvergenceSnapshot snapshot;
    snapshot.time = 1700000000;
    harness.node.setScraperSnapshot(snapshot);

    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    QVERIFY2(Preamble(fe, harness), "the root did not load");

    QObject* staking = MainItem(fe, "stakingIcon");
    QObject* scraper = MainItem(fe, "scraperIcon");
    QObject* sync = MainItem(fe, "syncIcon");
    QObject* connection = MainItem(fe, "connectionIcon");
    QVERIFY(staking);
    QVERIFY(scraper);
    QVERIFY(sync);
    QVERIFY(connection);
    ClientStatus& status = fe.provider().clientStatus();
    QVERIFY(SerialProbe(fe.callRunner()));

    // An icon shows the image `stem` in either theme, within the bound.
    const auto themed = [](QObject* icon, const char* stem) {
        const QString source = ImageSource(icon);
        return source.endsWith(QLatin1String(stem) + QStringLiteral("_light.svg")) ||
               source.endsWith(QLatin1String(stem) + QStringLiteral("_dark.svg"));
    };
    const auto shows = [&](QObject* icon, const char* stem) {
        return qml_test::PumpUntil([&] { return themed(icon, stem); }, 2s);
    };
    const auto scraper_event = [&](scrapereventtypes type, int event_status, ClientStatus::ScraperState state) {
        emit harness.client.updateScraperStatus(static_cast<int>(type), event_status);
        return qml_test::PumpUntil([&] { return status.scraperState() == state; }, 2s);
    };
    const auto set_connections = [&](int count, int level) {
        harness.node.m_node_count = count;
        emit harness.client.numConnectionsChanged(count);
        return qml_test::PumpUntil([&] { return status.connectionLevel() == level; }, 2s);
    };
    const auto set_height = [&](int height, int step) {
        emit harness.client.numBlocksChanged(height, 100);
        return qml_test::PumpUntil([&] { return status.syncProgress() == step; }, 2s);
    };

    // The staking icon: one image for each of the three staking states.
    emit harness.client.minerStatusChanged(true, 1000, 20, 86400);
    QVERIFY2(shows(staking, "ic_staking_yes"), qPrintable(ImageSource(staking)));
    emit harness.client.minerStatusChanged(false, 1000, 20, 0);
    QVERIFY2(shows(staking, "ic_staking_no"), qPrintable(ImageSource(staking)));
    emit harness.client.minerStatusChanged(false, 1000, 0, 0);
    QVERIFY2(shows(staking, "ic_staking_problem"), qPrintable(ImageSource(staking)));
    // The image bound for UnableToStake is served: its resource path is ":"
    // and what follows "qrc:". Both themes' files are in the resource.
    QVERIFY2(QFile::exists(QStringLiteral(":") + ImageSource(staking).mid(4)), qPrintable(ImageSource(staking)));
    QVERIFY(QFile::exists(QStringLiteral(":/icons/statusbar/ic_staking_problem_light.svg")));
    QVERIFY(QFile::exists(QStringLiteral(":/icons/statusbar/ic_staking_problem_dark.svg")));
    QVERIFY(!Text(staking).isEmpty());
    QCOMPARE(Text(staking), status.stakingToolTip());

    // The scraper icon: waiting before any event, with the waiting tooltip,
    // then one image for each state an event sets; downloading shows the
    // waiting image again.
    QVERIFY2(shows(scraper, "ic_scraper_waiting"), qPrintable(ImageSource(scraper)));
    QVERIFY2(qml_test::PumpUntil(
                 [&] { return Text(scraper) == QStringLiteral("Scraper: waiting on wallet to sync."); }, 2s),
             qPrintable(Text(scraper)));
    QVERIFY(scraper_event(scrapereventtypes::Convergence, CT_NEW, ClientStatus::Converged));
    QVERIFY2(shows(scraper, "ic_scraper_ok"), qPrintable(ImageSource(scraper)));
    QVERIFY(scraper_event(scrapereventtypes::Sleep, CT_NEW, ClientStatus::Inactive));
    QVERIFY2(shows(scraper, "ic_scraper_inactive"), qPrintable(ImageSource(scraper)));
    QVERIFY(scraper_event(scrapereventtypes::Convergence, CT_DELETED, ClientStatus::NoConvergence));
    QVERIFY2(shows(scraper, "ic_scraper_no_convergence"), qPrintable(ImageSource(scraper)));
    QVERIFY(scraper_event(scrapereventtypes::Stats, CT_NEW, ClientStatus::Downloading));
    QVERIFY2(shows(scraper, "ic_scraper_waiting"), qPrintable(ImageSource(scraper)));
    QVERIFY(!Text(scraper).isEmpty());
    QCOMPARE(Text(scraper), status.scraperToolTip());

    // The connection icon: one image for each of the five levels. Level 3
    // follows below, with the eight connections the sync icon needs.
    QVERIFY2(shows(connection, "ic_connection_na"), qPrintable(ImageSource(connection)));
    QVERIFY(set_connections(1, 1));
    QVERIFY2(shows(connection, "ic_connection_poor"), qPrintable(ImageSource(connection)));
    QVERIFY(set_connections(3, 2));
    QVERIFY2(shows(connection, "ic_connection_average"), qPrintable(ImageSource(connection)));
    QVERIFY(set_connections(10, 4));
    QVERIFY2(shows(connection, "ic_connection_good"), qPrintable(ImageSource(connection)));

    // The sync icon: up to date, each progress step while catching up, and no
    // connections.
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    harness.node.m_last_block_time = now - 60;
    harness.node.m_node_count = 8;
    emit harness.client.numConnectionsChanged(8);
    emit harness.client.numBlocksChanged(100, 100);
    QVERIFY(qml_test::PumpUntil([&] { return status.syncState() == ClientStatus::UpToDate; }, 2s));
    QVERIFY2(qml_test::PumpUntil(
                 [&] { return ImageSource(sync).endsWith(QStringLiteral("ic_syncing_uptodate.svg")); }, 2s),
             qPrintable(ImageSource(sync)));
    QVERIFY2(shows(connection, "ic_connection_normal"), qPrintable(ImageSource(connection)));
    QVERIFY(set_height(50, 50));
    QVERIFY2(shows(sync, "ic_syncing_50"), qPrintable(ImageSource(sync)));
    QVERIFY(set_height(90, 90));
    QVERIFY2(shows(sync, "ic_syncing_90"), qPrintable(ImageSource(sync)));
    QVERIFY(set_height(25, 25));
    QVERIFY2(shows(sync, "ic_syncing_25"), qPrintable(ImageSource(sync)));
    QVERIFY(set_height(75, 75));
    QVERIFY2(shows(sync, "ic_syncing_75"), qPrintable(ImageSource(sync)));
    QVERIFY(set_height(10, 0));
    QVERIFY2(shows(sync, "ic_syncing_unsynced"), qPrintable(ImageSource(sync)));
    harness.node.m_node_count = 0;
    emit harness.client.numConnectionsChanged(0);
    QVERIFY(qml_test::PumpUntil([&] { return status.syncState() == ClientStatus::NoConnections; }, 2s));
    QVERIFY2(shows(sync, "ic_syncing_stuck"), qPrintable(ImageSource(sync)));
    QVERIFY(!Text(sync).isEmpty());
    QCOMPARE(Text(sync), status.syncToolTip());
}

void QmlOverviewWiringTests::footerStakingToolTipFollowsMask()
{
    NoQuitOnLastWindow no_quit;
    qml_test::ModelHarness harness;
    qml_test::ScopeExit reap(ReapPool);
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    QVERIFY2(Preamble(fe, harness), "the root did not load");

    QObject* staking = MainItem(fe, "stakingIcon");
    QVERIFY(staking);
    ClientStatus& status = fe.provider().clientStatus();
    WalletSummary& summary = fe.provider().walletSummary();
    QVERIFY(SerialProbe(fe.callRunner()));

    // The icon's text is its popup's text: HelpHover aliases the popup's text
    // item.
    const auto popup_reads = [&](const QString& text) {
        return qml_test::PumpUntil([&] { return Text(staking) == text; }, 2s);
    };
    const QString shown = QStringLiteral("Staking.<br>Your weight is 20<br>Network weight is 1000<br>"
                                         "<b>Estimated</b> staking frequency is 1.01 times per day.");
    const QString masked = QStringLiteral("Staking.<br>Your weight is #.##<br>Network weight is 1000<br>"
                                          "<b>Estimated</b> staking frequency is #.##.");
    emit harness.client.minerStatusChanged(true, 1000, 20, 86400);
    QVERIFY2(popup_reads(shown), qPrintable(Text(staking)));
    QCOMPARE(status.stakingToolTip(), shown);
    // The status read the miner signal posted has been delivered, so from
    // here on only a switch of the mask formats the tooltip.
    QVERIFY(SerialProbe(fe.callRunner()));

    // The mask, switched through WalletSummary as the main window's shortcut
    // switches it, and back.
    summary.setMaskValues(true);
    QVERIFY(summary.maskValues());
    QVERIFY2(popup_reads(masked), qPrintable(Text(staking)));
    QCOMPARE(status.stakingToolTip(), masked);
    summary.setMaskValues(false);
    QVERIFY(!summary.maskValues());
    QVERIFY2(popup_reads(shown), qPrintable(Text(staking)));
    QCOMPARE(status.stakingToolTip(), shown);
}
