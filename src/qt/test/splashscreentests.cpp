// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "splashscreentests.h"

#include "qt/splashscreen.h"

// Q_INIT_RESOURCE must be called from outside any namespace.
static void InitBitcoinResources() { Q_INIT_RESOURCE(bitcoin); }

void SplashScreenTests::initTestCase()
{
    // The splash image and the window icon are embedded in the gridcoinqt
    // library's bitcoin.qrc; the GUI registers it in main(), so this binary has
    // to as well.
    InitBitcoinResources();
}

//! The splash is a normal top-level window without a frame. It is not of the
//! Qt::SplashScreen window type (see the class comment in qt/splashscreen.h).
void SplashScreenTests::theSplashIsANormalFramelessWindow()
{
    SplashScreen splash;

    QCOMPARE(splash.windowType(), Qt::Window);
    QVERIFY(splash.windowFlags().testFlag(Qt::FramelessWindowHint));
}

//! The splash asks to be kept above other windows when it is created, so the
//! loading progress comes up in front.
void SplashScreenTests::theSplashStartsAboveOtherWindows()
{
    SplashScreen splash;

    QVERIFY(splash.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
}

//! The splash carries the minimise hint, which a frameless window needs for a
//! window manager to offer "minimise" for it.
void SplashScreenTests::theSplashAllowsMinimise()
{
    SplashScreen splash;

    QVERIFY(splash.windowFlags().testFlag(Qt::WindowMinimizeButtonHint));
}

//! The splash has a window title, so its taskbar entry can be told from other
//! programs' entries.
void SplashScreenTests::theSplashHasATaskbarTitle()
{
    SplashScreen splash;

    QVERIFY(!splash.windowTitle().isEmpty());
}

//! The splash has an icon of its own for its taskbar entry.
void SplashScreenTests::theSplashHasAnIconOfItsOwn()
{
    // The icon is an SVG file, which Qt reads through its SVG image plugin.
    // Where that plugin is not installed, the wallet's windows get no icon
    // from Qt.
    if (QPixmap(":/images/gridcoin").isNull()) {
        QSKIP("This Qt cannot read the SVG icon.");
    }

    SplashScreen splash;

    // A widget with no icon of its own reports the application's, so the
    // attribute is what says the splash set one.
    QVERIFY(splash.testAttribute(Qt::WA_SetWindowIcon));
    QVERIFY(!splash.windowIcon().isNull());
}

//! The splash has a tooltip that tells the user a click puts it out of the way.
void SplashScreenTests::theSplashSaysWhatAClickDoes()
{
    SplashScreen splash;

    QVERIFY(!splash.toolTip().isEmpty());
}

//! A click on the splash releases it and minimises it: it no longer asks to be
//! kept above other windows, and it is minimised, not closed.
void SplashScreenTests::aClickReleasesTheSplashAndMinimisesIt()
{
    SplashScreen splash;
    splash.show();
    QVERIFY(!splash.isMinimized());

    QTest::mouseClick(&splash, Qt::LeftButton);

    QVERIFY(!splash.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    QVERIFY(splash.isMinimized());
    QVERIFY(splash.isVisible());
    // What the click does not change.
    QCOMPARE(splash.windowType(), Qt::Window);
    QVERIFY(splash.windowFlags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(splash.windowFlags().testFlag(Qt::WindowMinimizeButtonHint));
}

//! Once released, the splash does not go back above other windows when it is
//! shown again from the taskbar.
void SplashScreenTests::aReleasedSplashStaysReleased()
{
    SplashScreen splash;
    splash.show();
    QTest::mouseClick(&splash, Qt::LeftButton);

    splash.showNormal();

    QVERIFY(!splash.isMinimized());
    QVERIFY(!splash.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
}
