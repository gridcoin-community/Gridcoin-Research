// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_AUTOSTARTTESTS_H
#define BITCOIN_QT_TEST_AUTOSTARTTESTS_H

#include <QByteArray>
#include <QObject>
#include <QTest>

#include <memory>

class QTemporaryDir;

class AutoStartTests : public QObject
{
    Q_OBJECT

public:
    AutoStartTests();
    ~AutoStartTests();

private slots:
    void init();
    void cleanup();

    void regtestCreatesNoLoginItem();
    void regtestLeavesTheMainnetLoginItemAlone();

private:
    std::unique_ptr<QTemporaryDir> m_config_home;
    std::unique_ptr<QTemporaryDir> m_data_root;
    QByteArray m_saved_config_home;
    bool m_had_config_home = false;
};

#endif // BITCOIN_QT_TEST_AUTOSTARTTESTS_H
