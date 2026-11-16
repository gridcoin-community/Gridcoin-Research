// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "sendadaptertests.h"

#include "interfacefakes_qml.h"
#include "qmltestutil.h"

#include <amount.h>
#include <chainparams.h>
#include <interfaces/wallet.h>
#include <interfaces/wallet_coin_source.h>
#include <key_io.h>
#include <pubkey.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/sendadapter.h>
#include <qt/quick/sendrecipientmodel.h>
#include <qt/quick/walletopgate.h>
#include <qt/quick/walletpromptlatch.h>
#include <qt/test/interfacefakes.h>
#include <qt/test/modelfakes.h>
#include <qt/walletmodel.h>
#include <uint256.h>
#include <util.h>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QHash>
#include <QMetaObject>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QTranslator>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using Method = qt_test::ScriptedWallet::Method;
using Owner = WalletPromptLatch::Owner;
using Role = SendRecipientModel::Role;

//! The carried root's URL; these front ends never construct, so it is never
//! loaded.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

const std::string PASSPHRASE = "correct horse";
const std::string TXID = "abcdef0123456789";
constexpr qint64 FEE = 123456789;

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

//! The models a test drives, in the order GuiMain builds them: the scripted
//! wallet behind a real WalletModel.
struct Models {
    qt_test::ScriptedWallet wallet;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager side_stakes;
    OptionsModel options{node, side_stakes};
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source{interfaces::MakeSyntheticCoinSource(0, 1)};
    WalletModel model{wallet, tx_source, *coin_source, &options};
};

//! The models, then a front end with the gate and the adapter attached over
//! them, then the detach, which runs before the front end and the models go.
struct Harness {
    Harness() : fe(ROOT_URL, qml_test::CountingCallbacks(disconnects))
    {
        m.wallet.setPassphrase(PASSPHRASE);
        gate().attach(m.wallet, fe.callRunner());
        adapter().attach(m.model, nullptr, fe.callRunner());
    }
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    WalletOpGate& gate() { return fe.provider().walletOps(); }
    SendAdapter& adapter() { return fe.provider().sendAdapter(); }

    Models m;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe;
    qml_test::ScopeExit detach{[this] { fe.detachModels(); }};
};

//! Spies on the adapter's signals and the gate's request and finish.
struct Spies {
    explicit Spies(Harness& h)
        : confirm(&h.adapter(), &SendAdapter::sendConfirmationRequested),
          fee(&h.adapter(), &SendAdapter::feeConfirmationRequested),
          finished(&h.adapter(), &SendAdapter::finished),
          accepted(&h.adapter(), &SendAdapter::uriAccepted),
          rejected(&h.adapter(), &SendAdapter::uriRejected),
          unlock(&h.gate(), &WalletOpGate::unlockRequested),
          gate_finished(&h.gate(), &WalletOpGate::finished)
    {
    }
    QSignalSpy confirm;
    QSignalSpy fee;
    QSignalSpy finished;
    QSignalSpy accepted;
    QSignalSpy rejected;
    QSignalSpy unlock;
    QSignalSpy gate_finished;
};

quint64 IdOf(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(0).toULongLong();
}

SendAdapter::Outcome OutcomeOf(const QSignalSpy& finished, int index)
{
    return finished.at(index).at(1).value<SendAdapter::Outcome>();
}

QString TitleOf(const QSignalSpy& finished, int index)
{
    return finished.at(index).at(2).toString();
}

QString MessageOf(const QSignalSpy& finished, int index)
{
    return finished.at(index).at(3).toString();
}

QString TxidOf(const QSignalSpy& finished, int index)
{
    return finished.at(index).at(4).toString();
}

//! Sets row `row`'s recipient and amount text.
bool Fill(SendAdapter& adapter, int row, const QString& address, const QString& amount = QStringLiteral("1"))
{
    return adapter.updateRecipient(row, QVariantMap{{QStringLiteral("recipient"), address},
                                                    {QStringLiteral("amount"), amount}});
}

QVariant RoleOf(SendAdapter& adapter, int row, int role)
{
    QAbstractItemModel* model = adapter.recipients();
    return model->data(model->index(row, 0), role);
}

int Rows(SendAdapter& adapter)
{
    return adapter.recipients()->rowCount();
}

QString RowAddress(SendAdapter& adapter, int row)
{
    return RoleOf(adapter, row, Role::RecipientRole).toString();
}

//! Sends the rows and answers the confirmation yes; the request's id, or 0.
quint64 SendAndConfirm(SendAdapter& adapter)
{
    const quint64 id = adapter.send();
    if (id == 0 || !adapter.confirmSend(id, true)) return 0;
    return id;
}

//! Answers the gate's last passphrase request with the right passphrase.
bool AnswerUnlock(Harness& h, const QSignalSpy& unlock)
{
    if (unlock.isEmpty()) return false;
    return h.gate().answerUnlock(IdOf(unlock, unlock.count() - 1), QString::fromStdString(PASSPHRASE));
}

//! Posts a no-op on the serial lane and pumps until it is delivered.
bool SerialProbe(QmlCallRunner& runner)
{
    auto delivered = std::make_shared<bool>(false);
    if (!runner.postSerial([] {}, [delivered] { *delivered = true; })) return false;
    return qml_test::PumpUntil([delivered] { return *delivered; }, 2s);
}

//! Waits, without processing events, until `done()` holds or `bound` passes.
bool WaitNoEvents(const std::function<bool()>& done, std::chrono::milliseconds bound)
{
    const auto deadline = std::chrono::steady_clock::now() + bound;
    while (std::chrono::steady_clock::now() < deadline) {
        if (done()) return true;
        std::this_thread::sleep_for(1ms);
    }
    return done();
}

//! The logged calls of `method`, in order.
std::vector<qt_test::ScriptedWallet::Call> CallsOf(const qt_test::ScriptedWallet& wallet, Method method)
{
    std::vector<qt_test::ScriptedWallet::Call> calls;
    for (const qt_test::ScriptedWallet::Call& call : wallet.log()) {
        if (call.method == method) calls.push_back(call);
    }
    return calls;
}

//! The logged calls of the methods in `methods`, in log order.
std::vector<Method> LogOrder(const qt_test::ScriptedWallet& wallet, std::initializer_list<Method> methods)
{
    std::vector<Method> order;
    for (const qt_test::ScriptedWallet::Call& call : wallet.log()) {
        for (Method method : methods) {
            if (call.method == method) order.push_back(call.method);
        }
    }
    return order;
}

//! Hands the adapter the bridge's URIs as QmlFrontEnd::attachModels does.
void Wire(SendAdapter& adapter, QmlCoreBridge& bridge)
{
    adapter.setUriSource([&bridge] { return bridge.takePendingUri(); },
                         [&bridge](const QString& uri) { bridge.handleURI(uri); });
    QObject::connect(&bridge, &QmlCoreBridge::pendingUriChanged, &adapter, &SendAdapter::claimPendingUri);
    adapter.claimPendingUri();
}

//! Answers every fee question with yes, from a queued handler, as a prompt
//! does; records each confirmFee return.
class AutoYesFee : public QObject
{
public:
    AutoYesFee(SendAdapter& adapter) : m_adapter(adapter)
    {
        QObject::connect(
            &adapter, &SendAdapter::feeConfirmationRequested, this,
            [this](quint64 id, qint64, const QString&, const QString&) { returns.push_back(m_adapter.confirmFee(id, true)); },
            Qt::QueuedConnection);
    }
    std::vector<bool> returns;

private:
    SendAdapter& m_adapter;
};

QString SendTitle()
{
    return QCoreApplication::translate("SendCoinsDialog", "Send Coins");
}

QString BusyMessage()
{
    return QCoreApplication::translate("SendCoinsDialog", "Another wallet operation is in progress. Please try again when it has finished.");
}

QString UriTitle()
{
    return QCoreApplication::translate("BitcoinGUI", "URI handling");
}

QString UriText()
{
    return QCoreApplication::translate("BitcoinGUI", "URI can not be parsed! This can be caused by an invalid Gridcoin address or malformed URI parameters.");
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

} // namespace

void QmlSendAdapterTests::initTestCase()
{
    // OptionsModel opens QSettings; keep its writes out of the user's store,
    // as the Qt tests' OptionsModelTests does.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName(QStringLiteral("GridcoinTest"));
    QCoreApplication::setApplicationName(QStringLiteral("sendadaptertests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());

    SelectParams(CBaseChainParams::REGTEST);
}

void QmlSendAdapterTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void QmlSendAdapterTests::init()
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

void QmlSendAdapterTests::cleanup()
{
    QSettings().clear();
    if (m_prev_datadir_set) {
        gArgs.ForceSetArg("-datadir", m_prev_datadir);
    } else {
        gArgs.ClearForcedArg("-datadir");
    }
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

void QmlSendAdapterTests::largeFeeDoesNotHang()
{
    qml_test::Watchdog watchdog("QmlSendAdapterTests::largeFeeDoesNotHang");
    Harness h;
    Spies s(h);
    h.m.wallet.scriptSend({FeeReq(FEE), Ok()});
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    const quint64 id = SendAndConfirm(h.adapter());
    QVERIFY(id != 0);

    // A helper answers the fee question at 2 s, through the event queue.
    auto alive = std::make_shared<std::atomic<bool>>(true);
    std::thread helper([&h, id, alive] {
        std::this_thread::sleep_for(2s);
        QMetaObject::invokeMethod(
            qApp, [&h, id, alive] { if (*alive) h.adapter().confirmFee(id, true); }, Qt::QueuedConnection);
    });
    qml_test::ScopeExit join_helper([&helper] { helper.join(); });
    qml_test::ScopeExit retire([alive] { *alive = false; });

    const auto start = std::chrono::steady_clock::now();
    const auto since_start = [&start] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    };
    QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 1s));
    const auto asked_at = since_start();
    QVERIFY2(asked_at < 1000ms, qPrintable(QStringLiteral("the fee question came at %1 ms").arg(asked_at.count())));
    QCOMPARE(h.m.wallet.sendCalls(), 1);
    QCOMPARE(IdOf(s.fee, 0), id);
    QCOMPARE(s.fee.at(0).at(1).toLongLong(), FEE);

    // The serial lane is free while the question is open.
    auto delivered = std::make_shared<bool>(false);
    QVERIFY(h.fe.callRunner().postSerial([] {}, [delivered] { *delivered = true; }));
    QVERIFY(qml_test::PumpUntil([delivered] { return *delivered; }, 1s));
    QVERIFY2(since_start() < 2000ms, "the probe waited for the fee answer");

    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 5s));
    const std::vector<qt_test::ScriptedWallet::Call> sends = CallsOf(h.m.wallet, Method::SendCoins);
    QCOMPARE(sends.size(), size_t{2});
    QCOMPARE(sends[1].accepted_fee, int64_t{FEE});
    QCOMPARE(IdOf(s.finished, 0), id);
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Sent);
    QCOMPARE(TxidOf(s.finished, 0), QString::fromStdString(TXID));
    QCOMPARE(MessageOf(s.finished, 0), QString());
}

void QmlSendAdapterTests::feeDeclined()
{
    Harness h;
    Spies s(h);
    h.m.wallet.setLockState(true, true, false);
    h.m.wallet.scriptSend({FeeReq(FEE)});
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    const quint64 id = SendAndConfirm(h.adapter());
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
    QVERIFY(AnswerUnlock(h, s.unlock));
    QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 2s));

    QVERIFY(h.adapter().confirmFee(id, false));
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Aborted);
    QCOMPARE(MessageOf(s.finished, 0), QString());
    QCOMPARE(h.m.wallet.sendCalls(), 1);
    QVERIFY(SerialProbe(h.fe.callRunner()));
    QCOMPARE(h.m.wallet.lockCalls(), 1);
    QVERIFY(!h.adapter().busy());
}

void QmlSendAdapterTests::feeCap()
{
    Harness h;
    Spies s(h);
    std::vector<interfaces::SendCoinsResult> results;
    for (int i = 0; i <= 10; ++i) results.push_back(FeeReq(FEE + i));
    h.m.wallet.scriptSend(results);
    AutoYesFee yes(h.adapter());
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    const quint64 id = SendAndConfirm(h.adapter());
    QVERIFY(id != 0);

    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 5s));
    QCOMPARE(h.m.wallet.sendCalls(), 11);
    QCOMPARE(s.fee.count(), 10);
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Failed);
    QCOMPARE(TitleOf(s.finished, 0), SendTitle());
    QCOMPARE(MessageOf(s.finished, 0), QStringLiteral("Error: Transaction creation failed."));
    QCOMPARE(yes.returns.size(), size_t{10});
    for (bool returned : yes.returns) QVERIFY(returned);
}

void QmlSendAdapterTests::uriFillsAndNavigates()
{
    qml_test::QueuingFrontEnd bridge_fe;
    QmlProvider bridge_provider(bridge_fe);
    QmlCoreBridge bridge(bridge_provider.shell());
    Harness h;
    Spies s(h);

    // A URI that arrived before the adapter was wired is claimed by the
    // wiring's own claim.
    bridge.handleURI(Uri(Address(1), QStringLiteral("label=Alice&amount=2")));
    Wire(h.adapter(), bridge);
    QCOMPARE(RowAddress(h.adapter(), 0), Address(1));
    QCOMPARE(RoleOf(h.adapter(), 0, Role::LabelRole).toString(), QStringLiteral("Alice"));
    QCOMPARE(RoleOf(h.adapter(), 0, Role::AmountRole).toLongLong(), qint64{2 * COIN});
    QCOMPARE(s.accepted.count(), 1);
    QVERIFY(bridge.pendingUri().isEmpty());

    // An invalid one is rejected with the Widgets texts.
    bridge.handleURI(QStringLiteral("gridcoin:notanaddress"));
    QCOMPARE(s.rejected.count(), 1);
    QCOMPARE(s.rejected.at(0).at(0).toString(), UriTitle());
    QCOMPARE(s.rejected.at(0).at(1).toString(), UriText());
    QCOMPARE(s.accepted.count(), 1);

    // While a send is in progress a URI waits in the bridge, and it fills
    // the cleared page when the send has finished.
    h.m.wallet.scriptSend({Ok()});
    const quint64 id = h.adapter().send();
    QVERIFY(id != 0);
    bridge.handleURI(Uri(Address(3), QStringLiteral("amount=3")));
    QVERIFY(!bridge.pendingUri().isEmpty());
    QCOMPARE(s.accepted.count(), 1);
    QVERIFY(h.adapter().confirmSend(id, true));
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Sent);
    QCOMPARE(RowAddress(h.adapter(), 0), Address(3));
    QCOMPARE(Rows(h.adapter()), 1);
    QCOMPARE(s.accepted.count(), 2);
    QVERIFY(bridge.pendingUri().isEmpty());
}

void QmlSendAdapterTests::everyStatusHasAMessage()
{
    constexpr qint64 SHORT_FEE = 987654321;
    struct Case {
        interfaces::SendCoinsStatus status;
        qint64 fee;
        QString message;
    };
    const std::vector<Case> cases{
        {interfaces::SendCoinsStatus::InvalidAddress, 0,
         QCoreApplication::translate("SendCoinsDialog", "The recipient address is not valid, please recheck.")},
        {interfaces::SendCoinsStatus::InvalidAmount, 0,
         QCoreApplication::translate("SendCoinsDialog", "The amount to pay must be larger than 0.")},
        {interfaces::SendCoinsStatus::AmountExceedsBalance, 0,
         QCoreApplication::translate("SendCoinsDialog", "The amount exceeds your balance.")},
        {interfaces::SendCoinsStatus::AmountWithFeeExceedsBalance, SHORT_FEE,
         QCoreApplication::translate("SendCoinsDialog",
                                     "The total exceeds your balance when the %1 transaction fee is included.")
             .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, SHORT_FEE))},
        {interfaces::SendCoinsStatus::FeeExceedsSubtractedAmount, SHORT_FEE,
         QCoreApplication::translate("SendCoinsDialog",
                                     "The transaction fee (%1) exceeds the amount being sent to a recipient "
                                     "with 'Subtract fee from amount' enabled. Please enter a larger amount.")
             .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, SHORT_FEE))},
        {interfaces::SendCoinsStatus::TransactionCreationFailed, 0,
         QCoreApplication::translate("SendCoinsDialog", "Error: Transaction creation failed.")},
        {interfaces::SendCoinsStatus::TransactionCommitFailed, 0,
         QCoreApplication::translate("SendCoinsDialog",
                                     "Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.")},
        {interfaces::SendCoinsStatus::WalletUnlockedForStakingOnly, 0,
         QCoreApplication::translate("SendCoinsDialog",
                                     "Error: The wallet is unlocked for staking only, so it will not send. "
                                     "Lock the wallet and unlock it again without the staking-only option to send.")},
    };

    Harness h;
    Spies s(h);
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    int done = 0;
    for (const Case& c : cases) {
        h.m.wallet.scriptSend({Result(c.status, c.fee)});
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == done + 1; }, 2s));
        const QString which = QStringLiteral("status %1").arg(static_cast<int>(c.status));
        QVERIFY2(OutcomeOf(s.finished, done) == SendAdapter::Failed, qPrintable(which));
        QVERIFY2(TitleOf(s.finished, done) == SendTitle(), qPrintable(which));
        QVERIFY2(MessageOf(s.finished, done) == c.message,
                 qPrintable(which + QStringLiteral(": ") + MessageOf(s.finished, done)));
        ++done;
    }

    h.m.wallet.scriptSend({Ok()});
    QVERIFY(SendAndConfirm(h.adapter()) != 0);
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == done + 1; }, 2s));
    QCOMPARE(OutcomeOf(s.finished, done), SendAdapter::Sent);
    QCOMPARE(TitleOf(s.finished, done), QString());
    QCOMPARE(MessageOf(s.finished, done), QString());
}

void QmlSendAdapterTests::duplicateNeverReachesInterface()
{
    Harness h;
    Spies s(h);
    QVERIFY(h.adapter().addRecipient());
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    QVERIFY(Fill(h.adapter(), 1, Address(1), QStringLiteral("2")));
    QVERIFY(SendAndConfirm(h.adapter()) != 0);
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Failed);
    QCOMPARE(MessageOf(s.finished, 0),
             QCoreApplication::translate("SendCoinsDialog",
                                         "Duplicate address found, can only send to each address once per send operation."));
    QCOMPARE(h.m.wallet.sendCalls(), 0);
}

void QmlSendAdapterTests::lockedSendPromptsFirst()
{
    Harness h;
    Spies s(h);
    h.m.wallet.setLockState(true, true, false);
    h.m.wallet.scriptSend({Ok()});
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    QVERIFY(SendAndConfirm(h.adapter()) != 0);
    QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
    QCOMPARE(h.m.wallet.sendCalls(), 0);

    QVERIFY(AnswerUnlock(h, s.unlock));
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    QVERIFY(SerialProbe(h.fe.callRunner()));
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Sent);
    QCOMPARE(h.m.wallet.unlockCalls(), 1);
    QCOMPARE(h.m.wallet.sendCalls(), 1);
    QCOMPARE(h.m.wallet.lockCalls(), 1);
    QCOMPARE(LogOrder(h.m.wallet, {Method::UnlockWallet, Method::SendCoins, Method::LockWallet}),
             (std::vector<Method>{Method::UnlockWallet, Method::SendCoins, Method::LockWallet}));
    QCOMPARE(Rows(h.adapter()), 1);
    QCOMPARE(RowAddress(h.adapter(), 0), QString());
}

void QmlSendAdapterTests::leaseHeldAcrossFeePrompt()
{
    Harness h;
    Spies s(h);
    h.m.wallet.setLockState(true, true, false);
    h.m.wallet.scriptSend({FeeReq(FEE), Ok()});
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    const quint64 id = SendAndConfirm(h.adapter());
    QVERIFY(id != 0);
    QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
    QVERIFY(AnswerUnlock(h, s.unlock));
    QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 2s));

    // The wallet stays unlocked while the question is open.
    QVERIFY(SerialProbe(h.fe.callRunner()));
    QCOMPARE(h.m.wallet.lockCalls(), 0);

    QVERIFY(h.adapter().confirmFee(id, true));
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    QVERIFY(SerialProbe(h.fe.callRunner()));
    QCOMPARE(h.m.wallet.sendCalls(), 2);
    QCOMPARE(h.m.wallet.lockCalls(), 1);
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Sent);
}

void QmlSendAdapterTests::sendRunsOnSerialLane()
{
    const std::thread::id gui = std::this_thread::get_id();
    Harness h;
    Spies s(h);
    h.m.wallet.scriptSend({Ok()});
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    QVERIFY(SendAndConfirm(h.adapter()) != 0);
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    const std::vector<qt_test::ScriptedWallet::Call> sends = CallsOf(h.m.wallet, Method::SendCoins);
    QCOMPARE(sends.size(), size_t{1});
    QVERIFY2(sends[0].thread != gui, "the send ran on the GUI thread");
    QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Sent);
}

void QmlSendAdapterTests::messageFirstRowAndHeight()
{
    Harness h;
    Spies s(h);
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    QVERIFY(h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("message"), QStringLiteral("first")}}));
    QVERIFY(h.adapter().addRecipient());
    QVERIFY(Fill(h.adapter(), 1, Address(2)));
    QVERIFY(h.adapter().updateRecipient(1, QVariantMap{{QStringLiteral("message"), QStringLiteral("second")}}));

    // Before any height a message is allowed, and only the first row's goes.
    QVERIFY(h.adapter().messageAllowed());
    h.m.wallet.scriptSend({Ok()});
    QVERIFY(SendAndConfirm(h.adapter()) != 0);
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    const std::vector<qt_test::ScriptedWallet::Call> sends = CallsOf(h.m.wallet, Method::SendCoins);
    QCOMPARE(sends.size(), size_t{1});
    QCOMPARE(sends[0].recipients.size(), size_t{2});
    QCOMPARE(sends[0].recipients[0].message, std::string("first"));
    QCOMPARE(sends[0].recipients[1].message, std::string());

    // The block after the height decides.
    ForcedDisableHeight disable("100");
    h.adapter().setChainHeight(98);
    QCOMPARE(h.adapter().messageAllowed(), true);
    h.adapter().setChainHeight(99);
    QCOMPARE(h.adapter().messageAllowed(), false);
}

void QmlSendAdapterTests::recipientEdits()
{
    Harness h;
    SendAdapter& adapter = h.adapter();

    QVERIFY(adapter.addRecipient());
    QCOMPARE(Rows(adapter), 2);
    QVERIFY(adapter.removeRecipient(1));
    QCOMPARE(Rows(adapter), 1);
    QVERIFY(!adapter.removeRecipient(0));
    QCOMPARE(Rows(adapter), 1);
    QVERIFY(!adapter.removeRecipient(5));

    QVERIFY(adapter.addRecipient());
    QVERIFY(Fill(adapter, 1, Address(2)));
    QVERIFY(adapter.clearRecipients());
    QCOMPARE(Rows(adapter), 1);
    QCOMPARE(RowAddress(adapter, 0), QString());
    QCOMPARE(RoleOf(adapter, 0, Role::AmountTextRole).toString(), QString());

    QVERIFY(adapter.updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), Address(1)},
                                                   {QStringLiteral("label"), QStringLiteral("L")},
                                                   {QStringLiteral("amount"), QStringLiteral("1.5")},
                                                   {QStringLiteral("message"), QStringLiteral("M")},
                                                   {QStringLiteral("subtractFee"), true}}));
    QCOMPARE(RowAddress(adapter, 0), Address(1));
    QCOMPARE(RoleOf(adapter, 0, Role::LabelRole).toString(), QStringLiteral("L"));
    QCOMPARE(RoleOf(adapter, 0, Role::AmountRole).toLongLong(), qint64{150000000});
    QCOMPARE(RoleOf(adapter, 0, Role::AmountTextRole).toString(), QStringLiteral("1.5"));
    QCOMPARE(RoleOf(adapter, 0, Role::MessageRole).toString(), QStringLiteral("M"));
    QCOMPARE(RoleOf(adapter, 0, Role::SubtractFeeRole).toBool(), true);
    QCOMPARE(RoleOf(adapter, 0, Role::AddressValidRole).toBool(), true);
    QCOMPARE(RoleOf(adapter, 0, Role::AmountValidRole).toBool(), true);
    QVERIFY(!adapter.updateRecipient(1, QVariantMap{{QStringLiteral("label"), QStringLiteral("x")}}));

    const QHash<int, QByteArray> names = adapter.recipients()->roleNames();
    QStringList listed;
    for (const QByteArray& name : names) listed << QString::fromLatin1(name);
    listed.sort();
    QCOMPARE(listed, (QStringList{QStringLiteral("addressValid"), QStringLiteral("amount"), QStringLiteral("amountText"),
                                  QStringLiteral("amountValid"), QStringLiteral("label"), QStringLiteral("message"),
                                  QStringLiteral("recipient"), QStringLiteral("subtractFee")}));
}

void QmlSendAdapterTests::invalidRowsMarkedNothingAsked()
{
    Harness h;
    Spies s(h);
    QVERIFY(Fill(h.adapter(), 0, QStringLiteral("notanaddress")));
    QVERIFY(h.adapter().addRecipient());
    QVERIFY(Fill(h.adapter(), 1, Address(2), QStringLiteral("0")));

    QCOMPARE(h.adapter().send(), quint64{0});
    QCOMPARE(RoleOf(h.adapter(), 0, Role::AddressValidRole).toBool(), false);
    QCOMPARE(RoleOf(h.adapter(), 0, Role::AmountValidRole).toBool(), true);
    QCOMPARE(RoleOf(h.adapter(), 1, Role::AddressValidRole).toBool(), true);
    QCOMPARE(RoleOf(h.adapter(), 1, Role::AmountValidRole).toBool(), false);
    QCOMPARE(s.confirm.count(), 0);
    QVERIFY(!h.adapter().busy());
    QVERIFY(SerialProbe(h.fe.callRunner()));
    QCOMPARE(h.m.wallet.sendCalls(), 0);

    // An edit marks its field valid again until the next send().
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    QCOMPARE(RoleOf(h.adapter(), 0, Role::AddressValidRole).toBool(), true);
}

void QmlSendAdapterTests::labelAndSubtractReachWallet()
{
    Harness h;
    Spies s(h);
    QVERIFY(h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), Address(1)},
                                                       {QStringLiteral("amount"), QStringLiteral("1")},
                                                       {QStringLiteral("label"), QStringLiteral("Lbl")},
                                                       {QStringLiteral("subtractFee"), true}}));
    h.m.wallet.scriptSend({Ok()});
    const quint64 id = h.adapter().send();
    QVERIFY(id != 0);
    QCOMPARE(s.confirm.count(), 1);
    const QString text = s.confirm.at(0).at(2).toString();
    QVERIFY2(text.contains(QStringLiteral(" ") + QCoreApplication::translate("SendCoinsDialog", "(minus fee)")),
             qPrintable(text));
    QVERIFY(h.adapter().confirmSend(id, true));
    QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
    const std::vector<qt_test::ScriptedWallet::Call> sends = CallsOf(h.m.wallet, Method::SendCoins);
    QCOMPARE(sends.size(), size_t{1});
    QCOMPARE(sends[0].recipients.size(), size_t{1});
    QCOMPARE(sends[0].recipients[0].label, std::string("Lbl"));
    QVERIFY(sends[0].recipients[0].subtract_fee_from_amount);
}

void QmlSendAdapterTests::displayUnit()
{
    Harness h;
    QSignalSpy unit_changed(&h.adapter(), &SendAdapter::unitNameChanged);
    QCOMPARE(h.adapter().unitName(), BitcoinUnits::name(BitcoinUnits::BTC));

    QVERIFY(h.m.options.setData(h.m.options.index(OptionsModel::DisplayUnit), BitcoinUnits::mBTC, Qt::EditRole));
    QCOMPARE(h.adapter().unitName(), BitcoinUnits::name(BitcoinUnits::mBTC));
    QCOMPARE(unit_changed.count(), 1);
    QVERIFY(h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("amount"), QStringLiteral("1234.5")}}));
    QCOMPARE(RoleOf(h.adapter(), 0, Role::AmountRole).toLongLong(), qint64{123450000});

    QVERIFY(h.m.options.setData(h.m.options.index(OptionsModel::DisplayUnit), BitcoinUnits::BTC, Qt::EditRole));
    QCOMPARE(RoleOf(h.adapter(), 0, Role::AmountTextRole).toString(), BitcoinUnits::format(BitcoinUnits::BTC, 123450000));
    QCOMPARE(RoleOf(h.adapter(), 0, Role::AmountRole).toLongLong(), qint64{123450000});
    QCOMPARE(h.adapter().unitName(), BitcoinUnits::name(BitcoinUnits::BTC));
    QCOMPARE(unit_changed.count(), 2);
}

void QmlSendAdapterTests::confirmationText()
{
    Harness h;
    Spies s(h);
    QVERIFY(Fill(h.adapter(), 0, Address(1)));
    QVERIFY(h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("label"), QStringLiteral("<b>x</b>")}}));
    QVERIFY(h.adapter().addRecipient());
    QVERIFY(Fill(h.adapter(), 1, Address(2), QStringLiteral("2")));

    const quint64 id = h.adapter().send();
    QVERIFY(id != 0);
    QCOMPARE(s.confirm.count(), 1);
    const QString part0 = QCoreApplication::translate("SendCoinsDialog", "<b>%1</b> to %2 (%3)")
                              .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, COIN),
                                   QStringLiteral("&lt;b&gt;x&lt;/b&gt;"), Address(1));
    const QString part1 = QCoreApplication::translate("SendCoinsDialog", "<b>%1</b> to %2 (%3)")
                              .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, 2 * COIN), QString(), Address(2));
    const QString expected = QCoreApplication::translate("SendCoinsDialog", "Are you sure you want to send %1?")
                                 .arg(part0 + QCoreApplication::translate("SendCoinsDialog", " and ") + part1);
    QCOMPARE(IdOf(s.confirm, 0), id);
    QCOMPARE(s.confirm.at(0).at(1).toString(), QCoreApplication::translate("SendCoinsDialog", "Confirm send coins"));
    QCOMPARE(s.confirm.at(0).at(2).toString(), expected);
    QVERIFY(s.confirm.at(0).at(2).toString().contains(QStringLiteral("&lt;b&gt;x&lt;/b&gt;")));
    QVERIFY(h.adapter().confirmSend(id, false));
}

void QmlSendAdapterTests::busyRefusesSecondSend()
{
    {
        Harness h;
        Spies s(h);
        h.m.wallet.scriptSend({Ok()});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));

        // Between the confirmation request and its answer, and while the
        // send is in flight.
        const quint64 id = h.adapter().send();
        QVERIFY(id != 0);
        QVERIFY(h.adapter().busy());
        QCOMPARE(h.adapter().send(), quint64{0});
        QCOMPARE(s.confirm.count(), 1);
        QVERIFY(h.adapter().confirmSend(id, true));
        QCOMPARE(h.adapter().send(), quint64{0});
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
        QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Sent);

        // Another owner holds the prompt latch: the send finishes as Busy.
        WalletPromptLatch& latch = h.fe.provider().walletPromptLatch();
        QVERIFY(latch.tryAcquire(Owner::Security));
        qml_test::ScopeExit release([&latch] { latch.release(Owner::Security); });
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 busy_id = h.adapter().send();
        QVERIFY(busy_id != 0);
        QVERIFY(h.adapter().confirmSend(busy_id, true));
        QCOMPARE(s.finished.count(), 2);
        QCOMPARE(IdOf(s.finished, 1), busy_id);
        QCOMPARE(OutcomeOf(s.finished, 1), SendAdapter::Busy);
        QVERIFY(!h.adapter().busy());
        QCOMPARE(TitleOf(s.finished, 1), QCoreApplication::translate("SendCoinsDialog", "Send Coins"));
        QCOMPARE(MessageOf(s.finished, 1), BusyMessage());
        QCOMPARE(h.m.wallet.sendCalls(), 1);
    }

    // The Busy texts are looked up in the Widgets send dialog's context.
    {
        MarkingTranslator translator({QStringLiteral("SendCoinsDialog")});
        QCoreApplication::installTranslator(&translator);
        qml_test::ScopeExit remove_translator([&translator] { QCoreApplication::removeTranslator(&translator); });
        Harness h;
        Spies s(h);
        WalletPromptLatch& latch = h.fe.provider().walletPromptLatch();
        QVERIFY(latch.tryAcquire(Owner::Security));
        qml_test::ScopeExit release([&latch] { latch.release(Owner::Security); });
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = h.adapter().send();
        QVERIFY(id != 0);
        QVERIFY(h.adapter().confirmSend(id, true));
        QCOMPARE(s.finished.count(), 1);
        QVERIFY2(TitleOf(s.finished, 0).startsWith(QStringLiteral("[SendCoinsDialog] ")), qPrintable(TitleOf(s.finished, 0)));
        QVERIFY2(MessageOf(s.finished, 0).startsWith(QStringLiteral("[SendCoinsDialog] ")),
                 qPrintable(MessageOf(s.finished, 0)));
    }
}

void QmlSendAdapterTests::confirmedRowsAreSent()
{
    ForcedDisableHeight disable("100");

    // Yes: the rows as confirmed are sent, whatever happened since.
    {
        qml_test::QueuingFrontEnd bridge_fe;
        QmlProvider bridge_provider(bridge_fe);
        QmlCoreBridge bridge(bridge_provider.shell());
        Harness h;
        Spies s(h);
        Wire(h.adapter(), bridge);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        QVERIFY(h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("message"), QStringLiteral("m")}}));
        h.adapter().setChainHeight(98);
        QVERIFY(h.adapter().messageAllowed());
        h.m.wallet.scriptSend({Ok()});
        const quint64 id = h.adapter().send();
        QVERIFY(id != 0);

        QVERIFY(!h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("amount"), QStringLiteral("5")}}));
        QCOMPARE(RoleOf(h.adapter(), 0, Role::AmountRole).toLongLong(), qint64{COIN});
        bridge.handleURI(Uri(Address(3), QStringLiteral("amount=3")));
        QVERIFY(!bridge.pendingUri().isEmpty());
        QCOMPARE(Rows(h.adapter()), 1);
        h.adapter().setChainHeight(99);
        QVERIFY(!h.adapter().messageAllowed());

        QVERIFY(h.adapter().confirmSend(id, true));
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
        const std::vector<qt_test::ScriptedWallet::Call> sends = CallsOf(h.m.wallet, Method::SendCoins);
        QCOMPARE(sends.size(), size_t{1});
        QCOMPARE(sends[0].recipients.size(), size_t{1});
        QCOMPARE(QString::fromStdString(sends[0].recipients[0].address), Address(1));
        QCOMPARE(sends[0].recipients[0].amount, int64_t{COIN});
        QCOMPARE(sends[0].recipients[0].message, std::string("m"));
        QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Sent);
        QCOMPARE(RowAddress(h.adapter(), 0), Address(3));
        QCOMPARE(s.accepted.count(), 1);
    }

    // No: busy ends with the Aborted send, and the URI is claimed.
    {
        qml_test::QueuingFrontEnd bridge_fe;
        QmlProvider bridge_provider(bridge_fe);
        QmlCoreBridge bridge(bridge_provider.shell());
        Harness h;
        Spies s(h);
        Wire(h.adapter(), bridge);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = h.adapter().send();
        QVERIFY(id != 0);
        bridge.handleURI(Uri(Address(3), QStringLiteral("amount=3")));
        QVERIFY(!bridge.pendingUri().isEmpty());

        QVERIFY(h.adapter().confirmSend(id, false));
        QCOMPARE(s.finished.count(), 1);
        QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Aborted);
        QVERIFY(!h.adapter().busy());
        QVERIFY(bridge.pendingUri().isEmpty());
        QCOMPARE(s.accepted.count(), 1);
        QCOMPARE(RowAddress(h.adapter(), 0), Address(1));
        QCOMPARE(RowAddress(h.adapter(), 1), Address(3));
    }

    // A fee question: every attempt sends the rows as confirmed.
    {
        Harness h;
        Spies s(h);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        QVERIFY(h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("message"), QStringLiteral("m")}}));
        h.adapter().setChainHeight(98);
        h.m.wallet.scriptSend({FeeReq(FEE), Ok()});
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 2s));
        h.adapter().setChainHeight(99);
        QVERIFY(h.adapter().confirmFee(id, true));
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
        const std::vector<qt_test::ScriptedWallet::Call> sends = CallsOf(h.m.wallet, Method::SendCoins);
        QCOMPARE(sends.size(), size_t{2});
        QCOMPARE(sends[0].recipients[0].message, std::string("m"));
        QCOMPARE(sends[1].recipients[0].message, std::string("m"));
    }
}

void QmlSendAdapterTests::unlockCancelledDuringSend()
{
    // Cancelled while the passphrase is asked for: Aborted, nothing sent, the
    // rows kept, and a URI that waited is claimed.
    {
        qml_test::QueuingFrontEnd bridge_fe;
        QmlProvider bridge_provider(bridge_fe);
        QmlCoreBridge bridge(bridge_provider.shell());
        Harness h;
        Spies s(h);
        Wire(h.adapter(), bridge);
        h.m.wallet.setLockState(true, true, false);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
        // With an amount, so the row it adds is valid for the next send().
        bridge.handleURI(Uri(Address(3), QStringLiteral("amount=3")));
        QVERIFY(!bridge.pendingUri().isEmpty());

        QVERIFY(h.gate().cancelUnlock(IdOf(s.unlock, 0)));
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
        QCOMPARE(IdOf(s.finished, 0), id);
        QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Aborted);
        QCOMPARE(MessageOf(s.finished, 0), QString());
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(h.m.wallet.sendCalls(), 0);
        QCOMPARE(h.m.wallet.unlockCalls(), 0);
        QCOMPARE(h.m.wallet.lockCalls(), 0);
        QVERIFY(!h.adapter().busy());
        QCOMPARE(RowAddress(h.adapter(), 0), Address(1));
        QCOMPARE(s.accepted.count(), 1);
        QVERIFY(bridge.pendingUri().isEmpty());
        const quint64 again = h.adapter().send();
        QVERIFY(again != 0);
        QVERIFY(h.adapter().confirmSend(again, false));
    }

    // Cancelled while the answer runs: the unlock is given back, nothing is
    // sent.
    {
        qml_test::Watchdog watchdog("QmlSendAdapterTests::unlockCancelledDuringSend");
        qt_test::FakeLatch unlock_latch;
        Harness h;
        qml_test::ScopeExit unlatch([&] {
            unlock_latch.open = true;
            h.m.wallet.m_unlock_latch = nullptr;
        });
        Spies s(h);
        h.m.wallet.setLockState(true, true, false);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
        h.m.wallet.m_unlock_latch = &unlock_latch;
        QVERIFY(AnswerUnlock(h, s.unlock));
        QVERIFY(WaitNoEvents([&] { return h.m.wallet.unlockCalls() == 1; }, 2s));
        QVERIFY(h.gate().cancelUnlock(IdOf(s.unlock, 0)));
        unlock_latch.open = true;

        QVERIFY2(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s), "the send did not finish");
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(h.m.wallet.unlockCalls(), 1);
        QCOMPARE(h.m.wallet.lockCalls(), 1);
        QCOMPARE(h.m.wallet.sendCalls(), 0);
        QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Aborted);
        QVERIFY(!h.adapter().busy());
    }
}

void QmlSendAdapterTests::negotiatorPerSend()
{
    // A lower fee asked for by the next send is asked for again: the
    // accepted fee starts over.
    {
        Harness h;
        Spies s(h);
        AutoYesFee yes(h.adapter());
        constexpr qint64 LOWER = FEE / 2;
        h.m.wallet.scriptSend({FeeReq(FEE), Ok(), FeeReq(LOWER), Ok()});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        QVERIFY(SendAndConfirm(h.adapter()) != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 2s));
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 second = SendAndConfirm(h.adapter());
        QVERIFY(second != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 2; }, 2s));

        std::vector<int64_t> fees;
        for (const qt_test::ScriptedWallet::Call& call : CallsOf(h.m.wallet, Method::SendCoins)) {
            fees.push_back(call.accepted_fee);
        }
        QCOMPARE(fees, (std::vector<int64_t>{0, FEE, 0, LOWER}));
        int asked_second = 0;
        for (int i = 0; i < s.fee.count(); ++i) {
            if (IdOf(s.fee, i) == second) {
                ++asked_second;
                QCOMPARE(s.fee.at(i).at(1).toLongLong(), LOWER);
            }
        }
        QCOMPARE(asked_second, 1);
        QCOMPARE(OutcomeOf(s.finished, 1), SendAdapter::Sent);
    }

    // A send that hit the cap leaves no count behind.
    {
        Harness h;
        Spies s(h);
        AutoYesFee yes(h.adapter());
        std::vector<interfaces::SendCoinsResult> results;
        for (int i = 0; i <= 10; ++i) results.push_back(FeeReq(FEE + i));
        results.push_back(FeeReq(FEE));
        results.push_back(Ok());
        h.m.wallet.scriptSend(results);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        QVERIFY(SendAndConfirm(h.adapter()) != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 1; }, 5s));
        QCOMPARE(OutcomeOf(s.finished, 0), SendAdapter::Failed);
        const int asked_first = s.fee.count();
        const quint64 second = SendAndConfirm(h.adapter());
        QVERIFY(second != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.finished.count() == 2; }, 2s));
        QCOMPARE(s.fee.count() - asked_first, 1);
        QCOMPARE(OutcomeOf(s.finished, 1), SendAdapter::Sent);
    }
}

void QmlSendAdapterTests::staleAnswersRefused()
{
    qml_test::Watchdog watchdog("QmlSendAdapterTests::staleAnswersRefused");

    // A second yes while the attempt is in flight.
    {
        qt_test::FakeLatch send_latch;
        Harness h;
        qml_test::ScopeExit unlatch([&] {
            send_latch.open = true;
            h.m.wallet.m_send_latch = nullptr;
        });
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.scriptSend({Ok()});
        h.m.wallet.m_send_latch_from = 0;
        h.m.wallet.m_send_latch = &send_latch;
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = h.adapter().send();
        QVERIFY(id != 0);
        const bool first = h.adapter().confirmSend(id, true);
        QVERIFY(qml_test::PumpUntil([&] { return h.m.wallet.sendCalls() == 1; }, 2s));
        const bool again = h.adapter().confirmSend(id, true);
        QCOMPARE(finishedSpy.count(), 0);
        QVERIFY(first);
        QVERIFY(!again);
        QCOMPARE(h.m.wallet.lockStateCalls(), 1);
        send_latch.open = true;
        QVERIFY(qml_test::PumpUntil([&] { return finishedSpy.count() == 1; }, 2s));
        qml_test::PumpFor(50ms);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Sent);
    }

    // A second fee answer while the next attempt is in flight.
    {
        qt_test::FakeLatch send_latch;
        Harness h;
        qml_test::ScopeExit unlatch([&] {
            send_latch.open = true;
            h.m.wallet.m_send_latch = nullptr;
        });
        Spies s(h);
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.scriptSend({FeeReq(FEE), Ok()});
        h.m.wallet.m_send_latch_from = 1;
        h.m.wallet.m_send_latch = &send_latch;
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 2s));
        const bool yes = h.adapter().confirmFee(id, true);
        QVERIFY(qml_test::PumpUntil([&] { return h.m.wallet.sendCalls() == 2; }, 2s));
        const bool yes2 = h.adapter().confirmFee(id, true);
        const bool no2 = h.adapter().confirmFee(id, false);
        send_latch.open = true;
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QVERIFY(qml_test::PumpUntil([&] { return finishedSpy.count() == 1; }, 2s));
        QCOMPARE(h.m.wallet.sendCalls(), 2);
        QVERIFY(yes);
        QVERIFY(!yes2);
        QVERIFY(!no2);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Sent);
    }

    // A second yes to the confirmation while a fee question is open.
    {
        Harness h;
        Spies s(h);
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.setLockState(true, true, false);
        h.m.wallet.scriptSend({FeeReq(FEE), Ok()});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
        QVERIFY(AnswerUnlock(h, s.unlock));
        QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 2s));
        const bool again = h.adapter().confirmSend(id, true);
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(h.m.wallet.lockCalls(), 0);
        QCOMPARE(finishedSpy.count(), 0);
        QVERIFY(!again);
        QVERIFY(h.adapter().confirmFee(id, true));
        QVERIFY(qml_test::PumpUntil([&] { return finishedSpy.count() == 1; }, 2s));
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Sent);
        QCOMPARE(LogOrder(h.m.wallet, {Method::UnlockWallet, Method::SendCoins, Method::LockWallet}),
                 (std::vector<Method>{Method::UnlockWallet, Method::SendCoins, Method::SendCoins, Method::LockWallet}));
    }

    // The answer matrix: in each state, an answer that state does not take
    // changes nothing.

    // An attempt in flight: no to the confirmation.
    {
        qt_test::FakeLatch send_latch;
        Harness h;
        qml_test::ScopeExit unlatch([&] {
            send_latch.open = true;
            h.m.wallet.m_send_latch = nullptr;
        });
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.scriptSend({Ok()});
        h.m.wallet.m_send_latch = &send_latch;
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return h.m.wallet.sendCalls() == 1; }, 2s));
        const bool no = h.adapter().confirmSend(id, false);
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(h.m.wallet.sendCalls(), 1);
        QVERIFY(!no);
        send_latch.open = true;
        QVERIFY(qml_test::PumpUntil([&] { return finishedSpy.count() == 1; }, 2s));
        QCOMPARE(h.m.wallet.sendCalls(), 1);
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Sent);
    }

    // A fee question open: no to the confirmation; then no to the fee ends it.
    {
        Harness h;
        Spies s(h);
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.scriptSend({FeeReq(FEE), Ok()});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 2s));
        const bool no = h.adapter().confirmSend(id, false);
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(h.m.wallet.sendCalls(), 1);
        QVERIFY(!no);
        QVERIFY(h.adapter().confirmFee(id, false));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Aborted);
    }

    // Waiting for the gate (the passphrase is asked for): every answer.
    {
        Harness h;
        Spies s(h);
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.setLockState(true, true, false);
        h.m.wallet.scriptSend({Ok()});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
        const bool no = h.adapter().confirmSend(id, false);
        const bool yes = h.adapter().confirmSend(id, true);
        const bool fee_yes = h.adapter().confirmFee(id, true);
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(h.m.wallet.sendCalls(), 0);
        QVERIFY(!no);
        QVERIFY(!yes);
        QVERIFY(!fee_yes);
        QVERIFY(h.gate().cancelUnlock(IdOf(s.unlock, 0)));
        QVERIFY(qml_test::PumpUntil([&] { return finishedSpy.count() == 1; }, 2s));
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Aborted);
    }

    // Waiting for the confirmation: fee answers.
    {
        Harness h;
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = h.adapter().send();
        QVERIFY(id != 0);
        const bool fee_yes = h.adapter().confirmFee(id, true);
        const bool fee_no = h.adapter().confirmFee(id, false);
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(h.m.wallet.sendCalls(), 0);
        QVERIFY(!fee_yes);
        QVERIFY(!fee_no);
        QVERIFY(h.adapter().confirmSend(id, false));
        QCOMPARE(finishedSpy.count(), 1);
    }

    // After Sent.
    {
        Harness h;
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.scriptSend({Ok()});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QVERIFY(qml_test::PumpUntil([&] { return finishedSpy.count() == 1; }, 2s));
        const bool yes = h.adapter().confirmSend(id, true);
        const bool fee_yes = h.adapter().confirmFee(id, true);
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(h.m.wallet.sendCalls(), 1);
        QVERIFY(!yes);
        QVERIFY(!fee_yes);
    }

    // After Busy.
    {
        Harness h;
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        WalletPromptLatch& latch = h.fe.provider().walletPromptLatch();
        QVERIFY(latch.tryAcquire(Owner::Security));
        qml_test::ScopeExit release([&latch] { latch.release(Owner::Security); });
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = SendAndConfirm(h.adapter());
        QVERIFY(id != 0);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Busy);
        const bool yes = h.adapter().confirmSend(id, true);
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(h.m.wallet.sendCalls(), 0);
        QVERIFY(!yes);
    }

    // An unknown id.
    {
        Harness h;
        QSignalSpy finishedSpy(&h.adapter(), &SendAdapter::finished);
        h.m.wallet.scriptSend({Ok()});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 id = h.adapter().send();
        QVERIFY(id != 0);
        const bool unknown = h.adapter().confirmSend(id + 1000, true);
        QVERIFY(SerialProbe(h.fe.callRunner()));
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(h.m.wallet.sendCalls(), 0);
        QVERIFY(!unknown);
        QVERIFY(h.adapter().confirmSend(id, true));
        QVERIFY(qml_test::PumpUntil([&] { return finishedSpy.count() == 1; }, 2s));
        QCOMPARE(OutcomeOf(finishedSpy, 0), SendAdapter::Sent);
    }
}

void QmlSendAdapterTests::unattachedInvokablesAreSafe()
{
    ForcedDisableHeight disable("100");
    const QString valid = Address(1);
    const QString first_uri = Uri(Address(2), QStringLiteral("amount=1"));
    const QString second_uri = Uri(Address(3), QStringLiteral("amount=1"));

    // Before attach.
    {
        qt_test::ScriptedWallet wallet;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        WalletOpGate& gate = fe.provider().walletOps();
        SendAdapter& adapter = fe.provider().sendAdapter();
        QSignalSpy confirm(&adapter, &SendAdapter::sendConfirmationRequested);

        QVERIFY(!gate.answerUnlock(1, QStringLiteral("p")));
        QVERIFY(!gate.cancelUnlock(1));
        bool called = false;
        QCOMPARE(gate.begin([&called](quint64) { called = true; },
                            [&called](quint64, WalletOpGate::Result) { called = true; }),
                 quint64{0});
        qml_test::PumpFor(100ms);
        QVERIFY(!called);
        QVERIFY(!gate.busy());

        QVERIFY(!adapter.updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), valid},
                                                        {QStringLiteral("amount"), QStringLiteral("1")}}));
        QCOMPARE(RowAddress(adapter, 0), QString());
        QVERIFY(!adapter.addRecipient());
        QVERIFY(!adapter.removeRecipient(0));
        QVERIFY(!adapter.clearRecipients());
        QCOMPARE(Rows(adapter), 1);
        QCOMPARE(adapter.send(), quint64{0});
        QCOMPARE(confirm.count(), 0);
        QVERIFY(!adapter.confirmSend(1, true));
        QVERIFY(!adapter.confirmFee(1, true));
        QVERIFY(!adapter.acceptUri(first_uri));
        adapter.claimPendingUri();
        QCOMPARE(RowAddress(adapter, 0), QString());
        adapter.setChainHeight(99);
        QVERIFY(adapter.messageAllowed());
        QVERIFY(!adapter.outOfSyncTitle().isEmpty());
        QVERIFY(!adapter.outOfSyncText().isEmpty());
        QVERIFY(!adapter.busy());
        QVERIFY(!adapter.unitName().isEmpty());
        QCOMPARE(wallet.log().size(), size_t{0});
    }

    // After detach.
    {
        qml_test::QueuingFrontEnd bridge_fe;
        QmlProvider bridge_provider(bridge_fe);
        QmlCoreBridge bridge(bridge_provider.shell());
        Harness h;
        Spies s(h);
        Wire(h.adapter(), bridge);
        QVERIFY(h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("recipient"), valid},
                                                           {QStringLiteral("amount"), QStringLiteral("1")}}));
        QVERIFY(h.adapter().acceptUri(first_uri));
        QCOMPARE(Rows(h.adapter()), 2);
        QCOMPARE(RowAddress(h.adapter(), 1), Address(2));
        QCOMPARE(RoleOf(h.adapter(), 1, Role::AmountRole).toLongLong(), qint64{COIN});
        QVERIFY(bridge.pendingUri().isEmpty());

        h.fe.detachModels();
        const size_t logged = h.m.wallet.log().size();
        QCOMPARE(RowAddress(h.adapter(), 0), valid);
        QCOMPARE(h.adapter().send(), quint64{0});
        QCOMPARE(s.confirm.count(), 0);
        QVERIFY(!h.adapter().updateRecipient(0, QVariantMap{{QStringLiteral("amount"), QStringLiteral("2")}}));
        QVERIFY(!h.adapter().addRecipient());
        QVERIFY(!h.adapter().acceptUri(second_uri));
        QVERIFY(bridge.pendingUri().isEmpty());
        bridge.handleURI(second_uri);
        h.adapter().claimPendingUri();
        QCOMPARE(Rows(h.adapter()), 2);
        QCOMPARE(RowAddress(h.adapter(), 0), valid);
        QVERIFY(!bridge.pendingUri().isEmpty());
        QVERIFY(!h.gate().answerUnlock(1, QStringLiteral("p")));
        h.adapter().setChainHeight(99);
        QVERIFY(h.adapter().messageAllowed());
        bool called = false;
        QCOMPARE(h.gate().begin([&called](quint64) { called = true; },
                                [&called](quint64, WalletOpGate::Result) { called = true; }),
                 quint64{0});
        qml_test::PumpFor(100ms);
        QVERIFY(!called);
        QVERIFY(!h.gate().busy());
        QCOMPARE(h.m.wallet.log().size(), logged);
    }

    // Attached, with a closed runner.
    {
        Harness h;
        h.fe.callRunner().close();
        const size_t logged = h.m.wallet.log().size();
        bool called = false;
        QCOMPARE(h.gate().begin([&called](quint64) { called = true; },
                                [&called](quint64, WalletOpGate::Result) { called = true; }),
                 quint64{0});
        qml_test::PumpFor(100ms);
        QVERIFY(!called);
        QVERIFY(!h.gate().busy());
        QCOMPARE(h.m.wallet.log().size(), logged);
    }
}

void QmlSendAdapterTests::invokablesDeadAfterDetach()
{
    // The gate waiting for a passphrase.
    {
        Harness h;
        Spies s(h);
        h.m.wallet.setLockState(true, true, false);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        QVERIFY(SendAndConfirm(h.adapter()) != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
        const quint64 gid = IdOf(s.unlock, 0);
        h.fe.detachModels();
        s.gate_finished.clear();
        s.finished.clear();
        QVERIFY(!h.gate().cancelUnlock(gid));
        qml_test::PumpFor(50ms);
        QCOMPARE(s.gate_finished.count(), 0);
        QCOMPARE(s.finished.count(), 0);
    }

    // The adapter waiting for the confirmation.
    {
        Harness h;
        Spies s(h);
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 sid = h.adapter().send();
        QVERIFY(sid != 0);
        h.fe.detachModels();
        s.gate_finished.clear();
        s.finished.clear();
        QVERIFY(!h.adapter().confirmSend(sid, false));
        QVERIFY(!h.adapter().confirmSend(sid, true));
        qml_test::PumpFor(50ms);
        QCOMPARE(s.gate_finished.count(), 0);
        QCOMPARE(s.finished.count(), 0);
    }

    // The adapter waiting for a fee answer, with the wallet unlocked for it.
    {
        Harness h;
        Spies s(h);
        h.m.wallet.setLockState(true, true, false);
        h.m.wallet.scriptSend({FeeReq(FEE)});
        QVERIFY(Fill(h.adapter(), 0, Address(1)));
        const quint64 sid = SendAndConfirm(h.adapter());
        QVERIFY(sid != 0);
        QVERIFY(qml_test::PumpUntil([&] { return s.unlock.count() == 1; }, 2s));
        QVERIFY(AnswerUnlock(h, s.unlock));
        QVERIFY(qml_test::PumpUntil([&] { return s.fee.count() == 1; }, 2s));
        QCOMPARE(IdOf(s.fee, 0), sid);
        QCOMPARE(s.fee.at(0).at(1).toLongLong(), FEE);
        h.fe.detachModels();
        QCOMPARE(h.m.wallet.lockCalls(), 1);
        QVERIFY(!h.adapter().busy());
        QVERIFY(!h.gate().busy());
        s.gate_finished.clear();
        s.finished.clear();
        QVERIFY(!h.adapter().confirmFee(sid, true));
        qml_test::PumpFor(50ms);
        QCOMPARE(s.gate_finished.count(), 0);
        QCOMPARE(s.finished.count(), 0);
        QCOMPARE(h.m.wallet.sendCalls(), 1);
    }
}
