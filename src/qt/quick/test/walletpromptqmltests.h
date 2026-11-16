// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_WALLETPROMPTQMLTESTS_H
#define BITCOIN_QT_QUICK_TEST_WALLETPROMPTQMLTESTS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <memory>
#include <string>

//! The QML that answers WalletOps and SendCoins: the unlock dialog, the
//! wallet prompts and their dialog, the send page, and the main window's way
//! to the send page (its out-of-sync question and the URI path). It runs no
//! event loop.
class QmlWalletPromptTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void unlockDialogBinds();
    void unlockDialogCloseCancels();
    void promptsAnswerEveryClose();
    void resultPromptOpenRule();
    void sendViewBinds();
    void navigationGuards();
    void bridgeUriReachesSend();
    void uriBeforeAttachClaimed();
    void pasteReplacesRecipient();

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

#endif // BITCOIN_QT_QUICK_TEST_WALLETPROMPTQMLTESTS_H
