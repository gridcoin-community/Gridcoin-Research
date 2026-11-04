// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/askpassphrasedialogtests.h"

#include "interfaces/wallet_coin_source.h"
#include "qt/askpassphrasedialog.h"
#include "qt/bitcoingui.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"
#include "util.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>

namespace {

//! A WalletModel over FakeWallet, and what the model borrows.
struct ModelOverFakeWallet
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel options{node, sidestakes};
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source{interfaces::MakeSyntheticCoinSource(1, 1)};
    WalletModel model;

    explicit ModelOverFakeWallet(qt_test::FakeWallet& wallet)
        : model(wallet, tx_source, *coin_source, &options)
    {
    }
};

//! Lets the fake's waits return however the case exits, before the dialog,
//! which waits for its check, is destroyed. Declare it after the dialog.
struct ReleaseOnExit
{
    qt_test::FakeWallet& wallet;

    ~ReleaseOnExit() { wallet.m_release = true; }
};

//! What exec() returns when the case's own timeout ended it, which no path of
//! the prompt does.
constexpr int TIMED_OUT{2};

//! What exec() returns when the case ended the prompt's loop from outside, as
//! an application quitting under the prompt does.
constexpr int ENDED_OUTSIDE{3};

//! The message box on screen, if one is.
QMessageBox* VisibleMessageBox()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* box = qobject_cast<QMessageBox*>(widget);
        if (box && box->isVisible()) return box;
    }
    return nullptr;
}

//! The thread the process's modal worker runs its jobs on.
std::thread::id ModalWorkerThread()
{
    GuiIpcWorker* const worker = GuiIpcWorker::Current(GuiIpcWorker::Role::Modal);
    if (!worker) return {};

    std::thread::id id;
    const std::future<void> done = worker->Post([&id] { id = std::this_thread::get_id(); });
    if (!done.valid() || done.wait_for(std::chrono::seconds{10}) != std::future_status::ready) return {};
    return id;
}

//! Type a passphrase into the prompt, and the new one twice for a change, and
//! press OK, which starts the check.
void SubmitPassphrase(AskPassphraseDialog& dialog)
{
    for (const char* name : {"oldPassphraseEdit", "newPassphraseEdit", "repeatNewPassphraseEdit"}) {
        QLineEdit* const edit = dialog.findChild<QLineEdit*>(name);
        QVERIFY(edit);
        edit->setText(QStringLiteral("dialog-test-passphrase"));
    }

    dialog.accept();
}

//! What the prompt looked like when the case cancelled it.
struct AtCancel
{
    bool ok_enabled{true};
    bool cancel_enabled{false};
    bool visible{false};
    QString status;
};

//! Run \p dialog under exec(), as its callers do. From inside the event loop:
//! submit the passphrase, wait for the fake's check to wait, then cancel the
//! prompt (reject()), recording what it looked like then, and call
//! \p after_cancel. A timeout ends the loop after ten seconds whatever happens.
int ExecAndCancel(AskPassphraseDialog& dialog, qt_test::FakeWallet& wallet, AtCancel& at_cancel,
                  QElapsedTimer& since_cancel, const std::function<void()>& after_cancel)
{
    QTimer::singleShot(0, &dialog, [&dialog] { SubmitPassphrase(dialog); });

    QTimer poll;
    poll.setInterval(5);
    QObject::connect(&poll, &QTimer::timeout, &dialog, [&] {
        if (!wallet.m_waiting) return;
        poll.stop();

        QDialogButtonBox* const buttons = dialog.findChild<QDialogButtonBox*>("buttonBox");
        QLabel* const status = dialog.findChild<QLabel*>("statusLabel");

        if (buttons) {
            at_cancel.ok_enabled = buttons->button(QDialogButtonBox::Ok)->isEnabled();
            at_cancel.cancel_enabled = buttons->button(QDialogButtonBox::Cancel)->isEnabled();
        }

        since_cancel.start();
        dialog.reject();

        at_cancel.visible = dialog.isVisible();
        if (status) at_cancel.status = status->text();

        if (after_cancel) after_cancel();
    });
    poll.start();

    QTimer::singleShot(10000, &dialog, [&dialog] { dialog.done(TIMED_OUT); });

    return dialog.exec();
}

//! Holds the process's modal worker with a job of its own until released, or
//! until \p backstop has passed, so that a check posted meanwhile waits behind
//! it.
class WorkerBlocker
{
public:
    explicit WorkerBlocker(std::chrono::milliseconds backstop)
    {
        GuiIpcWorker* const worker = GuiIpcWorker::Current(GuiIpcWorker::Role::Modal);
        if (!worker) return;

        const std::shared_future<void> unblocked = m_unblock.get_future().share();
        m_done = worker->Post([unblocked, backstop] { unblocked.wait_for(backstop); });
    }

    ~WorkerBlocker() { Release(); }

    bool Holding() const { return m_done.valid(); }

    void Release()
    {
        if (!m_released) {
            m_released = true;
            m_unblock.set_value();
        }
    }

    //! The blocker, and every job posted before this call, has run.
    bool Drained()
    {
        GuiIpcWorker* const worker = GuiIpcWorker::Current(GuiIpcWorker::Role::Modal);
        if (!worker || !m_done.valid()) return false;
        if (m_done.wait_for(std::chrono::seconds{10}) != std::future_status::ready) return false;

        const std::future<void> after = worker->Post([] {});
        return after.valid() && after.wait_for(std::chrono::seconds{10}) == std::future_status::ready;
    }

private:
    std::promise<void> m_unblock;
    std::future<void> m_done;
    bool m_released{false};
};

} // namespace

void AskPassphraseDialogTests::initTestCase()
{
    m_worker = std::make_unique<GuiIpcWorker>(GuiIpcWorker::Role::Modal);

    // OptionsModel reads QSettings. Redirected as OptionsModelTests does, so a
    // run cannot reach or leave anything in a real store.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("askpassphrasedialogtests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());
}

void AskPassphraseDialogTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();

    m_worker.reset();
}

void AskPassphraseDialogTests::init()
{
    // A temporary datadir, so that nothing reached through the options model
    // creates the default one on the host.
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void AskPassphraseDialogTests::cleanup()
{
    QSettings().clear();
    gArgs.ForceSetArg("-datadir", "");
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

void AskPassphraseDialogTests::cancelWhileWaitingEndsTheLoopAtOnce()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    AskPassphraseDialog dialog(AskPassphraseDialog::Elevate);
    dialog.setModel(&fixture.model);
    dialog.setCapabilityKind(CapabilityKind::Send);
    const ReleaseOnExit release{wallet};

    AtCancel at_cancel;
    QElapsedTimer since_cancel;
    const int result = ExecAndCancel(dialog, wallet, at_cancel, since_cancel, {});
    const qint64 took = since_cancel.elapsed();

    // While the check ran, only OK was disabled, so Cancel could be pressed.
    QVERIFY(!at_cancel.ok_enabled);
    QVERIFY(at_cancel.cancel_enabled);

    // Cancelling left the prompt open, saying so, rather than hiding it, which
    // would have ended exec() with the check still running.
    QVERIFY(at_cancel.visible);
    QCOMPARE(at_cancel.status, AskPassphraseDialog::tr("Cancelling..."));

    // The check saw the cancel and gave up, and the prompt ended through its
    // report: rejected, within a second, with nothing begun to end.
    QCOMPARE(result, int(QDialog::Rejected));
    QVERIFY2(took < 1000, qPrintable(QStringLiteral("exec() returned %1 ms after the cancel").arg(took)));
    QVERIFY(wallet.m_saw_cancel.load());
    QVERIFY(wallet.m_returned.load());
    QCOMPARE(wallet.m_end_elevation_calls.load(), 0);
}

void AskPassphraseDialogTests::cancelPastTheTurnKeepsTheLoopRunning()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    // A check past its turn, or one waiting in the node's process in the split
    // build, finishes whatever the cancel says.
    wallet.m_ignore_cancel = true;

    AskPassphraseDialog dialog(AskPassphraseDialog::Elevate);
    dialog.setModel(&fixture.model);
    dialog.setCapabilityKind(CapabilityKind::Send);
    const ReleaseOnExit release{wallet};

    // Counts while the check runs on: the GUI thread is not blocked.
    int ticks{0};
    QTimer ticker;
    ticker.setInterval(10);
    QObject::connect(&ticker, &QTimer::timeout, &dialog, [&ticks] { ++ticks; });

    int ticks_before_release{0};
    bool returned_before_release{true};

    AtCancel at_cancel;
    QElapsedTimer since_cancel;
    const int result = ExecAndCancel(dialog, wallet, at_cancel, since_cancel, [&] {
        ticker.start();

        // Then the passphrase is accepted.
        QTimer::singleShot(300, &dialog, [&] {
            ticks_before_release = ticks;
            returned_before_release = wallet.m_returned;
            wallet.m_release_result = true;
            wallet.m_release = true;
        });
    });

    QVERIFY(at_cancel.visible);

    // The loop ran on for the 300 ms the check took, and ended only once the
    // check had reported: rejected, with the capability it began ended, since
    // no operation was waiting for it.
    QVERIFY2(ticks_before_release >= 5, qPrintable(QStringLiteral("%1 ticks").arg(ticks_before_release)));
    QVERIFY(!returned_before_release);
    QVERIFY(wallet.m_returned.load());
    QCOMPARE(result, int(QDialog::Rejected));
    QCOMPARE(wallet.m_end_elevation_calls.load(), 1);
}

void AskPassphraseDialogTests::passphraseChangedAfterCancelIsReported()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);
    wallet.m_ignore_cancel = true;

    AskPassphraseDialog dialog(AskPassphraseDialog::ChangePass);
    dialog.setModel(&fixture.model);
    const ReleaseOnExit release{wallet};

    // Closes, and records, a message box the prompt shows.
    QStringList shown;
    QTimer closer;
    closer.setInterval(20);
    QObject::connect(&closer, &QTimer::timeout, &dialog, [&shown] {
        for (QWidget* const widget : QApplication::topLevelWidgets()) {
            QMessageBox* const box = qobject_cast<QMessageBox*>(widget);
            if (box && box->isVisible()) {
                shown << box->text();
                box->accept();
            }
        }
    });
    closer.start();

    AtCancel at_cancel;
    QElapsedTimer since_cancel;
    const int result = ExecAndCancel(dialog, wallet, at_cancel, since_cancel, [&] {
        // The change goes through after the cancel.
        QTimer::singleShot(100, &dialog, [&wallet] {
            wallet.m_release_result = true;
            wallet.m_release = true;
        });
    });

    // The prompt still ends rejected, but the user is told the passphrase did
    // change.
    QCOMPARE(result, int(QDialog::Rejected));
    QCOMPARE(shown, QStringList{AskPassphraseDialog::tr("Wallet passphrase was successfully changed.")});
}

// The check runs on the process's modal worker, not on a thread of its own.
void AskPassphraseDialogTests::checkRunsOnTheModalWorker()
{
    // Asked first, so that the worker's thread is running before the check
    // starts: an id is unique only among threads alive at the same time.
    const std::thread::id worker_thread = ModalWorkerThread();
    QVERIFY(worker_thread != std::thread::id{});
    QVERIFY(worker_thread != std::this_thread::get_id());

    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    AskPassphraseDialog dialog(AskPassphraseDialog::Elevate);
    dialog.setModel(&fixture.model);
    dialog.setCapabilityKind(CapabilityKind::Send);
    const ReleaseOnExit release{wallet};

    QTimer::singleShot(0, &dialog, [&dialog] { SubmitPassphrase(dialog); });

    // The passphrase is accepted once the check waits.
    QTimer poll;
    poll.setInterval(5);
    QObject::connect(&poll, &QTimer::timeout, &dialog, [&] {
        if (!wallet.m_waiting) return;
        poll.stop();
        wallet.m_release_result = true;
        wallet.m_release = true;
    });
    poll.start();

    QTimer::singleShot(10000, &dialog, [&dialog] { dialog.done(TIMED_OUT); });

    const int result = dialog.exec();

    QCOMPARE(result, int(QDialog::Accepted));
    QVERIFY(wallet.m_call_thread.load() == worker_thread);
}

// A check still waiting for its turn on the modal worker gives up at once
// when the prompt is cancelled. The prompt ends rejected without the
// passphrase being tried, and when the check's turn comes, after the prompt
// is gone, it touches nothing.
void AskPassphraseDialogTests::cancelWhileQueuedEndsAtOnceAndTriesNothing()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    // Holds the modal worker until released, so the check waits behind it.
    std::promise<void> unblock;
    const std::shared_future<void> unblocked = unblock.get_future().share();
    GuiIpcWorker* const worker = GuiIpcWorker::Current(GuiIpcWorker::Role::Modal);
    QVERIFY(worker);
    const std::future<void> blocker = worker->Post([unblocked] {
        unblocked.wait_for(std::chrono::seconds{10});
    });
    QVERIFY(blocker.valid());

    int result{0};
    qint64 took{-1};
    bool visible{false};
    QString status;

    {
        AskPassphraseDialog dialog(AskPassphraseDialog::Elevate);
        dialog.setModel(&fixture.model);
        dialog.setCapabilityKind(CapabilityKind::Send);

        QElapsedTimer since_cancel;

        QTimer::singleShot(0, &dialog, [&] {
            SubmitPassphrase(dialog);

            since_cancel.start();
            dialog.reject();

            visible = dialog.isVisible();
            if (QLabel* const label = dialog.findChild<QLabel*>("statusLabel")) status = label->text();
        });

        QTimer::singleShot(10000, &dialog, [&dialog] { dialog.done(TIMED_OUT); });

        result = dialog.exec();
        took = since_cancel.elapsed();
    }

    // The prompt is gone. Now the check's turn comes, and then a job after it.
    unblock.set_value();
    QVERIFY(blocker.wait_for(std::chrono::seconds{10}) == std::future_status::ready);
    const std::future<void> after = worker->Post([] {});
    QVERIFY(after.valid());
    QVERIFY(after.wait_for(std::chrono::seconds{10}) == std::future_status::ready);
    QCoreApplication::processEvents();

    // Cancelling left the prompt open, saying so, until the check reported.
    QVERIFY(visible);
    QCOMPARE(status, AskPassphraseDialog::tr("Cancelling..."));

    QCOMPARE(result, int(QDialog::Rejected));
    QVERIFY2(took < 1000, qPrintable(QStringLiteral("exec() returned %1 ms after the cancel").arg(took)));
    QVERIFY(!wallet.m_waiting.load());
    QCOMPARE(wallet.m_end_elevation_calls.load(), 0);
}

// A prompt destroyed while its check runs, as when the application quits with
// the prompt open, waits for the check, and ends the capability the check
// began, which no operation will claim.
void AskPassphraseDialogTests::destroyedUnderARunningCheckEndsItsCapability()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);
    wallet.m_ignore_cancel = true;

    int result{0};
    std::thread releaser;

    {
        AskPassphraseDialog dialog(AskPassphraseDialog::Elevate);
        dialog.setModel(&fixture.model);
        dialog.setCapabilityKind(CapabilityKind::Send);

        QTimer::singleShot(0, &dialog, [&dialog] { SubmitPassphrase(dialog); });

        // Once the check waits, the prompt's loop is ended from outside.
        QTimer poll;
        poll.setInterval(5);
        QObject::connect(&poll, &QTimer::timeout, &dialog, [&] {
            if (!wallet.m_waiting) return;
            poll.stop();
            dialog.done(ENDED_OUTSIDE);
        });
        poll.start();

        QTimer::singleShot(10000, &dialog, [&dialog] { dialog.done(TIMED_OUT); });

        result = dialog.exec();

        // The passphrase is accepted only once the dialog is being destroyed.
        releaser = std::thread([&wallet] {
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
            wallet.m_release_result = true;
            wallet.m_release = true;
        });
    }

    const bool returned_when_destroyed = wallet.m_returned;
    const int ended_when_destroyed = wallet.m_end_elevation_calls;
    releaser.join();

    // What the check posted to the destroyed prompt is dropped.
    QCoreApplication::processEvents();

    QCOMPARE(result, ENDED_OUTSIDE);
    QVERIFY(returned_when_destroyed);
    QCOMPARE(ended_when_destroyed, 1);
    QCOMPARE(wallet.m_end_elevation_calls.load(), 1);
}

// A prompt deleted after Cancel gave up its queued check, before the report of
// that was delivered, does not wait for the check's turn: given up, the check
// never runs. Nothing is tried when its turn comes.
void AskPassphraseDialogTests::deletedAfterCancelWhileQueuedReturnsAtOnce()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    // Two seconds at most, so that a deletion that waits for the check's turn
    // shows as a slow one rather than a hang.
    WorkerBlocker blocker(std::chrono::milliseconds{2000});
    QVERIFY(blocker.Holding());

    auto* dialog = new AskPassphraseDialog(AskPassphraseDialog::Elevate);
    dialog->setModel(&fixture.model);
    dialog->setCapabilityKind(CapabilityKind::Send);

    SubmitPassphrase(*dialog);
    dialog->reject();

    QElapsedTimer timer;
    timer.start();
    delete dialog;
    const qint64 took = timer.elapsed();

    blocker.Release();
    QVERIFY(blocker.Drained());

    // The report posted to the deleted prompt is dropped.
    QCoreApplication::processEvents();

    QVERIFY2(took < 1000, qPrintable(QStringLiteral("the deletion took %1 ms").arg(took)));
    QVERIFY(!wallet.m_waiting.load());
    QCOMPARE(wallet.m_end_elevation_calls.load(), 0);
}

// A prompt deleted while its check still waits for its turn on the worker,
// with no Cancel first, gives the check up and does not wait for its turn.
void AskPassphraseDialogTests::deletedWhileQueuedGivesTheCheckUp()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    WorkerBlocker blocker(std::chrono::milliseconds{2000});
    QVERIFY(blocker.Holding());

    auto* dialog = new AskPassphraseDialog(AskPassphraseDialog::Elevate);
    dialog->setModel(&fixture.model);
    dialog->setCapabilityKind(CapabilityKind::Send);

    SubmitPassphrase(*dialog);

    QElapsedTimer timer;
    timer.start();
    delete dialog;
    const qint64 took = timer.elapsed();

    blocker.Release();
    QVERIFY(blocker.Drained());
    QCoreApplication::processEvents();

    QVERIFY2(took < 1000, qPrintable(QStringLiteral("the deletion took %1 ms").arg(took)));
    QVERIFY(!wallet.m_waiting.load());
    QCOMPARE(wallet.m_end_elevation_calls.load(), 0);
}

// After an encryption in the split build the core stops, when the GUI's
// connection closes or by itself a while after the encryption, and the GUI then
// sees the connection drop, which asks for a quit (BitcoinGUI::requestQuit(),
// posted from the disconnect hook). The message after the encryption, which
// holds the warning about earlier backups, stays until the user closes it, and
// the quit happens then. Run under the application's own event loop, as in the
// GUI: outside it a quit does nothing.
void AskPassphraseDialogTests::encryptionMessageOutlivesTheCore()
{
    qt_test::FakeWallet wallet;
    wallet.m_encrypt_result = true;
    ModelOverFakeWallet fixture(wallet);

    AskPassphraseDialog dialog(AskPassphraseDialog::Encrypt);
    dialog.setModel(&fixture.model);
    dialog.setMultiprocess(true);

    bool confirmed = false;
    bool dropped = false;
    int ticks_after_drop = 0;
    bool open_after_drop = false;
    bool closed = false;

    QTimer observer;
    observer.setInterval(5);
    QObject::connect(&observer, &QTimer::timeout, this, [&] {
        QMessageBox* const box = VisibleMessageBox();
        if (!box || closed) return;

        if (!confirmed) {
            if (box->windowTitle() != QStringLiteral("Confirm wallet encryption")) return;
            confirmed = true;
            box->button(QMessageBox::Yes)->click();
            return;
        }

        if (box->windowTitle() != QStringLiteral("Wallet encrypted")) return;

        if (!dropped) {
            dropped = true;
            // What the disconnect hook posts when the connection to the core is lost.
            QMetaObject::invokeMethod(qApp, [] { BitcoinGUI::requestQuit(); }, Qt::QueuedConnection);
            return;
        }

        // The quit posted above has been delivered well before this.
        if (++ticks_after_drop < 20) return;

        open_after_drop = true;
        closed = true;
        box->button(QMessageBox::Ok)->click();
    });
    observer.start();

    // Ends the application's loop if nothing else does, so a regression fails rather than hangs.
    QTimer backstop;
    backstop.setSingleShot(true);
    QObject::connect(&backstop, &QTimer::timeout, qApp, [] { QCoreApplication::exit(TIMED_OUT); });
    backstop.start(15000);

    QTimer::singleShot(0, &dialog, [&dialog] {
        QTimer::singleShot(0, &dialog, [&dialog] { SubmitPassphrase(dialog); });
        dialog.exec();
    });

    const int app_result = QCoreApplication::exec();
    observer.stop();
    backstop.stop();

    QVERIFY(confirmed);
    QVERIFY(dropped);
    QVERIFY2(open_after_drop, "the message closed when the connection to the core was lost");
    QVERIFY(closed);
    QCOMPARE(app_result, 0);
    QVERIFY(!VisibleMessageBox());
}
