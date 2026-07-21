/********************************************************************************
** Form generated from reading UI file 'researcherwizardnoncruncherpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDNONCRUNCHERPAGE_H
#define UI_RESEARCHERWIZARDNONCRUNCHERPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardNoncruncherPage
{
public:
    QVBoxLayout *noncruncherPageLayout;
    QSpacerItem *headerSpacer;
    QLabel *noncruncherIconLabel;
    QLabel *headerLabel;
    QSpacerItem *verticalSpacer;
    QLabel *noncruncherLabel;
    QLabel *startOverLabel;
    QSpacerItem *footerSpacer;

    void setupUi(QWizardPage *ResearcherWizardNoncruncherPage)
    {
        if (ResearcherWizardNoncruncherPage->objectName().isEmpty())
            ResearcherWizardNoncruncherPage->setObjectName("ResearcherWizardNoncruncherPage");
        ResearcherWizardNoncruncherPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardNoncruncherPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardNoncruncherPage->setSizePolicy(sizePolicy);
        noncruncherPageLayout = new QVBoxLayout(ResearcherWizardNoncruncherPage);
        noncruncherPageLayout->setObjectName("noncruncherPageLayout");
        headerSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        noncruncherPageLayout->addItem(headerSpacer);

        noncruncherIconLabel = new QLabel(ResearcherWizardNoncruncherPage);
        noncruncherIconLabel->setObjectName("noncruncherIconLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(noncruncherIconLabel->sizePolicy().hasHeightForWidth());
        noncruncherIconLabel->setSizePolicy(sizePolicy1);
        noncruncherIconLabel->setMinimumSize(QSize(64, 64));
        noncruncherIconLabel->setMaximumSize(QSize(64, 64));
        noncruncherIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/ic_noncruncher_active")));
        noncruncherIconLabel->setScaledContents(true);
        noncruncherIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        noncruncherPageLayout->addWidget(noncruncherIconLabel, 0, Qt::AlignHCenter);

        headerLabel = new QLabel(ResearcherWizardNoncruncherPage);
        headerLabel->setObjectName("headerLabel");
        headerLabel->setAlignment(Qt::AlignCenter);
        headerLabel->setMargin(16);

        noncruncherPageLayout->addWidget(headerLabel);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        noncruncherPageLayout->addItem(verticalSpacer);

        noncruncherLabel = new QLabel(ResearcherWizardNoncruncherPage);
        noncruncherLabel->setObjectName("noncruncherLabel");
        noncruncherLabel->setAlignment(Qt::AlignCenter);
        noncruncherLabel->setWordWrap(true);

        noncruncherPageLayout->addWidget(noncruncherLabel, 0, Qt::AlignVCenter);

        startOverLabel = new QLabel(ResearcherWizardNoncruncherPage);
        startOverLabel->setObjectName("startOverLabel");
        startOverLabel->setAlignment(Qt::AlignCenter);
        startOverLabel->setMargin(20);

        noncruncherPageLayout->addWidget(startOverLabel);

        footerSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        noncruncherPageLayout->addItem(footerSpacer);


        retranslateUi(ResearcherWizardNoncruncherPage);

        QMetaObject::connectSlotsByName(ResearcherWizardNoncruncherPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardNoncruncherPage)
    {
        ResearcherWizardNoncruncherPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardNoncruncherPage", "Summary", nullptr));
        noncruncherIconLabel->setText(QString());
        headerLabel->setText(QCoreApplication::translate("ResearcherWizardNoncruncherPage", "Non-cruncher Mode", nullptr));
        noncruncherLabel->setText(QCoreApplication::translate("ResearcherWizardNoncruncherPage", "You opted out of research rewards and will earn staking rewards only.", nullptr));
        startOverLabel->setText(QCoreApplication::translate("ResearcherWizardNoncruncherPage", "Press \"Start Over\" if you want to switch modes.", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardNoncruncherPage: public Ui_ResearcherWizardNoncruncherPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDNONCRUNCHERPAGE_H
