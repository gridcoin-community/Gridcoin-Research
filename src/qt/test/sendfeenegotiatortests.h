// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_SENDFEENEGOTIATORTESTS_H
#define BITCOIN_QT_TEST_SENDFEENEGOTIATORTESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <memory>
#include <string>

//! The send dialog's fee-confirmation loop: a real SendCoinsDialog over a real
//! WalletModel and a scripted wallet, with its message boxes answered by a
//! script, and SendFeeNegotiator driven directly. Each test reads the attempts
//! the wallet saw and the boxes shown, or the steps the negotiator returned.
class SendFeeNegotiatorTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void sendDialogFeeLoopCharacterization();
    void sendDialogFeeInBalanceText();
    void sendDialogCoinControlEveryAttempt();

    void sendFeeNegotiator();

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

#endif // BITCOIN_QT_TEST_SENDFEENEGOTIATORTESTS_H
