/********************************************************************************
** Form generated from reading UI file 'researcherwizard.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARD_H
#define UI_RESEARCHERWIZARD_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWizard>
#include "researcher/researcherwizardauthpage.h"
#include "researcher/researcherwizardbeaconpage.h"
#include "researcher/researcherwizardemailpage.h"
#include "researcher/researcherwizardmodedetailpage.h"
#include "researcher/researcherwizardmodepage.h"
#include "researcher/researcherwizardnoncruncherpage.h"
#include "researcher/researcherwizardownershipproofpage.h"
#include "researcher/researcherwizardpoolpage.h"
#include "researcher/researcherwizardpoolsummarypage.h"
#include "researcher/researcherwizardprojectspage.h"
#include "researcher/researcherwizardsummarypage.h"

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizard
{
public:
    ResearcherWizardModePage *modePage;
    ResearcherWizardModeDetailPage *modeDetailPage;
    ResearcherWizardEmailPage *emailPage;
    ResearcherWizardProjectsPage *projectsPage;
    ResearcherWizardBeaconPage *beaconPage;
    ResearcherWizardAuthPage *authPage;
    ResearcherWizardSummaryPage *summaryPage;
    ResearcherWizardNoncruncherPage *NoncruncherPage;
    ResearcherWizardPoolPage *poolPage;
    ResearcherWizardPoolSummaryPage *poolSummaryPage;
    ResearcherWizardOwnershipProofPage *ownershipProofPage;

    void setupUi(QWizard *ResearcherWizard)
    {
        if (ResearcherWizard->objectName().isEmpty())
            ResearcherWizard->setObjectName("ResearcherWizard");
        ResearcherWizard->resize(800, 600);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizard->sizePolicy().hasHeightForWidth());
        ResearcherWizard->setSizePolicy(sizePolicy);
        ResearcherWizard->setSizeGripEnabled(true);
        ResearcherWizard->setModal(true);
        ResearcherWizard->setWizardStyle(QWizard::ClassicStyle);
        ResearcherWizard->setOptions(QWizard::HaveCustomButton1|QWizard::NoBackButtonOnLastPage|QWizard::NoBackButtonOnStartPage|QWizard::NoCancelButtonOnLastPage);
        modePage = new ResearcherWizardModePage();
        modePage->setObjectName("modePage");
        ResearcherWizard->addPage(modePage);
        modeDetailPage = new ResearcherWizardModeDetailPage();
        modeDetailPage->setObjectName("modeDetailPage");
        ResearcherWizard->addPage(modeDetailPage);
        emailPage = new ResearcherWizardEmailPage();
        emailPage->setObjectName("emailPage");
        sizePolicy.setHeightForWidth(emailPage->sizePolicy().hasHeightForWidth());
        emailPage->setSizePolicy(sizePolicy);
        ResearcherWizard->addPage(emailPage);
        projectsPage = new ResearcherWizardProjectsPage();
        projectsPage->setObjectName("projectsPage");
        sizePolicy.setHeightForWidth(projectsPage->sizePolicy().hasHeightForWidth());
        projectsPage->setSizePolicy(sizePolicy);
        ResearcherWizard->addPage(projectsPage);
        beaconPage = new ResearcherWizardBeaconPage();
        beaconPage->setObjectName("beaconPage");
        sizePolicy.setHeightForWidth(beaconPage->sizePolicy().hasHeightForWidth());
        beaconPage->setSizePolicy(sizePolicy);
        ResearcherWizard->addPage(beaconPage);
        authPage = new ResearcherWizardAuthPage();
        authPage->setObjectName("authPage");
        ResearcherWizard->addPage(authPage);
        summaryPage = new ResearcherWizardSummaryPage();
        summaryPage->setObjectName("summaryPage");
        sizePolicy.setHeightForWidth(summaryPage->sizePolicy().hasHeightForWidth());
        summaryPage->setSizePolicy(sizePolicy);
        ResearcherWizard->addPage(summaryPage);
        NoncruncherPage = new ResearcherWizardNoncruncherPage();
        NoncruncherPage->setObjectName("NoncruncherPage");
        ResearcherWizard->addPage(NoncruncherPage);
        poolPage = new ResearcherWizardPoolPage();
        poolPage->setObjectName("poolPage");
        ResearcherWizard->addPage(poolPage);
        poolSummaryPage = new ResearcherWizardPoolSummaryPage();
        poolSummaryPage->setObjectName("poolSummaryPage");
        ResearcherWizard->addPage(poolSummaryPage);
        ownershipProofPage = new ResearcherWizardOwnershipProofPage();
        ownershipProofPage->setObjectName("ownershipProofPage");
        sizePolicy.setHeightForWidth(ownershipProofPage->sizePolicy().hasHeightForWidth());
        ownershipProofPage->setSizePolicy(sizePolicy);
        ResearcherWizard->addPage(ownershipProofPage);

        retranslateUi(ResearcherWizard);

        QMetaObject::connectSlotsByName(ResearcherWizard);
    } // setupUi

    void retranslateUi(QWizard *ResearcherWizard)
    {
        ResearcherWizard->setWindowTitle(QCoreApplication::translate("ResearcherWizard", "Researcher Configuration", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizard: public Ui_ResearcherWizard {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARD_H
