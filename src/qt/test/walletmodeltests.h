// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_WALLETMODELTESTS_H
#define BITCOIN_QT_TEST_WALLETMODELTESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

//! WalletModel's coin list load over fakes (qt_test::FakeCoinSource): where the
//! wallet scan runs, what a scan that throws leads to, and the model's
//! destruction while a scan runs.
class WalletModelTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void coinLoadRunsOnTheCoinLoadWorker();
    void coinLoadFailureIsContainedAndReported();
    void destructionWaitsForTheCoinLoad();

private:
    // The QSettings redirect OptionsModel needs (as in OptionsModelTests), and
    // what it replaced.
    std::unique_ptr<QTemporaryDir> m_settings_root;
    QString m_prev_organization;
    QString m_prev_application;
    QSettings::Format m_prev_format{QSettings::NativeFormat};

    std::unique_ptr<QTemporaryDir> m_data_dir;
};

#endif // BITCOIN_QT_TEST_WALLETMODELTESTS_H
