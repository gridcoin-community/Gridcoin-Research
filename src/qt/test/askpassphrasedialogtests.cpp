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
#include <QTextDocument>
#include <QTimer>
#include <QWidget>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

//! What became of the message after an encryption in the split build when a
//! quit was asked for while it was open.
struct QuitWhileEncryptionMessage
{
    bool confirmed{false};
    bool asked{false};
    //! The message's text.
    QString text{};
    //! Still open well after the quit was asked for.
    bool open_after_quit{false};
    //! The case closed it, as the user would.
    bool closed_by_case{false};
    //! What the application's event loop returned.
    int app_result{-1};
};

//! Encrypt in the split build over a fake wallet, under the application's own
//! event loop as in the GUI (outside it a quit does nothing), confirming the
//! encryption. While the message after it is open, post \p quit, as its caller
//! does. When the message is still open well after the quit was delivered, close
//! it as the user would.
QuitWhileEncryptionMessage EncryptAndQuitWhileTheMessageIsOpen(QObject& context, std::function<void()> quit)
{
    QuitWhileEncryptionMessage run;

    qt_test::FakeWallet wallet;
    wallet.m_encrypt_result = true;
    ModelOverFakeWallet fixture(wallet);

    AskPassphraseDialog dialog(AskPassphraseDialog::Encrypt);
    dialog.setModel(&fixture.model);
    dialog.setMultiprocess(true);

    int ticks_after_quit = 0;

    QTimer observer;
    observer.setInterval(5);
    QObject::connect(&observer, &QTimer::timeout, &context, [&] {
        QMessageBox* const box = VisibleMessageBox();
        if (!box || run.closed_by_case) return;

        if (!run.confirmed) {
            if (box->windowTitle() != QStringLiteral("Confirm wallet encryption")) return;
            run.confirmed = true;
            box->button(QMessageBox::Yes)->click();
            return;
        }

        if (box->windowTitle() != QStringLiteral("Wallet encrypted")) return;

        if (!run.asked) {
            run.asked = true;
            run.text = box->text();
            QMetaObject::invokeMethod(qApp, quit, Qt::QueuedConnection);
            return;
        }

        // The quit posted above has been delivered well before this.
        if (++ticks_after_quit < 20) return;

        run.open_after_quit = true;
        run.closed_by_case = true;
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

    run.app_result = QCoreApplication::exec();
    observer.stop();
    backstop.stop();

    return run;
}

//! The message after an encryption, as the user saw it.
struct EncryptionMessage
{
    bool confirmed{false};
    //! The message's text.
    QString text{};
    //! What the user reads: the text as the message box renders it.
    QString shown{};
    //! The message's details, plain text behind its Show Details button.
    QString details{};
    //! What the application's event loop returned.
    int app_result{-1};
};

//! Encrypt over a fake wallet that reports \p backups as the automatic backups from
//! before the encryption, in the split build when \p multiprocess and in the monolith
//! otherwise. Runs under the application's own event loop, as in the GUI, confirming the
//! encryption, and closes the message after it as the user would; the GUI quits then.
EncryptionMessage EncryptAndReadTheMessage(QObject& context, bool multiprocess, std::vector<std::string> backups)
{
    EncryptionMessage run;

    qt_test::FakeWallet wallet;
    wallet.m_encrypt_result = true;
    wallet.m_pre_encryption_backups = std::move(backups);
    ModelOverFakeWallet fixture(wallet);

    AskPassphraseDialog dialog(AskPassphraseDialog::Encrypt);
    dialog.setModel(&fixture.model);
    dialog.setMultiprocess(multiprocess);

    bool closed = false;

    QTimer observer;
    observer.setInterval(5);
    QObject::connect(&observer, &QTimer::timeout, &context, [&] {
        QMessageBox* const box = VisibleMessageBox();
        if (!box || closed) return;

        if (!run.confirmed) {
            if (box->windowTitle() != QStringLiteral("Confirm wallet encryption")) return;
            run.confirmed = true;
            box->button(QMessageBox::Yes)->click();
            return;
        }

        if (box->windowTitle() != QStringLiteral("Wallet encrypted")) return;

        run.text = box->text();
        run.details = box->detailedText();
        if (QLabel* const label = box->findChild<QLabel*>(QStringLiteral("qt_msgbox_label"))) {
            QTextDocument rendered;
            rendered.setHtml(label->text());
            run.shown = label->textFormat() == Qt::PlainText ? label->text() : rendered.toPlainText();
        }

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

    run.app_result = QCoreApplication::exec();
    observer.stop();
    backstop.stop();

    return run;
}

//! A name with markup in it, which the message must show as it is.
const std::string MARKUP_BACKUP{"/srv/gridcoin/walletbackups/wallet.dat-2026-02-01T00-00-00Z-<b>beacon</b>&amp;"};

//! The backups the cases below report.
std::vector<std::string> ReportedBackups()
{
    return {"/srv/gridcoin/walletbackups/wallet.dat-2026-01-01T00-00-00Z", MARKUP_BACKUP};
}

//! Checks that \p run shows each of ReportedBackups() as it is, after the warning about
//! earlier backups.
void CheckTheBackupsAreListed(const EncryptionMessage& run)
{
    QVERIFY(run.confirmed);
    QCOMPARE(run.app_result, 0);
    QVERIFY(!VisibleMessageBox());

    QVERIFY2(run.text.contains(QStringLiteral("IMPORTANT: Any earlier backups of your wallet file")),
             qPrintable(run.text));
    QVERIFY2(run.shown.contains(QStringLiteral("These automatic backups were made before the encryption and still "
                                               "hold your unencrypted keys:")),
             qPrintable(run.shown));

    for (const std::string& backup : ReportedBackups()) {
        QVERIFY2(run.shown.contains(QString::fromStdString(backup)), qPrintable(run.shown));
    }

    // The markup in the name is escaped, so none of it is read as markup.
    QVERIFY2(!run.text.contains(QStringLiteral("<b>beacon</b>")), qPrintable(run.text));
    QVERIFY2(run.text.indexOf(QStringLiteral("IMPORTANT: Any earlier backups")) <
                 run.text.indexOf(QStringLiteral("These automatic backups")),
             qPrintable(run.text));

    // The text names them all, so there are no details.
    QVERIFY2(run.details.isEmpty(), qPrintable(run.details));
}

//! What an autounlock prompt showed, and how it ended.
struct AutoUnlockRun
{
    //! The text above the passphrase.
    QString warning{};
    //! The messages it showed, in order.
    QStringList shown{};
    //! Still open after its message was closed, so ended from outside.
    bool open_after_message{false};
    //! What exec() returned.
    int result{-1};
};

//! Run an autounlock prompt of \p mode under exec(), over \p wallet, whose call
//! answers at once, given \p status as BitcoinGUI gives it. The passphrase is
//! submitted, every message is recorded and closed as the user would close it, and
//! a prompt still open after its message is ended from outside.
AutoUnlockRun RunAutoUnlockPrompt(AskPassphraseDialog::Mode mode, qt_test::FakeWallet& wallet,
                                  const AutoUnlockStoreStatus& status,
                                  const QString& passphrase = QStringLiteral("dialog-test-passphrase"))
{
    AutoUnlockRun run;
    wallet.m_release_result = true;
    wallet.m_release = true;
    ModelOverFakeWallet fixture(wallet);

    AskPassphraseDialog dialog(mode);
    dialog.setModel(&fixture.model);
    dialog.setAutoUnlockStatus(status);

    if (QLabel* const warning = dialog.findChild<QLabel*>("warningLabel")) run.warning = warning->text();

    QTimer closer;
    closer.setInterval(20);
    QObject::connect(&closer, &QTimer::timeout, &dialog, [&run, &dialog] {
        QMessageBox* const box = VisibleMessageBox();
        if (!box) return;

        run.shown << box->text();
        box->accept();

        QTimer::singleShot(300, &dialog, [&run, &dialog] {
            if (!dialog.isVisible()) return;
            run.open_after_message = true;
            dialog.done(ENDED_OUTSIDE);
        });
    });
    closer.start();

    QTimer::singleShot(0, &dialog, [&dialog, &passphrase] {
        QLineEdit* const edit = dialog.findChild<QLineEdit*>("oldPassphraseEdit");
        QVERIFY(edit);
        edit->setText(passphrase);
        dialog.accept();
    });
    QTimer::singleShot(10000, &dialog, [&dialog] { dialog.done(TIMED_OUT); });

    run.result = dialog.exec();
    closer.stop();

    return run;
}

//! A status as the Windows store reports it, in \p scope.
AutoUnlockStoreStatus WindowsStatus(const std::string& scope)
{
    AutoUnlockStoreStatus status;
    status.supported = true;
    status.scope = scope;
    status.location = "C:\\Users\\someone\\AppData\\Roaming\\GridcoinResearch\\autounlock\\passphrase.dpapi";
    return status;
}

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

// The Linux service setup keeps the autounlock passphrase where the core cannot
// write it: after a change the user is told it is still the old one.
void AskPassphraseDialogTests::passphraseChangeSaysAHelperServedAutoUnlockIsStale()
{
    qt_test::FakeWallet wallet;
    wallet.m_autounlock_status.configured = true;
    wallet.m_autounlock_status.scope = "helper";
    wallet.m_autounlock_status.location = "the credential";
    ModelOverFakeWallet fixture(wallet);
    wallet.m_ignore_cancel = true;

    AskPassphraseDialog dialog(AskPassphraseDialog::ChangePass);
    dialog.setModel(&fixture.model);
    const ReleaseOnExit release{wallet};

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
    ExecAndCancel(dialog, wallet, at_cancel, since_cancel, [&] {
        QTimer::singleShot(100, &dialog, [&wallet] {
            wallet.m_release_result = true;
            wallet.m_release = true;
        });
    });

    QCOMPARE(shown.size(), 1);
    QVERIFY(shown.front().startsWith(AskPassphraseDialog::tr("Wallet passphrase was successfully changed.")));
    QVERIFY(shown.front().contains("still has the old passphrase, in the credential"));
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

// The setup prompt says, above the passphrase, who can obtain it in the scope the
// core stores it in. The text is the core's, translated where a translation is
// loaded, so the RPC and the GUI say the same.
void AskPassphraseDialogTests::autoUnlockSetupShowsTheScopeText()
{
    for (const std::string scope : {"machine", "service"}) {
        const QString text = AskPassphraseDialog::AutoUnlockScopeText(scope);
        QCOMPARE(text, QString::fromStdString(AutoUnlockScopeWarning(scope)));

        qt_test::FakeWallet wallet;
        ModelOverFakeWallet fixture(wallet);
        AskPassphraseDialog dialog(AskPassphraseDialog::SetupAutoUnlock);
        dialog.setModel(&fixture.model);
        dialog.setAutoUnlockStatus(WindowsStatus(scope));

        QLabel* const warning = dialog.findChild<QLabel*>("warningLabel");
        QVERIFY(warning);
        QVERIFY2(warning->text().contains(text), qPrintable(warning->text()));

        // One passphrase, which the core checks: there is no new one to type.
        QLineEdit* const new_passphrase = dialog.findChild<QLineEdit*>("newPassphraseEdit");
        QVERIFY(new_passphrase);
        QVERIFY(new_passphrase->isHidden());
        QVERIFY(!dialog.findChild<QLineEdit*>("oldPassphraseEdit")->isHidden());
    }

    QVERIFY(AskPassphraseDialog::AutoUnlockScopeText("machine") != AskPassphraseDialog::AutoUnlockScopeText("service"));
    QVERIFY(AskPassphraseDialog::AutoUnlockScopeText("elsewhere").isEmpty());

    AskPassphraseDialog clear(AskPassphraseDialog::ClearAutoUnlock);
    QLabel* const warning = clear.findChild<QLabel*>("warningLabel");
    QVERIFY(warning);
    QCOMPARE(warning->text(),
             AskPassphraseDialog::tr("Turning autounlock off deletes the stored passphrase and locks the wallet now."));
}

void AskPassphraseDialogTests::autoUnlockSetupSaysWhereItIsStored()
{
    qt_test::FakeWallet wallet;
    const AutoUnlockStoreStatus status = WindowsStatus("service");

    const AutoUnlockRun run = RunAutoUnlockPrompt(AskPassphraseDialog::SetupAutoUnlock, wallet, status);

    QCOMPARE(run.result, int(QDialog::Accepted));
    QCOMPARE(run.shown.size(), 1);
    QVERIFY2(run.shown[0].contains(QStringLiteral("is set up")), qPrintable(run.shown[0]));
    QVERIFY2(run.shown[0].contains(QString::fromStdString(status.location)), qPrintable(run.shown[0]));
    QVERIFY2(!run.shown[0].contains(QStringLiteral("read back")), qPrintable(run.shown[0]));
    QCOMPARE(wallet.m_set_autounlock_calls.load(), 1);
    QCOMPARE(wallet.m_clear_autounlock_calls.load(), 0);
}

// A file written but not read back is stored, and the user is told it is checked at
// the next start.
void AskPassphraseDialogTests::autoUnlockSetupUnverifiedSaysItIsCheckedAtNextStart()
{
    qt_test::FakeWallet wallet;
    wallet.m_autounlock_result = AutoUnlockSetupResult::OkUnverified;
    const AutoUnlockStoreStatus status = WindowsStatus("machine");

    const AutoUnlockRun run = RunAutoUnlockPrompt(AskPassphraseDialog::SetupAutoUnlock, wallet, status);

    QCOMPARE(run.result, int(QDialog::Accepted));
    QCOMPARE(run.shown.size(), 1);
    QVERIFY2(run.shown[0].contains(QStringLiteral("could not be read back")), qPrintable(run.shown[0]));
    QVERIFY2(run.shown[0].contains(QStringLiteral("checked the next time the Gridcoin core starts")),
             qPrintable(run.shown[0]));
    QVERIFY2(run.shown[0].contains(QString::fromStdString(status.location)), qPrintable(run.shown[0]));
    QCOMPARE(wallet.m_set_autounlock_calls.load(), 1);
}

void AskPassphraseDialogTests::autoUnlockClearSaysTheWalletIsLocked()
{
    qt_test::FakeWallet wallet;

    const AutoUnlockRun run = RunAutoUnlockPrompt(AskPassphraseDialog::ClearAutoUnlock, wallet, WindowsStatus("machine"));

    QCOMPARE(run.result, int(QDialog::Accepted));
    QCOMPARE(run.shown.size(), 1);
    QVERIFY2(run.shown[0].contains(QStringLiteral("the wallet is locked")), qPrintable(run.shown[0]));
    QCOMPARE(wallet.m_clear_autounlock_calls.load(), 1);
    QCOMPARE(wallet.m_set_autounlock_calls.load(), 0);
}

// A wrong passphrase is said, and the prompt stays up for it to be typed again.
void AskPassphraseDialogTests::autoUnlockWrongPassphraseKeepsThePromptOpen()
{
    for (const AskPassphraseDialog::Mode mode : {AskPassphraseDialog::SetupAutoUnlock,
                                                 AskPassphraseDialog::ClearAutoUnlock}) {
        qt_test::FakeWallet wallet;
        wallet.m_autounlock_result = AutoUnlockSetupResult::WrongPassphrase;

        const AutoUnlockRun run = RunAutoUnlockPrompt(mode, wallet, WindowsStatus("machine"));

        QCOMPARE(run.shown.size(), 1);
        QVERIFY2(run.shown[0].contains(QStringLiteral("incorrect")), qPrintable(run.shown[0]));
        QVERIFY(run.open_after_message);
        QCOMPARE(run.result, ENDED_OUTSIDE);
    }
}

void AskPassphraseDialogTests::autoUnlockStoreFailureEndsThePrompt()
{
    for (const AskPassphraseDialog::Mode mode : {AskPassphraseDialog::SetupAutoUnlock,
                                                 AskPassphraseDialog::ClearAutoUnlock}) {
        qt_test::FakeWallet wallet;
        wallet.m_autounlock_result = AutoUnlockSetupResult::StoreFailed;

        const AutoUnlockRun run = RunAutoUnlockPrompt(mode, wallet, WindowsStatus("machine"));

        QCOMPARE(run.result, int(QDialog::Rejected));
        QCOMPARE(run.shown.size(), 1);
        QVERIFY2(run.shown[0].contains(QStringLiteral("debug.log")), qPrintable(run.shown[0]));
        QVERIFY(!run.open_after_message);
    }
}

// A wrong passphrase with a null character in it gets the hint the unlock prompt gives.
void AskPassphraseDialogTests::autoUnlockWrongPassphraseWithANullCharacterSaysSo()
{
    qt_test::FakeWallet wallet;
    wallet.m_autounlock_result = AutoUnlockSetupResult::WrongPassphrase;
    const QString with_null = QStringLiteral("before") + QChar(0) + QStringLiteral("after");

    const AutoUnlockRun run = RunAutoUnlockPrompt(AskPassphraseDialog::SetupAutoUnlock, wallet,
                                                  WindowsStatus("machine"), with_null);

    QCOMPARE(run.shown.size(), 1);
    QVERIFY2(run.shown[0].contains(QStringLiteral("null character")), qPrintable(run.shown[0]));
    QVERIFY(run.open_after_message);
}

// A call that ended without an answer leaves it unknown whether autounlock changed, and
// the user is told so.
void AskPassphraseDialogTests::autoUnlockCallThatThrowsSaysItIsNotKnown()
{
    qt_test::FakeWallet wallet;
    wallet.m_autounlock_throws = true;

    const AutoUnlockRun run = RunAutoUnlockPrompt(AskPassphraseDialog::ClearAutoUnlock, wallet,
                                                  WindowsStatus("machine"));

    QCOMPARE(run.result, int(QDialog::Rejected));
    QCOMPARE(run.shown.size(), 1);
    QVERIFY2(run.shown[0].contains(QStringLiteral("not known whether autounlock was changed")),
             qPrintable(run.shown[0]));
}

// The same when the prompt was cancelled while the call ran on: the call was made, so its
// outcome is not known, and that is said before the prompt ends.
void AskPassphraseDialogTests::autoUnlockCancelledCallThatThrowsSaysItIsNotKnown()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);
    wallet.m_ignore_cancel = true;
    wallet.m_autounlock_throws = true;

    AskPassphraseDialog dialog(AskPassphraseDialog::SetupAutoUnlock);
    dialog.setModel(&fixture.model);
    dialog.setAutoUnlockStatus(WindowsStatus("machine"));
    const ReleaseOnExit release{wallet};

    QStringList shown;
    QTimer closer;
    closer.setInterval(20);
    QObject::connect(&closer, &QTimer::timeout, &dialog, [&shown] {
        if (QMessageBox* const box = VisibleMessageBox()) {
            shown << box->text();
            box->accept();
        }
    });
    closer.start();

    AtCancel at_cancel;
    QElapsedTimer since_cancel;
    const int result = ExecAndCancel(dialog, wallet, at_cancel, since_cancel, [&] {
        QTimer::singleShot(100, &dialog, [&wallet] { wallet.m_release = true; });
    });

    QCOMPARE(result, int(QDialog::Rejected));
    QCOMPARE(shown.size(), 1);
    QVERIFY2(shown[0].contains(QStringLiteral("not known whether autounlock was changed")), qPrintable(shown[0]));
    QCOMPARE(wallet.m_set_autounlock_calls.load(), 1);
}

void AskPassphraseDialogTests::walletModelPassesTheAutoUnlockStatusOn()
{
    qt_test::FakeWallet wallet;
    wallet.m_autounlock_status = WindowsStatus("service");
    wallet.m_autounlock_status.configured = true;
    ModelOverFakeWallet fixture(wallet);

    const AutoUnlockStoreStatus status = fixture.model.getAutoUnlockStatus();
    QVERIFY(status.supported);
    QVERIFY(status.configured);
    QCOMPARE(status.scope, std::string("service"));
    QCOMPARE(status.location, wallet.m_autounlock_status.location);
}

// After an encryption in the split build the core stops, when the GUI's
// connection closes or by itself a while after the encryption, and the GUI then
// sees the connection drop, which asks for a quit
// (BitcoinGUI::requestQuitForCoreStop(), posted from the disconnect hook). The
// message after the encryption, which holds the warning about earlier backups,
// stays until the user closes it, and the quit happens then.
void AskPassphraseDialogTests::encryptionMessageOutlivesTheCore()
{
    // What the disconnect hook posts when the connection to the core is lost.
    const QuitWhileEncryptionMessage run = EncryptAndQuitWhileTheMessageIsOpen(
        *this, [] { BitcoinGUI::requestQuitForCoreStop(); });

    QVERIFY(run.confirmed);
    QVERIFY(run.asked);
    QVERIFY2(run.text.contains(QStringLiteral("IMPORTANT: Any earlier backups of your wallet file")),
             qPrintable(run.text));
    QVERIFY2(run.open_after_quit, "the message closed when the connection to the core was lost");
    QVERIFY(run.closed_by_case);
    QCOMPARE(run.app_result, 0);
    QVERIFY(!VisibleMessageBox());
}

// A quit from a termination signal (logout, system shutdown, the service
// manager) is made at once, also while the message after an encryption is
// open. The GUI's signal handler asks for it with BitcoinGUI::requestQuit().
void AskPassphraseDialogTests::terminationSignalDoesNotWaitForTheEncryptionMessage()
{
    const QuitWhileEncryptionMessage run =
        EncryptAndQuitWhileTheMessageIsOpen(*this, [] { BitcoinGUI::requestQuit(); });

    QVERIFY(run.confirmed);
    QVERIFY(run.asked);
    QVERIFY2(!run.open_after_quit, "the quit waited for the message");
    QVERIFY(!run.closed_by_case);
    QCOMPARE(run.app_result, 0);
    QVERIFY(!VisibleMessageBox());
}

// The message after an encryption in the split build says when the core stops:
// when the message is closed, or by itself a minute after the encryption when
// the message is left open, so it is true whichever comes first. The warning
// about earlier backups follows it.
void AskPassphraseDialogTests::encryptionMessageSaysWhenTheCoreStops()
{
    const QuitWhileEncryptionMessage run = EncryptAndQuitWhileTheMessageIsOpen(
        *this, [] { BitcoinGUI::requestQuitForCoreStop(); });

    QVERIFY2(run.text.contains(QStringLiteral("The Gridcoin core stops to finish the encryption when you close this "
                                              "message, or after a minute if it is left open.")),
             qPrintable(run.text));
    QVERIFY2(run.text.contains(QStringLiteral("IMPORTANT: Any earlier backups of your wallet file")),
             qPrintable(run.text));
    // The fake reports no backups from before the encryption, so none are listed.
    QVERIFY2(!run.text.contains(QStringLiteral("These automatic backups")), qPrintable(run.text));
    QCOMPARE(run.app_result, 0);
}

// The message after an encryption in the split build names the automatic backups the core
// found from before it, each as it is: a name is escaped, so markup in it is shown and not
// read. The warning about earlier backups comes first, as it did.
void AskPassphraseDialogTests::encryptionMessageListsTheBackupsBeforeIt()
{
    CheckTheBackupsAreListed(EncryptAndReadTheMessage(*this, /*multiprocess=*/true, ReportedBackups()));
}

// The same in the monolith, where the GUI quits when the message is closed.
void AskPassphraseDialogTests::encryptionMessageInTheMonolithListsTheBackupsBeforeIt()
{
    CheckTheBackupsAreListed(EncryptAndReadTheMessage(*this, /*multiprocess=*/false, ReportedBackups()));
}

// A wallet that ran unencrypted for years has a backup for every day, more than a message
// can show. The text names the first ten and says how many more there are, and the
// details, which scroll, list every one as plain text.
void AskPassphraseDialogTests::encryptionMessageWithManyBackupsListsThemAllInItsDetails()
{
    std::vector<std::string> backups;
    for (int day = 1; day <= 12; ++day) {
        backups.push_back(QStringLiteral("/srv/gridcoin/walletbackups/wallet.dat-2026-01-%1T00-00-00Z")
                              .arg(day, 2, 10, QLatin1Char('0'))
                              .toStdString());
    }
    backups.push_back(MARKUP_BACKUP);

    const EncryptionMessage run = EncryptAndReadTheMessage(*this, /*multiprocess=*/true, backups);

    QVERIFY(run.confirmed);
    QCOMPARE(run.app_result, 0);

    for (size_t i = 0; i < backups.size(); ++i) {
        const QString backup = QString::fromStdString(backups[i]);
        QVERIFY2(run.shown.contains(backup) == (i < 10), qPrintable(run.shown));
        QVERIFY2(run.details.split(QLatin1Char('\n')).contains(backup), qPrintable(run.details));
    }
    QVERIFY2(run.shown.contains(QStringLiteral("and 3 more, all listed under Show Details.")), qPrintable(run.shown));
}

// The end of the user's session (Windows WM_QUERYENDSESSION) in the split build
// quits at once, also while the message after an encryption is open. The GUI's
// local node is not shut down: its shutdown would arrive as the quit for the
// core's stop (QueueShutdown), which the message holds.
void AskPassphraseDialogTests::sessionEndDoesNotWaitForTheEncryptionMessage()
{
    qt_test::FakeNode node;
    // As main() wires the local node: its shutdown posts the quit for the core's stop.
    node.m_on_start_shutdown = [] {
        QMetaObject::invokeMethod(qApp, [] { BitcoinGUI::requestQuitForCoreStop(); }, Qt::QueuedConnection);
    };

    const QuitWhileEncryptionMessage run = EncryptAndQuitWhileTheMessageIsOpen(
        *this, [&node] { BitcoinGUI::requestQuitForSessionEnd(node, /*multiprocess=*/true); });

    QVERIFY(run.confirmed);
    QVERIFY(run.asked);
    QVERIFY2(!run.open_after_quit, "the session's end waited for the message");
    QVERIFY(!run.closed_by_case);
    QCOMPARE(run.app_result, 0);
    QCOMPARE(node.m_start_shutdown_calls, 0);
}

// In the monolith the node runs in this process: the session's end shuts it
// down, as before, and the node's shutdown quits the GUI (QueueShutdown).
void AskPassphraseDialogTests::sessionEndInTheMonolithShutsTheNodeDown()
{
    qt_test::FakeNode node;

    BitcoinGUI::requestQuitForSessionEnd(node, /*multiprocess=*/false);

    QCOMPARE(node.m_start_shutdown_calls, 1);
}
