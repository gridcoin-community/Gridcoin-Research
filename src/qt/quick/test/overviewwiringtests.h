// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_OVERVIEWWIRINGTESTS_H
#define BITCOIN_QT_QUICK_TEST_OVERVIEWWIRINGTESTS_H

#include <QObject>
#include <QTest>

//! The carried overview, status footer, About window and main window bound to
//! the status adapters, through the real root and QmlFrontEnd over a
//! qml_test::ModelHarness. None runs the application's event loop.
class QmlOverviewWiringTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void overviewShowsAdapters();
    void untrustedTextIsPlain();
    void footerFollowsState();
    void buildBannerFromShellState();
    void pollTitleNavigates();
    void aboutWindowBindings();
    void footerIconsFollowState();
    void footerStakingToolTipFollowsMask();
};

#endif // BITCOIN_QT_QUICK_TEST_OVERVIEWWIRINGTESTS_H
