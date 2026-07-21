/********************************************************************************
** Form generated from reading UI file 'votewizard.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VOTEWIZARD_H
#define UI_VOTEWIZARD_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWizard>
#include "voting/votewizardballotpage.h"
#include "voting/votewizardsummarypage.h"

QT_BEGIN_NAMESPACE

class Ui_VoteWizard
{
public:
    VoteWizardBallotPage *ballotPage;
    VoteWizardSummaryPage *summaryPage;

    void setupUi(QWizard *VoteWizard)
    {
        if (VoteWizard->objectName().isEmpty())
            VoteWizard->setObjectName("VoteWizard");
        VoteWizard->resize(740, 580);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(VoteWizard->sizePolicy().hasHeightForWidth());
        VoteWizard->setSizePolicy(sizePolicy);
        VoteWizard->setSizeGripEnabled(true);
        VoteWizard->setModal(true);
        VoteWizard->setWizardStyle(QWizard::ClassicStyle);
        VoteWizard->setOptions(QWizard::NoBackButtonOnLastPage|QWizard::NoBackButtonOnStartPage|QWizard::NoCancelButtonOnLastPage);
        ballotPage = new VoteWizardBallotPage();
        ballotPage->setObjectName("ballotPage");
        VoteWizard->addPage(ballotPage);
        summaryPage = new VoteWizardSummaryPage();
        summaryPage->setObjectName("summaryPage");
        VoteWizard->addPage(summaryPage);

        retranslateUi(VoteWizard);

        QMetaObject::connectSlotsByName(VoteWizard);
    } // setupUi

    void retranslateUi(QWizard *VoteWizard)
    {
        VoteWizard->setWindowTitle(QCoreApplication::translate("VoteWizard", "Vote", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VoteWizard: public Ui_VoteWizard {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VOTEWIZARD_H
