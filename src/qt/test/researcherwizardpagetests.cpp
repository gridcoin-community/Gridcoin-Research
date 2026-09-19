// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/researcherwizardpagetests.h"

#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizardmodedetailpage.h"
#include "qt/researcher/researcherwizardmodepage.h"
#include "qt/researcher/researcherwizardpoolpage.h"
#include "qt/test/interfacefakes.h"

#include <QDesktopServices>
#include <QRadioButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QWizardPage>

//!
//! \file researcherwizardpagetests.cpp
//! \brief Re-entering a wizard page must not multiply its slot connections.
//!
//! Each case drives a page against a fake researcher context, calls
//! initializePage() twice (a second forward entry, or a start-over), and then
//! performs one user action. The count of what that one action did is the
//! assertion: with the connections made in setModel() it is one; with them
//! made in initializePage() it is one per entry.
//!

void ResearcherWizardPageTests::poolPageOpensALinkOncePerClickAfterReentry()
{
    qt_test::FakeResearcherContext context;
    context.m_pools = {{"grcpool.com", "https://grcpool.com/"}};

    ResearcherModel model(context);
    ResearcherWizardPoolPage page;
    page.setModel(&model, nullptr);

    page.initializePage();
    page.initializePage();

    // Both entries ran: each one switches the researcher to pool mode.
    QCOMPARE(context.m_switch_mode_calls, 2);

    auto* table = page.findChild<QTableWidget*>("poolTableWidget");
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 1);

    // openLink() hands the cell's URL to QDesktopServices; route the https
    // scheme to a counter for the duration of the click.
    UrlOpenCounter counter;
    QDesktopServices::setUrlHandler("https", &counter, "open");

    const bool clicked = QMetaObject::invokeMethod(table, "cellClicked", Q_ARG(int, 0), Q_ARG(int, 0));

    QDesktopServices::unsetUrlHandler("https");

    QVERIFY(clicked);
    QCOMPARE(counter.m_opens, 1);
}

void ResearcherWizardPageTests::modePageReportsOneCompleteChangePerToggleAfterReentry()
{
    qt_test::FakeResearcherContext context;
    context.m_snapshot.has_eligible_projects = true;

    ResearcherModel model(context);
    ResearcherWizardModePage page;
    page.setModel(&model);

    page.initializePage();
    page.initializePage();

    auto* solo = page.findChild<QRadioButton*>("soloRadioButton");
    auto* pool = page.findChild<QRadioButton*>("poolRadioButton");
    QVERIFY(solo);
    QVERIFY(pool);
    QVERIFY(solo->isChecked());

    // Switching the exclusive group from solo to pool toggles two buttons,
    // and each toggle reaches one select slot, which reports completeChanged:
    // two per set of connections.
    QSignalSpy complete_changed(&page, &QWizardPage::completeChanged);

    pool->setChecked(true);

    QCOMPARE(complete_changed.count(), 2);
}

void ResearcherWizardPageTests::modeDetailPageReportsOneCompleteChangePerClickAfterReentry()
{
    qt_test::FakeResearcherContext context;
    context.m_snapshot.has_eligible_projects = true;

    ResearcherModel model(context);
    ResearcherWizardModeDetailPage page;
    page.setModel(&model);

    page.initializePage();
    page.initializePage();

    auto* pool = page.findChild<QRadioButton*>("poolRadioButton");
    QVERIFY(pool);

    // A click on a grouped button emits the group's clicked signal once, and
    // the page's slot reports completeChanged once per connection.
    QSignalSpy complete_changed(&page, &QWizardPage::completeChanged);

    pool->click();

    QCOMPARE(complete_changed.count(), 1);
}
