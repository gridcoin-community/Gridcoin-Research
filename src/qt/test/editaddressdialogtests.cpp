// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/editaddressdialogtests.h"

#include "logging.h"
#include "qt/addresstablemodel.h"
#include "qt/editaddressdialog.h"
#include "qt/guiipcworker.h"
#include "qt/guistatecache.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"
#include "sync.h"
#include "util.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

//! A wallet whose picker list counts its calls. The head never calls it: the
//! core computes the list. Off the thread that made it, it takes
//! qt_test::cs_main_stand_in first, where the wallet's own walk takes cs_main.
class UnbookedFakeWallet : public qt_test::FakeWallet
{
public:
    std::atomic<int> m_unbooked_calls{0};

    std::vector<std::string> getUnbookedReceiveAddresses() override
    {
        ++m_unbooked_calls;

        if (std::this_thread::get_id() != m_maker_thread) {
            LOCK(qt_test::cs_main_stand_in);
        }

        return {"b", "a"};
    }

private:
    const std::thread::id m_maker_thread{std::this_thread::get_id()};
};

//! A WalletModel over fakes, connected to a GuiStateCache over a FakeGuiState
//! as StartGridcoinQt connects them: the model is declared after the cache,
//! so it goes first. The model can be destroyed before the rest.
struct DialogFixture
{
    explicit DialogFixture(bool connect_cache = true)
    {
        if (connect_cache) model->setGuiState(&cache);
    }

    UnbookedFakeWallet wallet;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel options{node, sidestakes};
    qt_test::FakeWalletTxSource tx_source;
    qt_test::FakeCoinSource coin_source;
    qt_test::FakeGuiState gui_state;
    GuiStateCache cache{gui_state};
    std::unique_ptr<WalletModel> model{std::make_unique<WalletModel>(wallet, tx_source, coin_source, &options)};

    AddressTableModel* Table() { return model->getAddressTableModel(); }
};

QComboBox* ComboOf(EditAddressDialog& dialog)
{
    return dialog.findChild<QComboBox*>();
}

QPushButton* OkOf(EditAddressDialog& dialog)
{
    QDialogButtonBox* const box = dialog.findChild<QDialogButtonBox*>();
    return box ? box->button(QDialogButtonBox::Ok) : nullptr;
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

} // namespace

void EditAddressDialogTests::initTestCase()
{
    // OptionsModel reads QSettings. Redirected as WalletModelTests does, so a
    // run cannot reach or leave anything in a real store.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("editaddressdialogtests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());
}

void EditAddressDialogTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void EditAddressDialogTests::init()
{
    // A temporary datadir, so that nothing reached through the options model
    // creates the default one on the host.
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void EditAddressDialogTests::cleanup()
{
    QSettings().clear();
    gArgs.ForceSetArg("-datadir", "");
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

// The dialog asks the core, shows that it is loading, and fills its combo
// from the answer, sorted by the model.
void EditAddressDialogTests::askingFillsTheComboFromTheCoresAnswer()
{
    DialogFixture fixture;
    EditAddressDialog dialog(EditAddressDialog::AddExistingReceivingAddress);
    dialog.setModel(fixture.Table());

    QComboBox* const combo = ComboOf(dialog);
    QPushButton* const ok = OkOf(dialog);
    QVERIFY(combo);
    QVERIFY(ok);

    // Asked of the core, never of the wallet on this thread.
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
    QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});
    QCOMPARE(combo->placeholderText(), QString("Loading..."));
    QVERIFY(!combo->isEnabled());
    QVERIFY(!ok->isEnabled());

    fixture.gui_state.Answer(fixture.gui_state.m_unbooked_requests.back(), {"b", "a"}, "");

    QTRY_COMPARE_WITH_TIMEOUT(combo->placeholderText(), QString(), 10000);
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->itemText(0), QString("a"));
    QCOMPARE(combo->itemText(1), QString("b"));
    QCOMPARE(combo->currentIndex(), 0);
    QVERIFY(combo->isEnabled());
    QVERIFY(ok->isEnabled());
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// An answer with no address ends the loading state too.
void EditAddressDialogTests::anEmptyAnswerClearsTheLoadingState()
{
    DialogFixture fixture;
    EditAddressDialog dialog(EditAddressDialog::AddExistingReceivingAddress);
    dialog.setModel(fixture.Table());

    QComboBox* const combo = ComboOf(dialog);
    QPushButton* const ok = OkOf(dialog);
    QVERIFY(combo);
    QVERIFY(ok);
    QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});

    fixture.gui_state.Answer(fixture.gui_state.m_unbooked_requests.back(), {}, "");

    QTRY_COMPARE_WITH_TIMEOUT(combo->placeholderText(), QString(), 10000);
    QCOMPARE(combo->count(), 0);
    QVERIFY(combo->isEnabled());
    QVERIFY(ok->isEnabled());
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// With no core state cache connected, the request fails, queued, and the
// wallet is never asked on this thread instead.
void EditAddressDialogTests::noCacheIsAFailedLoadWithNoGuiThreadCall()
{
    DialogFixture fixture(/*connect_cache=*/false);
    EditAddressDialog dialog(EditAddressDialog::AddExistingReceivingAddress);
    dialog.setModel(fixture.Table());

    QComboBox* const combo = ComboOf(dialog);
    QPushButton* const ok = OkOf(dialog);
    QVERIFY(combo);
    QVERIFY(ok);
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);

    QTRY_VERIFY_WITH_TIMEOUT(ok->isEnabled(), 10000);
    QCOMPARE(combo->placeholderText(), QString());
    QCOMPARE(combo->count(), 0);
    QVERIFY(combo->isEnabled());
    QVERIFY(fixture.gui_state.m_unbooked_requests.empty());
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// A dialog closed before its answer arrives is never reached by it.
void EditAddressDialogTests::aClosedDialogIsNeverTouched()
{
    DialogFixture fixture;
    QSignalSpy loaded(fixture.Table(), &AddressTableModel::unbookedReceiveAddressesLoaded);

    auto dialog = std::make_unique<EditAddressDialog>(EditAddressDialog::AddExistingReceivingAddress);
    dialog->setModel(fixture.Table());
    QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});
    const uint64_t request_id = fixture.gui_state.m_unbooked_requests.back();

    dialog.reset();
    fixture.gui_state.Answer(request_id, {"b", "a"}, "");

    // The model relays the answer, and no dialog is there to take it.
    QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// An answer that arrives after the wallet model is gone is dropped; the
// cache that delivers it stays.
void EditAddressDialogTests::anAnswerAfterTheModelIsGoneIsDropped()
{
    DialogFixture fixture;
    QSignalSpy answered(&fixture.cache, &GuiStateCache::unbookedReceiveAddressesAnswered);

    uint64_t request_id = 0;
    {
        EditAddressDialog dialog(EditAddressDialog::AddExistingReceivingAddress);
        dialog.setModel(fixture.Table());
        QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});
        request_id = fixture.gui_state.m_unbooked_requests.back();
    }

    fixture.model.reset();
    fixture.gui_state.Answer(request_id, {"b", "a"}, "");

    QTRY_COMPARE_WITH_TIMEOUT(answered.count(), 1, 10000);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// An answer that carries an error is shown as an empty picker, and logged.
void EditAddressDialogTests::anErrorAnswerIsShownAsAnEmptyPicker()
{
    DialogFixture fixture;
    EditAddressDialog dialog(EditAddressDialog::AddExistingReceivingAddress);
    dialog.setModel(fixture.Table());

    QComboBox* const combo = ComboOf(dialog);
    QPushButton* const ok = OkOf(dialog);
    QVERIFY(combo);
    QVERIFY(ok);
    QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});

    LogCapture log;
    fixture.gui_state.Answer(fixture.gui_state.m_unbooked_requests.back(), {}, "no wallet is loaded");

    QTRY_COMPARE_WITH_TIMEOUT(combo->placeholderText(), QString(), 10000);
    QCOMPARE(combo->count(), 0);
    QVERIFY(combo->isEnabled());
    QVERIFY(ok->isEnabled());
    QCOMPARE(log.Count("the unbooked address list could not be loaded: no wallet is loaded"), 1);
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// A request that throws, as one across IPC does when the core drops the
// connection, escapes nothing: the failure is queued, as with no cache.
void EditAddressDialogTests::aRequestThatThrowsIsAFailedLoad()
{
    DialogFixture fixture;
    fixture.gui_state.m_request_throws = true;

    EditAddressDialog dialog(EditAddressDialog::AddExistingReceivingAddress);
    dialog.setModel(fixture.Table());

    QComboBox* const combo = ComboOf(dialog);
    QPushButton* const ok = OkOf(dialog);
    QVERIFY(combo);
    QVERIFY(ok);
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);

    QTRY_VERIFY_WITH_TIMEOUT(ok->isEnabled(), 10000);
    QCOMPARE(combo->placeholderText(), QString());
    QCOMPARE(combo->count(), 0);
    QVERIFY(combo->isEnabled());
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// Two dialogs in turn on one model: the first is closed unanswered, and its
// late answer does not fill the second, which waits for its own.
void EditAddressDialogTests::aLateAnswerToAnEarlierDialogIsIgnored()
{
    DialogFixture fixture;
    QSignalSpy loaded(fixture.Table(), &AddressTableModel::unbookedReceiveAddressesLoaded);

    {
        EditAddressDialog first(EditAddressDialog::AddExistingReceivingAddress);
        first.setModel(fixture.Table());
        QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});
    }

    EditAddressDialog second(EditAddressDialog::AddExistingReceivingAddress);
    second.setModel(fixture.Table());

    QComboBox* const combo = ComboOf(second);
    QPushButton* const ok = OkOf(second);
    QVERIFY(combo);
    QVERIFY(ok);

    // Read before either id is taken, so a single request fails here.
    QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{2});
    const uint64_t first_id = fixture.gui_state.m_unbooked_requests.front();
    const uint64_t second_id = fixture.gui_state.m_unbooked_requests.back();
    QVERIFY(first_id != second_id);

    fixture.gui_state.Answer(first_id, {"c"}, "");
    QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);

    QCOMPARE(combo->placeholderText(), QString("Loading..."));
    QCOMPARE(combo->count(), 0);
    QVERIFY(!combo->isEnabled());
    QVERIFY(!ok->isEnabled());

    fixture.gui_state.Answer(second_id, {"b", "a"}, "");

    QTRY_COMPARE_WITH_TIMEOUT(combo->placeholderText(), QString(), 10000);
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->itemText(0), QString("a"));
    QCOMPARE(combo->itemText(1), QString("b"));
    QVERIFY(combo->isEnabled());
    QVERIFY(ok->isEnabled());
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// A request whose answer never comes (one the dispatcher dropped) holds back
// no later one: the next dialog's request is made at once and answered.
void EditAddressDialogTests::anUnansweredRequestDoesNotBlockTheNext()
{
    DialogFixture fixture;

    {
        EditAddressDialog first(EditAddressDialog::AddExistingReceivingAddress);
        first.setModel(fixture.Table());
        QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});
    }

    EditAddressDialog second(EditAddressDialog::AddExistingReceivingAddress);
    second.setModel(fixture.Table());

    QComboBox* const combo = ComboOf(second);
    QPushButton* const ok = OkOf(second);
    QVERIFY(combo);
    QVERIFY(ok);

    QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{2});
    const uint64_t first_id = fixture.gui_state.m_unbooked_requests.front();
    const uint64_t second_id = fixture.gui_state.m_unbooked_requests.back();
    QVERIFY(first_id != second_id);

    fixture.gui_state.Answer(second_id, {"b", "a"}, "");

    QTRY_COMPARE_WITH_TIMEOUT(combo->placeholderText(), QString(), 10000);
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->itemText(0), QString("a"));
    QCOMPARE(combo->itemText(1), QString("b"));
    QVERIFY(combo->isEnabled());
    QVERIFY(ok->isEnabled());
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
}

// A model destroyed while the core holds cs_main (here its stand-in), with
// a picker request outstanding, does not wait for the core. The worker is
// declared as the GUI's main() declares it, so a walk posted to it would run.
void EditAddressDialogTests::destructionDoesNotWaitBehindTheCoreForThePicker()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::CoinLoad);
    DialogFixture fixture;

    qt_test::CsMainStandInHolder core_busy;

    {
        EditAddressDialog dialog(EditAddressDialog::AddExistingReceivingAddress);
        dialog.setModel(fixture.Table());
    }

    QElapsedTimer timer;
    timer.start();
    fixture.model.reset();
    const qint64 elapsed_ms = timer.elapsed();

    core_busy.Release();

    QVERIFY2(elapsed_ms < 10000, qPrintable(QString::number(elapsed_ms)));
    QCOMPARE(fixture.wallet.m_unbooked_calls.load(), 0);
    QCOMPARE(fixture.gui_state.m_unbooked_requests.size(), size_t{1});
}
