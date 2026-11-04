// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/askpassphrasedialogtests.h"

#include "interfaces/wallet_coin_source.h"
#include "qt/askpassphrasedialog.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"
#include "util.h"

#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QSignalSpy>

#include <memory>

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

//! Lets the fake's beginElevation() return however the case exits, before the
//! dialog, which joins the check's thread, is destroyed. Declare it after the
//! dialog.
struct ReleaseOnExit
{
    qt_test::FakeWallet& wallet;

    ~ReleaseOnExit() { wallet.m_release = true; }
};

//! Type a passphrase into the prompt and press OK, which starts the check.
void SubmitPassphrase(AskPassphraseDialog& dialog)
{
    QLineEdit* const edit = dialog.findChild<QLineEdit*>("oldPassphraseEdit");
    QVERIFY(edit);
    edit->setText(QStringLiteral("dialog-test-passphrase"));
    dialog.accept();
}

} // namespace

void AskPassphraseDialogTests::initTestCase()
{
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

void AskPassphraseDialogTests::cancelWhileWaitingClosesAtOnce()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    AskPassphraseDialog dialog(AskPassphraseDialog::Elevate);
    dialog.setModel(&fixture.model);
    dialog.setCapabilityKind(CapabilityKind::Send);
    const ReleaseOnExit release{wallet};

    QSignalSpy finished(&dialog, &QDialog::finished);

    dialog.show();
    SubmitPassphrase(dialog);
    QTRY_VERIFY_WITH_TIMEOUT(wallet.m_waiting.load(), 5000);

    // Cancelled while its check waits for a turn: the prompt goes at once.
    QElapsedTimer timer;
    timer.start();
    dialog.reject();
    const qint64 took = timer.elapsed();

    QVERIFY(!dialog.isVisible());
    QVERIFY2(took < 1000, qPrintable(QStringLiteral("reject() took %1 ms").arg(took)));

    // The check stopped waiting, because the cancel reached its wait, and the
    // prompt ends rejected, with nothing begun to end.
    QTRY_VERIFY_WITH_TIMEOUT(wallet.m_returned.load(), 5000);
    QVERIFY(wallet.m_saw_cancel.load());

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(finished.at(0).at(0).toInt(), int(QDialog::Rejected));
    QCOMPARE(wallet.m_end_elevation_calls.load(), 0);
}

void AskPassphraseDialogTests::lateAcceptanceIsEnded()
{
    qt_test::FakeWallet wallet;
    ModelOverFakeWallet fixture(wallet);

    // A check already past its turn finishes whatever the cancel says.
    wallet.m_ignore_cancel = true;

    AskPassphraseDialog dialog(AskPassphraseDialog::Elevate);
    dialog.setModel(&fixture.model);
    dialog.setCapabilityKind(CapabilityKind::Send);
    const ReleaseOnExit release{wallet};

    QSignalSpy finished(&dialog, &QDialog::finished);

    dialog.show();
    SubmitPassphrase(dialog);
    QTRY_VERIFY_WITH_TIMEOUT(wallet.m_waiting.load(), 5000);

    dialog.reject();
    QVERIFY(!dialog.isVisible());

    // The passphrase is then accepted. Nothing is waiting for the capability
    // it began, so the prompt ends it, and still ends rejected.
    wallet.m_release_result = true;
    wallet.m_release = true;

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(finished.at(0).at(0).toInt(), int(QDialog::Rejected));
    QCOMPARE(wallet.m_end_elevation_calls.load(), 1);
}
