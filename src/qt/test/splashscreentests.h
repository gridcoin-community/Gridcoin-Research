// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_SPLASHSCREENTESTS_H
#define BITCOIN_QT_TEST_SPLASHSCREENTESTS_H

#include <QObject>
#include <QTest>

class SplashScreenTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void theSplashIsANormalFramelessWindow();
    void theSplashStartsAboveOtherWindows();
    void theSplashAllowsMinimise();
    void theSplashHasATaskbarTitle();
    void theSplashHasAnIconOfItsOwn();
    void theSplashSaysWhatAClickDoes();
    void aClickReleasesTheSplashAndMinimisesIt();
    void aReleasedSplashStaysReleased();
};

#endif // BITCOIN_QT_TEST_SPLASHSCREENTESTS_H
