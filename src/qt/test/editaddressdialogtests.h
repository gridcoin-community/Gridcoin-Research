// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_EDITADDRESSDIALOGTESTS_H
#define BITCOIN_QT_TEST_EDITADDRESSDIALOGTESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

//! The "add existing receiving address" dialog's picker over fakes
//! (qt_test::FakeGuiState): the dialog asks the core for the list and fills
//! its combo from the answer, never asking the wallet on the GUI thread, and
//! neither the dialog nor the wallet model waits for the core.
class EditAddressDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void askingFillsTheComboFromTheCoresAnswer();
    void anEmptyAnswerClearsTheLoadingState();
    void noCacheIsAFailedLoadWithNoGuiThreadCall();
    void aClosedDialogIsNeverTouched();
    void anAnswerAfterTheModelIsGoneIsDropped();
    void anErrorAnswerIsShownAsAnEmptyPicker();
    void aRequestThatThrowsIsAFailedLoad();
    void aLateAnswerToAnEarlierDialogIsIgnored();
    void anUnansweredRequestDoesNotBlockTheNext();
    void destructionDoesNotWaitBehindTheCoreForThePicker();

private:
    // The QSettings redirect OptionsModel needs (as in WalletModelTests), and
    // what it replaced.
    std::unique_ptr<QTemporaryDir> m_settings_root;
    QString m_prev_organization;
    QString m_prev_application;
    QSettings::Format m_prev_format{QSettings::NativeFormat};

    std::unique_ptr<QTemporaryDir> m_data_dir;
};

#endif // BITCOIN_QT_TEST_EDITADDRESSDIALOGTESTS_H
