// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_UNLOCKSCOPETESTS_H
#define BITCOIN_QT_TEST_UNLOCKSCOPETESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <memory>
#include <string>

//! The scope a wallet operation's unlock leaves behind: WalletModel's unlock
//! context, driven through a real WalletModel over a scripted wallet, with a
//! slot on requireUnlock playing the passphrase dialog, and RestoreWalletScope
//! called directly. Each test reads the wallet calls the restore made.
class UnlockScopeTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void unlockContextCharacterization();
    void unlockContextNonStdThrow();
    void unlockContextNarrowFailsWarnRead();
    void unlockContextNarrowOkNoRead();

    void restoreWalletScope();
    void restoreWalletScopeNonStd();

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

#endif // BITCOIN_QT_TEST_UNLOCKSCOPETESTS_H
