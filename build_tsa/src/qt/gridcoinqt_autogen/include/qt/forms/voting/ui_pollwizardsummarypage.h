/********************************************************************************
** Form generated from reading UI file 'pollwizardsummarypage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLWIZARDSUMMARYPAGE_H
#define UI_POLLWIZARDSUMMARYPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_PollWizardSummaryPage
{
public:
    QVBoxLayout *summaryPageLayout;
    QSpacerItem *topSpacer;
    QLabel *iconlabel;
    QLabel *pageTitleLabel;
    QSpacerItem *titleSpacer;
    QLabel *pollTitleLabel;
    QSpacerItem *middleSpacer;
    QLabel *hintLabel;
    QLabel *pollIdLabel;
    QPushButton *copyToClipboardButton;
    QSpacerItem *bottomSpacer;

    void setupUi(QWizardPage *PollWizardSummaryPage)
    {
        if (PollWizardSummaryPage->objectName().isEmpty())
            PollWizardSummaryPage->setObjectName("PollWizardSummaryPage");
        PollWizardSummaryPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(PollWizardSummaryPage->sizePolicy().hasHeightForWidth());
        PollWizardSummaryPage->setSizePolicy(sizePolicy);
        summaryPageLayout = new QVBoxLayout(PollWizardSummaryPage);
        summaryPageLayout->setSpacing(9);
        summaryPageLayout->setObjectName("summaryPageLayout");
        summaryPageLayout->setContentsMargins(16, 16, 16, 16);
        topSpacer = new QSpacerItem(20, 60, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        summaryPageLayout->addItem(topSpacer);

        iconlabel = new QLabel(PollWizardSummaryPage);
        iconlabel->setObjectName("iconlabel");
        iconlabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));

        summaryPageLayout->addWidget(iconlabel, 0, Qt::AlignHCenter);

        pageTitleLabel = new QLabel(PollWizardSummaryPage);
        pageTitleLabel->setObjectName("pageTitleLabel");
        pageTitleLabel->setAlignment(Qt::AlignCenter);

        summaryPageLayout->addWidget(pageTitleLabel);

        titleSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        summaryPageLayout->addItem(titleSpacer);

        pollTitleLabel = new QLabel(PollWizardSummaryPage);
        pollTitleLabel->setObjectName("pollTitleLabel");
        pollTitleLabel->setAlignment(Qt::AlignCenter);
        pollTitleLabel->setWordWrap(true);

        summaryPageLayout->addWidget(pollTitleLabel);

        middleSpacer = new QSpacerItem(20, 24, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        summaryPageLayout->addItem(middleSpacer);

        hintLabel = new QLabel(PollWizardSummaryPage);
        hintLabel->setObjectName("hintLabel");
        hintLabel->setAlignment(Qt::AlignCenter);
        hintLabel->setWordWrap(true);

        summaryPageLayout->addWidget(hintLabel);

        pollIdLabel = new QLabel(PollWizardSummaryPage);
        pollIdLabel->setObjectName("pollIdLabel");
        pollIdLabel->setAlignment(Qt::AlignCenter);
        pollIdLabel->setWordWrap(true);
        pollIdLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        summaryPageLayout->addWidget(pollIdLabel);

        copyToClipboardButton = new QPushButton(PollWizardSummaryPage);
        copyToClipboardButton->setObjectName("copyToClipboardButton");

        summaryPageLayout->addWidget(copyToClipboardButton, 0, Qt::AlignHCenter);

        bottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        summaryPageLayout->addItem(bottomSpacer);


        retranslateUi(PollWizardSummaryPage);

        QMetaObject::connectSlotsByName(PollWizardSummaryPage);
    } // setupUi

    void retranslateUi(QWizardPage *PollWizardSummaryPage)
    {
        pageTitleLabel->setText(QCoreApplication::translate("PollWizardSummaryPage", "Poll Created", nullptr));
        hintLabel->setText(QCoreApplication::translate("PollWizardSummaryPage", "The poll will activate with the next block.", nullptr));
        copyToClipboardButton->setText(QCoreApplication::translate("PollWizardSummaryPage", "Copy ID", nullptr));
        (void)PollWizardSummaryPage;
    } // retranslateUi

};

namespace Ui {
    class PollWizardSummaryPage: public Ui_PollWizardSummaryPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLWIZARDSUMMARYPAGE_H
