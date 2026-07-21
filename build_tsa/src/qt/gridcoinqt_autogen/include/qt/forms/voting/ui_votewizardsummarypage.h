/********************************************************************************
** Form generated from reading UI file 'votewizardsummarypage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VOTEWIZARDSUMMARYPAGE_H
#define UI_VOTEWIZARDSUMMARYPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_VoteWizardSummaryPage
{
public:
    QVBoxLayout *summaryPageLayout;
    QSpacerItem *topSpacer;
    QLabel *iconlabel;
    QLabel *pageTitleLabel;
    QSpacerItem *titleSpacer;
    QLabel *pollTitleLabel;
    QLabel *responsesLabel;
    QSpacerItem *middleSpacer;
    QLabel *hintLabel;
    QLabel *voteIdLabel;
    QPushButton *copyToClipboardButton;
    QSpacerItem *bottomSpacer;

    void setupUi(QWizardPage *VoteWizardSummaryPage)
    {
        if (VoteWizardSummaryPage->objectName().isEmpty())
            VoteWizardSummaryPage->setObjectName("VoteWizardSummaryPage");
        VoteWizardSummaryPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(VoteWizardSummaryPage->sizePolicy().hasHeightForWidth());
        VoteWizardSummaryPage->setSizePolicy(sizePolicy);
        summaryPageLayout = new QVBoxLayout(VoteWizardSummaryPage);
        summaryPageLayout->setSpacing(9);
        summaryPageLayout->setObjectName("summaryPageLayout");
        summaryPageLayout->setContentsMargins(16, 16, 16, 16);
        topSpacer = new QSpacerItem(20, 60, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        summaryPageLayout->addItem(topSpacer);

        iconlabel = new QLabel(VoteWizardSummaryPage);
        iconlabel->setObjectName("iconlabel");
        iconlabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));

        summaryPageLayout->addWidget(iconlabel, 0, Qt::AlignHCenter);

        pageTitleLabel = new QLabel(VoteWizardSummaryPage);
        pageTitleLabel->setObjectName("pageTitleLabel");
        pageTitleLabel->setAlignment(Qt::AlignCenter);

        summaryPageLayout->addWidget(pageTitleLabel);

        titleSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        summaryPageLayout->addItem(titleSpacer);

        pollTitleLabel = new QLabel(VoteWizardSummaryPage);
        pollTitleLabel->setObjectName("pollTitleLabel");
        pollTitleLabel->setAlignment(Qt::AlignCenter);
        pollTitleLabel->setWordWrap(true);

        summaryPageLayout->addWidget(pollTitleLabel);

        responsesLabel = new QLabel(VoteWizardSummaryPage);
        responsesLabel->setObjectName("responsesLabel");
        responsesLabel->setAlignment(Qt::AlignCenter);
        responsesLabel->setWordWrap(true);
        responsesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        summaryPageLayout->addWidget(responsesLabel);

        middleSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        summaryPageLayout->addItem(middleSpacer);

        hintLabel = new QLabel(VoteWizardSummaryPage);
        hintLabel->setObjectName("hintLabel");
        hintLabel->setAlignment(Qt::AlignCenter);
        hintLabel->setWordWrap(true);

        summaryPageLayout->addWidget(hintLabel);

        voteIdLabel = new QLabel(VoteWizardSummaryPage);
        voteIdLabel->setObjectName("voteIdLabel");
        voteIdLabel->setAlignment(Qt::AlignCenter);
        voteIdLabel->setWordWrap(true);
        voteIdLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        summaryPageLayout->addWidget(voteIdLabel);

        copyToClipboardButton = new QPushButton(VoteWizardSummaryPage);
        copyToClipboardButton->setObjectName("copyToClipboardButton");

        summaryPageLayout->addWidget(copyToClipboardButton, 0, Qt::AlignHCenter);

        bottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        summaryPageLayout->addItem(bottomSpacer);


        retranslateUi(VoteWizardSummaryPage);

        QMetaObject::connectSlotsByName(VoteWizardSummaryPage);
    } // setupUi

    void retranslateUi(QWizardPage *VoteWizardSummaryPage)
    {
        pageTitleLabel->setText(QCoreApplication::translate("VoteWizardSummaryPage", "Vote Submitted", nullptr));
        hintLabel->setText(QCoreApplication::translate("VoteWizardSummaryPage", "Your vote will tally with the next block.", nullptr));
        copyToClipboardButton->setText(QCoreApplication::translate("VoteWizardSummaryPage", "Copy ID", nullptr));
        (void)VoteWizardSummaryPage;
    } // retranslateUi

};

namespace Ui {
    class VoteWizardSummaryPage: public Ui_VoteWizardSummaryPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VOTEWIZARDSUMMARYPAGE_H
