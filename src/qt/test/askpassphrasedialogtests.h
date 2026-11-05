// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_ASKPASSPHRASEDIALOGTESTS_H
#define BITCOIN_QT_TEST_ASKPASSPHRASEDIALOGTESTS_H

#include "qt/guiipcworker.h"

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

//! AskPassphraseDialog over a WalletModel made on qt_test::FakeWallet, whose
//! passphrase calls wait as a passphrase attempt waits for its turn. Each case
//! runs the prompt under exec(), as its callers do, and cancels it from inside
//! the event loop while its check waits. A check that sees the cancel gives up
//! and the prompt ends at once; one that does not keeps the prompt open, with
//! the event loop running, until it reports, and the prompt then acts only on
//! what it must: ending a capability begun, reporting a passphrase changed.
//! The checks run on a modal worker made for the class, as main() makes one
//! for the application.
class AskPassphraseDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void cancelWhileWaitingEndsTheLoopAtOnce();
    void cancelPastTheTurnKeepsTheLoopRunning();
    void passphraseChangedAfterCancelIsReported();
    void checkRunsOnTheModalWorker();
    void cancelWhileQueuedEndsAtOnceAndTriesNothing();
    void destroyedUnderARunningCheckEndsItsCapability();
    void deletedAfterCancelWhileQueuedReturnsAtOnce();
    void deletedWhileQueuedGivesTheCheckUp();
    void autoUnlockSetupShowsTheScopeText();
    void autoUnlockSetupSaysWhereItIsStored();
    void autoUnlockSetupUnverifiedSaysItIsCheckedAtNextStart();
    void autoUnlockClearSaysTheWalletIsLocked();
    void autoUnlockWrongPassphraseKeepsThePromptOpen();
    void autoUnlockStoreFailureEndsThePrompt();
    void walletModelPassesTheAutoUnlockStatusOn();
    // Last: these quit the application under the application's own event loop.
    void encryptionMessageOutlivesTheCore();
    void terminationSignalDoesNotWaitForTheEncryptionMessage();
    void encryptionMessageSaysWhenTheCoreStops();
    void encryptionMessageListsTheBackupsBeforeIt();
    void encryptionMessageInTheMonolithListsTheBackupsBeforeIt();
    void encryptionMessageWithManyBackupsListsThemAllInItsDetails();
    void sessionEndDoesNotWaitForTheEncryptionMessage();
    void sessionEndInTheMonolithShutsTheNodeDown();

private:
    std::unique_ptr<GuiIpcWorker> m_worker;

    // The QSettings redirect OptionsModel needs (as in OptionsModelTests), and
    // what it replaced.
    std::unique_ptr<QTemporaryDir> m_settings_root;
    QString m_prev_organization;
    QString m_prev_application;
    QSettings::Format m_prev_format{QSettings::NativeFormat};

    std::unique_ptr<QTemporaryDir> m_data_dir;
};

#endif // BITCOIN_QT_TEST_ASKPASSPHRASEDIALOGTESTS_H
