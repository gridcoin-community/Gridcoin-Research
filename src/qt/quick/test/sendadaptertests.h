// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_SENDADAPTERTESTS_H
#define BITCOIN_QT_QUICK_TEST_SENDADAPTERTESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <memory>
#include <string>

//! SendCoins, the send adapter, over a real WalletModel and a scripted
//! wallet, with WalletOps on the same front end's call runner: the rows, the
//! confirmation and fee questions, the outcomes and their texts, the URI path,
//! and the invokables before attach and after detach. It runs no event loop.
class QmlSendAdapterTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void largeFeeDoesNotHang();
    void feeDeclined();
    void feeCap();
    void uriFillsAndNavigates();
    void everyStatusHasAMessage();
    void duplicateNeverReachesInterface();
    void lockedSendPromptsFirst();
    void leaseHeldAcrossFeePrompt();
    void sendRunsOnSerialLane();
    void messageFirstRowAndHeight();
    void recipientEdits();
    void invalidRowsMarkedNothingAsked();
    void labelAndSubtractReachWallet();
    void displayUnit();
    void confirmationText();
    void busyRefusesSecondSend();
    void confirmedRowsAreSent();
    void unlockCancelledDuringSend();
    void negotiatorPerSend();
    void staleAnswersRefused();
    void unattachedInvokablesAreSafe();
    void invokablesDeadAfterDetach();

private:
    // Suite-wide QSettings redirect (see initTestCase) and what it replaced.
    std::unique_ptr<QTemporaryDir> m_settings_root;
    QString m_prev_organization;
    QString m_prev_application;
    QSettings::Format m_prev_format{QSettings::NativeFormat};

    // Per-function datadir (see init) and the -datadir it replaced.
    std::unique_ptr<QTemporaryDir> m_data_dir;
    bool m_prev_datadir_set{false};
    std::string m_prev_datadir;
};

#endif // BITCOIN_QT_QUICK_TEST_SENDADAPTERTESTS_H
