// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_WALLETOPGATETESTS_H
#define BITCOIN_QT_QUICK_TEST_WALLETOPGATETESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <memory>
#include <string>

//! WalletOps, the wallet-operation gate, on a front end's call runner over a
//! scripted wallet: the lock-state read, the passphrase request and its
//! answers, the lease that gives the wallet back its scope, the shared prompt
//! latch and the detach, and the threads its wallet calls (and the send
//! adapter's, through it) run on. It runs no event loop.
class QmlWalletOpGateTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void lockedUnlockRunsOpAndRelocks();
    void stakingOnlyElevatesAndNarrows();
    void beginReturnsBeforeTheRead();
    void cancelBeforeAnswer();
    void disconnectDuringUnlock();
    void secondBeginIsBusy();
    void modeFromOneRead();
    void wrongPassphraseKeepsRequest();
    void wrongPassphraseWithZeroByte();
    void elevateExpiredNeverUnlocks();
    void noStakingFlag();
    void pooledLaneDoesNotBlockGate();
    void staleIdRefused();
    void heldLeaseReleasedAtDetach();
    void restoreRunsAfterLastOp();
    void droppedReleaseIsMadeUp();
    void cancelDuringRunningAnswer();
    void cancelDuringFailingAnswer();
    void cancelAfterWorkerReadFlag();
    void promptLatchShared();
    void throwingOpReleasesBeforeFatal();
    void walletCallsOffGuiThread();
    void unlockPassphraseInSecureMemory();

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

#endif // BITCOIN_QT_QUICK_TEST_WALLETOPGATETESTS_H
