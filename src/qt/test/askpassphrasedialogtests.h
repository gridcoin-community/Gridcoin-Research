// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_ASKPASSPHRASEDIALOGTESTS_H
#define BITCOIN_QT_TEST_ASKPASSPHRASEDIALOGTESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

//! AskPassphraseDialog over a WalletModel made on qt_test::FakeWallet, whose
//! beginElevation() waits as a passphrase attempt waits for its turn. The
//! prompt can be cancelled while its check waits: it closes at once, and the
//! check's outcome is not acted on.
class AskPassphraseDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void cancelWhileWaitingClosesAtOnce();
    void lateAcceptanceIsEnded();

private:
    // The QSettings redirect OptionsModel needs (as in OptionsModelTests), and
    // what it replaced.
    std::unique_ptr<QTemporaryDir> m_settings_root;
    QString m_prev_organization;
    QString m_prev_application;
    QSettings::Format m_prev_format{QSettings::NativeFormat};

    std::unique_ptr<QTemporaryDir> m_data_dir;
};

#endif // BITCOIN_QT_TEST_ASKPASSPHRASEDIALOGTESTS_H
