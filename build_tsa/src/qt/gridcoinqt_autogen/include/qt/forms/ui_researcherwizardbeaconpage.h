/********************************************************************************
** Form generated from reading UI file 'researcherwizardbeaconpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDBEACONPAGE_H
#define UI_RESEARCHERWIZARDBEACONPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardBeaconPage
{
public:
    QVBoxLayout *beaconPageLayout;
    QSpacerItem *headerSpacer;
    QLabel *beaconIconLabel;
    QLabel *beaconStatusLabel;
    QHBoxLayout *verificationMethodOuterLayout;
    QSpacerItem *verificationMethodLeftSpacer;
    QWidget *verificationMethodWrapper;
    QVBoxLayout *verificationMethodLayout;
    QLabel *verificationMethodLabel;
    QRadioButton *v2RadioButton;
    QRadioButton *v3RadioButton;
    QLabel *v3UnavailableLabel;
    QSpacerItem *verificationMethodRightSpacer;
    QPushButton *sendBeaconButton;
    QHBoxLayout *cpidLayout;
    QSpacerItem *cpidLeftSpacer;
    QLabel *cpidLabelLabel;
    QLabel *cpidLabel;
    QSpacerItem *cpidRightSpacer;
    QWidget *continuePromptWrapper;
    QHBoxLayout *continuePromptLayout;
    QSpacerItem *continuePromptLeftSpacer;
    QLabel *continuePromptIconLabel;
    QLabel *continuePromptLabel;
    QSpacerItem *continuePromptRightSpacer;
    QSpacerItem *footerSpacer;

    void setupUi(QWizardPage *ResearcherWizardBeaconPage)
    {
        if (ResearcherWizardBeaconPage->objectName().isEmpty())
            ResearcherWizardBeaconPage->setObjectName("ResearcherWizardBeaconPage");
        ResearcherWizardBeaconPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardBeaconPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardBeaconPage->setSizePolicy(sizePolicy);
        beaconPageLayout = new QVBoxLayout(ResearcherWizardBeaconPage);
        beaconPageLayout->setObjectName("beaconPageLayout");
        headerSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        beaconPageLayout->addItem(headerSpacer);

        beaconIconLabel = new QLabel(ResearcherWizardBeaconPage);
        beaconIconLabel->setObjectName("beaconIconLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(beaconIconLabel->sizePolicy().hasHeightForWidth());
        beaconIconLabel->setSizePolicy(sizePolicy1);
        beaconIconLabel->setMinimumSize(QSize(48, 48));
        beaconIconLabel->setMaximumSize(QSize(48, 48));
        beaconIconLabel->setScaledContents(true);
        beaconIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        beaconPageLayout->addWidget(beaconIconLabel, 0, Qt::AlignHCenter);

        beaconStatusLabel = new QLabel(ResearcherWizardBeaconPage);
        beaconStatusLabel->setObjectName("beaconStatusLabel");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(beaconStatusLabel->sizePolicy().hasHeightForWidth());
        beaconStatusLabel->setSizePolicy(sizePolicy2);
        beaconStatusLabel->setText(QString::fromUtf8(""));
        beaconStatusLabel->setAlignment(Qt::AlignCenter);

        beaconPageLayout->addWidget(beaconStatusLabel);

        verificationMethodOuterLayout = new QHBoxLayout();
        verificationMethodOuterLayout->setObjectName("verificationMethodOuterLayout");
        verificationMethodLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        verificationMethodOuterLayout->addItem(verificationMethodLeftSpacer);

        verificationMethodWrapper = new QWidget(ResearcherWizardBeaconPage);
        verificationMethodWrapper->setObjectName("verificationMethodWrapper");
        verificationMethodLayout = new QVBoxLayout(verificationMethodWrapper);
        verificationMethodLayout->setObjectName("verificationMethodLayout");
        verificationMethodLabel = new QLabel(verificationMethodWrapper);
        verificationMethodLabel->setObjectName("verificationMethodLabel");
        verificationMethodLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        verificationMethodLayout->addWidget(verificationMethodLabel);

        v2RadioButton = new QRadioButton(verificationMethodWrapper);
        v2RadioButton->setObjectName("v2RadioButton");
        v2RadioButton->setChecked(true);

        verificationMethodLayout->addWidget(v2RadioButton);

        v3RadioButton = new QRadioButton(verificationMethodWrapper);
        v3RadioButton->setObjectName("v3RadioButton");

        verificationMethodLayout->addWidget(v3RadioButton);

        v3UnavailableLabel = new QLabel(verificationMethodWrapper);
        v3UnavailableLabel->setObjectName("v3UnavailableLabel");
        v3UnavailableLabel->setText(QString::fromUtf8(""));
        v3UnavailableLabel->setWordWrap(true);
        v3UnavailableLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        verificationMethodLayout->addWidget(v3UnavailableLabel);


        verificationMethodOuterLayout->addWidget(verificationMethodWrapper);

        verificationMethodRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        verificationMethodOuterLayout->addItem(verificationMethodRightSpacer);


        beaconPageLayout->addLayout(verificationMethodOuterLayout);

        sendBeaconButton = new QPushButton(ResearcherWizardBeaconPage);
        sendBeaconButton->setObjectName("sendBeaconButton");
        sizePolicy1.setHeightForWidth(sendBeaconButton->sizePolicy().hasHeightForWidth());
        sendBeaconButton->setSizePolicy(sizePolicy1);

        beaconPageLayout->addWidget(sendBeaconButton, 0, Qt::AlignHCenter);

        cpidLayout = new QHBoxLayout();
        cpidLayout->setObjectName("cpidLayout");
        cpidLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        cpidLayout->addItem(cpidLeftSpacer);

        cpidLabelLabel = new QLabel(ResearcherWizardBeaconPage);
        cpidLabelLabel->setObjectName("cpidLabelLabel");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(cpidLabelLabel->sizePolicy().hasHeightForWidth());
        cpidLabelLabel->setSizePolicy(sizePolicy3);
        cpidLabelLabel->setText(QString::fromUtf8("CPID:"));

        cpidLayout->addWidget(cpidLabelLabel);

        cpidLabel = new QLabel(ResearcherWizardBeaconPage);
        cpidLabel->setObjectName("cpidLabel");
        sizePolicy3.setHeightForWidth(cpidLabel->sizePolicy().hasHeightForWidth());
        cpidLabel->setSizePolicy(sizePolicy3);
        cpidLabel->setText(QString::fromUtf8(""));

        cpidLayout->addWidget(cpidLabel);

        cpidRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        cpidLayout->addItem(cpidRightSpacer);


        beaconPageLayout->addLayout(cpidLayout);

        continuePromptWrapper = new QWidget(ResearcherWizardBeaconPage);
        continuePromptWrapper->setObjectName("continuePromptWrapper");
        continuePromptLayout = new QHBoxLayout(continuePromptWrapper);
        continuePromptLayout->setObjectName("continuePromptLayout");
        continuePromptLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        continuePromptLayout->addItem(continuePromptLeftSpacer);

        continuePromptIconLabel = new QLabel(continuePromptWrapper);
        continuePromptIconLabel->setObjectName("continuePromptIconLabel");
        sizePolicy1.setHeightForWidth(continuePromptIconLabel->sizePolicy().hasHeightForWidth());
        continuePromptIconLabel->setSizePolicy(sizePolicy1);
        continuePromptIconLabel->setMinimumSize(QSize(16, 16));
        continuePromptIconLabel->setMaximumSize(QSize(16, 16));
        continuePromptIconLabel->setText(QString::fromUtf8(""));
        continuePromptIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        continuePromptIconLabel->setScaledContents(true);
        continuePromptIconLabel->setAlignment(Qt::AlignCenter);
        continuePromptIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        continuePromptLayout->addWidget(continuePromptIconLabel);

        continuePromptLabel = new QLabel(continuePromptWrapper);
        continuePromptLabel->setObjectName("continuePromptLabel");
        sizePolicy3.setHeightForWidth(continuePromptLabel->sizePolicy().hasHeightForWidth());
        continuePromptLabel->setSizePolicy(sizePolicy3);
        continuePromptLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        continuePromptLayout->addWidget(continuePromptLabel);

        continuePromptRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        continuePromptLayout->addItem(continuePromptRightSpacer);


        beaconPageLayout->addWidget(continuePromptWrapper);

        footerSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        beaconPageLayout->addItem(footerSpacer);


        retranslateUi(ResearcherWizardBeaconPage);

        QMetaObject::connectSlotsByName(ResearcherWizardBeaconPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardBeaconPage)
    {
        ResearcherWizardBeaconPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardBeaconPage", "Beacon Advertisement", nullptr));
        ResearcherWizardBeaconPage->setTitle(QCoreApplication::translate("ResearcherWizardBeaconPage", "Beacon Advertisement", nullptr));
        ResearcherWizardBeaconPage->setSubTitle(QCoreApplication::translate("ResearcherWizardBeaconPage", "A beacon links your BOINC accounts to your wallet. After sending a beacon, the network tracks your BOINC statistics to calculate research rewards.", nullptr));
        beaconIconLabel->setText(QString());
        verificationMethodLabel->setText(QCoreApplication::translate("ResearcherWizardBeaconPage", "Choose verification method:", nullptr));
        v2RadioButton->setText(QCoreApplication::translate("ResearcherWizardBeaconPage", "Classic verification (change BOINC username)", nullptr));
        v3RadioButton->setText(QCoreApplication::translate("ResearcherWizardBeaconPage", "Account ownership proof (recommended if available)", nullptr));
        sendBeaconButton->setText(QCoreApplication::translate("ResearcherWizardBeaconPage", "&Advertise Beacon", nullptr));
        continuePromptLabel->setText(QCoreApplication::translate("ResearcherWizardBeaconPage", "Press \"Next\" to continue.", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardBeaconPage: public Ui_ResearcherWizardBeaconPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDBEACONPAGE_H
