// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "aboutdialogtests.h"

#include "qt/aboutdialog.h"

#include <QPushButton>

// Q_INIT_RESOURCE must be called from outside any namespace.
static void InitBitcoinResources() { Q_INIT_RESOURCE(bitcoin); }

void AboutDialogTests::initTestCase()
{
    // The texts are embedded in the gridcoinqt library's bitcoin.qrc; the GUI
    // registers it in main(), so this binary has to as well.
    InitBitcoinResources();
}

//! Both bundled fonts' SIL OFL 1.1 notices are reachable through the About dialog:
//! each font's copyright line and the licence text itself, and no "not available"
//! fallback. A missing or renamed resource fails here rather than shipping a
//! button that shows nothing.
void AboutDialogTests::thirdPartyLicensesCarryBothFontLicenses()
{
    const QString text = AboutDialog::thirdPartyLicensesText();

    QVERIFY2(text.contains(QStringLiteral("The Inter Project Authors")), "Inter copyright missing");
    QVERIFY2(text.contains(QStringLiteral("The Inconsolata Project Authors")), "Inconsolata copyright missing");
    QCOMPARE(text.count(QStringLiteral("SIL OPEN FONT LICENSE Version 1.1")), 2);
    QVERIFY2(!text.contains(AboutDialog::tr("License text not available in this build.")),
             "a licence resource could not be read");
}

void AboutDialogTests::aboutDialogOffersTheThirdPartyLicensesButton()
{
    AboutDialog dialog;
    auto* button = dialog.findChild<QPushButton*>(QStringLiteral("thirdPartyLicensesButton"));
    QVERIFY(button);
    QVERIFY(button->isEnabled());
}
