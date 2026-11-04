// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_RPCCONSOLETESTS_H
#define BITCOIN_QT_TEST_RPCCONSOLETESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

class RPCConsoleTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void redactsListedCommand();
    void leavesOrdinaryCommandUnchanged();
    void redactsClaimHtlcPreimage();
    void redactsUnparseableClaimHtlc();
    void redactsMakeSeedPhrasePassword();
    void enteredCommandLeavesNoUndoStep();
    void clearedInputLeavesNoUndoStep();
    void passphraseCommandsKeepNoSecretInHistoryOrPane();
    void handlesLineLongerThanSecurePool();
    void routesPassphraseCommandsOverSecretPath();
    void routesPassphraseChangeWithTwoSecrets();
    void routesEncryptWalletOverSecretPath();
    void routesOrdinaryCommandOverPlainPath();
    void passphraseCommandWithoutArgsStaysOnSecretPath();
    void passphraseCommandIsNeverRunAgainUnderUnlock();
    void refusesPassphraseTooLongForSecurePool();
    void everyPassphraseMethodIsRedacted();
    void redactsMisCasedPassphraseCommand();

    // The file commands (interfaces::ConsoleFileCommands()), run through the file transfer.
    void fileCommandBareNameResolvesToDataDir();
    void dumpcontractsPathIsSecondArgument();
    void nonFileCommandIsNotIntercepted();
    void fileCommandWithoutPathIsAnError();
    void fileCommandWithTooManyArgumentsIsAnError();
    void fileCommandNeedsALoadedWallet();
    void downloadRunsOnTheWorkerAndShowsWhatItWrote();
    void downloadHoldsItsOwnCapabilityUntilTheSourceIsGone();
    void downloadNoteNamesTheLocalFile();
    void downloadWithNothingToSendIsReportedNotWritten();
    void downloadOfAnEmptyFileWritesItAndShowsTheReply();
    void downloadFirstReadErrorIsShown();
    void downloadToACoreFileIsRefusedBeforeThePrompt();
    void downloadKeepsAnExistingFile();
    void downloadToADestinationThatCannotBeCheckedSaysSo();
    void downloadCancelLeavesNothing();
    void beginThatThrowsIsShown();
    void uploadSendsTheFileAndShowsTheOutput();
    void uploadOfNoRegularFileIsRefusedBeforeThePrompt();
    void uploadCancelIsReportedUnlessTheCommandRan();
    void noPromptWhileAFileCommandHoldsItsCapability();

private:
    // QSettings and the data directory are redirected, as the options model the
    // file command cases build reads both.
    std::unique_ptr<QTemporaryDir> m_settings_root;
    QString m_prev_organization;
    QString m_prev_application;
    QSettings::Format m_prev_format{QSettings::NativeFormat};
    std::unique_ptr<QTemporaryDir> m_data_dir;
};

#endif // BITCOIN_QT_TEST_RPCCONSOLETESTS_H
