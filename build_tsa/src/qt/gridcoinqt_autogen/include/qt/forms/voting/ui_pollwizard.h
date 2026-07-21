/********************************************************************************
** Form generated from reading UI file 'pollwizard.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLWIZARD_H
#define UI_POLLWIZARD_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWizard>
#include "voting/pollwizarddetailspage.h"
#include "voting/pollwizardprojectpage.h"
#include "voting/pollwizardsummarypage.h"
#include "voting/pollwizardtypepage.h"

QT_BEGIN_NAMESPACE

class Ui_PollWizard
{
public:
    PollWizardTypePage *typePage;
    PollWizardProjectPage *projectPage;
    PollWizardDetailsPage *detailsPage;
    PollWizardSummaryPage *summaryPage;

    void setupUi(QWizard *PollWizard)
    {
        if (PollWizard->objectName().isEmpty())
            PollWizard->setObjectName("PollWizard");
        PollWizard->resize(670, 580);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(PollWizard->sizePolicy().hasHeightForWidth());
        PollWizard->setSizePolicy(sizePolicy);
        PollWizard->setSizeGripEnabled(true);
        PollWizard->setModal(true);
        PollWizard->setWizardStyle(QWizard::ClassicStyle);
        PollWizard->setOptions(QWizard::NoBackButtonOnLastPage|QWizard::NoBackButtonOnStartPage|QWizard::NoCancelButtonOnLastPage);
        typePage = new PollWizardTypePage();
        typePage->setObjectName("typePage");
        PollWizard->addPage(typePage);
        projectPage = new PollWizardProjectPage();
        projectPage->setObjectName("projectPage");
        PollWizard->addPage(projectPage);
        detailsPage = new PollWizardDetailsPage();
        detailsPage->setObjectName("detailsPage");
        PollWizard->addPage(detailsPage);
        summaryPage = new PollWizardSummaryPage();
        summaryPage->setObjectName("summaryPage");
        PollWizard->addPage(summaryPage);

        retranslateUi(PollWizard);

        QMetaObject::connectSlotsByName(PollWizard);
    } // setupUi

    void retranslateUi(QWizard *PollWizard)
    {
        PollWizard->setWindowTitle(QCoreApplication::translate("PollWizard", "Create a Poll", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PollWizard: public Ui_PollWizard {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLWIZARD_H
