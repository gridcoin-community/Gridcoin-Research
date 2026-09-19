// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_RESEARCHERWIZARDPAGETESTS_H
#define BITCOIN_QT_TEST_RESEARCHERWIZARDPAGETESTS_H

#include <QObject>
#include <QTest>
#include <QUrl>

//! Receives the URL a page hands to QDesktopServices, so a test can count
//! how many times one click opened the link instead of launching a browser.
class UrlOpenCounter : public QObject
{
    Q_OBJECT

public:
    int m_opens = 0;

public slots:
    void open(const QUrl& /*url*/) { ++m_opens; }
};

//! The researcher wizard's pages are re-entered: QWizard runs initializePage()
//! on every forward entry, and the start-over button restarts from the first
//! page. A signal connection made inside initializePage() is therefore made
//! once per visit, and its slot runs once per visit for a single click. These
//! cases enter each page twice and count what one click does.
class ResearcherWizardPageTests : public QObject
{
    Q_OBJECT

private slots:
    void poolPageOpensALinkOncePerClickAfterReentry();
    void modePageReportsOneCompleteChangePerToggleAfterReentry();
    void modeDetailPageReportsOneCompleteChangePerClickAfterReentry();
};

#endif // BITCOIN_QT_TEST_RESEARCHERWIZARDPAGETESTS_H
