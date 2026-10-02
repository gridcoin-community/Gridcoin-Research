// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_OPTIONSMODELTESTS_H
#define BITCOIN_QT_TEST_OPTIONSMODELTESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

//! OptionsModel driven by a fake interfaces::Node (qt_test::FakeNode). The
//! Options dialog's mapper submits every mapped field on OK, edited or not, so
//! these tests pin what a submit writes through changeSettings, the only path by
//! which OptionsModel::setData writes a core setting to gridcoinsettings.json:
//! an untouched field must write nothing (a write would pin the displayed value
//! over the config file), and an edited one must still be stored.
class OptionsModelTests : public QObject
{
    Q_OBJECT

public:
    OptionsModelTests();
    ~OptionsModelTests() override;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void unchangedReserveWritesNothing();
    void changedReserveIsStored();

    void unchangedCoreSettingsWriteNothing_data();
    void unchangedCoreSettingsWriteNothing();
    void changedCoreSettingIsStored_data();
    void changedCoreSettingIsStored();
    void mapperRoundTripWritesNothing();

    void unchangedProxyWritesNothing_unset_data();
    void unchangedProxyWritesNothing_unset();
    void unchangedProxyKeepsNodeProxy();
    void unchangedProxyTypedReadBack();
    void changedProxyIsPushed();
    void proxyFailedEnableRetries();
    void proxyFailedHostRetries();
    void proxyFailedPortRetries();
    void proxyFailedEnableThenValidHost();

private:
    // Suite-wide QSettings redirect (see initTestCase) and what it replaced.
    std::unique_ptr<QTemporaryDir> m_settings_root;
    QString m_prev_organization;
    QString m_prev_application;
    QSettings::Format m_prev_format{QSettings::NativeFormat};

    // Per-function datadir (see init).
    std::unique_ptr<QTemporaryDir> m_data_dir;
};

#endif // BITCOIN_QT_TEST_OPTIONSMODELTESTS_H
