/********************************************************************************
** Form generated from reading UI file 'votewizardballotpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VOTEWIZARDBALLOTPAGE_H
#define UI_VOTEWIZARDBALLOTPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>
#include "voting/polldetails.h"

QT_BEGIN_NAMESPACE

class Ui_VoteWizardBallotPage
{
public:
    QVBoxLayout *summaryPageLayout;
    PollDetails *details;
    QFrame *detailsLine;
    QLabel *errorLabel;
    QScrollArea *detailsScrollArea;
    QWidget *detailsScrollContents;
    QVBoxLayout *verticalLayout;
    QVBoxLayout *choicesLayout;
    QSpacerItem *bottomSpacer;

    void setupUi(QWizardPage *VoteWizardBallotPage)
    {
        if (VoteWizardBallotPage->objectName().isEmpty())
            VoteWizardBallotPage->setObjectName("VoteWizardBallotPage");
        VoteWizardBallotPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(VoteWizardBallotPage->sizePolicy().hasHeightForWidth());
        VoteWizardBallotPage->setSizePolicy(sizePolicy);
        summaryPageLayout = new QVBoxLayout(VoteWizardBallotPage);
        summaryPageLayout->setSpacing(9);
        summaryPageLayout->setObjectName("summaryPageLayout");
        summaryPageLayout->setContentsMargins(16, 16, 16, 16);
        details = new PollDetails(VoteWizardBallotPage);
        details->setObjectName("details");

        summaryPageLayout->addWidget(details);

        detailsLine = new QFrame(VoteWizardBallotPage);
        detailsLine->setObjectName("detailsLine");
        detailsLine->setFrameShape(QFrame::Shape::HLine);
        detailsLine->setFrameShadow(QFrame::Shadow::Sunken);

        summaryPageLayout->addWidget(detailsLine);

        errorLabel = new QLabel(VoteWizardBallotPage);
        errorLabel->setObjectName("errorLabel");
        errorLabel->setWordWrap(true);

        summaryPageLayout->addWidget(errorLabel);

        detailsScrollArea = new QScrollArea(VoteWizardBallotPage);
        detailsScrollArea->setObjectName("detailsScrollArea");
        detailsScrollArea->setWidgetResizable(true);
        detailsScrollContents = new QWidget();
        detailsScrollContents->setObjectName("detailsScrollContents");
        detailsScrollContents->setGeometry(QRect(0, 0, 596, 392));
        verticalLayout = new QVBoxLayout(detailsScrollContents);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        choicesLayout = new QVBoxLayout();
        choicesLayout->setSpacing(9);
        choicesLayout->setObjectName("choicesLayout");

        verticalLayout->addLayout(choicesLayout);

        bottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(bottomSpacer);

        detailsScrollArea->setWidget(detailsScrollContents);

        summaryPageLayout->addWidget(detailsScrollArea);


        retranslateUi(VoteWizardBallotPage);

        QMetaObject::connectSlotsByName(VoteWizardBallotPage);
    } // setupUi

    void retranslateUi(QWizardPage *VoteWizardBallotPage)
    {
        (void)VoteWizardBallotPage;
    } // retranslateUi

};

namespace Ui {
    class VoteWizardBallotPage: public Ui_VoteWizardBallotPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VOTEWIZARDBALLOTPAGE_H
